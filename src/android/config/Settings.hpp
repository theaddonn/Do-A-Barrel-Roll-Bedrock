#pragma once

#include "core/Config.h"
#include <memory>
#include <mutex>
#include <span>
#include <string_view>

namespace dabr {
namespace core = do_a_barrel_roll::core;
enum class SettingKind { Toggle, Number, Choice, Keybind };
struct SettingSpec {
    const char* key;
    const char* path;
    const char* title;
    const char* description;
    const char* category;
    SettingKind kind;
    double minimum{}, maximum{}, step{};
    const char* dependsOn{};
};
std::span<const SettingSpec> settingSpecs();
std::string valueOf(const core::Config&, const SettingSpec&);

class Settings {
public:
    static Settings& get();
    bool load(const std::filesystem::path&);
    bool change(std::string_view key, std::string_view value);
    std::shared_ptr<const core::Config> snapshot() const;
private:
    std::mutex mutex;
    std::shared_ptr<const core::Config> current{std::make_shared<const core::Config>()};
    std::filesystem::path path;
};
}
