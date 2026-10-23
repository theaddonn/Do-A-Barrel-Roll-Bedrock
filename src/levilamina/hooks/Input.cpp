#include "levilamina/hooks/State.h"

#include "ll/api/memory/Hook.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/math/Vec2.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace do_a_barrel_roll::hooks {

namespace {

using Clock = std::chrono::steady_clock;

constexpr double kDegreesPerMouseUnit = 0.15;
constexpr double kResyncAngleDeg      = 15.0;
constexpr float  kTickSeconds         = 0.05f;

Clock::time_point lastFrame{};
Clock::time_point lastTick{};
bool              applyingOwnTurn = false;
// Controller-authoritative orientation; the game applies turn deltas lagged and clamped.
bool   ownValid = false;
double ownPitch = 0.0, ownYaw = 0.0;

// Turn delta is Vec2(-pitch, yaw) in degrees, already sensitivity-scaled.
void accumulate(Vec2 const& d) {
    // Gamepad look arrives through the same turn path; drop it while the stick drives flight through XInput.
    if (state::pad.stickActive) return;
    state::mouseDx += d.y / kDegreesPerMouseUnit;
    state::mouseDy += -d.x / kDegreesPerMouseUnit;
}

void resetFrameInput() {
    state::mouseDx = state::mouseDy = 0.0;
    state::keys                     = {};
    state::pad.axes                 = {};
}

LL_TYPE_INSTANCE_HOOK(
    ApplyTurnDeltaHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::_applyTurnDelta,
    void,
    Vec2 const& turnOffset
) {
    if (!applyingOwnTurn && state::isFlightActive(*this)) {
        accumulate(turnOffset);
        return;
    }
    origin(turnOffset);
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerTurnHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::localPlayerTurn,
    void,
    Vec2 const& deltaRot
) {
    if (state::isFlightActive(*this)) {
        accumulate(deltaRot);
        return;
    }
    origin(deltaRot);
}

LL_TYPE_INSTANCE_HOOK(
    ClientUpdateHook,
    ll::memory::HookPriority::Normal,
    ClientInstance,
    &ClientInstance::$update,
    bool,
    bool isInitFinished
) {
    // Poll before the game update so the stick state used to filter this frame's turn deltas is current.
    pollGamepad(isInitFinished && state::isHudTopScreen(*this));
    bool result = origin(isInitFinished);

    auto  now = Clock::now();
    float dt  = lastFrame.time_since_epoch().count() == 0
                  ? 0.0f
                  : std::chrono::duration<float>(now - lastFrame).count();
    lastFrame = now;
    dt        = std::clamp(dt, 0.0f, 0.25f);

    auto* player = getLocalPlayer();
    if (!player) {
        resetFrameInput();
        state::cameraRollDeg  = 0.0f;
        state::cameraOverride = false;
        state::hudVisible     = false;
        ownValid              = false;
        return result;
    }
    if (!state::isHudTopScreen(*this)) {
        // A transient overlay must not snap the camera upright, so the camera state is kept.
        resetFrameInput();
        return result;
    }

    auto actual = state::snapshot(*player);
    auto self   = actual;
    if (ownValid) {
        auto a = core::lookVector(actual.pitch, actual.yaw);
        auto b = core::lookVector(ownPitch, ownYaw);
        if (a.dot(b) < std::cos(kResyncAngleDeg * core::kToRad)) ownValid = false;
    }
    if (!ownValid) {
        ownPitch = actual.pitch;
        ownYaw   = actual.yaw;
        ownValid = true;
    }
    self.pitch = ownPitch;
    self.yaw   = ownYaw;

    core::FrameInput in;
    in.dt          = dt;
    in.mouseDx     = state::mouseDx;
    in.mouseDy     = state::mouseDy;
    in.keys        = state::keys;
    in.pad         = state::pad.axes;
    in.player      = self;
    state::mouseDx = state::mouseDy = 0.0;

    auto res = state::controller.frame(in);
    if (res.handled) {
        ownPitch = res.pitch;
        ownYaw   = res.yaw;

        // The server re-syncs mRot, so rotation goes through _applyTurnDelta, relative to the actual rotation.
        double gp = res.pitch - actual.pitch;
        double gy = std::remainder(res.yaw - actual.yaw, 360.0);
        if (gp != 0.0 || gy != 0.0) {
            applyingOwnTurn = true;
            player->_applyTurnDelta(Vec2{static_cast<float>(-gp), static_cast<float>(gy)});
            applyingOwnTurn = false;
        }
        state::cameraOverride = true;
        state::cameraPitch    = res.pitch;
        state::cameraYaw      = res.yaw;
        state::cameraRollDeg  = state::controller.getRoll();
    } else {
        ownValid              = false;
        state::cameraOverride = false;
        // Partial tick estimate for the decaying roll-back after flight ends.
        float tickDelta = lastTick.time_since_epoch().count() == 0
                            ? 1.0f
                            : std::clamp(std::chrono::duration<float>(now - lastTick).count() / kTickSeconds, 0.0f, 1.0f);
        state::cameraRollDeg = state::controller.cameraRoll(tickDelta);
    }
    state::hudVisible = state::controller.isRolling();
    return result;
}

} // namespace

void markTick() { lastTick = Clock::now(); }

void installInput() {
    lastFrame = {};
    ownValid  = false;
    ApplyTurnDeltaHook::hook();
    LocalPlayerTurnHook::hook();
    ClientUpdateHook::hook();
}

void uninstallInput() {
    ApplyTurnDeltaHook::unhook();
    LocalPlayerTurnHook::unhook();
    ClientUpdateHook::unhook();
}

} // namespace do_a_barrel_roll::hooks
