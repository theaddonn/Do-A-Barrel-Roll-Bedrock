#pragma once

#include "Build.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace dabr {
struct Vec2 { float x{}, y{}; };
struct Vec3 { float x{}, y{}, z{}; };
struct Quaternion { float x{}, y{}, z{}, w{1}; };
struct Rect { float left{}, right{}, top{}, bottom{}; };
struct Color { float r{1}, g{1}, b{1}, a{1}; };
using ActionBarFn = void (*)(void*, const std::string&, const std::optional<std::string>&);
using UpdateFn = bool (*)(void*, bool);
using TickFn = void (*)(void*);
using TurnFn = void (*)(void*, const Vec2&);
using SetupCameraFn = void (*)(void*, void*, float);
using RenderPrepFn = void (*)(void*, void*, void*, void*);
using RenderFn = void (*)(void*, void*, const void*);
using BoolFn = bool (*)(const void*);
using FallFlyTicksFn = int (*)(const void*);
using CameraComponentFn = void* (*)(void*);
using GlideRotation = std::optional<std::pair<float, float>>;
using GlideRotationFn = GlideRotation (*)(const void*, float, bool);
using UiRenderFn = void (*)(void*, void*);
using CursorRenderFn = void (*)(void*, void*, void*, void*, int);
struct GamepadEvent {
    int type{};
    std::array<std::byte, 44> data{};
    int controllerId{};
};
using GamepadEventFn = GamepadEvent (*)(void*, const void*);
struct Targets {
    ActionBarFn actionBar{};
    UpdateFn update{};
    TickFn tick{};
    TurnFn applyTurn{}, localTurn{};
    SetupCameraFn setupCamera{};
    CameraComponentFn cameraComponent{};
    RenderPrepFn renderPrep{};
    RenderFn render{};
    BoolFn tryGlide{}, isGliding{}, inWater{}, onGround{};
    FallFlyTicksFn fallFlyTicks{};
    GlideRotationFn modelRotation{};
    GamepadEventFn gamepad{};
    UiRenderFn uiRender{};
    CursorRenderFn cursor{};
    void** uiVtable{};
};

template<class T> T read(const void* object, std::size_t offset = 0) {
    T result;
    std::memcpy(&result, static_cast<const std::byte*>(object) + offset, sizeof(result));
    return result;
}

template<class T> void write(void* object, std::size_t offset, const T& value) {
    std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof(value));
}

template<class F> F method(const void* object, std::size_t slot) {
    if (!object) return nullptr;
    const auto table = read<const void* const*>(object);
    return table ? reinterpret_cast<F>(const_cast<void*>(table[slot])) : nullptr;
}
constexpr std::uint32_t typeHash(std::string_view value) {
    std::uint32_t hash = 2166136261u;
    for (const unsigned char byte : value) hash = (hash ^ byte) * 16777619u;
    return hash;
}
static_assert(sizeof(void*) == 8);
static_assert(sizeof(std::string) == 24);
static_assert(sizeof(std::filesystem::path) == 24);
static_assert(sizeof(GamepadEvent) == 52);
static_assert(sizeof(std::optional<std::string>) == 32);
static_assert(sizeof(GlideRotation) == 12);
static_assert(typeHash("MinecraftCamera::CameraComponent") == 0x4f6047c7u);
}
