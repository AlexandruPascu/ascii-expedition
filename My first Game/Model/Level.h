#pragma once

#include "Stone.h"
#include "Difficulty.h"
#include <string>
#include <cstdint>
#include <vector>

namespace MyGame
{
    struct Point { int x, y; };
    struct Coin { int x, y; bool collected; };
    enum class Power { None, Shield, Heart, Time, Speed };
    struct Pickup { int x, y; Power power; bool collected; };
    struct Crate { Stone body; int health, points; Power drop; };

    enum class PatrolKind { Guard, Scout, Sentry };
    struct Room { int x, y, width, height; bool bonus; };

    struct Hazard
    {
        Stone body;
        Point from, to;
        double speed, phase;
        int stunnedMillis, cycleMillis;
        PatrolKind kind;
        Hazard(Point start, Point end, double velocity, double initialPhase = 0, PatrolKind type = PatrolKind::Guard);
        Stone DangerBounds() const;
        bool Charging() const { return kind == PatrolKind::Sentry && !stunnedMillis && cycleMillis >= 1600 && cycleMillis < 2400; }
        bool Pulsing() const { return kind == PatrolKind::Sentry && !stunnedMillis && cycleMillis >= 2400; }
        bool Hurts(int x, int y, int width, int height) const;
        void Update(int millis);
    };

    struct Level
    {
        enum { Width = 80, Height = 30, TutorialStages = 2 };
        std::string layoutName;
        std::vector<Room> rooms;
        Point spawn;
        Stone exit;
        std::vector<Stone> walls;
        std::vector<Hazard> hazards;
        std::vector<Coin> coins;
        std::vector<Crate> crates;
        std::vector<Pickup> pickups;
    };

    Level GenerateLevel(std::uint32_t seed, int stage, Difficulty difficulty = Difficulty::Normal);
    bool Overlaps(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2);
    bool Touches(int x, int y, int width, int height, const Stone& stone);
}
