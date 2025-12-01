#pragma once
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <utility>
#include <string>
#include <random>

enum class Cell : uint8_t
{
    Empty = 0,
    X = 1,
    O = 2
};
struct Pos
{
    int x, y;
};
inline bool operator==(const Pos &a, const Pos &b) { return a.x == b.x && a.y == b.y; }

static constexpr int CONNECT = 4;
static constexpr int INF_SCORE = 1'000'000'000;

struct Bounds
{
    int minx, miny, maxx, maxy;
};

struct IBoard
{
    virtual Cell get(int x, int y) const = 0;
    virtual void set(int x, int y, Cell c) = 0;
    virtual bool exists(int x, int y) const = 0;
    virtual Bounds bounds() const = 0;
    virtual size_t count() const = 0;
    virtual ~IBoard() = default;
};

struct PairHash
{
    size_t operator()(const std::pair<int, int> &p) const noexcept
    {
        return (static_cast<uint64_t>(static_cast<uint32_t>(p.first)) << 32) ^ static_cast<uint32_t>(p.second);
    }
};

struct MapBoard : IBoard
{
    std::unordered_map<std::pair<int, int>, Cell, PairHash> cells; // PairHash хеш функция
    int minx = std::numeric_limits<int>::max();
    int miny = std::numeric_limits<int>::max();
    int maxx = std::numeric_limits<int>::min();
    int maxy = std::numeric_limits<int>::min();
    size_t nonEmpty = 0;

    Cell get(int x, int y) const override
    {
        auto it = cells.find({x, y});
        if (it == cells.end())
            return Cell::Empty;
        return it->second;
    }
    void set(int x, int y, Cell c) override
    {
        auto key = std::make_pair(x, y);
        auto it = cells.find(key);
        Cell prev = (it == cells.end() ? Cell::Empty : it->second);
        if (c == Cell::Empty)
        {
            if (it != cells.end())
            {
                cells.erase(it);
                if (prev != Cell::Empty && nonEmpty > 0)
                    --nonEmpty;
            }
        }
        else
        {
            if (it == cells.end())
            {
                cells.emplace(key, c);
                ++nonEmpty;
            }
            else
            {
                if (prev == Cell::Empty)
                    ++nonEmpty;
                it->second = c;
            }
            if (x < minx)
                minx = x;
            if (x > maxx)
                maxx = x;
            if (y < miny)
                miny = y;
            if (y > maxy)
                maxy = y;
        }
    }
    bool exists(int x, int y) const override
    {
        auto it = cells.find({x, y});
        return it != cells.end() && it->second != Cell::Empty;
    }
    Bounds bounds() const override
    {
        if (nonEmpty == 0)
            return {1, 1, 0, 0};
        return {minx, miny, maxx, maxy};
    }
    size_t count() const override { return nonEmpty; }
};

inline bool checkWinFrom(const IBoard &b, int x, int y, Cell who)
{
    if (who == Cell::Empty)
        return false;
    static const int dx[4] = {1, 0, 1, 1};
    static const int dy[4] = {0, 1, 1, -1};
    for (int d = 0; d < 4; ++d)
    {
        int cnt = 1;
        for (int s = -1; s <= 1; s += 2)
        {
            int nx = x + s * dx[d], ny = y + s * dy[d];
            while (b.get(nx, ny) == who)
            {
                ++cnt;
                nx += s * dx[d];
                ny += s * dy[d];
            }
        }
        if (cnt >= CONNECT)
            return true;
    }
    return false;
}

inline std::vector<Pos> genCandidates(const IBoard &b, int margin = 2, int neighRadius = 2)
{
    Bounds bb = b.bounds();
    if (bb.minx > bb.maxx)
        return {{0, 0}};
    bb.minx -= margin;
    bb.miny -= margin;
    bb.maxx += margin;
    bb.maxy += margin;
    auto hasNeighbor = [&](int x, int y)
    {
        for (int dx = -neighRadius; dx <= neighRadius; ++dx)
            for (int dy = -neighRadius; dy <= neighRadius; ++dy)
                if (dx || dy)
                {
                    if (b.get(x + dx, y + dy) != Cell::Empty)
                        return true;
                }
        return false;
    };
    std::vector<Pos> out;
    for (int y = bb.miny; y <= bb.maxy; ++y)
        for (int x = bb.minx; x <= bb.maxx; ++x)
            if (b.get(x, y) == Cell::Empty && hasNeighbor(x, y))
                out.push_back({x, y});
    if (out.empty())
        out.push_back({0, 0});
    return out;
}

