#include "Hooks.hpp"
#include "Game.hpp"
#include "State.hpp"
#include "runtime/Runtime.hpp"
#include "ui/Feedback.hpp"
#include <algorithm>
#include <cmath>

namespace dabr {
namespace {
UpdateFn updateNext{};
TickFn tickNext{};
TurnFn applyTurnNext{}, localTurnNext{};
BoolFn tryGlideNext{};
GlideRotationFn modelNext{};
GamepadEventFn gamepadNext{};
thread_local bool applyingTurn{};

void syncConfig() {
    auto& state = flight();
    state.config = *Settings::get().snapshot();
}
void accumulate(const Vec2& delta) {
    auto& state = flight();
    if (!Runtime::get().hudTop() || state.input.padLook || Input::get().padDriving()) return;
    if (std::isfinite(delta.x) && std::isfinite(delta.y)) {
        state.mouseX += delta.y / 0.15;
        state.mouseY -= delta.x / 0.15;
    }
}
void applyTurnHook(void* actor, const Vec2& delta) {
    const auto& runtime = Runtime::get();
    if (!applyingTurn && runtime.active() && actor == flight().player &&
        flight().controller.isFallFlying(game::snapshot(actor, runtime.native()))) { accumulate(delta); return; }
    applyTurnNext(actor, delta);
}
void localTurnHook(void* actor, const Vec2& delta) {
    const auto& runtime = Runtime::get();
    if (!applyingTurn && runtime.active() && actor == flight().player &&
        flight().controller.isFallFlying(game::snapshot(actor, runtime.native()))) { accumulate(delta); return; }
    localTurnNext(actor, delta);
}

bool updateHook(void* client, bool initialized) {
    auto& state = flight();
    auto& runtime = Runtime::get();
    if (runtime.active()) {
        state.client = client;
        syncConfig();
        state.input = Input::get().poll(state.config, initialized && runtime.hudTop());
    } else Input::get().allow(false);
    const bool result = updateNext(client, initialized);
    const auto now = std::chrono::steady_clock::now();
    if (!runtime.active()) {
        if (state.player) { restoreCamera(); state.reset(); runtime.modMenu().frame(false, state.config); }
        return result;
    }
    if (initialized) flushMessages(client, runtime.native());
    const auto dt = state.lastFrame.time_since_epoch().count() == 0 ? 0.0 : std::chrono::duration<double>(now - state.lastFrame).count();
    state.lastFrame = now;
    auto* player = game::localPlayer(client);
    if (!player) {
        if (state.player) { restoreCamera(); state.reset(); }
        runtime.modMenu().frame(false, state.config);
        Input::get().clear();
        return result;
    }
    auto* world = game::level(player);
    if (state.player != player || state.level != world) {
        restoreCamera();
        state.reset();
        state.client = client; state.player = player; state.level = world;
    }
    auto* dimension = game::dimension(player);
    if (state.dimension != dimension) {
        state.dimension = dimension;
        state.ownValid = false;
    }
    syncConfig();
    state.input = Input::get().poll(state.config, initialized && runtime.hudTop());
    if (!runtime.hudTop()) {
        state.mouseX = state.mouseY = 0;
        runtime.modMenu().frame(false, state.config);
        return result;
    }
    auto actual = game::snapshot(player, runtime.native());
    if (!std::isfinite(actual.pitch) || !std::isfinite(actual.yaw)) return result;
    if (state.ownValid && core::lookVector(actual.pitch, actual.yaw).dot(core::lookVector(state.pitch, state.yaw)) < std::cos(15 * core::kToRad)) state.ownValid = false;
    if (!state.ownValid) { state.pitch = actual.pitch; state.yaw = actual.yaw; state.ownValid = true; }
    if (!state.controller.isFallFlying(actual) && state.controller.isRolling()) state.controller.tick(actual);
    core::FrameInput input;
    input.dt = std::clamp(dt, 0.0, 0.25);
    input.mouseDx = state.mouseX; input.mouseDy = state.mouseY;
    input.touchLook = state.input.touchLook;
    input.keys = state.input.keys;
    input.pad = state.input.pad;
    input.player = actual;
    input.player.pitch = state.pitch; input.player.yaw = state.yaw;
    state.mouseX = state.mouseY = 0;
    const auto output = state.controller.frame(input);
    if (output.handled) {
        state.pitch = output.pitch; state.yaw = output.yaw;
        const auto pitch = output.pitch - actual.pitch;
        const auto yaw = std::remainder(output.yaw - actual.yaw, 360.0);
        if (pitch != 0 || yaw != 0) {
            applyingTurn = true;
            applyTurnNext(player, Vec2{static_cast<float>(-pitch), static_cast<float>(yaw)});
            applyingTurn = false;
        }
        state.cameraOverride = true;
        state.cameraRoll = state.controller.getRoll();
    } else {
        state.ownValid = state.cameraOverride = false;
        const auto partial = state.lastTick.time_since_epoch().count() == 0 ? 1.0f :
            std::clamp(std::chrono::duration<float>(now - state.lastTick).count() / 0.05f, 0.0f, 1.0f);
        state.cameraRoll = state.controller.cameraRoll(partial);
        if (std::abs(state.cameraRoll) < 0.0001f) state.cameraRoll = 0;
    }
    state.hudVisible = state.controller.isRolling();
    runtime.modMenu().frame(state.hudVisible, state.config);
    return result;
}

void tickHook(void* player) {
    tickNext(player);
    auto& runtime = Runtime::get();
    auto& state = flight();
    if (!runtime.active()) return;
    if (!state.client || game::localPlayer(state.client) != player) return;
    const auto world = game::level(player);
    if (state.player && (state.player != player || state.level != world)) {
        restoreCamera();
        state.reset();

    }
    state.player = player;
    state.level = world;
    state.lastTick = std::chrono::steady_clock::now();
    syncConfig();
    const auto actual = game::snapshot(player, runtime.native());
    state.controller.tick(actual);
    if (state.config.modEnabled && state.config.activationBehaviour != core::ActivationBehaviour::TripleJump &&
        !actual.onGround && !actual.isSubmergedInWater && actual.isJumping) (void)state.controller.onGlideStartCheck();
    state.controller.onTickMovementEnd(actual.onGround, actual.isJumping);
}

bool tryGlideHook(const void* player) {
    auto& runtime = Runtime::get();
    auto& state = flight();
    if (runtime.active() && player == state.player && state.config.modEnabled) {
        const auto actual = game::snapshot(const_cast<void*>(player), runtime.native());
        if (!actual.onGround && !actual.isSubmergedInWater && actual.isJumping && state.controller.onGlideStartCheck()) return false;
    }
    return tryGlideNext(player);
}

GlideRotation modelHook(const void* player, float partial, bool firstPerson) {
    auto result = modelNext(player, partial, firstPerson);
    if (!Runtime::get().active() || !result || firstPerson) return result;
    auto& state = flight();
    if (player != state.player) return result;
    if (state.cameraOverride) {
        const auto t = Runtime::get().native().fallFlyTicks(player) + partial;
        result->first = (-90.0f - static_cast<float>(state.pitch)) * std::clamp(t * t / 100.0f, 0.0f, 1.0f);
    }
    if (state.cameraRoll != 0 && std::isfinite(state.cameraRoll)) result->second = state.cameraRoll;
    return result;
}

GamepadEvent gamepadHook(void* self, const void* token) {
    auto result = gamepadNext(self, token);
    if (Runtime::get().active()) Input::get().gamepad(result);
    return result;
}
}

FlightState& flight() { static FlightState state; return state; }
void FlightState::reset() {
    controller.reset();
    player = level = dimension = nullptr;
    input = {};
    mouseX = mouseY = pitch = yaw = 0;
    cameraRoll = 0;
    ownValid = cameraOverride = hudVisible = false;
    lastFrame = lastTick = {};
    clearHud();
    Input::get().clear();
}

bool installGameHooks(Host& host, const Targets& target) {
    return host.install(reinterpret_cast<void*>(target.update), reinterpret_cast<void*>(updateHook), reinterpret_cast<void**>(&updateNext)) &&
        host.install(reinterpret_cast<void*>(target.tick), reinterpret_cast<void*>(tickHook), reinterpret_cast<void**>(&tickNext)) &&
        host.install(reinterpret_cast<void*>(target.applyTurn), reinterpret_cast<void*>(applyTurnHook), reinterpret_cast<void**>(&applyTurnNext)) &&
        host.install(reinterpret_cast<void*>(target.localTurn), reinterpret_cast<void*>(localTurnHook), reinterpret_cast<void**>(&localTurnNext)) &&
        host.install(reinterpret_cast<void*>(target.tryGlide), reinterpret_cast<void*>(tryGlideHook), reinterpret_cast<void**>(&tryGlideNext)) &&
        host.install(reinterpret_cast<void*>(target.modelRotation), reinterpret_cast<void*>(modelHook), reinterpret_cast<void**>(&modelNext)) &&
        host.install(reinterpret_cast<void*>(target.gamepad), reinterpret_cast<void*>(gamepadHook), reinterpret_cast<void**>(&gamepadNext));
}
}
