#pragma once

// Engine-independent port of the client-side logic of "Do a Barrel Roll" (see Math.h for angle conventions).
// Single-threaded: call frame() once per rendered frame and tick() once per game tick (20 Hz).
// Velocity is in blocks per tick, dt in seconds.

#include "levilamina/core/Config.h"
#include "common/Math.h"
#include "common/Smoother.h"

#include <optional>

namespace do_a_barrel_roll::core {

/// A (pitch, yaw, roll) triple of deltas or angles.
struct RotationInstant {
    double pitch = 0.0;
    double yaw   = 0.0;
    double roll  = 0.0;

    [[nodiscard]] RotationInstant add(double p, double y, double r) const { return {pitch + p, yaw + y, roll + r}; }
    [[nodiscard]] RotationInstant multiply(double p, double y, double r) const {
        return {pitch * p, yaw * y, roll * r};
    }
    /// Adds an upright (screen-space) x/y rotation, rotated into the rolled frame. currentRoll in radians.
    [[nodiscard]] RotationInstant addAbsolute(double x, double y, double currentRoll) const;
    [[nodiscard]] RotationInstant applySensitivity(const Sensitivity& s) const {
        return {pitch * s.pitch, yaw * s.yaw, roll * s.roll};
    }
};

struct KeyStates {
    bool rollLeft = false, rollRight = false;
    bool yawLeft = false, yawRight = false;
    bool pitchUp = false, pitchDown = false;
    bool thrustForward = false, thrustBackward = false;
};

/// Snapshot of the local player, supplied by the hook layer.
struct PlayerState {
    double pitch = 0.0; // degrees, +down
    double yaw   = 0.0; // degrees
    Vec3   velocity;    // blocks per tick
    bool   isGliding          = false;
    bool   isSubmergedInWater = false;
    bool   onGround           = false;
    bool   isJumping          = false;
};

/// Gamepad axes after deadzone, each in [-1, 1].
struct GamepadAxes {
    double stickX = 0.0; // right stick, +right
    double stickY = 0.0; // right stick, +up
    double yaw    = 0.0; // bumpers, +right
    [[nodiscard]] bool any() const { return stickX != 0.0 || stickY != 0.0 || yaw != 0.0; }
};

struct FrameInput {
    double      dt      = 0.0; // seconds since the previous frame
    double      mouseDx = 0.0; // raw cursor delta this frame
    double      mouseDy = 0.0;
    KeyStates   keys;
    GamepadAxes pad;
    PlayerState player;
};

struct FrameResult {
    /// True if the mod owns the camera this frame: the caller must not apply vanilla look changes.
    bool   handled = false;
    double pitch   = 0.0; // degrees, clamped to [-90, 90]
    double yaw     = 0.0; // degrees, continuous and unbounded like vanilla
    double roll    = 0.0; // degrees, also stored in the controller
};

struct ThrustResult {
    Vec3 velocityDelta;         // added to the vanilla glide velocity
    bool spawnParticle = false; // campfire_signal_smoke at (pos - velocity*0.5)
};

/// Triple-jump activation state (PlayerEntityMixin#checkGliding).
struct ActivationState {
    bool secondJump = false;
    bool thirdJump  = false;
    bool wasJumping = false;
};

class FlightController {
public:
    explicit FlightController(Config& config) : mConfig(config) {}

    /// DoABarrelRollClient.isFallFlying(): whether the mod currently controls flight. Includes a short grace
    /// period after the glide flag drops, since Bedrock may clear it for single ticks.
    [[nodiscard]] bool isFallFlying(const PlayerState& p) const;
    [[nodiscard]] bool isFallFlyingRaw(const PlayerState& p) const;

    /// Call where the game decides whether a jump starts gliding (in the air, jump pressed).
    /// @return true if the glide start must be cancelled (TRIPLE_JUMP only).
    [[nodiscard]] bool onGlideStartCheck();

    /// Call once per tick after movement.
    void onTickMovementEnd(bool onGround, bool jumpHeld);

    /// Mouse/keyboard/gamepad -> rotation. Call exactly once per frame, also with zero mouse delta: momentum,
    /// key rotation, banking and smoothing all need to run every frame while flying.
    [[nodiscard]] FrameResult frame(const FrameInput& in);

    /// Call once per game tick: updates the rolling flag and roll history, resets roll when not rolling and
    /// clears smoothers when not flying.
    void tick(const PlayerState& p);

    /// Port of LivingEntityMixin#wrapElytraVelocity. `tickCount` is the world game time (particle cadence).
    /// Returns nullopt if thrust is disabled or the player is not rolling.
    [[nodiscard]] std::optional<ThrustResult>
    thrust(const PlayerState& p, const KeyStates& keys, long long tickCount) const;

    [[nodiscard]] bool  isRolling() const { return mRolling; }
    [[nodiscard]] float getRoll() const { return mRoll; }
    /// Roll interpolated between ticks; tickDelta in [0, 1].
    [[nodiscard]] float getRoll(float tickDelta) const;

    /// Camera roll for this frame: the interpolated roll while rolling, otherwise a value easing back to 0
    /// (CameraMixin rollBack). Call once per rendered frame, it records state for the decay.
    [[nodiscard]] float cameraRoll(float tickDelta);

    /// Momentum vector (MouseMixin.mouseTurnVec), length <= 1. Zero when momentum mouse is off.
    [[nodiscard]] Vec2 mouseTurnVec() const { return mMouseTurnVec; }

private:
    void setRoll(float roll);

    [[nodiscard]] RotationInstant applyModifiers(RotationInstant delta, const FrameInput& in, double dt);
    [[nodiscard]] RotationInstant banking(RotationInstant r, const PlayerState& p, double dt) const;
    [[nodiscard]] RotationInstant reorient(RotationInstant r, double dt) const;
    [[nodiscard]] RotationInstant controlSurfaceEfficacy(RotationInstant r, const PlayerState& p) const;

    Config&         mConfig;
    ActivationState mActivation;

    Smoother mPitchSmoother, mYawSmoother, mRollSmoother;
    Vec2     mMouseTurnVec;

    int mGraceTicks = 0; // ticks left in which a dropped glide flag is ignored

    bool  mRolling  = false;
    float mRoll     = 0.0f;
    float mPrevRoll = 0.0f;

    float mRollBack     = 0.0f;
    float mLastRollBack = 0.0f;
};

} // namespace do_a_barrel_roll::core
