#pragma once

#include <dabr/Native.hpp>
#include <span>
#include <vector>

namespace dabr {
class Image {
public:
    bool locate();
    bool contains(std::uintptr_t, std::size_t, unsigned flags) const;
    bool resolve(Targets&);
private:
    std::size_t scan(std::span<const std::int16_t>, std::uintptr_t&) const;
    bool matches(std::uintptr_t, std::span<const std::int16_t>) const;
    std::uintptr_t vtable(std::string_view) const;
    std::uintptr_t functionEntry(std::uintptr_t, std::size_t) const;
    struct Segment { std::uintptr_t begin{}, end{}; unsigned flags{}; };
    std::vector<Segment> segments;
    std::uintptr_t unwindHeader{}, unwindTable{};
    std::uint32_t unwindCount{};
};
}
