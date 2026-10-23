#include "Input.hpp"
#include "Runtime.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <dlfcn.h>

namespace dabr {
std::int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
namespace {
struct LegacyInput {
    void (*touch)(bool (*)(int, int, float, float));
    void (*key)(bool (*)(int, unsigned int, bool));
    void (*showKeyboard)();
    void (*hideKeyboard)();
    void (*mouse)(bool (*)(int, bool));
    void (*text)(bool (*)(const char*, std::size_t));
};
}
Input& Input::get() { static Input input; return input; }
bool Input::padDriving() const { return Settings::get().snapshot()->enableGamepad && nowMs() < padActiveUntil.load(); }
void Input::allow(bool value) { allowed.store(value, std::memory_order_release); }

bool keyEvent(int code, unsigned int, bool down) {
    auto& input = Input::get();
    if (code < 0 || code >= static_cast<int>(input.held.size())) return false;
    const bool previous = input.held[code].exchange(down);
    if (!input.allowed.load(std::memory_order_acquire)) return false;
    const auto config = Settings::get().snapshot();
    if (!code || config->bindings[0] != code) return false;
    if (down && !previous && !input.logical[0].load()) input.toggles.fetch_or(1);
    return true;
}

bool touchEvent(int action, int, float, float) {
    auto& input = Input::get();
    if (action == 0 || action == 2 || action == 5) input.touchTime.store(nowMs());
    if (action == 3) input.clear(false);
    return false;
}

bool Input::connect() {
    if (registered) return true;
    auto* handle = dlopen("libpreloader.so", RTLD_NOW | RTLD_NOLOAD);
    if (!handle) return false;
    const auto get = reinterpret_cast<LegacyInput* (*)()>(dlsym(handle, "GetPreloaderInput"));
    auto* input = get ? get() : nullptr;
    if (!input || !input->touch || !input->key) { dlclose(handle); return false; }
    Dl_info self{};
    if (!dladdr(reinterpret_cast<void*>(keyEvent), &self) || !self.dli_fname ||
        !dlopen(self.dli_fname, RTLD_NOW | RTLD_NODELETE)) { dlclose(handle); return false; }
    input->touch(touchEvent);
    input->key(keyEvent);
    registered = true;
    dlclose(handle);
    return true;
}

void Input::button(std::size_t action, bool down) {
    if (action >= logical.size()) return;
    const bool previous = logical[action].exchange(down);
    if (action == 0 && down && !previous && allowed.load()) {
        const auto config = Settings::get().snapshot();
        const auto code = config->bindings[action];
        if (!(code > 0 && code < static_cast<int>(held.size()) && held[code].load())) toggles.fetch_or(1u << action);
    }
}

void Input::clear(bool padState) {
    for (auto& key : held) key.store(false);
    for (auto& key : logical) key.store(false);
    toggles.store(0);
    if (!padState) return;
    std::lock_guard lock(mutex);
    pads.clear();
    lastStick = 0;
    padActiveUntil.store(0);
}

void Input::gamepad(const GamepadEvent& event) {
    if (event.controllerId < 0 || event.controllerId > 31) return;
    std::lock_guard lock(mutex);
    auto& pad = pads[event.controllerId];
    const int id = read<int>(event.data.data());
    if (event.type == 1 && id == 2) {
        const auto state = read<int>(event.data.data(), 4);
        const auto x = read<float>(event.data.data(), 8), y = read<float>(event.data.data(), 12);
        if (std::isfinite(x) && std::isfinite(y)) {
            pad.x = state == 2 ? 0 : std::clamp<double>(x, -1, 1);
            pad.y = state == 2 ? 0 : std::clamp<double>(y, -1, 1);
            const auto config = Settings::get().snapshot();
            if (config->enableGamepad && std::hypot(pad.x, pad.y) > config->gamepadDeadzone) padActiveUntil.store(nowMs() + 150);
        }
    } else if (event.type == 0) {
        const bool down = read<int>(event.data.data(), 4) == 1;
        if (id == 5) pad.left = down;
        if (id == 6) pad.right = down;
    } else if (event.type == 5 && !read<bool>(event.data.data())) pads.erase(event.controllerId);
}

FlightInput Input::poll(const core::Config& config, bool permitted) {
    allow(permitted);
    FlightInput out;
    if (!permitted) { clear(false); return out; }
    const auto pending = toggles.exchange(0);
    if (pending & 1) Runtime::get().toggle();
    const auto down = [&](std::size_t i) {
        const int code = config.bindings[i];
        return logical[i].load() || (code > 0 && code < static_cast<int>(held.size()) && held[code].load());
    };
    out.keys.pitchUp = down(1); out.keys.pitchDown = down(2);
    out.keys.yawLeft = down(3); out.keys.yawRight = down(4);
    out.keys.rollLeft = down(5); out.keys.rollRight = down(6);
    out.touchLook = nowMs() - touchTime.load() < 300;
    if (!config.enableGamepad) return out;
    std::lock_guard lock(mutex);
    double x = 0, y = 0;
    for (const auto& [id, pad] : pads) {
        if (pad.x * pad.x + pad.y * pad.y > x * x + y * y) { x = pad.x; y = pad.y; }
        out.pad.yaw = std::clamp(out.pad.yaw + (pad.right ? 1 : 0) - (pad.left ? 1 : 0), -1.0, 1.0);
    }
    const double magnitude = std::hypot(x, y), deadzone = config.gamepadDeadzone;
    if (magnitude > deadzone) {
        const double scale = std::min((magnitude - deadzone) / (1 - deadzone), 1.0) / magnitude;
        out.pad.stickX = x * scale;
        out.pad.stickY = y * scale;
        lastStick = nowMs();
    }
    out.padLook = lastStick && nowMs() - lastStick < 150;
    return out;
}
}
