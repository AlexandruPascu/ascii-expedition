#pragma once
#include "Model/Game.h"
#include <array>
#include <vector>

namespace MyGame { namespace AI {
enum { Actions=9, Features=12, ObservationSize=Actions*Features, TickMillis=100, MaxDecisions=60 };
struct Plan {
    enum Kind { Star, Box, Heart, Time, Speed, Shield, Exit } kind=Star;
    int index=-1;
    Point target{0,0}, aim{0,0};
    std::vector<Point> path;
    std::array<float,Features> features{};
};
// Operates on the live Game. Reset plans after human input or a stage/run change.
class Controller {
public:
    explicit Controller(Game& liveGame) : game(liveGame) {}
    Controller(const Controller&)=delete;
    Controller& operator=(const Controller&)=delete;
    const std::array<float,ObservationSize>& Observe();
    void Begin(int action);
    bool Tick();
    void Reset();
    bool Active() const { return active; }
    int Decisions() const { return decisions; }
private:
    Game& game;
    int decisions=0, skillTicks=0;
    bool ready=false, active=false;
    std::size_t pathIndex=0;
    Plan current;
    std::array<Plan,Actions> plans;
    std::array<float,ObservationSize> observation{};
    void Prepare();
    bool Reached(const Plan& plan) const;
};
class Episode {
public:
    explicit Episode(std::uint32_t seed,bool skipTutorial=true,Difficulty mode=Difficulty::Normal)
        : game(seed,skipTutorial,mode),controller(game) {}
    Episode(const Episode&)=delete;
    Episode& operator=(const Episode&)=delete;
    const Game& Rules() const { return game; }
    const std::array<float,ObservationSize>& Observe() { return controller.Observe(); }
    void Begin(int action);
    bool Tick() { return controller.Tick(); }
    void Step(int action);
    void NextLevel();
    bool Done() const { return game.Status()!=Game::State::Playing || controller.Decisions()>=MaxDecisions; }
    bool Truncated() const { return controller.Decisions()>=MaxDecisions && game.Status()==Game::State::Playing; }
    int Decisions() const { return controller.Decisions(); }
    std::string Render() const;
private:
    Game game;
    Controller controller;
};
} }
