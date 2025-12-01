#pragma once
#include "Board.hpp"
#include <unordered_map>
#include <optional>

struct TTEntry
{
    int depth{};
    int score{};
    enum Flag : uint8_t
    {
        EXACT,
        LOWER,
        UPPER
    } flag{EXACT};
    Move best{};
};

class AI
{
public:
    enum Mode
    {
        GREEDY_1PLY = 1,
        ALPHABETA = 2,
        ID_DEEPEN = 3
    };

    void set_mode(Mode m) { mode = m; }
    void set_depth(int d) { maxDepth = d < 1 ? 1 : d; }
    int get_depth() const { return maxDepth; }
    Mode get_mode() const { return mode; }

    void set_time_budget(int ms) { timeBudgetMs = ms; }

    Move choose_move(Board &b);

private:
    Mode mode{ALPHABETA};
    int maxDepth{4};
    int timeBudgetMs{800};

    // Transposition table
    std::unordered_map<std::uint64_t, TTEntry> tt;
    size_t ttMaxSize = 300000;

    Move greedy(Board &b);
    int negamax(Board &b, int depth, int alpha, int beta, Cell toMove, int need, Timer *deadline = nullptr, bool *outOfTime = nullptr, Move *pv = nullptr);
    Move alphabeta_root(Board &b, int depth);
    Move iterative_deepening(Board &b);
};
