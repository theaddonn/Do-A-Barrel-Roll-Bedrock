#include "Game.hpp"
#include <cmath>

namespace dabr::game {
namespace {
const void* context(const void* actor) { return static_cast<const std::byte*>(actor) + build::field::ActorContext; }
bool finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
}
void* component(const void* ctx, std::uint32_t hash, std::size_t stride) {
    if (!ctx) return nullptr;
    const auto* registry = read<const void*>(ctx, build::field::ContextRegistry);
    if (!registry) return nullptr;
    const auto buckets = read<const std::uint64_t*>(registry, build::field::RegistryBuckets);
    const auto bucketsEnd = read<const std::uint64_t*>(registry, build::field::RegistryBucketsEnd);
    const auto* nodes = read<const std::byte*>(registry, build::field::RegistryNodes);
    const auto* nodesEnd = read<const std::byte*>(registry, build::field::RegistryNodesEnd);
    if (!buckets || !bucketsEnd || !nodes || bucketsEnd <= buckets || nodesEnd <= nodes) return nullptr;
    const auto bucketCount = static_cast<std::size_t>(bucketsEnd - buckets);
    const auto nodeCount = static_cast<std::size_t>(nodesEnd - nodes) / build::field::RegistryNodeStride;
    if (bucketCount > 1048576 || nodeCount > 65536 || (bucketCount & (bucketCount - 1))) return nullptr;
    auto index = buckets[hash & (bucketCount - 1)];
    const void* storage = nullptr;
    for (std::size_t count = 0; index < nodeCount && count < nodeCount; ++count) {
        const auto* node = nodes + index * build::field::RegistryNodeStride;
        if (read<std::uint32_t>(node, build::field::RegistryNodeKey) == hash) {
            storage = read<const void*>(node, build::field::RegistryNodeStorage);
            break;
        }
        index = read<std::uint64_t>(node);
    }
    if (!storage) return nullptr;
    const auto entity = read<std::uint32_t>(ctx, build::field::ContextEntity);
    const auto entityIndex = entity & 0x3ffffu;
    const auto sparse = read<const std::uint32_t* const*>(storage, build::field::StorageSparse);
    const auto sparseEnd = read<const std::uint32_t* const*>(storage, build::field::StorageSparseEnd);
    if (!sparse || !sparseEnd || sparseEnd <= sparse || sparseEnd - sparse > 128) return nullptr;
    const auto page = entityIndex >> 11;
    if (page >= static_cast<std::size_t>(sparseEnd - sparse) || !sparse[page]) return nullptr;
    const auto packed = sparse[page][entityIndex & 2047];
    if ((packed ^ (entity & 0xfffc0000u)) >= 0x3ffffu) return nullptr;
    if (!stride) return const_cast<void*>(storage);
    const auto pages = read<std::byte* const*>(storage, build::field::StoragePages);
    const auto packedIndex = packed & 0x3ffffu;
    if (!pages || !pages[packedIndex >> 7]) return nullptr;
    return pages[packedIndex >> 7] + (packedIndex & 127) * stride;
}

void* localPlayer(void* client) {
    const auto get = method<void* (*)(void*)>(client, build::slot::LocalPlayer);
    return get ? get(client) : nullptr;
}
Vec3 velocity(const void* actor) {
    const auto* state = actor ? read<const void*>(actor, build::field::ActorState) : nullptr;
    return state ? read<Vec3>(state, build::field::Velocity) : Vec3{};
}
void* level(const void* actor) { return actor ? read<void*>(actor, build::field::ActorLevel) : nullptr; }
void* dimension(const void* actor) { return actor ? read<void*>(actor, build::field::ActorDimension) : nullptr; }
core::PlayerState snapshot(void* actor, const Targets& targets) {
    core::PlayerState result;
    if (!actor) return result;
    const auto* rotation = read<const void*>(actor, build::field::ActorRotation);
    const auto angles = rotation ? read<Vec2>(rotation) : Vec2{};
    result.pitch = angles.x; result.yaw = angles.y;
    const auto v = velocity(actor);
    if (finite(v)) result.velocity = {v.x, v.y, v.z};
    result.isGliding = targets.isGliding(actor);
    result.isSubmergedInWater = targets.inWater(actor);
    result.onGround = targets.onGround(context(actor));
    if (const auto* move = component(context(actor), typeHash("MoveInputComponent"), build::field::MoveInputSize))
        result.isJumping = (read<std::uint16_t>(move, build::field::MoveInputFlags) & 16) != 0;
    return result;
}
}
