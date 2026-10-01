#pragma once
#include "Policy.h"

namespace MyGame { namespace AI {
// Owns input timing, never the Game; the ordinary UI keeps rendering the same state.
class Autopilot {
public:
    Autopilot(Game& liveGame,Kind selected=Kind::Cem,bool enabled=false)
        : game(liveGame),controller(liveGame),kind(selected),enabled(enabled) {}
    Autopilot(const Autopilot&)=delete;
    Autopilot& operator=(const Autopilot&)=delete;
    bool Enabled() const { return enabled; }
    Kind Selected() const { return kind; }
    void Enable(bool value);
    void Switch();
    void Reset();
    void Advance(int millis);
    std::string Status() const;
private:
    Game& game;
    Controller controller;
    Kind kind;
    bool enabled;
    int pendingMillis=0,clearMillis=0;
    std::string error;
};
} }
