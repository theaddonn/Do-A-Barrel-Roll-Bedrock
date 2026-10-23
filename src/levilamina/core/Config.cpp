#include "levilamina/core/Config.h"

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

// Type-tolerant getters: a wrongly typed value keeps the default instead of throwing.
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

} // namespace

bool Config::load(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) return true; // no file: defaults
    json root = json::parse(in, nullptr, /*allow_exceptions=*/false);
    if (root.is_discarded() || !root.is_object()) return false;

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

    get(sub(general, "hud"), "show_horizon", showHorizon);

    const json& b = sub(general, "banking");
    get(b, "enable_banking", enableBanking);
    get(b, "banking_strength", bankingStrength);
    get(b, "simulate_control_surface_efficacy", simulateControlSurfaceEfficacy);
    get(b, "automatic_righting", automaticRighting);
    get(b, "righting_strength", rightingStrength);

    const json& t = sub(general, "thrust");
    get(t, "enable_thrust", enableThrust);
    get(t, "max_thrust", maxThrust);
    get(t, "thrust_acceleration", thrustAcceleration);
    get(t, "thrust_particles", thrustParticles);

    const json& misc = sub(general, "misc");
    get(misc, "enable_easter_eggs", enableEasterEggs);
    get(misc, "enable_gamepad", enableGamepad);
    get(misc, "gamepad_deadzone", gamepadDeadzone);
    get(misc, "thrust_allow_remote", thrustAllowRemote);

    const json& s = sub(root, "sensitivity");
    readSens(sub(s, "camera_smoothing"), smoothing);
    readSens(sub(s, "desktop"), desktop);
    readSens(sub(s, "controller"), controller);
    return true;
}

bool Config::save(const std::filesystem::path& path) const {
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
          {"hud", {{"show_horizon", showHorizon}}},
          {"banking",
           {{"enable_banking", enableBanking},
            {"banking_strength", bankingStrength},
            {"simulate_control_surface_efficacy", simulateControlSurfaceEfficacy},
            {"automatic_righting", automaticRighting},
            {"righting_strength", rightingStrength}}},
          {"thrust",
           {{"enable_thrust", enableThrust},
            {"max_thrust", maxThrust},
            {"thrust_acceleration", thrustAcceleration},
            {"thrust_particles", thrustParticles}}},
          {"misc",
           {{"enable_easter_eggs", enableEasterEggs},
            {"enable_gamepad", enableGamepad},
            {"gamepad_deadzone", gamepadDeadzone},
            {"thrust_allow_remote", thrustAllowRemote}}}}},
        {"sensitivity",
         {{"camera_smoothing", writeSens(smoothing)},
          {"desktop", writeSens(desktop)},
          {"controller", writeSens(controller)}}},
    };
    std::error_code ec;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path);
    if (!out) return false;
    out << root.dump(2);
    return static_cast<bool>(out);
}

} // namespace do_a_barrel_roll::core
