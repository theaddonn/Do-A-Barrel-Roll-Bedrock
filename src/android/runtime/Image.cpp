#include "Image.hpp"
#include <algorithm>
#include <elf.h>
#include <link.h>

namespace dabr {
namespace {
std::vector<std::int16_t> bytePattern(std::string_view text) {
    std::vector<std::int16_t> result;
    const auto hex = [](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        return -1;
    };
    std::size_t position = 0;
    while (position < text.size()) {
        position = text.find_first_not_of(" \t\r\n", position);
        if (position == text.npos) break;
        auto end = text.find_first_of(" \t\r\n", position);
        if (end == text.npos) end = text.size();
        const auto token = text.substr(position, end - position);
        if (token == "?" || token == "??") result.push_back(-1);
        else {
            if (token.size() != 2 || hex(token[0]) < 0 || hex(token[1]) < 0) return {};
            result.push_back(static_cast<std::int16_t>(hex(token[0]) * 16 + hex(token[1])));
        }
        if (result.size() > 4096) return {};
        position = end;
    }
    return result;
}
}
bool Image::locate() {
    segments.clear();
    unwindHeader = unwindTable = unwindCount = 0;
    dl_iterate_phdr([](dl_phdr_info* info, std::size_t, void* argument) {
        std::string_view name(info->dlpi_name ? info->dlpi_name : "");
        name.remove_prefix(name.find_last_of('/') == name.npos ? 0 : name.find_last_of('/') + 1);
        if (name != "libminecraftpe.so") return 0;
        auto& image = *static_cast<Image*>(argument);
        for (std::size_t index = 0; index < info->dlpi_phnum; ++index) {
            const auto& header = info->dlpi_phdr[index];
            if (header.p_type == PT_GNU_EH_FRAME) image.unwindHeader = info->dlpi_addr + header.p_vaddr;
            if (header.p_type != PT_LOAD || !(header.p_flags & PF_R)) continue;
            const auto begin = info->dlpi_addr + header.p_vaddr;
            if (header.p_memsz && begin + header.p_memsz > begin)
                image.segments.push_back({begin, begin + header.p_memsz, header.p_flags});
        }
        return 1;
    }, this);
    if (unwindHeader && contains(unwindHeader, 12, PF_R) &&
        read<std::uint32_t>(reinterpret_cast<const void*>(unwindHeader)) == 0x3b031b01u) {
        const auto count = read<std::uint32_t>(reinterpret_cast<const void*>(unwindHeader + 8));
        if (count && count <= 0x1000000 && contains(unwindHeader + 12, static_cast<std::size_t>(count) * 8, PF_R)) {
            unwindTable = unwindHeader + 12;
            unwindCount = count;
        }
    }
    return !segments.empty();
}

bool Image::contains(std::uintptr_t address, std::size_t bytes, unsigned flags) const {
    for (const auto& segment : segments)
        if ((segment.flags & flags) == flags && address >= segment.begin &&
            address <= segment.end && bytes <= segment.end - address) return true;
    return false;
}

bool Image::matches(std::uintptr_t address, std::span<const std::int16_t> pattern) const {
    if ((address & 3) || !contains(address, pattern.size(), PF_R | PF_X)) return false;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(address);
    for (std::size_t index = 0; index < pattern.size(); ++index)
        if (pattern[index] >= 0 && bytes[index] != pattern[index]) return false;
    return true;
}

std::uintptr_t Image::functionEntry(std::uintptr_t anchor, std::size_t length) const {
    if (!unwindTable || !unwindCount || !contains(anchor, length, PF_R | PF_X)) return 0;
    const auto entry = [&](std::size_t index) {
        return static_cast<std::uintptr_t>(static_cast<std::intptr_t>(unwindHeader) +
            read<std::int32_t>(reinterpret_cast<const void*>(unwindTable + index * 8)));
    };
    std::size_t first = 0, last = unwindCount;
    while (first < last) {
        const auto middle = first + (last - first) / 2;
        if (entry(middle) <= anchor) first = middle + 1;
        else last = middle;
    }
    if (!first) return 0;
    const auto begin = entry(first - 1);
    if (!contains(begin, 4, PF_R | PF_X) || anchor - begin > 4096) return 0;
    if (first < unwindCount && anchor + length > entry(first)) return 0;
    return begin;
}

std::size_t Image::scan(std::span<const std::int16_t> pattern, std::uintptr_t& result) const {
    std::size_t anchorStart = 0, anchorLength = 0, start = 0;
    for (std::size_t index = 0; index <= pattern.size(); ++index) {
        if (index < pattern.size() && pattern[index] >= 0) continue;
        if (index - start > anchorLength) { anchorStart = start; anchorLength = index - start; }
        start = index + 1;
    }
    if (!anchorLength) return 0;
    std::vector<std::uint8_t> anchor;
    for (std::size_t index = 0; index < anchorLength; ++index) anchor.push_back(static_cast<std::uint8_t>(pattern[anchorStart + index]));
    std::size_t count = 0;
    for (const auto& segment : segments) {
        if (!(segment.flags & PF_X) || segment.end - segment.begin < pattern.size()) continue;
        auto cursor = segment.begin + anchorStart;
        const auto last = segment.end - pattern.size() + anchorStart;
        while (cursor <= last) {
            const auto* found = static_cast<const std::uint8_t*>(std::memchr(reinterpret_cast<const void*>(cursor), anchor[0], last - cursor + 1));
            if (!found) break;
            const auto position = reinterpret_cast<std::uintptr_t>(found);
            const auto candidate = position - anchorStart;
            if (!(candidate & 3) && std::memcmp(found, anchor.data(), anchor.size()) == 0 && matches(candidate, pattern)) {
                result = candidate;
                if (++count == 2) return count;
            }
            cursor = position + 1;
        }
    }
    return count;
}

std::uintptr_t Image::vtable(std::string_view name) const {
    std::vector<std::uintptr_t> names, types;
    for (const auto& segment : segments) {
        if (segment.end - segment.begin < name.size() + 1) continue;
        auto cursor = segment.begin;
        const auto last = segment.end - name.size() - 1;
        while (cursor <= last) {
            const auto* found = static_cast<const char*>(std::memchr(reinterpret_cast<const void*>(cursor), name.front(), last - cursor + 1));
            if (!found) break;
            const auto address = reinterpret_cast<std::uintptr_t>(found);
            if (!std::memcmp(found, name.data(), name.size()) && found[name.size()] == 0) names.push_back(address);
            cursor = address + 1;
        }
    }
    if (names.empty()) return 0;
    for (const auto& segment : segments) {
        if (segment.flags & PF_X || segment.end - segment.begin < 16) continue;
        for (auto address = (segment.begin + 7) & ~std::uintptr_t(7); address <= segment.end - 8; address += 8) {
            const auto value = read<std::uintptr_t>(reinterpret_cast<const void*>(address));
            if (std::find(names.begin(), names.end(), value) != names.end() && contains(address - 8, 16, PF_R)) types.push_back(address - 8);
        }
    }
    if (types.empty()) return 0;
    std::uintptr_t result = 0;
    for (const auto& segment : segments) {
        if (segment.flags & PF_X || segment.end - segment.begin < 16) continue;
        for (auto address = (segment.begin + 7) & ~std::uintptr_t(7); address <= segment.end - 8; address += 8) {
            const auto value = read<std::uintptr_t>(reinterpret_cast<const void*>(address));
            if (std::find(types.begin(), types.end(), value) == types.end() || !contains(address - 8, 16, PF_R) || read<std::uintptr_t>(reinterpret_cast<const void*>(address - 8))) continue;
            const auto table = address + 8;
            if (!contains(table, (build::slot::UiFullClip + 1) * 8, PF_R)) continue;
            const auto fill = read<std::uintptr_t>(reinterpret_cast<const void*>(table + build::slot::UiFillRectangle * 8));
            const auto clip = read<std::uintptr_t>(reinterpret_cast<const void*>(table + build::slot::UiFullClip * 8));
            if (!contains(fill, 4, PF_R | PF_X) || !contains(clip, 4, PF_R | PF_X)) continue;
            if (result && result != table) return 0;
            result = table;
        }
    }
    return result;
}

bool Image::resolve(Targets& output) {
    if (!unwindTable) { return false; }
    std::array<std::uintptr_t, static_cast<std::size_t>(build::Symbol::Count)> addresses{};
    std::array<std::uintptr_t, static_cast<std::size_t>(build::Symbol::Count)> anchors{};
    for (const auto& spec : build::Code) {
        const auto pattern = bytePattern(spec.pattern);
        if (pattern.empty()) { return false; }
        std::uintptr_t match = 0;
        const auto count = scan(pattern, match);
        if (count != 1) {
            return false;
        }
        auto entry = functionEntry(match, pattern.size());
        if (!entry) { return false; }
        if (spec.resolve == build::ResolveKind::Call) {
            if (spec.callOffset > pattern.size() || pattern.size() - spec.callOffset < 4) { return false; }
            const auto call = match + spec.callOffset;
            const auto word = read<std::uint32_t>(reinterpret_cast<const void*>(call));
            if ((word & 0xfc000000u) != 0x94000000u) { return false; }
            std::int64_t displacement = word & 0x03ffffffu;
            if (displacement & 0x02000000) displacement -= 0x04000000;
            entry = static_cast<std::uintptr_t>(static_cast<std::int64_t>(call) + displacement * 4);
            if (functionEntry(entry, 4) != entry) { return false; }
        }
        anchors[static_cast<std::size_t>(spec.id)] = match;
        addresses[static_cast<std::size_t>(spec.id)] = entry;
    }
    for (const auto& spec : build::References) {
        const auto source = anchors[static_cast<std::size_t>(spec.source)];
        const auto call = source + spec.callOffset;
        if (!source || !contains(call, 4, PF_R | PF_X)) { return false; }
        const auto word = read<std::uint32_t>(reinterpret_cast<const void*>(call));
        if ((word & 0xfc000000u) != 0x94000000u) { return false; }
        std::int64_t displacement = word & 0x03ffffffu;
        if (displacement & 0x02000000) displacement -= 0x04000000;
        const auto target = static_cast<std::uintptr_t>(static_cast<std::int64_t>(call) + displacement * 4);
        if (addresses[static_cast<std::size_t>(spec.id)] != target) { return false; }
    }
    const auto table = vtable("24MinecraftUIRenderContext");
    if (!table) { return false; }
    Targets result;
    for (std::size_t index = 0; index < addresses.size(); ++index) {
        const auto address = addresses[index];
        if (!address) { return false; }
        switch (static_cast<build::Symbol>(index)) {
            case build::Symbol::ActionBar: result.actionBar = reinterpret_cast<ActionBarFn>(address); break;
            case build::Symbol::Update: result.update = reinterpret_cast<UpdateFn>(address); break;
            case build::Symbol::Tick: result.tick = reinterpret_cast<TickFn>(address); break;
            case build::Symbol::ApplyTurn: result.applyTurn = reinterpret_cast<TurnFn>(address); break;
            case build::Symbol::LocalTurn: result.localTurn = reinterpret_cast<TurnFn>(address); break;
            case build::Symbol::SetupCamera: result.setupCamera = reinterpret_cast<SetupCameraFn>(address); break;
            case build::Symbol::CameraComponent: result.cameraComponent = reinterpret_cast<CameraComponentFn>(address); break;
            case build::Symbol::RenderPrep: result.renderPrep = reinterpret_cast<RenderPrepFn>(address); break;
            case build::Symbol::Render: result.render = reinterpret_cast<RenderFn>(address); break;
            case build::Symbol::TryGlide: result.tryGlide = reinterpret_cast<BoolFn>(address); break;
            case build::Symbol::IsGliding: result.isGliding = reinterpret_cast<BoolFn>(address); break;
            case build::Symbol::InWater: result.inWater = reinterpret_cast<BoolFn>(address); break;
            case build::Symbol::OnGround: result.onGround = reinterpret_cast<BoolFn>(address); break;
            case build::Symbol::FallFlyTicks: result.fallFlyTicks = reinterpret_cast<FallFlyTicksFn>(address); break;
            case build::Symbol::ModelRotation: result.modelRotation = reinterpret_cast<GlideRotationFn>(address); break;
            case build::Symbol::Gamepad: result.gamepad = reinterpret_cast<GamepadEventFn>(address); break;
            case build::Symbol::UiRender: result.uiRender = reinterpret_cast<UiRenderFn>(address); break;
            case build::Symbol::Cursor: result.cursor = reinterpret_cast<CursorRenderFn>(address); break;
            case build::Symbol::Count: return false;
        }
    }
    result.uiVtable = reinterpret_cast<void**>(table);
    output = result;
    return true;
}
}
