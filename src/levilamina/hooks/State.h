#pragma once

// Shared runtime state of the Bedrock hook layer. Everything runs on the client main/render thread.

#include "levilamina/core/Config.h"
#include "levilamina/core/FlightController.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>

class LocalPlayer;
class ClientInstance;

namespace do_a_barrel_roll::state {

inline core::Config           config;
inline core::FlightController controller{config};

/// Keyboard keys held, polled once per frame by hooks/Keys.cpp.
inline core::KeyStates keys;

/// Raw turn delta accumulated by the look hooks since the last frame, already converted to "Java mouse units"
/// (degrees / 0.15, so that FlightController's 0.15 factor yields degrees again).
inline double mouseDx = 0.0;
inline double mouseDy = 0.0;

/// Gamepad state polled once per frame by hooks/Gamepad.cpp (XInput). Inputs are zero unless the HUD is the top
/// screen, the window is focused and general.misc.enable_gamepad is set.
struct GamepadState {
    bool              connected = false;
    core::GamepadAxes axes;
    bool              stickActive    = false; // right stick beyond the deadzone, or was within the last 150 ms
    bool              thrustForward  = false; // right trigger
    bool              thrustBackward = false; // left trigger
};
inline GamepadState pad;

/// Camera roll (degrees, sign already applied) computed once per frame, consumed by the setupCamera hook.
inline float cameraRollDeg = 0.0f;

/// While true the camera orientation is built from these controller angles (degrees; roll sign already applied)
/// instead of the game's view rotation. Set once per frame by the update hook.
inline bool   cameraOverride = false;
inline double cameraPitch = 0.0, cameraYaw = 0.0;

/// Whether the mod currently controls flight / HUD should draw (updated per frame).
inline bool hudVisible = false;

/// Game tick counter (increments in the LocalPlayer tick hook).
inline long long tickCount = 0;

inline std::filesystem::path configPath;

/// Cross-thread thrust/server coordination (the integrated server runs on its own thread). Times are steady-clock ms.
inline std::atomic<std::int64_t> lastThrustMs{-1000000};       // client applied thrust to the local player
inline std::atomic<std::int64_t> lastServerPolicyMs{-1000000}; // in-process server evaluated a movement correction
inline std::atomic<std::int64_t> lastFlyingMs{-1000000};       // local player was fall-flying
inline std::atomic<float>        clientVelX{0}, clientVelY{0}, clientVelZ{0}; // local player velocity after thrust
inline std::atomic<std::int64_t> localPlayerId{0};             // unique id of the local player (same on the server)

[[nodiscard]] inline std::int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

/// Keyboard keys merged with the gamepad triggers; the single source of flight key input.
[[nodiscard]] core::KeyStates inputKeys();

/// Snapshot of the local player in the core's units (pitch/yaw degrees, velocity blocks/tick).
[[nodiscard]] core::PlayerState snapshot(LocalPlayer& player);

/// True while the mod owns the camera of `player` (enabled, gliding, ...).
[[nodiscard]] bool isFlightActive(LocalPlayer& player);

/// True if the foreground window belongs to this process.
[[nodiscard]] bool isGameFocused();

/// True if the in-game HUD is the top screen (no menu / chat / inventory open).
[[nodiscard]] bool isHudTopScreen(ClientInstance& client);

} // namespace do_a_barrel_roll::state

namespace do_a_barrel_roll::hooks {

void installInput();
void uninstallInput();
void installCamera();
void uninstallCamera();
void installPlayer();
void uninstallPlayer();
void installPlayerModel();
void uninstallPlayerModel();
void installServer();
void uninstallServer();
void installHud();
void uninstallHud();
void installGamepad();
void uninstallGamepad();
/// Polls the gamepad into state::pad. Call once per frame; `allow` = flight input may be read right now.
void pollGamepad(bool allow);
void installKeys();
void uninstallKeys();

} // namespace do_a_barrel_roll::hooks
