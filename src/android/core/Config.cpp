#include "core/Config.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace do_a_barrel_roll::core {

using nlohmann::json;

const char* toString(ActivationBehaviour b) {
    switch (b) {
    case ActivationBehaviour::TripleJump:
        return "TRIPLE_JUMP";
    case ActivationBehaviour::Hybrid:
        return "HYBRID";
    case ActivationBehaviour::HybridToggle:
        return "HYBRID_TOGGLE";
    default:
        return "VANILLA";
    }
}

namespace {

ActivationBehaviour parseBehaviour(const std::string& s, ActivationBehaviour fallback) {
    if (s == "VANILLA") return ActivationBehaviour::Vanilla;
    if (s == "TRIPLE_JUMP") return ActivationBehaviour::TripleJump;
    if (s == "HYBRID") return ActivationBehaviour::Hybrid;
    if (s == "HYBRID_TOGGLE") return ActivationBehaviour::HybridToggle;
    return fallback;
}

template <typename T>
void get(const json& j, const char* key, T& out) {
    if (!j.is_object()) return;
    auto it = j.find(key);
    if (it == j.end()) return;
    try {
        out = it->get<T>();
    } catch (...) {}
}

const json& sub(const json& j, const char* key) {
    static const json empty = json::object();
    if (!j.is_object()) return empty;
    auto it = j.find(key);
    return it == j.end() ? empty : *it;
}

void readSens(const json& j, Sensitivity& s) {
    get(j, "pitch", s.pitch);
    get(j, "yaw", s.yaw);
    get(j, "roll", s.roll);
}

json writeSens(const Sensitivity& s) { return json{{"pitch", s.pitch}, {"yaw", s.yaw}, {"roll", s.roll}}; }

}

void Config::readJson(const json& root) {
    get(root, "format_version_do_not_edit", formatVersion);
    const json& general = sub(root, "general");
    get(general, "mod_enabled", modEnabled);

    const json& c = sub(general, "controls");
    get(c, "switch_roll_and_yaw", switchRollAndYaw);
    get(c, "invert_pitch", invertPitch);
    get(c, "momentum_based_mouse", momentumBasedMouse);
    get(c, "momentum_mouse_deadzone", momentumMouseDeadzone);
    get(c, "show_momentum_widget", showMomentumWidget);
    get(c, "disable_when_submerged", disableWhenSubmerged);
    std::string beh;
    get(c, "activation_behaviour", beh);
    activationBehaviour = parseBehaviour(beh, activationBehaviour);

    const json& b = sub(general, "banking");
    get(b, "enable_banking", enableBanking);
    get(b, "banking_strength", bankingStrength);
    get(b, "simulate_control_surface_efficacy", simulateControlSurfaceEfficacy);
    get(b, "automatic_righting", automaticRighting);
    get(b, "righting_strength", rightingStrength);

    const json& misc = sub(general, "misc");
    get(misc, "enable_gamepad", enableGamepad);
    get(misc, "gamepad_deadzone", gamepadDeadzone);

    const json& s = sub(root, "sensitivity");
    readSens(sub(s, "camera_smoothing"), smoothing);
    readSens(sub(s, "desktop"), desktop);
    readSens(sub(s, "controller"), controller);
    const auto& android = sub(root, "android");
    get(android, "touch_buttons", touchButtons);
    get(android, "button_opacity", buttonOpacity);
    readSens(sub(s, "touch"), touch);
    const auto& keys = sub(android, "keybinds");
    constexpr const char* names[]{"toggle_enabled", "pitch_up", "pitch_down", "yaw_left", "yaw_right", "roll_left", "roll_right"};
    for (std::size_t i = 0; i < bindings.size(); ++i) get(keys, names[i], bindings[i]);
}

json Config::toJson() const {
    json root = {
        {"format_version_do_not_edit", 2},
        {"general",
         {{"mod_enabled", modEnabled},
          {"controls",
           {{"switch_roll_and_yaw", switchRollAndYaw},
            {"invert_pitch", invertPitch},
            {"momentum_based_mouse", momentumBasedMouse},
            {"momentum_mouse_deadzone", momentumMouseDeadzone},
            {"show_momentum_widget", showMomentumWidget},
            {"activation_behaviour", toString(activationBehaviour)},
            {"disable_when_submerged", disableWhenSubmerged}}},
          {"banking",
           {{"enable_banking", enableBanking},
            {"banking_strength", bankingStrength},
            {"simulate_control_surface_efficacy", simulateControlSurfaceEfficacy},
            {"automatic_righting", automaticRighting},
            {"righting_strength", rightingStrength}}},
          {"misc",
           {{"enable_gamepad", enableGamepad},
            {"gamepad_deadzone", gamepadDeadzone}}}}},
        {"sensitivity",
         {{"camera_smoothing", writeSens(smoothing)},
          {"desktop", writeSens(desktop)},
          {"controller", writeSens(controller)}, {"touch", writeSens(touch)}}},
    };
    constexpr const char* names[]{"toggle_enabled", "pitch_up", "pitch_down", "yaw_left", "yaw_right", "roll_left", "roll_right"};
    root["android"]["touch_buttons"] = touchButtons;
    root["android"]["button_opacity"] = buttonOpacity;
    for (std::size_t i = 0; i < bindings.size(); ++i) root["android"]["keybinds"][names[i]] = bindings[i];
    return root;
}

bool Config::load(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) return true;
    json root = json::parse(in, nullptr, false);
    if (!root.is_object()) return false;
    readJson(root);
    return true;
}

bool Config::save(const std::filesystem::path& path) const {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return false;
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::trunc);
        if (!out) return false;
        out << toJson().dump(2) << '\n';
        out.flush();
        if (!out) return false;
    }
    std::filesystem::rename(temporary, path, ec);
    return !ec;
}

}
