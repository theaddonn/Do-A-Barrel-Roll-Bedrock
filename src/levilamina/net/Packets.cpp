#include "levilamina/net/Packets.h"

#include "mc/deps/core/utility/BinaryStream.h"
#include "mc/deps/core/utility/ReadOnlyBinaryStream.h"

namespace do_a_barrel_roll::net {

namespace {

template <class T>
Bedrock::Result<void> take(Bedrock::Result<T>&& result, T& out) {
    if (!result) return nonstd::make_unexpected(result.error());
    out = std::move(result).value();
    return {};
}

#define DABR_READ(getter, out)                                                                                         \
    if (auto r = take(bs.getter(), out); !r) return r

void putBool(BinaryStream& bs, bool v) { bs.writeBool(v, "", ""); }
void putInt(BinaryStream& bs, int v) { bs.writeVarInt(v, "", ""); }
void putFloat(BinaryStream& bs, float v) { bs.writeFloat(v, "", ""); }

} // namespace

void HelloC2SPacket::write(BinaryStream& bs) const { putInt(bs, protocolVersion); }

Bedrock::Result<void> HelloC2SPacket::read(ReadOnlyBinaryStream& bs) {
    DABR_READ(getVarInt, protocolVersion);
    return {};
}

void ConfigSyncS2CPacket::write(BinaryStream& bs) const {
    putInt(bs, protocolVersion);
    putBool(bs, config.allowThrusting);
    putBool(bs, config.forceEnabled);
    putBool(bs, isLimited);
    putBool(bs, compatible);
}

Bedrock::Result<void> ConfigSyncS2CPacket::read(ReadOnlyBinaryStream& bs) {
    DABR_READ(getVarInt, protocolVersion);
    DABR_READ(getBool, config.allowThrusting);
    DABR_READ(getBool, config.forceEnabled);
    DABR_READ(getBool, isLimited);
    DABR_READ(getBool, compatible);
    return {};
}

void ConfigResponseC2SPacket::write(BinaryStream& bs) const {
    putInt(bs, protocolVersion);
    putBool(bs, success);
}

Bedrock::Result<void> ConfigResponseC2SPacket::read(ReadOnlyBinaryStream& bs) {
    DABR_READ(getVarInt, protocolVersion);
    DABR_READ(getBool, success);
    return {};
}

void RollSyncC2SPacket::write(BinaryStream& bs) const {
    putBool(bs, rolling);
    putFloat(bs, roll);
    putBool(bs, thrusting);
    if (thrusting) {
        for (float v : velocity) putFloat(bs, v);
    }
}

Bedrock::Result<void> RollSyncC2SPacket::read(ReadOnlyBinaryStream& bs) {
    DABR_READ(getBool, rolling);
    DABR_READ(getFloat, roll);
    DABR_READ(getBool, thrusting);
    if (thrusting) {
        for (float& v : velocity) DABR_READ(getFloat, v);
    }
    return {};
}

void RollSyncS2CPacket::write(BinaryStream& bs) const {
    bs.writeSignedInt64(actorId, "", "");
    putBool(bs, rolling);
    putFloat(bs, roll);
}

Bedrock::Result<void> RollSyncS2CPacket::read(ReadOnlyBinaryStream& bs) {
    std::int64_t id{};
    DABR_READ(getSignedInt64, id);
    actorId = id;
    DABR_READ(getBool, rolling);
    DABR_READ(getFloat, roll);
    return {};
}

} // namespace do_a_barrel_roll::net
