#include "Menu.hpp"
#include "Input.hpp"
#include <algorithm>
#include <dlfcn.h>
#include <nlohmann/json.hpp>

namespace dabr {
namespace {
constexpr auto moduleId = "doabarrelroll";
constexpr auto controlsId = "doabarrelroll_controls";
constexpr const char* actions[]{"toggle_enabled", "pitch_up", "pitch_down", "yaw_left", "yaw_right", "roll_left", "roll_right"};
constexpr const char* labels[]{"Flight", "Pitch up", "Pitch down", "Yaw left", "Yaw right", "Roll left", "Roll right"};

std::string schema(const core::Config& current) {
    using nlohmann::json;
    json root{{"version", 2}, {"default_category", "flight"}, {"categories", json::array()}, {"nodes", json::array()}};
    constexpr const char* categories[][2]{{"flight", "Flight"}, {"controls", "Steering"}, {"touch", "Touch"}, {"keys", "Keybinds"}, {"controller", "Controller"}, {"sensitivity", "Mouse"}, {"smoothing", "Smoothing"}};
    for (const auto& category : categories) root["categories"].push_back({{"id", category[0]}, {"title", category[1]}});
    const core::Config defaults;
    for (const auto& spec : settingSpecs()) {
        const char* type = spec.kind == SettingKind::Toggle ? "toggle" : spec.kind == SettingKind::Choice ? "choice" : spec.kind == SettingKind::Keybind ? "keybind" : "slider_float";
        json node{{"id", spec.key}, {"key", spec.key}, {"title", spec.title}, {"description", spec.description}, {"category", spec.category}, {"type", type}, {"default_value", valueOf(defaults, spec)}, {"current_value", valueOf(current, spec)}};
        if (spec.kind == SettingKind::Number || spec.kind == SettingKind::Keybind) {
            node["min_value"] = std::to_string(spec.minimum);
            node["max_value"] = std::to_string(spec.maximum);
            node["step"] = std::to_string(spec.step);
        }
        if (spec.dependsOn) node["visible_when"] = json::array({{{"key", spec.dependsOn}, {"op", "truthy"}, {"value", ""}}});
        if (spec.kind == SettingKind::Choice) {
            node["style"] = "dropdown";
            node["options"] = json::array({{{"value", "VANILLA"}, {"label", "Vanilla"}}, {{"value", "TRIPLE_JUMP"}, {"label", "Triple jump"}}, {{"value", "HYBRID"}, {"label", "Hybrid"}}, {{"value", "HYBRID_TOGGLE"}, {"label", "Hybrid toggle"}}});
        }
        root["nodes"].push_back(std::move(node));
    }
    return root.dump();
}
}

bool Menu::connect(std::string_view identity) {
    if (registered) return true;
    handle = dlopen("libpreloader.so", RTLD_NOW | RTLD_NOLOAD);
    if (!handle) return false;
    owner = identity;
    registerModule = reinterpret_cast<decltype(registerModule)>(dlsym(handle, "_ZN2pl7modmenu14registerModuleERKNS0_10ModuleInfoE"));
    unregisterModule = reinterpret_cast<decltype(unregisterModule)>(dlsym(handle, "_ZN2pl7modmenu16unregisterModuleENSt6__ndk117basic_string_viewIcNS1_11char_traitsIcEEEE"));
    setEnabled = reinterpret_cast<decltype(setEnabled)>(dlsym(handle, "_ZN2pl7modmenu16setModuleEnabledENSt6__ndk117basic_string_viewIcNS1_11char_traitsIcEEEEb"));
    setSchema = reinterpret_cast<decltype(setSchema)>(dlsym(handle, "_ZN2pl7modmenu19setConfigSchemaJsonENSt6__ndk117basic_string_viewIcNS1_11char_traitsIcEEEES5_"));
    registerButton = reinterpret_cast<decltype(registerButton)>(dlsym(handle, "_ZN2pl7modmenu14registerButtonERKNS0_10ButtonInfoE"));
    unregisterButton = reinterpret_cast<decltype(unregisterButton)>(dlsym(handle, "_ZN2pl7modmenu16unregisterButtonENSt6__ndk117basic_string_viewIcNS1_11char_traitsIcEEEE"));
    if (!registerModule || !unregisterModule || !setSchema || !setEnabled || !registerButton || !unregisterButton) { disconnect(); return false; }
    const auto current = Settings::get().snapshot();
    pl::modmenu::ModuleInfo module;
    module.moduleId = moduleId;
    module.displayName = "Do a Barrel Roll";
    module.description = "Client-side elytra pitch, yaw and roll with banking and smoothing.";
    module.modId = owner;
    module.defaultEnabled = current->modEnabled;
    module.hideInHudEditor = true;
    module.onToggle = [this](std::string_view, bool enabled) {
        Settings::get().change("mod_enabled", enabled ? "true" : "false");
        refresh();
    };
    module.onConfigChanged = [this](std::string_view, std::string_view key, std::string_view value) {
        Settings::get().change(key, value);
        refresh();
        if (key == "mod_enabled") enabled(Settings::get().snapshot()->modEnabled);
    };
    module.onKeybind = [](std::string_view, std::string_view key, bool down) {
        for (std::size_t i = 0; i < std::size(actions); ++i) if (key == actions[i]) Input::get().button(i, down);
    };
    for (const auto& spec : settingSpecs()) {
        const auto type = spec.kind == SettingKind::Toggle ? pl::modmenu::ConfigType::Toggle : spec.kind == SettingKind::Keybind ? pl::modmenu::ConfigType::Keybind : spec.kind == SettingKind::Choice ? pl::modmenu::ConfigType::Radio : pl::modmenu::ConfigType::SliderFloat;
        module.configs.push_back({spec.key, spec.title, type, valueOf(*current, spec), std::to_string(spec.minimum), std::to_string(spec.maximum), spec.dependsOn ? spec.dependsOn : ""});
    }
    if (!registerModule(module)) { disconnect(); return false; }
    registered = true;
    if (!setSchema(moduleId, schema(*current))) { disconnect(); return false; }
    pl::modmenu::ModuleInfo controls;
    controls.moduleId = controlsId;
    controls.displayName = "Flight buttons";
    controls.description = "Touch buttons for Do a Barrel Roll. Layout is editable in the HUD editor; visibility follows active elytra flight.";
    controls.modId = owner;
    controls.defaultEnabled = false;
    if (!registerModule(controls)) { disconnect(); return false; }
    buttons(current->buttonOpacity);
    return true;
}

void Menu::buttons(double value) {
    opacity = value;
    const auto alpha = static_cast<std::uint32_t>(std::clamp(value, 10.0, 100.0) * 2.55);
    for (std::size_t i = 0; i < std::size(actions); ++i) {
        pl::modmenu::ButtonInfo info;
        info.buttonId = std::string("doabarrelroll_") + actions[i];
        info.moduleId = controlsId;
        info.modId = owner;
        info.displayName = labels[i];
        info.label = labels[i];
        info.behavior = pl::modmenu::ButtonBehavior::Hold;
        info.defaultVisible = i == 3 || i == 4;
        info.widthScale = 1.2f;
        info.normalBgColor = (alpha << 24) | 0x202020u;
        info.activeBgColor = (alpha << 24) | 0x528c32u;
        info.borderColor = 0x99ffffffu;
        info.textColor = info.activeTextColor = 0xffffffffu;
        info.onEvent = [i](std::string_view, pl::modmenu::ButtonEvent event, float) {
            if (event == pl::modmenu::ButtonEvent::Down) Input::get().button(i, true);
            else if (event == pl::modmenu::ButtonEvent::Up) Input::get().button(i, false);
        };
        unregisterButton(info.buttonId);
        Input::get().button(i, false);
        registerButton(info);
    }
}
void Menu::frame(bool flying, const core::Config& config) {
    if (!registered) return;
    if (config.buttonOpacity != opacity) buttons(config.buttonOpacity);
    const bool visible = flying && config.touchButtons;
    if (visible != controlsVisible) { controlsVisible = visible; setEnabled(controlsId, visible); }
}
void Menu::refresh() {
    std::lock_guard lock(schemaMutex);
    if (registered && setSchema) setSchema(moduleId, schema(*Settings::get().snapshot()));
}
void Menu::enabled(bool value) {
    if (registered) setEnabled(moduleId, value);
    refresh();
}
void Menu::disconnect() {
    if (registered) {
        if (unregisterButton) for (const auto* action : actions) unregisterButton(std::string("doabarrelroll_") + action);
        if (unregisterModule) { unregisterModule(controlsId); unregisterModule(moduleId); }
    }
    registered = false;
    controlsVisible = false;
    opacity = -1;
    if (handle) dlclose(handle);
    handle = nullptr;
}
}
