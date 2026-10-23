#include "levilamina/hooks/State.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/provider/PlayerMoveInput.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace do_a_barrel_roll::state {

core::KeyStates inputKeys() {
    core::KeyStates merged = keys;
    merged.thrustForward |= pad.thrustForward;
    merged.thrustBackward |= pad.thrustBackward;
    return merged;
}

core::PlayerState snapshot(LocalPlayer& player) {
    core::PlayerState s;
    // Bedrock rotation is Vec2(x = pitch, y = yaw) in degrees, pitch + = down.
    Vec2 const& rot = player.getRotation();
    s.pitch         = rot.x;
    s.yaw           = rot.y;

    Vec3 const& velocity = player.getPosDelta();
    s.velocity           = {velocity.x, velocity.y, velocity.z};

    s.isGliding          = player.isGliding();
    s.isSubmergedInWater = player.isInWater();
    s.onGround           = player.isOnGround();
    s.isJumping          = PlayerMoveInput::isJumping(player.getEntityContext());
    return s;
}

bool isFlightActive(LocalPlayer& player) { return controller.isFallFlying(snapshot(player)); }

bool isGameFocused() {
    HWND foreground = GetForegroundWindow();
    if (!foreground) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(foreground, &pid);
    return pid == GetCurrentProcessId();
}

bool isHudTopScreen(ClientInstance& client) { return client.getTopScreenName() == "hud_screen"; }

} // namespace do_a_barrel_roll::state
