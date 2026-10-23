#pragma once

#include "config/Settings.hpp"
#include "core/FlightController.h"
#include <dabr/Native.hpp>

namespace dabr::game {
void* component(const void* context, std::uint32_t hash, std::size_t stride);
void* localPlayer(void* client);
core::PlayerState snapshot(void* actor, const Targets&);
Vec3 velocity(const void* actor);
void* level(const void* actor);
void* dimension(const void* actor);
}
