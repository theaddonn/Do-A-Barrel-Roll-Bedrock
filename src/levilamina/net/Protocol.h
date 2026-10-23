#pragma once

namespace do_a_barrel_roll::net {

constexpr int PROTOCOL_VERSION = 1;

/// Registered by the server mod. A client only starts the handshake after seeing this command in the server's
/// command list, so vanilla servers never receive a LeviLamina runtime packet.
constexpr const char* MARKER_COMMAND = "doabarrelroll";

} // namespace do_a_barrel_roll::net
