// setupCamera builds every matrix, snapshot and vector from CameraComponent::mOrientation of the entity tagged
// RenderCameraComponent, so roll is injected there before the call and restored after rendering (the camera is set up
// in both _renderLevelPrep and render). If the component is not found, the finished view matrix is edited instead.

#include "levilamina/hooks/State.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/screens/InGamePlayScreen.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/deps/ecs/gamerefs_entity/EntityContext.h"
#include "mc/deps/minecraft_camera/components/CameraComponent.h"
#include "mc/deps/minecraft_camera/components/RenderCameraComponent.h"
#include "mc/deps/renderer/Camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>

// The mod's entt must compute the same component type ids as the game.
static_assert(
    entt::type_hash<MinecraftCamera::CameraComponent>::value() == 0x4f6047c7u,
    "entt type hash of CameraComponent differs from the game's"
);
static_assert(
    entt::type_hash<MinecraftCamera::RenderCameraComponent>::value() == 0x119e772bu,
    "entt type hash of RenderCameraComponent differs from the game's"
);

namespace do_a_barrel_roll::hooks {

namespace {

using Registry = entt::basic_registry<EntityId>;

struct Basis {
    glm::dvec3 r, u, f;
};

struct PendingRestore {
    bool      active   = false;
    Registry* registry = nullptr;
    EntityId  entity{};
    glm::quat original{};
};
PendingRestore pending;

glm::dvec3 rotateAxis(glm::dvec3 v, glm::dvec3 k, double a) {
    double c = std::cos(a), s = std::sin(a);
    return v * c + glm::cross(k, v) * s + k * (glm::dot(k, v) * (1.0 - c));
}

// Positive roll is right wing down, the same convention as the core.
Basis rollBasis(Basis b, double rollDeg) {
    double a = glm::radians(rollDeg);
    b.r      = rotateAxis(b.r, b.f, a);
    b.u      = rotateAxis(b.u, b.f, a);
    return b;
}

Basis makeBasis(double pitchDeg, double yawDeg, double rollDeg) {
    double     p = glm::radians(pitchDeg), y = glm::radians(yawDeg);
    glm::dvec3 f{-std::sin(y) * std::cos(p), -std::sin(p), std::cos(y) * std::cos(p)};
    glm::dvec3 r{-std::cos(y), 0.0, -std::sin(y)};
    Basis      b{r, glm::cross(r, f), f};
    return rollBasis(b, rollDeg);
}

Basis wantedBasis(Basis const& current) {
    float roll = state::cameraRollDeg;
    return state::cameraOverride ? makeBasis(state::cameraPitch, state::cameraYaw, roll) : rollBasis(current, roll);
}

glm::dvec3 row(glm::mat4 const& m, int i) { return {m[0][i], m[1][i], m[2][i]}; }

MinecraftCamera::CameraComponent* findRenderCamera(Registry*& registryOut, EntityId& entityOut) {
    auto client = ll::service::getClientInstance();
    if (!client) return nullptr;
    auto* player = client->getLocalPlayer();
    if (!player) return nullptr;
    Registry&       reg  = player->getEntityContext().getRegistry();
    Registry const& creg = reg;
    auto const*     tags = creg.storage<MinecraftCamera::RenderCameraComponent>();
    if (!tags) return nullptr;
    for (EntityId e : *tags) {
        if (auto const* cc = creg.try_get<MinecraftCamera::CameraComponent>(e)) {
            registryOut = &reg;
            entityOut   = e;
            return const_cast<MinecraftCamera::CameraComponent*>(cc);
        }
    }
    return nullptr;
}

void restoreOrientation() {
    if (!pending.active) return;
    pending.active = false;
    if (auto* cc = pending.registry->try_get<MinecraftCamera::CameraComponent>(pending.entity)) {
        cc->mOrientation.get() = pending.original;
    }
}

bool injectIntoComponent(MinecraftCamera::CameraComponent& cc, Registry* reg, EntityId entity) {
    glm::quat q0 = cc.mOrientation.get();
    glm::mat3 r0 = glm::mat3_cast(q0);
    Basis     t  = wantedBasis({glm::dvec3(r0[0]), glm::dvec3(r0[1]), -glm::dvec3(r0[2])});

    glm::quat q1 = glm::normalize(glm::quat_cast(glm::mat3{glm::vec3(t.r), glm::vec3(t.u), glm::vec3(-t.f)}));
    if (!std::isfinite(q1.x) || !std::isfinite(q1.y) || !std::isfinite(q1.z) || !std::isfinite(q1.w)) return false;
    // q and -q are the same rotation; stay in the game's hemisphere to avoid sign flips.
    if (glm::dot(q0, q1) < 0.0f) q1 = -q1;

    pending               = {true, reg, entity, q0};
    cc.mOrientation.get() = q1;
    return true;
}

bool applyMatrixFallback(mce::Camera& camera) {
    glm::mat4& view = *camera.viewMatrixStack->getTop()._m;

    glm::dvec3 rr = row(view, 0), uu = row(view, 1), ff = -row(view, 2);
    Basis      b0{rr, uu, ff};
    if (auto client = ll::service::getClientInstance()) {
        if (auto* player = client->getLocalPlayer()) {
            auto const& rot = player->getRotation();
            Basis       pb  = makeBasis(rot.x, rot.y, 0.0);
            if (glm::dot(ff, pb.f) < 0.7 || glm::dot(rr, pb.r) < 0.7) b0 = pb;
        }
    }
    Basis     target = wantedBasis(b0);
    glm::mat3 w      = glm::outerProduct(b0.r, target.r) + glm::outerProduct(b0.u, target.u)
                + glm::outerProduct(b0.f, target.f);
    glm::vec3 eye = -(glm::transpose(glm::mat3(view)) * glm::vec3(view[3]));
    glm::mat4 res = view * glm::translate(glm::mat4(1.0f), eye) * glm::mat4(w) * glm::translate(glm::mat4(1.0f), -eye);
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (!std::isfinite(res[c][r])) return false;
    view = res;
    return true;
}

LL_TYPE_INSTANCE_HOOK(
    SetupCameraHook,
    ll::memory::HookPriority::Normal,
    LevelRendererPlayer,
    &LevelRendererPlayer::setupCamera,
    void,
    mce::Camera& camera,
    float        a
) {
    // Never stack the roll on itself if a previous scope was not closed.
    restoreOrientation();

    if (!state::cameraOverride && state::cameraRollDeg == 0.0f) {
        origin(camera, a);
        return;
    }

    Registry* reg = nullptr;
    EntityId  entity{};
    auto*     cc       = findRenderCamera(reg, entity);
    bool      injected = cc && injectIntoComponent(*cc, reg, entity);

    origin(camera, a);

    if (!injected && applyMatrixFallback(camera)) camera.updateViewMatrixDependencies();
}

LL_TYPE_INSTANCE_HOOK(
    RenderLevelPrepHook,
    ll::memory::HookPriority::Normal,
    InGamePlayScreen,
    &InGamePlayScreen::$_renderLevelPrep,
    void,
    ScreenContext& screenContext,
    LevelRenderer& levelRenderer,
    Actor&         cameraEntity
) {
    origin(screenContext, levelRenderer, cameraEntity);
    restoreOrientation();
}

LL_TYPE_INSTANCE_HOOK(
    ScreenRenderHook,
    ll::memory::HookPriority::Normal,
    InGamePlayScreen,
    &InGamePlayScreen::$render,
    void,
    ScreenContext&           screenContext,
    FrameRenderObject const& renderObj
) {
    origin(screenContext, renderObj);
    restoreOrientation();
}

} // namespace

void installCamera() {
    pending = {};
    SetupCameraHook::hook();
    RenderLevelPrepHook::hook();
    ScreenRenderHook::hook();
}

void uninstallCamera() {
    ScreenRenderHook::unhook();
    RenderLevelPrepHook::unhook();
    restoreOrientation();
    SetupCameraHook::unhook();
}

} // namespace do_a_barrel_roll::hooks
