#pragma once

#include "Model/Game.h"
#include <algorithm>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>

namespace GameTest
{
using namespace MyGame;

inline void Check(bool ok, const std::string& message)
{
    if (!ok) throw std::runtime_error(message);
}

inline Stone Sweep(const Hazard& h)
{
    return h.DangerBounds();
}

inline bool Blocked(const Level& map, int x, int y, bool wholePatrol = true)
{
    if (x < 1 || y < 1 || x + 3 > Game::Width - 1 || y + 3 > Game::Height - 1) return true;
    for (const Stone& wall : map.walls) if (Touches(x, y, 3, 3, wall)) return true;
    for (const Crate& crate : map.crates) if (crate.health && Touches(x, y, 3, 3, crate.body)) return true;
    for (const Hazard& h : map.hazards)
        if (Touches(x, y, 3, 3, wholePatrol ? Sweep(h) : h.body)) return true;
    return false;
}

inline std::vector<int> Flood(const Level& map, Point start, bool wholePatrol = true, bool avoidExit = false)
{
    std::vector<int> previous(Game::Width * Game::Height, -1);
    if (Blocked(map, start.x, start.y, wholePatrol)) return previous;
    std::queue<int> pending;
    const int first = start.y * Game::Width + start.x;
    previous[first] = first;
    pending.push(first);
    const int dx[] = {1, -1, 0, 0}, dy[] = {0, 0, 1, -1};
    while (!pending.empty())
    {
        const int cell = pending.front(); pending.pop();
        for (int i = 0; i < 4; ++i)
        {
            const int x = cell % Game::Width + dx[i], y = cell / Game::Width + dy[i];
            if (Blocked(map, x, y, wholePatrol) || (avoidExit && Touches(x, y, 3, 3, map.exit))) continue;
            const int next = y * Game::Width + x;
            if (previous[next] == -1) { previous[next] = cell; pending.push(next); }
        }
    }
    return previous;
}

inline void WalkTo(Game& game, int x, int y, bool wholePatrol = true, bool allowExit = false)
{
    const auto previous = Flood(game.Map(), {game.Player().GetX(), game.Player().GetY()},
                                wholePatrol, !allowExit && game.ExitOpen());
    const int start = game.Player().GetY() * Game::Width + game.Player().GetX();
    const int target = y * Game::Width + x;
    Check(target >= 0 && target < static_cast<int>(previous.size()) && previous[target] != -1, "Destination must be reachable");
    std::vector<int> path;
    for (int cell = target; cell != start; cell = previous[cell]) path.push_back(cell);
    std::reverse(path.begin(), path.end());
    for (int cell : path)
    {
        game.Move(cell % Game::Width - game.Player().GetX(), cell / Game::Width - game.Player().GetY(), true);
        if (allowExit && game.Status() == Game::State::Cleared) return;
        Check(game.Player().GetX() == cell % Game::Width && game.Player().GetY() == cell / Game::Width,
              "Movement must follow the verified route");
    }
}

inline void Clear(Game& game)
{
    const auto coins = game.Coins();
    for (std::size_t i = 0; i < coins.size(); ++i)
        if (!game.Coins()[i].collected) WalkTo(game, coins[i].x - 1, coins[i].y - 1);
    Check(game.ExitOpen(), "Collecting all stars must unlock exit");
    WalkTo(game, game.Exit().GetX(), game.Exit().GetY(), true, true);
    Check(game.Status() == Game::State::Cleared, "Reaching open exit must clear stage");
}

inline std::string Fingerprint(const Level& level)
{
    std::ostringstream text;
    auto stone = [&](const Stone& s) { text << s.GetX() << ',' << s.GetY() << ',' << s.GetShape().GetWidth() << ',' << s.GetShape().GetHeight() << ';'; };
    text << level.layoutName << ';' << level.spawn.x << ',' << level.spawn.y << ';'; stone(level.exit);
    for (const Room& r : level.rooms) text << r.x << ',' << r.y << ',' << r.width << ',' << r.height << ',' << r.bonus << ';';
    for (const Stone& s : level.walls) stone(s);
    for (const Coin& c : level.coins) text << c.x << ',' << c.y << ';';
    for (const Crate& c : level.crates) { stone(c.body); text << c.health << ',' << c.points << ',' << static_cast<int>(c.drop) << ';'; }
    for (const Hazard& h : level.hazards)
    {
        stone(h.body);
        text << h.from.x << ',' << h.from.y << ',' << h.to.x << ',' << h.to.y << ',' << h.speed << ',' << h.phase << ',' << h.cycleMillis << ',' << static_cast<int>(h.kind) << ';';
    }
    for (const Pickup& p : level.pickups) text << p.x << ',' << p.y << ',' << static_cast<int>(p.power) << ';';
    return text.str();
}

inline void FaceCrate(Game& game, std::size_t index)
{
    const Stone& crate = game.Crates()[index].body;
    const int dx[] = {1, -1, 0, 0}, dy[] = {0, 0, 1, -1};
    const auto reachable = Flood(game.Map(), {game.Player().GetX(), game.Player().GetY()}, true, game.ExitOpen());
    for (int i = 0; i < 4; ++i)
    {
        const int x = crate.GetX() - 4 * dx[i], y = crate.GetY() - 4 * dy[i];
        if (Blocked(game.Map(), x, y) || reachable[y * Game::Width + x] == -1) continue;
        WalkTo(game, x, y);
        game.Move(dx[i], dy[i], true); // Stand next to the crate and aim at it.
        return;
    }
    throw std::runtime_error("Every bonus crate must have a safe firing position");
}

}