inline int windowScore(int my, int empty)
{
    if (my == 4)
        return INF_SCORE;
    if (my == 3 && empty == 1)
        return 1200;
    if (my == 2 && empty == 2)
        return 120;
    if (my == 1 && empty == 3)
        return 15;
    return 0;
}
inline int evaluate(const IBoard &b, Cell me)
{
    if (me == Cell::Empty)
        return 0;
    Cell opp = (me == Cell::O) ? Cell::X : Cell::O;
    Bounds bb = b.bounds();
    if (bb.minx > bb.maxx)
        return 0;
    long score = 0;
    static const int DX[4] = {1, 0, 1, 1};
    static const int DY[4] = {0, 1, 1, -1};
    auto evalDir = [&](int sx, int sy, int dx, int dy)
    {
        int my = 0, oppc = 0, emp = 0;
        int x = sx, y = sy;
        for (int k = 0; k < CONNECT; ++k)
        {
            Cell c = b.get(x, y);
            if (c == me)
                ++my;
            else if (c == opp)
                ++oppc;
            else
                ++emp;
            x += dx;
            y += dy;
        }
        if (oppc == 0)
            score += windowScore(my, emp);
        if (my == 0)
            score -= windowScore(oppc, emp);
    };
    for (int y = bb.miny - 1; y <= bb.maxy + 1; ++y)
        for (int x = bb.minx - 1; x <= bb.maxx + 1; ++x)
            for (int d = 0; d < 4; ++d)
            {
                int ex = x + (CONNECT - 1) * DX[d];
                int ey = y + (CONNECT - 1) * DY[d];
                if (ex < bb.minx - 1 || ex > bb.maxx + 1 || ey < bb.miny - 1 || ey > bb.maxy + 1)
                    continue;
                evalDir(x, y, DX[d], DY[d]);
            }
    if (score > INF_SCORE / 2)
        score = INF_SCORE / 2;
    if (score < -INF_SCORE / 2)
        score = -INF_SCORE / 2;
    return static_cast<int>(score);
}

inline std::vector<Pos> listAllEmptyNear(const IBoard &b, int margin = 3)
{
    Bounds bb = b.bounds();
    if (bb.minx > bb.maxx)
        return {{0, 0}};
    bb.minx -= margin;
    bb.miny -= margin;
    bb.maxx += margin;
    bb.maxy += margin;
    std::vector<Pos> out;
    for (int y = bb.miny; y <= bb.maxy; ++y)
        for (int x = bb.minx; x <= bb.maxx; ++x)
            if (b.get(x, y) == Cell::Empty)
                out.push_back({x, y});
    if (out.empty())
        out.push_back({0, 0});
    return out;
}

inline std::vector<Pos> immediateWinningMoves(const IBoard &b, Cell who)
{
    std::unordered_set<std::pair<int, int>, PairHash> uniq;
    Bounds bb = b.bounds();
    if (bb.minx > bb.maxx)
        return {};
    static const int DX[4] = {1, 0, 1, 1};
    static const int DY[4] = {0, 1, 1, -1};

    for (int d = 0; d < 4; ++d)
    {
        int dx = DX[d], dy = DY[d];
        for (int y = bb.miny - 1; y <= bb.maxy + 1; ++y)
        {
            for (int x = bb.minx - 1; x <= bb.maxx + 1; ++x)
            {
                int ex = x + (CONNECT - 1) * dx;
                int ey = y + (CONNECT - 1) * dy;
                if (ex < bb.miny - 1000000)
                {
                }
                if (ex < bb.minx - 2 || ex > bb.maxx + 2 || ey < bb.miny - 2 || ey > bb.maxy + 2)
                    continue;

                int countMe = 0, countOpp = 0, countEmp = 0;
                int emptyX = 0, emptyY = 0;
                for (int k = 0; k < CONNECT; ++k)
                {
                    int cx = x + k * dx, cy = y + k * dy;
                    Cell c = b.get(cx, cy);
                    if (c == who)
                        ++countMe;
                    else if (c == Cell::Empty)
                    {
                        ++countEmp;
                        emptyX = cx;
                        emptyY = cy;
                    }
                    else
                        ++countOpp;
                }
                if (countMe == CONNECT - 1 && countEmp == 1 && countOpp == 0)
                    uniq.insert({emptyX, emptyY});
            }
        }
    }
    std::vector<Pos> res;
    res.reserve(uniq.size());
    for (auto &p : uniq)
        res.push_back({p.first, p.second});
    return res;
}

