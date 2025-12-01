#pragma once
#include <unordered_map>
#include <array>
#include <cstdint>
#include "Coord.hpp"
#include "Utils.hpp"
#include "Board.hpp"

// Zobrist hashing for sparse infinite board
class ZobristHash
{
public:
    static ZobristHash &instance()
    {
        static ZobristHash z;
        return z;
    }
    std::uint64_t key_for(const Coord &c, Cell who)
    {
        auto it = table.find(c);
        if (it == table.end())
        {
            std::array<std::uint64_t, 3> arr{};
            arr[1] = dist(rng());
            arr[2] = dist(rng());
            it = table.emplace(c, arr).first;
        }
        return (who == Cell::X) ? it->second[1] : it->second[2];
    }

private:
    std::unordered_map<Coord, std::array<std::uint64_t, 3>, CoordHasher> table;
    std::uniform_int_distribution<std::uint64_t> dist;
    ZobristHash() : dist(0, ~0ULL) {}
};
