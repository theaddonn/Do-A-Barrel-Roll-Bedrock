#pragma once

#include "core/Config.h"
#include "common/Math.h"
#include "common/Smoother.h"

namespace do_a_barrel_roll::core {

struct RotationInstant {
    double pitch = 0.0;
    double yaw   = 0.0;
    double roll  = 0.0;

    [[nodiscard]] RotationInstant add(double p, double y, double r) const { return {pitch + p, yaw + y, roll + r}; }
    [[nodiscard]] RotationInstant multiply(double p, double y, double r) const {
        return {pitch * p, yaw * y, roll * r};
    }

    [[nodiscard]] RotationInstant addAbsolute(double x, double y, double currentRoll) const;
    [[nodiscard]] RotationInstant applySensitivity(const Sensitivity& s) const {
        return {pitch * s.pitch, yaw * s.yaw, roll * s.roll};
    }
};

struct KeyStates {
    bool rollLeft = false, rollRight = false;
    bool yawLeft = false, yawRight = false;
    bool pitchUp = false, pitchDown = false;
};

struct PlayerState {
    double pitch = 0.0;
    double yaw   = 0.0;
    Vec3   velocity;
    bool   isGliding          = false;
    bool   isSubmergedInWater = false;
    bool   onGround           = false;
    bool   isJumping          = false;
};

struct GamepadAxes {
    double stickX = 0.0;
    double stickY = 0.0;
    double yaw    = 0.0;
    [[nodiscard]] bool any() const { return stickX != 0.0 || stickY != 0.0 || yaw != 0.0; }
};

struct FrameInput {
    double      dt      = 0.0;
    double      mouseDx = 0.0;
    double      mouseDy = 0.0;
    bool        touchLook = false;
    KeyStates   keys;
    GamepadAxes pad;
    PlayerState player;
};

struct FrameResult {

    bool   handled = false;
    double pitch   = 0.0;
    double yaw     = 0.0;
    double roll    = 0.0;
};

struct ActivationState {
    bool secondJump = false;
    bool thirdJump  = false;
    bool wasJumping = false;
};

class FlightController {
public:
    explicit FlightController(Config& config) : mConfig(config) {}

    [[nodiscard]] bool isFallFlying(const PlayerState& p) const;
    [[nodiscard]] bool isFallFlyingRaw(const PlayerState& p) const;

    [[nodiscard]] bool onGlideStartCheck();

    void onTickMovementEnd(bool onGround, bool jumpHeld);

    [[nodiscard]] FrameResult frame(const FrameInput& in);

    void tick(const PlayerState& p);
    void reset();

    [[nodiscard]] bool  isRolling() const { return mRolling; }
    [[nodiscard]] float getRoll() const { return mRoll; }

    [[nodiscard]] float getRoll(float tickDelta) const;

    [[nodiscard]] float cameraRoll(float tickDelta);

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

    int mGraceTicks = 0;

    bool  mRolling  = false;
    float mRoll     = 0.0f;
    float mPrevRoll = 0.0f;

    float mRollBack     = 0.0f;
    float mLastRollBack = 0.0f;
};

}
