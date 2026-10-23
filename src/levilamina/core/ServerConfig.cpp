#include "levilamina/core/ServerConfig.h"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace do_a_barrel_roll::core {

using nlohmann::json;

namespace {

template <typename T>
void get(const json& j, const char* key, T& out) {
    auto it = j.find(key);
    if (it == j.end()) return;
    try {
        out = it->get<T>();
    } catch (...) {}
}

} // namespace

bool ServerConfig::load(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) return true;
    json j = json::parse(in, nullptr, false);
    if (!j.is_object()) return false;
    get(j, "allowThrusting", allowThrusting);
    get(j, "forceEnabled", forceEnabled);
    get(j, "forceInstalled", forceInstalled);
    get(j, "installedTimeout", installedTimeout);
    get(j, "maxThrustSpeed", maxThrustSpeed);
    if (!(maxThrustSpeed >= 0.0)) maxThrustSpeed = 0.0;
    installedTimeout = std::max(installedTimeout, 1);
    return true;
}

bool ServerConfig::save(const std::filesystem::path& path) const {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path);
    if (!out) return false;
    json j{
        {"allowThrusting",   allowThrusting  },
        {"forceEnabled",     forceEnabled    },
        {"forceInstalled",   forceInstalled  },
        {"installedTimeout", installedTimeout},
        {"maxThrustSpeed",   maxThrustSpeed  },
    };
    out << j.dump(2) << '\n';
    return static_cast<bool>(out);
}

} // namespace do_a_barrel_roll::core
