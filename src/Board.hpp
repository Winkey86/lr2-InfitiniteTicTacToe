#pragma once
#include <unordered_map>
#include <vector>
#include <limits>
#include "Coord.hpp"
#include "Utils.hpp"

enum class Cell : uint8_t
{
    Empty = 0,
    X = 1,
    O = 2
};

struct Move
{
    int x{};
    int y{};
    Move() = default;
    Move(int X, int Y) : x(X), y(Y) {}
    bool operator==(const Move &o) const noexcept { return x == o.x && y == o.y; }
};

class ZobristHash;

class Board
{
public:
    Board();

    bool place(int x, int y, Cell who); // returns false if occupied
    void undo(int x, int y);
    bool is_empty(int x, int y) const;

    Cell at(int x, int y) const;
    bool is_win_from(int x, int y, Cell who, int need = 4) const;

    // generate candidate moves near existing pieces
    std::vector<Move> candidates(int radius = 2) const;

    // box of occupied cells
    int min_x() const { return minX; }
    int max_x() const { return maxX; }
    int min_y() const { return minY; }
    int max_y() const { return maxY; }
    bool empty() const { return cells.empty(); }

    // evaluation helper (heuristic static evaluation for 'O' - 'X')
    int evaluate(int need = 4) const;

    // zobrist key for TT
    std::uint64_t hash() const { return zkey; }
    void toggle_hash(int x, int y, Cell who); // used by place/undo

private:
    std::unordered_map<Coord, Cell, CoordHasher> cells;
    int minX{0}, maxX{0}, minY{0}, maxY{0};
    std::uint64_t zkey{0};
    friend class ZobristHash;

    int line_score_from(int x, int y, int dx, int dy, Cell who, int need) const;
};
