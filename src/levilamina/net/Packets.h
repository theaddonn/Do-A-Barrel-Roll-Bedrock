#pragma once

// Custom packets exchanged between the client and server mod. They travel inside LeviLamina's runtime packet
// (MinecraftPacketIds::LeviLaminaRuntimePacket), which vanilla peers never see because the client only talks to
// servers that advertise the mod (see Protocol.h).

#include "mc/deps/core/utility/optional_ref.h"

#include "ll/api/network/packet/Packet.h"
#include "levilamina/core/ServerConfig.h"

#include <cstdint>

namespace do_a_barrel_roll::net {

/// Client -> server: the client found the mod's marker command and asks for the server config.
class HelloC2SPacket : public ll::network::PacketBase<HelloC2SPacket> {
public:
    int protocolVersion = 0;

    void                  write(BinaryStream& bs) const override;
    Bedrock::Result<void> read(ReadOnlyBinaryStream& bs) override;
};

/// Server -> client: the config that applies to this client.
class ConfigSyncS2CPacket : public ll::network::PacketBase<ConfigSyncS2CPacket> {
public:
    int                       protocolVersion = 0;
    core::LimitedServerConfig config;
    bool                      isLimited  = false;
    bool                      compatible = true;

    void                  write(BinaryStream& bs) const override;
    Bedrock::Result<void> read(ReadOnlyBinaryStream& bs) override;
};

/// Client -> server: reply to a ConfigSync.
class ConfigResponseC2SPacket : public ll::network::PacketBase<ConfigResponseC2SPacket> {
public:
    int  protocolVersion = 0;
    bool success         = false;

    void                  write(BinaryStream& bs) const override;
    Bedrock::Result<void> read(ReadOnlyBinaryStream& bs) override;
};

/// Client -> server: own roll and thrust state. `velocity` is only meaningful while `thrusting`.
class RollSyncC2SPacket : public ll::network::PacketBase<RollSyncC2SPacket> {
public:
    bool  rolling   = false;
    float roll      = 0.0f;
    bool  thrusting = false;
    float velocity[3]{};

    void                  write(BinaryStream& bs) const override;
    Bedrock::Result<void> read(ReadOnlyBinaryStream& bs) override;
};

/// Server -> client: roll of another player.
class RollSyncS2CPacket : public ll::network::PacketBase<RollSyncS2CPacket> {
public:
    std::int64_t actorId = 0;
    bool         rolling = false;
    float        roll    = 0.0f;

    void                  write(BinaryStream& bs) const override;
    Bedrock::Result<void> read(ReadOnlyBinaryStream& bs) override;
};

} // namespace do_a_barrel_roll::net
