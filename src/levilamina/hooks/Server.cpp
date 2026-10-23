// Server-side cooperation for thrust.
//
// Bedrock movement is server-authoritative with client prediction: the server re-simulates the player from its inputs
// (which know nothing about thrust) and corrects the client when the positions differ. In a world hosted by this
// process the server policy runs in-process, so it is hooked: while the local player is thrusting, a correction for
// that player becomes AcceptClient. A remote server never calls this code; Player.cpp detects that and skips thrust.

#include "levilamina/hooks/State.h"

#include "ll/api/memory/Hook.h"
#include "mc/entity/components/CorrectionMethod.h"
#include "mc/entity/components/MovementCorrection.h"
#include "mc/entity/components/ServerCorrectionPolicy.h"
#include "mc/network/packet/PlayerAuthInputPacket.h"
#include "mc/world/actor/player/Player.h"

namespace do_a_barrel_roll::hooks {

namespace {

constexpr std::int64_t kThrustWindowMs = 300;
// The server's own simulation diverges after thrusting (it never saw the extra velocity), so keep accepting the
// client for the rest of the flight, plus a grace period after landing or stopping the glide.
constexpr std::int64_t kFlightGraceMs  = 2000;
constexpr std::int64_t kThrustMemoryMs = 120000;

bool divergedByThrust(std::int64_t now) {
    auto const lastThrust = state::lastThrustMs.load();
    if (now - lastThrust <= kThrustWindowMs) return true;
    return now - lastThrust <= kThrustMemoryMs && now - state::lastFlyingMs.load() <= kFlightGraceMs;
}

LL_TYPE_INSTANCE_HOOK(
    ShouldCorrectMovementHook,
    ll::memory::HookPriority::Normal,
    ServerCorrectionPolicy,
    &ServerCorrectionPolicy::$shouldCorrectMovement,
    MovementCorrection,
    EntityContext&               entity,
    PlayerAuthInputPacket const& packet,
    uint64                       frame,
    uchar const                  currentCounter,
    bool                         isStrictMovement
) {
    MovementCorrection result = origin(entity, packet, frame, currentCounter, isStrictMovement);

    auto const now = state::nowMs();
    state::lastServerPolicyMs.store(now);

    auto* player = Player::tryGetFromEntity(entity, false);
    if (!player || static_cast<std::int64_t>(player->getOrCreateUniqueID().rawID) != state::localPlayerId.load()) {
        return result;
    }
    if (!divergedByThrust(now)) return result;

    // Keep the server's velocity in line with the client's, so its simulation stops drifting away.
    const_cast<Vec3&>(player->getPosDelta()) =
        Vec3{state::clientVelX.load(), state::clientVelY.load(), state::clientVelZ.load()};
    if (result.mMethod != CorrectionMethod::None) {
        result.mMethod         = CorrectionMethod::AcceptClient;
        result.mAcceptPosition = packet.mPos;
    }
    return result;
}

} // namespace

void installServer() { ShouldCorrectMovementHook::hook(); }
void uninstallServer() { ShouldCorrectMovementHook::unhook(); }

} // namespace do_a_barrel_roll::hooks
