#include "levilamina/core/FlightController.h"

#include <algorithm>
#include <utility>

namespace do_a_barrel_roll::core {

namespace {

const double  kRollReorientCutoff = std::sqrt(10.0 / 3.0); // RotationModifiers.ROLL_REORIENT_CUTOFF
constexpr int kGraceTicks         = 6;

double          fixNaN(double v) { return std::isnan(v) ? 0.0 : v; }
RotationInstant fixNaN(RotationInstant r) { return {fixNaN(r.pitch), fixNaN(r.yaw), fixNaN(r.roll)}; }

} // namespace

RotationInstant RotationInstant::addAbsolute(double x, double y, double currentRoll) const {
    double c = std::cos(currentRoll), s = std::sin(currentRoll);
    return {pitch - y * c - x * s, yaw - y * s + x * c, roll};
}

bool FlightController::isFallFlying(const PlayerState& p) const {
    if (isFallFlyingRaw(p)) return true;
    return mGraceTicks > 0 && mRolling && mConfig.effectiveEnabled() && !p.onGround
        && !(mConfig.disableWhenSubmerged && p.isSubmergedInWater);
}

bool FlightController::isFallFlyingRaw(const PlayerState& p) const {
    bool hybrid = mConfig.activationBehaviour == ActivationBehaviour::Hybrid
               || mConfig.activationBehaviour == ActivationBehaviour::HybridToggle;
    if (hybrid && !mActivation.thirdJump) return false;
    if (!mConfig.effectiveEnabled()) return false;
    if (mConfig.disableWhenSubmerged && p.isSubmergedInWater) return false;
    return p.isGliding;
}

bool FlightController::onGlideStartCheck() {
    auto behaviour = mConfig.activationBehaviour;
    if (behaviour == ActivationBehaviour::Vanilla) return false;

    bool shouldCancel = behaviour == ActivationBehaviour::TripleJump;
    if (!mActivation.wasJumping) {
        mActivation.wasJumping = true;
        if (!mActivation.secondJump) {
            mActivation.secondJump = true;
            if (shouldCancel) return true;
        } else {
            mActivation.thirdJump = behaviour != ActivationBehaviour::HybridToggle || !mActivation.thirdJump;
        }
        return false;
    }
    return shouldCancel;
}

void FlightController::onTickMovementEnd(bool onGround, bool jumpHeld) {
    if (onGround) {
        mActivation.secondJump = false;
        mActivation.thirdJump  = false;
    }
    mActivation.wasJumping = jumpHeld;
}

float FlightController::getRoll(float tickDelta) const {
    if (tickDelta == 1.0f) return mRoll;
    return static_cast<float>(lerp(tickDelta, mPrevRoll, mRoll));
}

void FlightController::setRoll(float roll) {
    if (!std::isfinite(roll)) return;
    float last = mRoll;
    mRoll      = roll;
    if (roll < -90 && last > 90) mPrevRoll -= 360;
    else if (roll > 90 && last < -90) mPrevRoll += 360;
}

float FlightController::cameraRoll(float tickDelta) {
    if (mRolling) {
        float r   = getRoll(tickDelta);
        mRollBack = mLastRollBack = r;
        return r;
    }
    return static_cast<float>(lerp(tickDelta, mLastRollBack, mRollBack));
}

void FlightController::tick(const PlayerState& p) {
    if (isFallFlyingRaw(p)) mGraceTicks = kGraceTicks;
    else if (mGraceTicks > 0) --mGraceTicks;

    mRolling = isFallFlying(p);
    if (!mRolling) {
        mPitchSmoother.clear();
        mYawSmoother.clear();
        mRollSmoother.clear();
    }
    mPrevRoll = mRoll;
    if (!mRolling) {
        setRoll(0.0f);
        // CameraMixin#interpolateRollnt: decays by half per tick while not rolling
        mLastRollBack  = mRollBack;
        mRollBack     -= mRollBack * 0.5f;
    }
}

RotationInstant FlightController::banking(RotationInstant r, const PlayerState& p, double dt) const {
    // Default formulas of ModConfig.advanced:
    //   x = sin(roll) * cos(pitch) * 10 * strength, y = (cos(roll) - 1) * cos(pitch) * 10 * strength
    double roll  = mRoll * kToRad;
    double pitch = p.pitch * kToRad;
    double s     = mConfig.bankingStrength;
    double dX    = fixNaN(std::sin(roll) * std::cos(pitch) * 10 * s);
    double dY    = fixNaN((-1 + std::cos(roll)) * std::cos(pitch) * 10 * s);
    return r.addAbsolute(dX * dt, dY * dt, roll);
}

RotationInstant FlightController::reorient(RotationInstant r, double dt) const {
    double roll      = mRoll * kToRad;
    double strength  = 10 * mConfig.rightingStrength;
    double rollDelta = 0;
    if (-kRollReorientCutoff < roll && roll < kRollReorientCutoff) rollDelta = -std::pow(roll, 3) / 3.0 + roll;
    return r.add(0, 0, -rollDelta * strength * dt);
}

RotationInstant FlightController::controlSurfaceEfficacy(RotationInstant r, const PlayerState& p) const {
    // Default formula for elevator, aileron and rudder alike: velocity . look
    double e = p.velocity.dot(lookVector(p.pitch, p.yaw));
    if (std::isnan(e)) e = 1;
    return r.multiply(e, e, e);
}

RotationInstant FlightController::applyModifiers(RotationInstant delta, const FrameInput& in, double dt) {
    delta = fixNaN(delta);

    // Mouse X arrives in the yaw slot: it rolls unless switchRollAndYaw.
    if (!mConfig.switchRollAndYaw) std::swap(delta.yaw, delta.roll);
    if (mConfig.invertPitch) delta.pitch = -delta.pitch;

    // Key controls, not affected by the swap
    double d = 1800.0 * dt, p = 0, y = 0, r = 0;
    if (in.keys.pitchUp) p -= d;
    if (in.keys.pitchDown) p += d;
    if (in.keys.yawLeft) y -= d;
    if (in.keys.yawRight) y += d;
    if (in.keys.rollLeft) r -= d;
    if (in.keys.rollRight) r += d;
    delta = fixNaN(delta.add(p, y, r));

    delta = fixNaN(delta.applySensitivity(mConfig.desktop));

    // Gamepad is rate based. Stick Y pitches, stick X rolls (yaws with switchRollAndYaw, then the bumpers roll).
    if (in.pad.any()) {
        double m     = dt * 1200.0;
        double pitch = -in.pad.stickY;
        if (mConfig.invertPitch) pitch = -pitch;
        double roll = in.pad.stickX, yaw = in.pad.yaw;
        if (mConfig.switchRollAndYaw) std::swap(roll, yaw);
        const auto& c = mConfig.controller;
        delta         = fixNaN(delta.add(pitch * m * c.pitch, yaw * m * c.yaw, roll * m * c.roll));
    }

    if (mConfig.simulateControlSurfaceEfficacy) delta = controlSurfaceEfficacy(delta, in.player);
    const auto& sm = mConfig.smoothing;
    delta          = {
        sm.pitch == 0 ? delta.pitch : mPitchSmoother.smooth(delta.pitch, 1 / sm.pitch * dt),
        sm.yaw == 0 ? delta.yaw : mYawSmoother.smooth(delta.yaw, 1 / sm.yaw * dt),
        sm.roll == 0 ? delta.roll : mRollSmoother.smooth(delta.roll, 1 / sm.roll * dt),
    };
    if (mConfig.enableBanking) delta = banking(delta, in.player, dt);
    if (mConfig.automaticRighting) delta = reorient(delta, dt);
    return fixNaN(delta);
}

FrameResult FlightController::frame(const FrameInput& in) {
    FrameResult res;
    res.pitch = in.player.pitch;
    res.yaw   = in.player.yaw;
    res.roll  = mRoll;

    if (!mRolling) {
        mMouseTurnVec = {};
        return res;
    }

    Vec2 turn{in.mouseDx, in.mouseDy}; // x -> yaw slot, y -> pitch slot
    if (mConfig.momentumBasedMouse) {
        mMouseTurnVec = mMouseTurnVec + turn * (1.0 / 300.0);
        if (mMouseTurnVec.lengthSquared() > 1.0) mMouseTurnVec.normalize();
        double deadzone   = mConfig.momentumMouseDeadzone;
        bool   inDeadzone = mMouseTurnVec.lengthSquared() < deadzone * deadzone;
        turn              = inDeadzone ? Vec2{} : mMouseTurnVec * (1200.0 * in.dt);
    } else {
        mMouseTurnVec = {};
    }

    RotationInstant delta = applyModifiers({turn.y, turn.x, 0.0}, in, in.dt);
    LookResult look = changeLookInLocalFrame(in.player.pitch, in.player.yaw, mRoll, delta.pitch, delta.yaw, delta.roll);

    res.pitch    = clamp(look.pitch, -90.0, 90.0);
    res.yaw      = look.yaw;
    mRoll       += static_cast<float>(look.deltaRoll);
    mPrevRoll   += static_cast<float>(look.deltaRoll);
    res.roll     = mRoll;
    res.handled  = true;
    return res;
}

std::optional<ThrustResult>
FlightController::thrust(const PlayerState& p, const KeyStates& keys, long long tickCount) const {
    if (!mRolling || !mConfig.effectiveThrust()) return std::nullopt;

    Vec3   rotation = lookVector(p.pitch, p.yaw);
    double throttle = keys.thrustForward ? 1 : keys.thrustBackward ? -1 : 0;

    ThrustResult out;
    if (mConfig.thrustParticles) {
        int density = static_cast<int>(clamp(throttle * 10, 0, 10));
        if (throttle > 0.1 && tickCount % (11 - density) == 0) out.spawnParticle = true;
    }

    double maxSpeed = mConfig.maxThrust;
    double speedIncrease =
        std::max(maxSpeed - p.velocity.length() * sign(rotation.dot(p.velocity) * throttle), 0.0) / maxSpeed * throttle;
    out.velocityDelta = rotation * (mConfig.thrustAcceleration * speedIncrease);
    return out;
}

} // namespace do_a_barrel_roll::core