struct TempPlace
{
    IBoard *b;
    Pos p;
    Cell prev;
    TempPlace(IBoard *b_, Pos p_, Cell c) : b(b_), p(p_)
    {
        prev = b->get(p.x, p.y);
        b->set(p.x, p.y, c);
    }
    ~TempPlace() { b->set(p.x, p.y, prev); }
};

inline std::vector<Pos> forkPoints(IBoard &b, Cell who)
{
    // «Вилка»: ход who в клетку p, после которого у who появляется >=2 различных немедленных выигрыша.
    auto empties = listAllEmptyNear(b, 3);
    std::vector<Pos> forks;
    for (auto p : empties)
    {
        if (b.get(p.x, p.y) != Cell::Empty)
            continue;
        TempPlace t(&b, p, who);
        if (!checkWinFrom(b, p.x, p.y, who))
        {
            auto wins = immediateWinningMoves(b, who);
            if ((int)wins.size() >= 2)
                forks.push_back(p);
        }
    }
    return forks;
}

// ===== Greedy =====
struct TempPlace1
{
    IBoard *b;
    Pos p;
    Cell prev;
    TempPlace1(IBoard *b_, Pos p_, Cell c) : b(b_), p(p_)
    {
        prev = b->get(p.x, p.y);
        b->set(p.x, p.y, c);
    }
    ~TempPlace1() { b->set(p.x, p.y, prev); }
};
inline Pos ai_greedy(IBoard &b, Cell me)
{
    Cell opp = (me == Cell::O) ? Cell::X : Cell::O;

    // 1) Немедленная победа
    if (auto wins = immediateWinningMoves(b, me); !wins.empty())
        return wins.front();

    // 2) Немедленная блокировка выигрыша соперника
    if (auto oppWins = immediateWinningMoves(b, opp); !oppWins.empty())
        return oppWins.front();

    // 3) Блок «вилки» соперника — срезаем ходы X, которые порождают >=2 проигрышей на следующий ход
    if (auto forks = forkPoints(b, opp); !forks.empty())
        return forks.front();

    auto cand = genCandidates(b);
    int best = -INF_SCORE, idx = 0;
    for (int i = 0; i < (int)cand.size(); ++i)
    {
        TempPlace1 t(&b, cand[i], me);
        int sc = evaluate(b, me);
        if (sc > best)
        {
            best = sc;
            idx = i;
        }
    }
    return cand[idx];
}

// ===== Negamax + alpha-beta =====
struct TempPlace2
{
    IBoard *b;
    Pos p;
    Cell prev;
    TempPlace2(IBoard *b_, Pos p_, Cell c) : b(b_), p(p_)
    {
        prev = b->get(p.x, p.y);
        b->set(p.x, p.y, c);
    }
    ~TempPlace2() { b->set(p.x, p.y, prev); }
};
inline int negamax(IBoard &b, int depth, int alpha, int beta, Cell toMove, Pos lastMove, Cell me)
{
    if (lastMove.x != std::numeric_limits<int>::min())
    {
        Cell opp = (toMove == Cell::O) ? Cell::X : Cell::O;
        if (checkWinFrom(b, lastMove.x, lastMove.y, opp))
            return (toMove == me ? -INF_SCORE : INF_SCORE);
    }
    if (depth == 0)
        return evaluate(b, me);
    auto cand = genCandidates(b);
    if (cand.empty())
        return 0;

    std::vector<std::pair<int, Pos>> ordered;
    ordered.reserve(cand.size());
    for (auto p : cand)
    {
        TempPlace2 t(&b, p, toMove);
        ordered.push_back({evaluate(b, me), p});
    }
    std::sort(ordered.begin(), ordered.end(), [](auto &a, auto &b)
              { return a.first > b.first; });
    int best = -INF_SCORE;
    Cell next = (toMove == Cell::O) ? Cell::X : Cell::O;
    for (auto &sp : ordered)
    {
        Pos p = sp.second;
        TempPlace2 t(&b, p, toMove);
        int val = -negamax(b, depth - 1, -beta, -alpha, next, p, me);
        if (val > best)
            best = val;
        if (best > alpha)
            alpha = best;
        if (alpha >= beta)
            break;
    }
    return best;
}
inline Pos ai_negamax(IBoard &b, Cell me, int depth)
{
    Cell opp = (me == Cell::O) ? Cell::X : Cell::O;

    if (auto wins = immediateWinningMoves(b, me); !wins.empty())
        return wins.front();
    if (auto oppWins = immediateWinningMoves(b, opp); !oppWins.empty())
        return oppWins.front();
    if (auto forks = forkPoints(b, opp); !forks.empty())
        return forks.front();

    auto cand = genCandidates(b);
    if (cand.empty())
        return {0, 0};
    int best = -INF_SCORE;
    Pos bestP = cand.front();
    int alpha = -INF_SCORE, beta = INF_SCORE;
    for (auto p : cand)
    {
        TempPlace2 t(&b, p, me);
        int val = -negamax(b, depth - 1, -beta, -alpha, opp, p, me);
        if (val > best)
        {
            best = val;
            bestP = p;
        }
        if (val > alpha)
            alpha = val;
    }
    return bestP;
}

