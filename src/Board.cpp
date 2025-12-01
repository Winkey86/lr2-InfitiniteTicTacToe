#include "Board.hpp"
#include "Zobrist.hpp"
#include <algorithm>
#include <array>

static bool between(int a, int b, int c) { return a <= b && b <= c; }

Board::Board() {}

bool Board::is_empty(int x, int y) const
{
    auto it = cells.find(Coord{x, y});
    return it == cells.end();
}

Cell Board::at(int x, int y) const
{
    auto it = cells.find(Coord{x, y});
    return (it == cells.end()) ? Cell::Empty : it->second;
}

void Board::toggle_hash(int x, int y, Cell who)
{
    zkey ^= ZobristHash::instance().key_for(Coord{x, y}, who);
}

bool Board::place(int x, int y, Cell who)
{
    auto key = Coord{x, y};
    if (cells.find(key) != cells.end())
        return false;
    cells.emplace(key, who);
    if (cells.size() == 1)
    {
        minX = maxX = x;
        minY = maxY = y;
    }
    else
    {
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }
    toggle_hash(x, y, who);
    return true;
}

void Board::undo(int x, int y)
{
    auto it = cells.find(Coord{x, y});
    if (it == cells.end())
        return;
    Cell who = it->second;
    toggle_hash(x, y, who);
    cells.erase(it);
    if (cells.empty())
    {
        minX = maxX = minY = maxY = 0;
        return;
    }
    if (x == minX || x == maxX || y == minY || y == maxY)
    {
        // recompute bbox lazily
        minX = std::numeric_limits<int>::max();
        minY = std::numeric_limits<int>::max();
        maxX = std::numeric_limits<int>::min();
        maxY = std::numeric_limits<int>::min();
        for (auto &kv : cells)
        {
            minX = std::min(minX, kv.first.x);
            maxX = std::max(maxX, kv.first.x);
            minY = std::min(minY, kv.first.y);
            maxY = std::max(maxY, kv.first.y);
        }
    }
}

bool Board::is_win_from(int x, int y, Cell who, int need) const
{
    if (at(x, y) != who)
        return false;
    static const int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    for (auto &d : dirs)
    {
        int dx = d[0], dy = d[1];
        int cnt = 1;
        for (int s = 1; s < need; ++s)
        {
            if (at(x + dx * s, y + dy * s) == who)
                ++cnt;
            else
                break;
        }
        for (int s = 1; s < need; ++s)
        {
            if (at(x - dx * s, y - dy * s) == who)
                ++cnt;
            else
                break;
        }
        if (cnt >= need)
            return true;
    }
    return false;
}

std::vector<Move> Board::candidates(int radius) const
{
    std::vector<Move> out;
    out.reserve(cells.size() * 8);
    if (cells.empty())
    {
        out.emplace_back(0, 0);
        return out;
    }
    std::unordered_map<Coord, bool, CoordHasher> seen;
    for (auto &kv : cells)
    {
        int x = kv.first.x, y = kv.first.y;
        for (int dx = -radius; dx <= radius; ++dx)
        {
            for (int dy = -radius; dy <= radius; ++dy)
            {
                int nx = x + dx, ny = y + dy;
                Coord c{nx, ny};
                if (cells.find(c) == cells.end() && !seen.count(c))
                {
                    seen[c] = true;
                    out.emplace_back(nx, ny);
                }
            }
        }
    }
    return out;
}

int Board::line_score_from(int x, int y, int dx, int dy, Cell who, int need) const
{
    if (at(x, y) != who)
        return 0;
    int cnt = 1;
    bool open1 = false, open2 = false;
    int nx = x + dx, ny = y + dy;
    while (at(nx, ny) == who)
    {
        cnt++;
        nx += dx;
        ny += dy;
    }
    if (at(nx, ny) == Cell::Empty)
        open1 = true;
    nx = x - dx;
    ny = y - dy;
    while (at(nx, ny) == who)
    {
        cnt++;
        nx -= dx;
        ny -= dy;
    }
    if (at(nx, ny) == Cell::Empty)
        open2 = true;
    int open = (open1 ? 1 : 0) + (open2 ? 1 : 0);
    if (cnt >= need)
        return 1000000;
    if (cnt == need - 1)
    {
        if (open == 2)
            return 10000;
        if (open == 1)
            return 3000;
    }
    if (cnt == need - 2)
    {
        if (open == 2)
            return 500;
        if (open == 1)
            return 120;
    }
    if (cnt == 1 && open == 2)
        return 10;
    return 0;
}

int Board::evaluate(int need) const
{
    int scoreO = 0, scoreX = 0;
    static const int dirs[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    for (auto &kv : cells)
    {
        int x = kv.first.x, y = kv.first.y;
        Cell c = kv.second;
        for (auto &d : dirs)
        {
            int s = line_score_from(x, y, d[0], d[1], c, need);
            if (c == Cell::O)
                scoreO += s;
            else
                scoreX += s;
        }
    }
    return scoreO - scoreX;
}
