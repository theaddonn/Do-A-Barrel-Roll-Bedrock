#pragma once

#include "Signatures.hpp"
#include <cstddef>
#include <cstdint>

namespace dabr::build {
namespace field {
inline constexpr std::size_t ActorContext = 0x8;
inline constexpr std::size_t ContextRegistry = 0x8;
inline constexpr std::size_t ContextEntity = 0x10;
inline constexpr std::size_t ActorState = 0x208;
inline constexpr std::size_t ActorRotation = 0x218;
inline constexpr std::size_t ActorLevel = 0x1d0;
inline constexpr std::size_t ActorDimension = 0x1c0;
inline constexpr std::size_t Velocity = 0x18;
inline constexpr std::size_t RegistryBuckets = 0x38;
inline constexpr std::size_t RegistryBucketsEnd = 0x40;
inline constexpr std::size_t RegistryNodes = 0x50;
inline constexpr std::size_t RegistryNodesEnd = 0x58;
inline constexpr std::size_t RegistryNodeStride = 0x20;
inline constexpr std::size_t RegistryNodeKey = 0x8;
inline constexpr std::size_t RegistryNodeStorage = 0x10;
inline constexpr std::size_t StorageSparse = 0x8;
inline constexpr std::size_t StorageSparseEnd = 0x10;
inline constexpr std::size_t StoragePages = 0x50;
inline constexpr std::size_t MoveInputSize = 0x64;
inline constexpr std::size_t MoveInputFlags = 0x60;
inline constexpr std::size_t CameraOrientation = 0x28;
inline constexpr std::size_t ControlSize = 0x40;
inline constexpr std::size_t ClientGuiData = 0x638;
inline constexpr std::size_t GuiValidity = 0x30;
}
namespace slot {
inline constexpr std::size_t LocalPlayer = 32;
inline constexpr std::size_t UiFillRectangle = 16;
inline constexpr std::size_t UiFullClip = 27;
}
namespace preloader {
inline constexpr std::size_t ContextId = 0x8;
inline constexpr std::size_t ContextRoot = 0xe0;
}
}
