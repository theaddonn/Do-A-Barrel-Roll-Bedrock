#pragma once

// Port of nl.enjarai.doabarrelroll.config.ModConfigServer / LimitedModConfigServer.

#include <filesystem>

namespace do_a_barrel_roll::core {

/// The part of the server config that is sent to a client.
struct LimitedServerConfig {
    bool allowThrusting = false;
    bool forceEnabled   = false;

    bool operator==(const LimitedServerConfig&) const = default;
};

struct ServerConfig {
    bool allowThrusting = false;
    bool forceEnabled   = false;
    /// Kick players that do not complete the handshake within installedTimeout ticks of joining.
    bool forceInstalled   = false;
    int  installedTimeout = 400;
    /// Largest velocity (blocks per tick) accepted from a thrusting client; 0 is unlimited.
    double maxThrustSpeed = 8.0;

    [[nodiscard]] LimitedServerConfig limited() const { return {allowThrusting, forceEnabled}; }

    /// Missing file, missing or mistyped keys keep defaults. Returns false only if the file is unparsable.
    bool load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path) const;
};

} // namespace do_a_barrel_roll::core
