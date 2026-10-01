#pragma once
#include "Model/Game.h"
#include <array>
#include <vector>

namespace MyGame { namespace AI {
// Versioned observation/action contract shared by CEM, PPO and replay.
enum { Actions=9, Features=12, ObservationSize=Actions*Features, TickMillis=100, MaxDecisions=60 };
struct Plan {
    enum Kind { Star, Box, Heart, Time, Speed, Shield, Exit } kind=Star;
    int index=-1;
    Point target{0,0}, aim{0,0};
    std::vector<Point> path;
    std::array<float,Features> features{};
};
class Episode {
public:
    explicit Episode(std::uint32_t seed) : game(seed,true) {}
    const Game& Rules() const { return game; }
    const std::array<float,ObservationSize>& Observe();
    void Begin(int action);
    bool Tick(); // One real input and 100 ms of game time; true while this skill continues.
    void Step(int action);
    bool Done() const { return game.Status()!=Game::State::Playing || decisions>=MaxDecisions; }
    bool Truncated() const { return decisions>=MaxDecisions && game.Status()==Game::State::Playing; }
    int Decisions() const { return decisions; }
    std::string Render() const;
private:
    Game game;
    int decisions=0, skillTicks=0;
    bool ready=false, active=false;
    std::size_t pathIndex=0;
    Plan current;
    std::array<Plan,Actions> plans;
    std::array<float,ObservationSize> observation{};
    void Prepare();
    bool Reached(const Plan& plan) const;
};
} }
