// Per-tick hook: flight controller tick and thrust (velocity + smoke particles).
//
// Bedrock has no travelGliding() to wrap, so thrust is added to the player's velocity once per tick after the
// vanilla tick. Server agreement is handled in Server.cpp.

#include "levilamina/hooks/State.h"

#include <cmath>

#include "ll/api/memory/Hook.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/provider/PlayerMoveInput.h"
#include "mc/world/level/ILevel.h"
#include "mc/world/level/Level.h"

namespace do_a_barrel_roll::hooks {

void markTick(); // Input.cpp

namespace {

constexpr std::int64_t kServerAliveMs = 1500;

// The legacy Level::addParticle path returns null on this version, so use the particle effect system.
void spawnThrustParticle(LocalPlayer& player, Vec3 const& velocity) {
    Vec3 const pos   = player.getFeetPos() - velocity * 0.5f;
    auto&      level = static_cast<ILevel&>(player.getLevel()); // Level::addParticleEffect hides the ILevel overload
    level.addParticleEffect(HashedString{"minecraft:campfire_tall_smoke_particle"}, pos);
}

void applyVelocity(LocalPlayer& player, Vec3 const& velocity) {
    player.lerpMotion(velocity);
    Vec3 const actual = player.getPosDelta();
    if (std::abs(actual.x - velocity.x) + std::abs(actual.y - velocity.y) + std::abs(actual.z - velocity.z) > 1e-4f) {
        const_cast<Vec3&>(player.getPosDelta()) = velocity; // lerpMotion did not stick
    }
}

LL_TYPE_INSTANCE_HOOK(
    NormalTickHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::$normalTick,
    void
) {
    origin();
    markTick();
    ++state::tickCount;

    state::localPlayerId.store(static_cast<std::int64_t>(getOrCreateUniqueID().rawID));

    auto const player = state::snapshot(*this);
    state::controller.tick(player);

    if (state::controller.isFallFlying(player)) {
        auto const now = state::nowMs();
        state::lastFlyingMs.store(now);

        if (auto const thrust = state::controller.thrust(player, state::inputKeys(), state::tickCount)) {
            auto const& delta = thrust->velocityDelta;
            // Thrust needs the in-process server to accept the client position, otherwise a remote server rubber-bands.
            bool const serverAlive = now - state::lastServerPolicyMs.load() < kServerAliveMs;
            if (serverAlive || state::config.thrustAllowRemote) {
                Vec3 const before = getPosDelta();
                if (delta.x != 0.0 || delta.y != 0.0 || delta.z != 0.0) {
                    state::lastThrustMs.store(now);
                    applyVelocity(
                        *this,
                        {before.x + static_cast<float>(delta.x),
                         before.y + static_cast<float>(delta.y),
                         before.z + static_cast<float>(delta.z)}
                    );
                }
                if (thrust->spawnParticle) spawnThrustParticle(*this, before);
            }
        }
    }

    Vec3 const velocity = getPosDelta(); // read by the in-process server (Server.cpp)
    state::clientVelX.store(velocity.x);
    state::clientVelY.store(velocity.y);
    state::clientVelZ.store(velocity.z);
    // The hybrid modes count air jumps here too, since tryStartGliding is not called once already gliding.
    // (Not for TRIPLE_JUMP: consuming the press edge before the hook would make it cancel the third jump.)
    if (state::config.activationBehaviour != core::ActivationBehaviour::TripleJump && !player.onGround
        && !player.isSubmergedInWater && player.isJumping) {
        (void)state::controller.onGlideStartCheck();
    }
    state::controller.onTickMovementEnd(player.onGround, player.isJumping);
}

// Jump presses in the air are counted for the activation behaviours; TRIPLE_JUMP swallows the early ones.
LL_TYPE_INSTANCE_HOOK(
    TryStartGlidingHook,
    ll::memory::HookPriority::Normal,
    Player,
    &Player::tryStartGliding,
    bool
) {
    if (!isLocalPlayer() || isOnGround() || isInWater() || !PlayerMoveInput::isJumping(getEntityContext())) {
        return origin();
    }
    if (state::controller.onGlideStartCheck()) return false;
    return origin();
}

} // namespace

void installPlayer() {
    NormalTickHook::hook();
    TryStartGlidingHook::hook();
}
void uninstallPlayer() {
    NormalTickHook::unhook();
    TryStartGlidingHook::unhook();
}

} // namespace do_a_barrel_roll::hooks
