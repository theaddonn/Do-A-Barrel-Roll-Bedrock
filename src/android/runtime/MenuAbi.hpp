#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pl::modmenu {

    enum class ConfigType {
        Toggle,
        SliderInt,
        SliderFloat,
        Radio,
        Color,
        Keybind,
        Text,
        Button,
    };

    enum class ButtonBehavior {
        Click,
        Hold,
        Toggle,
    };

    enum class ButtonEvent {
        Click,
        Down,
        Up,
        StateChanged,
        Scroll,
    };

    enum class ButtonStylePreset {
        Keycap,
        Accent,
    };

    enum class ButtonIconFormat {
        Auto,
        Png,
        Webp,
        Svg,
        Resource,
    };

    enum class DrawCommandType {
        Text,
        Rect,
        Line,
        RectFilled,
        CircleFilled,
        TriangleFilled,
        Image,
    };

    enum HudSnapFlags : std::uint32_t {
        HudSnapNone = 0,
        HudSnapGrid = 1u << 0,
        HudSnapElements = 1u << 1,
        HudSnapScreenCenter = 1u << 2,
    };

    struct HudEditorElement {
        std::string elementId;
        std::string displayName;
        std::string positionKeyX{"hudPosX"};
        std::string positionKeyY{"hudPosY"};
        std::string snapGroup;
        float x{};
        float y{};
        float width{};
        float height{};
        float gridSize{};
        float snapThreshold{12.0f};
        float gridGap{4.0f};
        std::uint32_t snapFlags{};
    };

    struct HudSurfaceSize {
        float width{};
        float height{};
    };

    struct ConfigEntry {
        std::string key;
        std::string displayName;
        ConfigType type{};
        std::string defaultValue;
        std::string minValue;
        std::string maxValue;
        std::string dependsOn;
    };

    struct ModuleInfo {
        std::string moduleId;
        std::string displayName;
        std::string description;
        std::string modId;
        bool defaultEnabled{};
        bool hideInHudEditor{};
        std::vector<ConfigEntry> configs;
        std::function<void(std::string_view moduleId, bool enabled)> onToggle;
        std::function<void(std::string_view moduleId, std::string_view key,
                std::string_view value)>
        onConfigChanged;
        std::function<void(std::string_view moduleId, std::string_view key,
                bool isDown)>
        onKeybind;
    };

    struct ButtonInfo {
        std::string buttonId;
        std::string moduleId;
        std::string displayName;
        std::string modId;
        std::string label;
        int androidKeyCode{};
        ButtonBehavior behavior{ButtonBehavior::Click};
        bool defaultVisible{true};
        ButtonStylePreset stylePreset{ButtonStylePreset::Keycap};
        uint32_t normalBgColor{};
        uint32_t activeBgColor{};
        uint32_t borderColor{};
        uint32_t textColor{};
        uint32_t activeTextColor{};
        float widthScale{};
        float heightScale{1.0f};
        ButtonIconFormat iconFormat{ButtonIconFormat::Auto};
        bool hideLabelWhenIconPresent{true};
        std::vector<unsigned char> iconData;
        std::vector<unsigned char> activeIconData;
        std::function<void(std::string_view buttonId, ButtonEvent event, float value)>
        onEvent;
    };

    struct DrawCommand {
        DrawCommandType type{};
        float x{};
        float y{};
        float w{};
        float h{};
        float x3{};
        float y3{};
        uint32_t color{};
        float size{};
        std::string text;
        std::string fontId;
        std::string imageId;
    };

}
