#include "AI.hpp"
#include "Zobrist.hpp"
#include <algorithm>
#include <limits>

static inline Cell other(Cell c) { return c == Cell::X ? Cell::O : Cell::X; }

Move AI::greedy(Board &b)
{

    auto cand = b.candidates(2);
    for (auto m : cand)
    {
        if (!b.is_empty(m.x, m.y))
            continue;
        b.place(m.x, m.y, Cell::O);
        bool win = b.is_win_from(m.x, m.y, Cell::O, 4);
        b.undo(m.x, m.y);
        if (win)
            return m;
    }
    for (auto m : cand)
    {
        if (!b.is_empty(m.x, m.y))
            continue;
        b.place(m.x, m.y, Cell::X);
        bool oppwin = b.is_win_from(m.x, m.y, Cell::X, 4);
        b.undo(m.x, m.y);
        if (oppwin)
            return m;
    }
    int bestScore = std::numeric_limits<int>::min();
    Move best = cand.empty() ? Move{0, 0} : cand.front();
    for (auto m : cand)
    {
        b.place(m.x, m.y, Cell::O);
        int sc = b.evaluate(4);
        b.undo(m.x, m.y);
        if (sc > bestScore)
        {
            bestScore = sc;
            best = m;
        }
    }
    return best;
}

int AI::negamax(Board &b, int depth, int alpha, int beta, Cell toMove, int need, Timer *deadline, bool *outOfTime, Move *pv)
{
    if (deadline && deadline->elapsed_ms() >= 0 && deadline->elapsed_ms() > 0 && outOfTime)
    {
        if (deadline->elapsed_ms() > 0 && deadline->elapsed_ms() > timeBudgetMs)
        {
            *outOfTime = true;
            return 0;
        }
    }
    auto h = b.hash();
    auto it = tt.find(h);
    if (it != tt.end())
    {
        const TTEntry &e = it->second;
        if (e.depth >= depth)
        {
            if (e.flag == TTEntry::EXACT)
            {
                if (pv)
                    *pv = e.best;
                return e.score;
            }
            else if (e.flag == TTEntry::LOWER)
                alpha = std::max(alpha, e.score);
            else if (e.flag == TTEntry::UPPER)
                beta = std::min(beta, e.score);
            if (alpha >= beta)
            {
                if (pv)
                    *pv = e.best;
                return e.score;
            }
        }
    }

    auto cand = b.candidates(2);
    for (auto m : cand)
    {
        // if previous move created win, detect early in recursion depth check after applying a move below
        (void)m;
    }

    if (depth == 0 || cand.empty())
    {
        int eval = b.evaluate(need);
        return (toMove == Cell::O) ? eval : -eval; // negamax POV: score for player to move
    }

    // move ordering: try winning moves first, then blocks, then heuristic sort
    std::vector<std::pair<int, Move>> scored;
    scored.reserve(cand.size());
    for (auto m : cand)
    {
        int s = 0;
        b.place(m.x, m.y, toMove);
        if (b.is_win_from(m.x, m.y, toMove, need))
            s = 10000000;
        else
        {
            auto cc = b.candidates(2);
            for (auto m2 : cc)
            {
                b.place(m2.x, m2.y, other(toMove));
                if (b.is_win_from(m2.x, m2.y, other(toMove), need))
                {
                    s = 500000;
                    b.undo(m2.x, m2.y);
                    break;
                }
                b.undo(m2.x, m2.y);
            }
            s += b.evaluate(need);
        }
        b.undo(m.x, m.y);
        scored.emplace_back(s, m);
    }
    std::sort(scored.begin(), scored.end(), [](auto &a, auto &b)
              { return a.first > b.first; });

    Move bestMove{};
    int bestScore = std::numeric_limits<int>::min();

    for (auto &sm : scored)
    {
        Move m = sm.second;
        b.place(m.x, m.y, toMove);
        if (b.is_win_from(m.x, m.y, toMove, need))
        {
            b.undo(m.x, m.y);
            return 900000 - (10 * (maxDepth - depth));
        }
        int score = -negamax(b, depth - 1, -beta, -alpha, other(toMove), need, deadline, outOfTime, pv);
        b.undo(m.x, m.y);
        if (outOfTime && *outOfTime)
            return 0;
        if (score > bestScore)
        {
            bestScore = score;
            bestMove = m;
        }
        alpha = std::max(alpha, score);
        if (alpha >= beta)
            break;
    }

    TTEntry entry;
    entry.depth = depth;
    entry.score = bestScore;
    entry.best = bestMove;
    if (bestScore <= alpha)
        entry.flag = TTEntry::UPPER;
    else if (bestScore >= beta)
        entry.flag = TTEntry::LOWER;
    else
        entry.flag = TTEntry::EXACT;
    if (tt.size() > ttMaxSize)
    {
        size_t n = tt.size() / 2;
        for (auto it2 = tt.begin(); it2 != tt.end() && n > 0;)
        {
            it2 = tt.erase(it2);
            --n;
        }
    }
    tt[h] = entry;

    if (pv)
        *pv = bestMove;
    return bestScore;
}

Move AI::alphabeta_root(Board &b, int depth)
{
    auto cand = b.candidates(2);
    if (cand.empty())
        return Move{0, 0};
    int alpha = std::numeric_limits<int>::min() + 100000;
    int beta = std::numeric_limits<int>::max() - 100000;

    Move best = cand.front();
    int bestScore = std::numeric_limits<int>::min();
    // order: try greedy wins first
    std::sort(cand.begin(), cand.end(), [&](const Move &a, const Move &c)
              {
        // heuristic compare by proximity to center of bbox
        int cx=(b.min_x()+b.max_x())/2, cy=(b.min_y()+b.max_y())/2;
        auto da = std::max(std::abs(a.x-cx), std::abs(a.y-cy));
        auto dc = std::max(std::abs(c.x-cx), std::abs(c.y-cy));
        return da<dc; });

    for (auto m : cand)
    {
        if (!b.is_empty(m.x, m.y))
            continue;
        b.place(m.x, m.y, Cell::O);
        int score;
        if (b.is_win_from(m.x, m.y, Cell::O, 4))
        {
            b.undo(m.x, m.y);
            return m;
        }
        score = -negamax(b, depth - 1, -beta, -alpha, Cell::X, 4);
        b.undo(m.x, m.y);
        if (score > bestScore)
        {
            bestScore = score;
            best = m;
        }
        alpha = std::max(alpha, score);
        if (alpha >= beta)
            break;
    }
    return best;
}

Move AI::iterative_deepening(Board &b)
{
    Timer t;
    Move best = greedy(b);
    bool outOfTime = false;
    for (int d = 1; d <= maxDepth; ++d)
    {
        outOfTime = false;
        Move pv{};
        int alpha = std::numeric_limits<int>::min() + 100000;
        int beta = std::numeric_limits<int>::max() - 100000;
        int score = negamax(b, d, alpha, beta, Cell::O, 4, &t, &outOfTime, &pv);
        if (outOfTime)
            break;
        if (pv.x != 0 || pv.y != 0)
            best = pv; // update best line
    }
    return best;
}

Move AI::choose_move(Board &b)
{
    if (b.empty())
        return Move{0, 0};
    switch (mode)
    {
    case GREEDY_1PLY:
        return greedy(b);
    case ALPHABETA:
        return alphabeta_root(b, maxDepth);
    case ID_DEEPEN:
        return iterative_deepening(b);
    default:
        return greedy(b);
    }
}
