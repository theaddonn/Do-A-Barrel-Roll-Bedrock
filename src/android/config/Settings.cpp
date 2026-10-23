#include "Settings.hpp"
#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>

namespace dabr {
namespace {
using K = SettingKind;
const SettingSpec specs[]{
    {"mod_enabled", "/general/mod_enabled", "Enabled", "Enable full pitch, yaw and roll during elytra flight.", "flight", K::Toggle},
    {"activation_behaviour", "/general/controls/activation_behaviour", "Activation", "Vanilla uses the normal glide press. Triple jump delays gliding. Hybrid takes control on the next jump press; Hybrid toggle also releases control.", "flight", K::Choice},
    {"disable_when_submerged", "/general/controls/disable_when_submerged", "Disable in water", "Release flight control when the player is in water.", "flight", K::Toggle},
    {"switch_roll_and_yaw", "/general/controls/switch_roll_and_yaw", "Swap roll and yaw", "Make horizontal look steer yaw; yaw buttons and controller bumpers then roll.", "controls", K::Toggle},
    {"invert_pitch", "/general/controls/invert_pitch", "Invert pitch", "Reverse vertical flight input.", "controls", K::Toggle},
    {"momentum_based_mouse", "/general/controls/momentum_based_mouse", "Momentum steering", "Dragging sets a persistent turn vector. Move back toward the center to stop turning.", "controls", K::Toggle},
    {"momentum_mouse_deadzone", "/general/controls/momentum_mouse_deadzone", "Momentum deadzone", "Size of the neutral region in the momentum widget.", "controls", K::Number, 0, 0.95, 0.01, "momentum_based_mouse"},
    {"enable_banking", "/general/banking/enable_banking", "Banking", "Turn and lose lift naturally when the aircraft is banked.", "flight", K::Toggle},
    {"banking_strength", "/general/banking/banking_strength", "Banking strength", "Strength of the original sine and cosine banking formulas.", "flight", K::Number, 0, 100, 1, "enable_banking"},
    {"simulate_control_surface_efficacy", "/general/banking/simulate_control_surface_efficacy", "Control surface efficacy", "Scale flight input by velocity along the direction you face.", "flight", K::Toggle},
    {"automatic_righting", "/general/banking/automatic_righting", "Automatic righting", "Ease moderate banks toward level flight using the original cubic curve.", "flight", K::Toggle},
    {"righting_strength", "/general/banking/righting_strength", "Righting strength", "Strength of automatic leveling.", "flight", K::Number, 0, 100, 1, "automatic_righting"},
    {"show_momentum_widget", "/general/controls/show_momentum_widget", "Momentum widget", "Move the native crosshair and draw its steering line while momentum steering is active.", "controls", K::Toggle, 0, 0, 0, "momentum_based_mouse"},
    {"touch_buttons", "/android/touch_buttons", "Touch controls", "Show flight buttons. Move, resize or hide individual buttons in the launcher HUD editor.", "touch", K::Toggle},
    {"button_opacity", "/android/button_opacity", "Button opacity", "Opacity of flight touch buttons.", "touch", K::Number, 10, 100, 1, "touch_buttons"},
    {"enable_gamepad", "/general/misc/enable_gamepad", "Controller flight input", "Use the right stick for pitch and roll and the bumpers for yaw.", "controller", K::Toggle},
    {"gamepad_deadzone", "/general/misc/gamepad_deadzone", "Controller deadzone", "Radial deadzone, rescaled from neutral to full stick travel.", "controller", K::Number, 0, 0.95, 0.01, "enable_gamepad"},
    {"desktop_pitch", "/sensitivity/desktop/pitch", "Mouse pitch", "Pitch multiplier for mouse and keyboard steering.", "sensitivity", K::Number, 0, 5, 0.05},
    {"desktop_yaw", "/sensitivity/desktop/yaw", "Mouse yaw", "Yaw multiplier for mouse and keyboard steering.", "sensitivity", K::Number, 0, 5, 0.05},
    {"desktop_roll", "/sensitivity/desktop/roll", "Mouse roll", "Roll multiplier for mouse and keyboard steering.", "sensitivity", K::Number, 0, 5, 0.05},
    {"touch_pitch", "/sensitivity/touch/pitch", "Touch pitch", "Pitch multiplier for touch look gestures.", "touch", K::Number, 0, 5, 0.05},
    {"touch_yaw", "/sensitivity/touch/yaw", "Touch yaw", "Yaw multiplier for touch look gestures.", "touch", K::Number, 0, 5, 0.05},
    {"touch_roll", "/sensitivity/touch/roll", "Touch roll", "Roll multiplier for touch look gestures.", "touch", K::Number, 0, 5, 0.05},
    {"controller_pitch", "/sensitivity/controller/pitch", "Controller pitch", "Pitch multiplier for the right stick.", "controller", K::Number, 0, 5, 0.05, "enable_gamepad"},
    {"controller_yaw", "/sensitivity/controller/yaw", "Controller yaw", "Yaw multiplier for the bumpers.", "controller", K::Number, 0, 5, 0.05, "enable_gamepad"},
    {"controller_roll", "/sensitivity/controller/roll", "Controller roll", "Roll multiplier for the right stick.", "controller", K::Number, 0, 5, 0.05, "enable_gamepad"},
    {"smoothing_pitch", "/sensitivity/camera_smoothing/pitch", "Pitch smoothing", "Zero disables smoothing. Larger values add more lag.", "smoothing", K::Number, 0, 10, 0.1},
    {"smoothing_yaw", "/sensitivity/camera_smoothing/yaw", "Yaw smoothing", "Zero disables smoothing. Larger values add more lag.", "smoothing", K::Number, 0, 10, 0.1},
    {"smoothing_roll", "/sensitivity/camera_smoothing/roll", "Roll smoothing", "Zero disables smoothing. Larger values add more lag.", "smoothing", K::Number, 0, 10, 0.1},
    {"toggle_enabled", "/android/keybinds/toggle_enabled", "Toggle mod", "Toggle flight control; default I.", "keys", K::Keybind, 0, 512, 1},
    {"pitch_up", "/android/keybinds/pitch_up", "Pitch up", "Hold to pitch upward.", "keys", K::Keybind, 0, 512, 1},
    {"pitch_down", "/android/keybinds/pitch_down", "Pitch down", "Hold to pitch downward.", "keys", K::Keybind, 0, 512, 1},
    {"yaw_left", "/android/keybinds/yaw_left", "Yaw left", "Hold to yaw left; default A.", "keys", K::Keybind, 0, 512, 1},
    {"yaw_right", "/android/keybinds/yaw_right", "Yaw right", "Hold to yaw right; default D.", "keys", K::Keybind, 0, 512, 1},
    {"roll_left", "/android/keybinds/roll_left", "Roll left", "Hold to roll left.", "keys", K::Keybind, 0, 512, 1},
    {"roll_right", "/android/keybinds/roll_right", "Roll right", "Hold to roll right.", "keys", K::Keybind, 0, 512, 1},
};
}

std::span<const SettingSpec> settingSpecs() { return specs; }
std::string valueOf(const core::Config& config, const SettingSpec& spec) {
    const auto value = config.toJson().at(nlohmann::json::json_pointer(spec.path));
    return value.is_string() ? value.get<std::string>() : value.dump();
}
Settings& Settings::get() { static Settings instance; return instance; }
std::shared_ptr<const core::Config> Settings::snapshot() const { return std::atomic_load(&current); }

bool Settings::load(const std::filesystem::path& root) {
    std::lock_guard lock(mutex);
    path = root / "config" / "config.json";
    core::Config config;
    if (!config.load(path)) return false;
    auto values = config.toJson();
    for (const auto& spec : specs) {
        if (spec.kind != K::Number && spec.kind != K::Keybind) continue;
        auto& value = values[nlohmann::json::json_pointer(spec.path)];
        double number = value.get<double>();
        if (!std::isfinite(number)) number = nlohmann::json(core::Config{}.toJson()).at(nlohmann::json::json_pointer(spec.path)).get<double>();
        number = std::clamp(number, spec.minimum, spec.maximum);
        value = spec.kind == K::Keybind ? nlohmann::json(static_cast<int>(number)) : nlohmann::json(number);
    }
    config.readJson(values);
    if (!config.save(path)) return false;
    std::atomic_store(&current, std::make_shared<const core::Config>(config));
    return true;
}

bool Settings::change(std::string_view key, std::string_view text) {
    std::lock_guard lock(mutex);
    for (const auto& spec : specs) {
        if (key != spec.key) continue;
        auto config = *snapshot();
        auto root = config.toJson();
        nlohmann::json value;
        if (spec.kind == K::Choice) {
            if (text != "VANILLA" && text != "TRIPLE_JUMP" && text != "HYBRID" && text != "HYBRID_TOGGLE") return false;
            value = text;
        } else {
            value = nlohmann::json::parse(text, nullptr, false);
            if (spec.kind == K::Toggle) {
                if (!value.is_boolean()) return false;
            } else {
                if (!value.is_number()) return false;
                auto number = value.get<double>();
                if (!std::isfinite(number)) return false;
                number = std::clamp(number, spec.minimum, spec.maximum);
                value = spec.kind == K::Keybind ? nlohmann::json(static_cast<int>(number)) : nlohmann::json(number);
            }
        }
        if (root.at(nlohmann::json::json_pointer(spec.path)) == value) return true;
        root[nlohmann::json::json_pointer(spec.path)] = std::move(value);
        config.readJson(root);
        if (!config.save(path)) return false;
        std::atomic_store(&current, std::make_shared<const core::Config>(config));
        return true;
    }
    return false;
}
}
