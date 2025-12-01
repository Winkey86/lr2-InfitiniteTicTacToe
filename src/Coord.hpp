#pragma once
#include <cstdint>
#include <functional>

struct Coord {
    int x{};
    int y{};
    bool operator==(const Coord& o) const noexcept { return x==o.x && y==o.y; }
    bool operator!=(const Coord& o) const noexcept { return !(*this==o); }
};

struct CoordHasher {
    std::size_t operator()(const Coord& c) const noexcept {
        // 64-bit mix of two 32-bit ints
        std::uint64_t k = ( (std::uint64_t)(std::uint32_t)c.x << 32 ) ^ (std::uint32_t)c.y;
        // finalize
        k ^= (k >> 33);
        k *= 0xff51afd7ed558ccdULL;
        k ^= (k >> 33);
        k *= 0xc4ceb9fe1a85ec53ULL;
        k ^= (k >> 33);
        return (std::size_t)k;
    }
};
