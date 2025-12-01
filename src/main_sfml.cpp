#ifdef TTT_SFML_FRONTEND
#include "Game.hpp"
#include <SFML/Graphics.hpp>
#include <cmath>
#include <future>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>
#include <optional>

// ======== Camera / coords =========
static sf::Vector2f worldToScreen(float wx, float wy, float cell, sf::Vector2f center, sf::Vector2f cam)
{
    return {(wx - cam.x) * cell + center.x, (wy - cam.y) * cell + center.y};
}
static sf::Vector2f worldCellCenterToScreen(int cx, int cy, float cell, sf::Vector2f center, sf::Vector2f cam)
{
    return worldToScreen(cx + 0.5f, cy + 0.5f, cell, center, cam);
}
static sf::Vector2f screenToWorld(sf::Vector2i mp, float cell, sf::Vector2f center, sf::Vector2f cam)
{
    return {(mp.x - center.x) / cell + cam.x, (mp.y - center.y) / cell + cam.y};
}
static Pos screenToCell(sf::Vector2i mp, float cell, sf::Vector2f center, sf::Vector2f cam)
{
    auto w = screenToWorld(mp, cell, center, cam);
    return {(int)std::floor(w.x), (int)std::floor(w.y)};
}

static bool tryLoadFont(sf::Font &font)
{
    if (const char *env = std::getenv("TTT_FONT_PATH"))
        if (font.loadFromFile(env))
            return true;
    const char *tries[] = {
        "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/tahoma.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf"};
    for (auto p : tries)
        if (font.loadFromFile(p))
            return true;
    return false;
}

// вычисляем концы выигравшего отрезка
static std::optional<std::pair<Pos, Pos>> winningSegment(const IBoard &b, int x, int y, Cell who, int need = CONNECT)
{
    if (who == Cell::Empty)
        return std::nullopt;
    static const int DX[4] = {1, 0, 1, 1};
    static const int DY[4] = {0, 1, 1, -1};
    for (int d = 0; d < 4; ++d)
    {
        int cnt = 1;
        int bx = x, by = y, ex = x, ey = y;
        int nx = x + DX[d], ny = y + DY[d];
        while (b.get(nx, ny) == who)
        {
            cnt++;
            ex = nx;
            ey = ny;
            nx += DX[d];
            ny += DY[d];
        }
        nx = x - DX[d];
        ny = y - DY[d];
        while (b.get(nx, ny) == who)
        {
            cnt++;
            bx = nx;
            by = ny;
            nx -= DX[d];
            ny -= DY[d];
        }
        if (cnt >= need)
            return std::make_pair(Pos{bx, by}, Pos{ex, ey});
    }
    return std::nullopt;
}

// рисуем O как кольцо
static void drawO(sf::RenderTarget &win, sf::Vector2f center, float cell, sf::Color colorOutline)
{
    float rOuter = cell * 0.38f;
    float rInner = cell * 0.24f;
    sf::CircleShape ring(rOuter);
    ring.setOrigin(rOuter, rOuter);
    ring.setPosition(center);
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineThickness(rOuter - rInner);
    ring.setOutlineColor(colorOutline);
    win.draw(ring);
}

// рисуем X двумя диагональными полосами
static void drawX(sf::RenderTarget &win, sf::Vector2f center, float cell, sf::Color color)
{
    float len = cell * 0.62f;
    float thick = cell * 0.16f;
    sf::RectangleShape bar(sf::Vector2f(len, thick));
    bar.setOrigin(len / 2.f, thick / 2.f);
    bar.setFillColor(color);
    bar.setPosition(center);
    bar.setRotation(45.f);
    win.draw(bar);
    bar.setRotation(-45.f);
    win.draw(bar);
}