// ===== Simple MCTS-like playouts =====
struct MCTSParams
{
    int iters = 1200;
    int playoutDepth = 12;
};
struct TempPlace3
{
    IBoard *b;
    Pos p;
    Cell prev;
    TempPlace3(IBoard *b_, Pos p_, Cell c) : b(b_), p(p_)
    {
        prev = b->get(p.x, p.y);
        b->set(p.x, p.y, c);
    }
    ~TempPlace3() { b->set(p.x, p.y, prev); }
};
inline int playout(IBoard &b, Pos start, Cell me, int depthLimit, std::mt19937 &rng)
{
    Cell turn = me;
    {
        TempPlace3 t(&b, start, me);
        if (checkWinFrom(b, start.x, start.y, me))
            return +1;
        turn = (me == Cell::O) ? Cell::X : Cell::O;
    }
    for (int d = 0; d < depthLimit; ++d)
    {
        auto cand = genCandidates(b);
        if (cand.empty())
            break;
        std::uniform_int_distribution<int> dist(0, (int)cand.size() - 1);
        Pos p = cand[dist(rng)];
        TempPlace3 t(&b, p, turn);
        if (checkWinFrom(b, p.x, p.y, turn))
            return (turn == me) ? +1 : -1;
        turn = (turn == Cell::O) ? Cell::X : Cell::O;
    }
    return 0;
}
inline Pos ai_mcts(IBoard &b, Cell me, const MCTSParams &P = {})
{
    Cell opp = (me == Cell::O) ? Cell::X : Cell::O;

    if (auto wins = immediateWinningMoves(b, me); !wins.empty())
        return wins.front();
    if (auto oppWins = immediateWinningMoves(b, opp); !oppWins.empty())
        return oppWins.front();
    if (auto forks = forkPoints(b, opp); !forks.empty())
        return forks.front();

    auto cand = genCandidates(b);
    if (cand.size() == 1)
        return cand.front();
    if (cand.empty())
        return {0, 0};
    std::mt19937 rng(1337u);
    int bestScore = -1e9;
    Pos best = cand.front();
    for (auto p : cand)
    {
        int score = 0, quota = std::max(1, P.iters / (int)cand.size());
        for (int i = 0; i < quota; ++i)
            score += playout(b, p, me, P.playoutDepth, rng);
        if (score > bestScore)
        {
            bestScore = score;
            best = p;
        }
    }
    return best;
}

enum class Algo
{
    Greedy = 1,
    Negamax = 2,
    MCTS = 3
};

struct Game
{
    MapBoard board;
    Cell turn = Cell::X;
    Algo algo = Algo::Negamax;
    int depth = 3;
    MCTSParams mcts{1200, 12};

    void reset()
    {
        board = MapBoard{};
        turn = Cell::X;
    }
    bool placeIfEmpty(int x, int y, Cell who)
    {
        if (board.get(x, y) != Cell::Empty)
            return false;
        board.set(x, y, who);
        return true;
    }
    inline bool justWonAt(int x, int y) const
    {
        Cell who = board.get(x, y);
        return who != Cell::Empty && checkWinFrom(board, x, y, who);
    }
    bool isHuman(Cell c) const { return c == Cell::X; }
    std::string cellChar(Cell c) const { return c == Cell::X ? "X" : (c == Cell::O ? "O" : "."); }
};
