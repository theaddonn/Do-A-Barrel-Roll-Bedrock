#pragma once

#include "config/Settings.hpp"
#include "core/FlightController.h"
#include <dabr/Native.hpp>
#include <array>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace dabr {
struct FlightInput {
    core::KeyStates keys;
    core::GamepadAxes pad;
    bool touchLook{}, padLook{};
};
class Input {
public:
    static Input& get();
    bool connect();
    FlightInput poll(const core::Config&, bool allow);
    void gamepad(const GamepadEvent&);
    void button(std::size_t action, bool down);
    void clear(bool padState = true);
    void allow(bool);
    bool padDriving() const;

private:
    struct Pad {
        double x{}, y{};
        bool left{}, right{};
    };
    std::array<std::atomic_bool, 513> held{};
    std::array<std::atomic_bool, 7> logical{};
    std::atomic_uint toggles{};
    std::atomic_bool allowed{};
    std::atomic<std::int64_t> touchTime{};
    std::mutex mutex;
    std::unordered_map<int, Pad> pads;
    std::int64_t lastStick{};
    std::atomic<std::int64_t> padActiveUntil{};
    bool registered{};
    friend bool keyEvent(int, unsigned int, bool);
    friend bool touchEvent(int, int, float, float);
};
std::int64_t nowMs();
}