int main()
{
    sf::RenderWindow win(sf::VideoMode(1280, 800), "TTT4Infinite - 4-in-a-row (SFML)");
    win.setFramerateLimit(60);

    Game g; // поле на хэш-таблице
    float cell = 40.f;
    sf::Vector2f cam(0.f, 0.f);
    sf::Vector2f center(win.getSize().x / 2.f, win.getSize().y / 2.f);

    sf::Font font;
    bool haveFont = tryLoadFont(font);
    sf::Text hud;
    if (haveFont)
    {
        hud.setFont(font);
        hud.setCharacterSize(16);
        hud.setFillColor(sf::Color(160, 170, 180));
    }

    auto aiMove = [&](Cell who) -> Pos
    {
        if (g.algo == Algo::Greedy)
            return ai_greedy(g.board, who);
        if (g.algo == Algo::Negamax)
            return ai_negamax(g.board, who, g.depth);
        return ai_mcts(g.board, who, g.mcts);
    };

    std::atomic<bool> aiThinking{false};
    std::future<Pos> aiFuture;
    auto startAI = [&]
    {
        aiThinking = true;
        aiFuture = std::async(std::launch::async, aiMove, Cell::O);
    };
    std::optional<std::pair<Pos, Pos>> lastWinSeg;
    Cell winner = Cell::Empty;
    auto finishAIIfReady = [&]
    {
        if (aiThinking)
        {
            using namespace std::chrono_literals;
            if (aiFuture.wait_for(0ms) == std::future_status::ready)
            {
                Pos p = aiFuture.get();
                aiThinking = false;
                if (g.placeIfEmpty(p.x, p.y, Cell::O))
                {
                    auto seg = winningSegment(g.board, p.x, p.y, Cell::O);
                    if (seg)
                    {
                        lastWinSeg = seg;
                        winner = Cell::O;
                    }
                }
            }
        }
    };

    // ПКМ-перетаскивание камеры
    bool dragging = false;
    sf::Vector2i dragStart{};
    sf::Vector2f camStart{};
    auto startDrag = [&](sf::Vector2i mp)
    { dragging=true; dragStart=mp; camStart=cam; };
    auto updateDrag = [&](sf::Vector2i mp)
    { if (!dragging) return; sf::Vector2i d = mp - dragStart; cam = camStart - sf::Vector2f(d.x / cell, d.y / cell); };
    auto stopDrag = [&]
    { dragging = false; };

    // Зум колесиком — точка под курсором остаётся на месте
    auto applyWheelZoom = [&](float delta, sf::Vector2i mp)
    {
        float prevCell = cell;
        if (delta > 0)
            cell = std::min(160.f, cell * (1.f + 0.10f * delta));
        else
            cell = std::max(10.f, cell * (1.f + 0.10f * delta));
        auto wBefore = screenToWorld(mp, prevCell, center, cam);
        auto wAfter = screenToWorld(mp, cell, center, cam);
        cam += (wBefore - wAfter);
    };

    bool flashWin = false;
    int flashFrames = 0;
    float spinnerAngle = 0.f;

    while (win.isOpen())
    {
        sf::Event ev;
        while (win.pollEvent(ev))
        {
            if (ev.type == sf::Event::Closed)
                win.close();
            if (ev.type == sf::Event::Resized)
                center = {ev.size.width / 2.f, ev.size.height / 2.f};

            if (ev.type == sf::Event::MouseWheelScrolled)
                applyWheelZoom(ev.mouseWheelScroll.delta, {ev.mouseWheelScroll.x, ev.mouseWheelScroll.y});
            if (ev.type == sf::Event::MouseButtonPressed && ev.mouseButton.button == sf::Mouse::Right)
                startDrag({ev.mouseButton.x, ev.mouseButton.y});
            if (ev.type == sf::Event::MouseMoved)
                updateDrag({ev.mouseMove.x, ev.mouseMove.y});
            if (ev.type == sf::Event::MouseButtonReleased && ev.mouseButton.button == sf::Mouse::Right)
                stopDrag();

            if (ev.type == sf::Event::KeyPressed)
            {
                if (ev.key.code == sf::Keyboard::Escape)
                    win.close();
                if (ev.key.code == sf::Keyboard::Num1)
                    g.algo = Algo::Greedy;
                if (ev.key.code == sf::Keyboard::Num2)
                    g.algo = Algo::Negamax;
                if (ev.key.code == sf::Keyboard::Num3)
                    g.algo = Algo::MCTS;
                if (ev.key.code == sf::Keyboard::LBracket)
                {
                    if (g.algo == Algo::Negamax)
                        g.depth = std::max(1, g.depth - 1);
                    else
                        g.mcts.iters = std::max(200, g.mcts.iters - 200);
                }
                if (ev.key.code == sf::Keyboard::RBracket)
                {
                    if (g.algo == Algo::Negamax)
                        ++g.depth;
                    else
                        g.mcts.iters += 200;
                }
                if (ev.key.code == sf::Keyboard::R)
                {
                    g.reset();
                    aiThinking = false;
                    winner = Cell::Empty;
                    lastWinSeg.reset();
                }
            }

            // ЛКМ — ход игрока
            if (ev.type == sf::Event::MouseButtonPressed && ev.mouseButton.button == sf::Mouse::Left)
            {
                if (!aiThinking && winner == Cell::Empty)
                {
                    Pos p = screenToCell({ev.mouseButton.x, ev.mouseButton.y}, cell, center, cam);
                    if (g.board.get(p.x, p.y) == Cell::Empty)
                    {
                        g.board.set(p.x, p.y, Cell::X);
                        auto seg = winningSegment(g.board, p.x, p.y, Cell::X);
                        if (seg)
                        {
                            lastWinSeg = seg;
                            winner = Cell::X;
                            flashWin = true;
                            flashFrames = 60;
                        }
                        else
                        {
                            startAI();
                        }
                    }
                }
            }
        }

        finishAIIfReady();

        win.clear(sf::Color(25, 25, 28));

        // сетка
        int halfCols = (int)std::ceil(win.getSize().x / (2 * cell)) + 2;
        int halfRows = (int)std::ceil(win.getSize().y / (2 * cell)) + 2;
        int minx = (int)std::floor(cam.x) - halfCols, maxx = (int)std::floor(cam.x) + halfCols;
        int miny = (int)std::floor(cam.y) - halfRows, maxy = (int)std::floor(cam.y) + halfRows;
        sf::VertexArray va(sf::Lines);
        for (int x = minx; x <= maxx; ++x)
        {
            va.append(sf::Vertex(worldToScreen((float)x, (float)miny, cell, center, cam), sf::Color(60, 60, 70)));
            va.append(sf::Vertex(worldToScreen((float)x, (float)maxy, cell, center, cam), sf::Color(60, 60, 70)));
        }
        for (int y = miny; y <= maxy; ++y)
        {
            va.append(sf::Vertex(worldToScreen((float)minx, (float)y, cell, center, cam), sf::Color(60, 60, 70)));
            va.append(sf::Vertex(worldToScreen((float)maxx, (float)y, cell, center, cam), sf::Color(60, 60, 70)));
        }
        win.draw(va);

        // фигуры
        for (auto &kv : g.board.cells)
        {
            int x = kv.first.first, y = kv.first.second;
            Cell c = kv.second;
            if (c == Cell::Empty)
                continue;
            auto cc = worldCellCenterToScreen(x, y, cell, center, cam);
            if (c == Cell::X)
                drawX(win, cc, cell, sf::Color(220, 80, 80));
            else
                drawO(win, cc, cell, sf::Color(80, 140, 220));
        }

        // зачёркивание победной линии + баннер
        if (lastWinSeg)
        {
            auto A = worldCellCenterToScreen(lastWinSeg->first.x, lastWinSeg->first.y, cell, center, cam);
            auto B = worldCellCenterToScreen(lastWinSeg->second.x, lastWinSeg->second.y, cell, center, cam);
            sf::Vector2f dir = B - A;
            float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            float angle = std::atan2(dir.y, dir.x) * 180.f / 3.14159265f;
            sf::RectangleShape strike(sf::Vector2f(len + cell * 0.8f, cell * 0.18f));
            strike.setOrigin(strike.getSize().x / 2.f, strike.getSize().y / 2.f);
            strike.setPosition((A + B) / 2.f);
            strike.setRotation(angle);
            strike.setFillColor(sf::Color(250, 200, 60, 230));
            win.draw(strike);

            if (haveFont)
            {
                std::string msg = (winner == Cell::X ? "X wins!" : "O wins!");
                sf::Text t(msg + "   Press R to restart", font, 24);
                t.setFillColor(sf::Color(240, 240, 240));
                t.setPosition(16.f, (float)win.getSize().y - 40.f);
                win.draw(t);
            }
        }

        // HUD
        if (haveFont)
        {
            std::string mode = (g.algo == Algo::Greedy ? "Mode 1 (Greedy)" : (g.algo == Algo::Negamax ? "Mode 2 (Negamax)" : "Mode 3 (MCTS)"));
            std::string depthStr = (g.algo == Algo::Negamax ? ("Depth " + std::to_string(g.depth)) : ("Iters " + std::to_string(g.mcts.iters)));
            hud.setString(mode + "    " + depthStr + "   [1/2/3 switch, [/] depth/iters, R reset]");
            hud.setPosition(8.f, 6.f);
            win.draw(hud);
        }

        // панель «AI is thinking...»
        if (aiThinking)
        {
            spinnerAngle += 3.f;
            float rr = 24.f;
            sf::Vector2f c(center.x + win.getSize().x * 0.37f, center.y + win.getSize().y * 0.30f);
            sf::CircleShape ring(rr);
            ring.setOrigin(rr, rr);
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineThickness(3.f);
            ring.setOutlineColor(sf::Color(80, 140, 220, 200));
            ring.setPosition(c);
            win.draw(ring);

            sf::RectangleShape tick(sf::Vector2f(rr, 3.f));
            tick.setOrigin(0, 1.5f);
            tick.setFillColor(sf::Color(80, 140, 220, 200));
            tick.setPosition(c);
            tick.setRotation(spinnerAngle);
            win.draw(tick);

            sf::RectangleShape panel(sf::Vector2f(200.f, 50.f));
            panel.setOrigin(panel.getSize().x / 2.f, panel.getSize().y / 2.f);
            panel.setPosition(c.x, c.y + 70.f);
            panel.setFillColor(sf::Color(0, 0, 0, 120));
            panel.setOutlineThickness(1.f);
            panel.setOutlineColor(sf::Color(80, 140, 220, 150));
            win.draw(panel);

            if (haveFont)
            {
                sf::Text t("AI is thinking...", font, 18);
                t.setFillColor(sf::Color(160, 170, 180));
                t.setPosition(c.x - 80.f, c.y + 58.f);
                win.draw(t);
            }
        }

        // вспышка при победе
        if (flashWin)
        {
            sf::RectangleShape border(sf::Vector2f((float)win.getSize().x - 8, (float)win.getSize().y - 8));
            border.setPosition(4, 4);
            border.setFillColor(sf::Color::Transparent);
            border.setOutlineThickness(4);
            border.setOutlineColor(sf::Color(250, 200, 60, 180));
            win.draw(border);
            if (--flashFrames <= 0)
                flashWin = false;
        }

        win.display();
    }
    return 0;
}
#endif
