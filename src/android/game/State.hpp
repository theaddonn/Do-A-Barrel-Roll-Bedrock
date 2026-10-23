#pragma once

#include "core/FlightController.h"
#include "runtime/Input.hpp"
#include <chrono>

namespace dabr {
struct FlightState {
    core::Config config;
    core::FlightController controller{config};
    void* client{};
    void* player{};
    void* level{};
    void* dimension{};
    FlightInput input;
    double mouseX{}, mouseY{};
    double pitch{}, yaw{};
    float cameraRoll{};
    bool ownValid{}, cameraOverride{}, hudVisible{};
    std::chrono::steady_clock::time_point lastFrame{}, lastTick{};
    void reset();
};
FlightState& flight();
}
