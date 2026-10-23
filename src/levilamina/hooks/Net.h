#pragma once

#include <cstdint>
#include <optional>

namespace do_a_barrel_roll::hooks {

void installNet();
void uninstallNet();

/// Roll in degrees (render convention, same as state::cameraRollDeg) of another player that is rolling, as reported
/// by the server mod.
[[nodiscard]] std::optional<float> otherPlayerRoll(std::int64_t actorId);

/// True while connected to a server that runs the server mod and completed the handshake.
[[nodiscard]] bool serverModConnected();

} // namespace do_a_barrel_roll::hooks
