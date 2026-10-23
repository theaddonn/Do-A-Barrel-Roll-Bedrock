// Gamepad flight controls, read directly via XInput (loaded dynamically, works under Wine/Proton).
//
// The game turns the right stick into look deltas that reach the same turn path as the mouse with no marker telling
// them apart, so Input.cpp drops turn deltas while the stick is deflected (state::pad.stickActive) and the stick is
// fed to FlightController as rate input instead.
//
//   right stick X  roll (yaw when switch_roll_and_yaw)
//   right stick Y  pitch (stick up = nose up)
//   LB / RB        yaw left / right (roll when switch_roll_and_yaw)
//   RT / LT        thrust forward / backward

#include "levilamina/hooks/State.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace do_a_barrel_roll::hooks {

namespace {

using Clock = std::chrono::steady_clock;

// Minimal XInput ABI (avoids depending on the SDK header / import library).
struct XGamepad {
    WORD  wButtons;
    BYTE  bLeftTrigger, bRightTrigger;
    SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY;
};
struct XState {
    DWORD    dwPacketNumber;
    XGamepad Gamepad;
};
using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XState*);

constexpr WORD   kLeftShoulder     = 0x0100;
constexpr WORD   kRightShoulder    = 0x0200;
constexpr double kTriggerThreshold = 0.3;
constexpr auto   kStickHold        = std::chrono::milliseconds(150);

XInputGetStateFn  getState = nullptr;
bool              loaded   = false;
bool              active   = false;
Clock::time_point lastStickActive{};

void load() {
    if (loaded) return;
    loaded = true;
    for (auto const* name : {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"}) {
        if (HMODULE mod = LoadLibraryW(name)) {
            getState = reinterpret_cast<XInputGetStateFn>(GetProcAddress(mod, "XInputGetState"));
            if (getState) return;
        }
    }
}

double axis(SHORT v) { return std::clamp(static_cast<double>(v) / (v < 0 ? 32768.0 : 32767.0), -1.0, 1.0); }

// Radial deadzone with rescaling so the output ramps from 0 at the edge of the deadzone.
void radial(double& x, double& y, double deadzone) {
    double const magnitude = std::hypot(x, y);
    if (magnitude <= deadzone) {
        x = y = 0.0;
        return;
    }
    double const scale = std::min((magnitude - deadzone) / (1.0 - deadzone), 1.0) / magnitude;
    x *= scale;
    y *= scale;
}

} // namespace

void pollGamepad(bool allow) {
    auto& pad = state::pad;
    pad.axes           = {};
    pad.thrustForward  = false;
    pad.thrustBackward = false;
    if (!active || !state::config.enableGamepad) {
        pad.connected   = false;
        pad.stickActive = false;
        return;
    }
    load();
    // Several XInput slots can be populated (e.g. a physical pad plus an emulated one under Proton), so they are merged.
    XGamepad gamepad{};
    double   rx = 0.0, ry = 0.0;
    pad.connected = false;
    for (DWORD slot = 0; getState && slot < 4; ++slot) {
        XState xstate{};
        if (getState(slot, &xstate) != ERROR_SUCCESS) continue;
        pad.connected = true;
        auto const& g = xstate.Gamepad;
        gamepad.wButtons      |= g.wButtons;
        gamepad.bLeftTrigger  = std::max(gamepad.bLeftTrigger, g.bLeftTrigger);
        gamepad.bRightTrigger = std::max(gamepad.bRightTrigger, g.bRightTrigger);
        double const x = axis(g.sThumbRX), y = axis(g.sThumbRY);
        if (x * x + y * y > rx * rx + ry * ry) rx = x, ry = y; // the same pad may appear in two slots, so don't sum
    }
    if (!pad.connected) {
        pad.stickActive = false;
        return;
    }
    radial(rx, ry, std::clamp(state::config.gamepadDeadzone, 0.0, 0.95));

    // Tracked even while unfocused: the game may still see the stick, so its turn deltas must be dropped.
    auto const now = Clock::now();
    if (rx != 0.0 || ry != 0.0) lastStickActive = now;
    pad.stickActive = now - lastStickActive < kStickHold;
    if (!allow || !state::isGameFocused()) return;

    pad.axes.stickX = rx;
    pad.axes.stickY = ry;
    pad.axes.yaw    = ((gamepad.wButtons & kRightShoulder) ? 1.0 : 0.0) - ((gamepad.wButtons & kLeftShoulder) ? 1.0 : 0.0);
    pad.thrustForward  = gamepad.bRightTrigger / 255.0 > kTriggerThreshold;
    pad.thrustBackward = gamepad.bLeftTrigger / 255.0 > kTriggerThreshold;
}

void installGamepad() { active = true; }

void uninstallGamepad() {
    active = false;
    state::pad = {};
}

} // namespace do_a_barrel_roll::hooks
