// Server half of thrust. Bedrock movement is server-authoritative with client prediction: the server re-simulates
// the player from its inputs (which know nothing about thrust) and corrects the client when the positions differ.
// While a handshaken client reports thrusting, and for a grace period afterwards, its position is accepted instead.

#include "levilamina/server/ServerState.h"

#include "ll/api/memory/Hook.h"
#include "mc/entity/components/CorrectionMethod.h"
#include "mc/entity/components/MovementCorrection.h"
#include "mc/entity/components/ServerCorrectionPolicy.h"
#include "mc/network/packet/PlayerAuthInputPacket.h"
#include "mc/world/actor/player/Player.h"

namespace do_a_barrel_roll::server {

namespace {

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

    auto* player = Player::tryGetFromEntity(entity, false);
    if (!player) return result;

    Vec3 velocity;
    {
        std::lock_guard lock(mutex);
        auto const      it = players.find(static_cast<std::int64_t>(player->getOrCreateUniqueID().rawID));
        if (it == players.end() || it->second.handshake != HandshakeState::Accepted
            || !it->second.applied.allowThrusting || nowMs() > it->second.acceptClientUntilMs) {
            return result;
        }
        velocity = Vec3{it->second.velocity[0], it->second.velocity[1], it->second.velocity[2]};
    }

    // Keep the server's velocity in line with the client's, so its simulation stops drifting away.
    const_cast<Vec3&>(player->getPosDelta()) = velocity;
    if (result.mMethod != CorrectionMethod::None) {
        result.mMethod         = CorrectionMethod::AcceptClient;
        result.mAcceptPosition = packet.mPos;
    }
    return result;
}

} // namespace

void installThrust() { ShouldCorrectMovementHook::hook(); }
void uninstallThrust() { ShouldCorrectMovementHook::unhook(); }

} // namespace do_a_barrel_roll::server
