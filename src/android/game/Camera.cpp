#include "Camera.hpp"
#include "Game.hpp"
#include "Hooks.hpp"
#include "State.hpp"
#include "runtime/Runtime.hpp"
#include <cmath>

namespace dabr {
namespace {
SetupCameraFn setupNext{};
RenderPrepFn prepNext{};
RenderFn renderNext{};
struct Basis { core::Vec3 right, up, forward; };
struct Pending {
    void* component{};
    Quaternion orientation;
};
thread_local Pending pending;

core::Vec3 rotate(core::Vec3 v, Quaternion q) {
    const core::Vec3 axis{q.x, q.y, q.z};
    return v + axis.cross(v) * (2 * q.w) + axis.cross(axis.cross(v)) * 2;
}
Basis rollBasis(Basis basis, double degrees) {
    basis.right = basis.right.rotatedAxis(degrees * core::kToRad, basis.forward);
    basis.up = basis.up.rotatedAxis(degrees * core::kToRad, basis.forward);
    return basis;
}
Basis makeBasis(double pitch, double yaw, double roll) {
    const auto p = pitch * core::kToRad, y = yaw * core::kToRad;
    const core::Vec3 forward{-std::sin(y) * std::cos(p), -std::sin(p), std::cos(y) * std::cos(p)};
    const core::Vec3 right{-std::cos(y), 0, -std::sin(y)};
    return rollBasis({right, right.cross(forward), forward}, roll);
}
Quaternion quaternion(Basis basis) {
    const double m[3][3]{{basis.right.x, basis.up.x, -basis.forward.x}, {basis.right.y, basis.up.y, -basis.forward.y}, {basis.right.z, basis.up.z, -basis.forward.z}};
    double x, y, z, w;
    const auto trace = m[0][0] + m[1][1] + m[2][2];
    if (trace > 0) {
        const auto s = std::sqrt(trace + 1) * 2;
        w = 0.25 * s; x = (m[2][1] - m[1][2]) / s; y = (m[0][2] - m[2][0]) / s; z = (m[1][0] - m[0][1]) / s;
    } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
        const auto s = std::sqrt(1 + m[0][0] - m[1][1] - m[2][2]) * 2;
        w = (m[2][1] - m[1][2]) / s; x = 0.25 * s; y = (m[0][1] + m[1][0]) / s; z = (m[0][2] + m[2][0]) / s;
    } else if (m[1][1] > m[2][2]) {
        const auto s = std::sqrt(1 + m[1][1] - m[0][0] - m[2][2]) * 2;
        w = (m[0][2] - m[2][0]) / s; x = (m[0][1] + m[1][0]) / s; y = 0.25 * s; z = (m[1][2] + m[2][1]) / s;
    } else {
        const auto s = std::sqrt(1 + m[2][2] - m[0][0] - m[1][1]) * 2;
        w = (m[1][0] - m[0][1]) / s; x = (m[0][2] + m[2][0]) / s; y = (m[1][2] + m[2][1]) / s; z = 0.25 * s;
    }
    const auto length = std::sqrt(x * x + y * y + z * z + w * w);
    return {static_cast<float>(x / length), static_cast<float>(y / length), static_cast<float>(z / length), static_cast<float>(w / length)};
}
void setupHook(void* renderer, void* camera, float partial) {
    restoreCamera();
    auto& runtime = Runtime::get();
    auto& state = flight();
    if (runtime.active() && state.client && state.player && (state.cameraOverride || state.cameraRoll != 0)) {
        auto* component = runtime.native().cameraComponent(state.client);
        if (component) {
            const auto original = read<Quaternion>(component, build::field::CameraOrientation);
            const Basis current{rotate({1, 0, 0}, original), rotate({0, 1, 0}, original), rotate({0, 0, -1}, original)};
            Basis target = rollBasis(current, state.cameraRoll);
            if (state.cameraOverride) {
                const auto actual = game::snapshot(state.player, runtime.native());
                const bool front = current.forward.dot(core::lookVector(actual.pitch, actual.yaw)) < -0.5;
                target = front ? makeBasis(-state.pitch, state.yaw + 180, -state.cameraRoll) : makeBasis(state.pitch, state.yaw, state.cameraRoll);
            }
            auto value = quaternion(target);
            if (std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && std::isfinite(value.w)) {
                if (value.x * original.x + value.y * original.y + value.z * original.z + value.w * original.w < 0) { value.x = -value.x; value.y = -value.y; value.z = -value.z; value.w = -value.w; }
                pending = {component, original};
                write(component, build::field::CameraOrientation, value);
            }
        }
    }
    setupNext(renderer, camera, partial);
}
void prepHook(void* screen, void* context, void* renderer, void* actor) {
    prepNext(screen, context, renderer, actor);
    restoreCamera();
}
void renderHook(void* screen, void* context, const void* frame) {
    renderNext(screen, context, frame);
    restoreCamera();
}
}
void restoreCamera() {
    if (!pending.component) return;
    write(pending.component, build::field::CameraOrientation, pending.orientation);
    pending.component = nullptr;
}
bool installCameraHooks(Host& host, const Targets& target) {
    return host.install(reinterpret_cast<void*>(target.setupCamera), reinterpret_cast<void*>(setupHook), reinterpret_cast<void**>(&setupNext)) &&
        host.install(reinterpret_cast<void*>(target.renderPrep), reinterpret_cast<void*>(prepHook), reinterpret_cast<void**>(&prepNext)) &&
        host.install(reinterpret_cast<void*>(target.render), reinterpret_cast<void*>(renderHook), reinterpret_cast<void**>(&renderNext));
}
}
