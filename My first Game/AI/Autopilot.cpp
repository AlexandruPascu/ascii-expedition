#include "Autopilot.h"
#include <algorithm>
#include <exception>

namespace MyGame { namespace AI {
void Autopilot::Reset() { controller.Reset(); pendingMillis=clearMillis=0; error.clear(); }
void Autopilot::Enable(bool value) {
    if(enabled && !value && game.Status()==Game::State::Playing) game.Update(pendingMillis);
    enabled=value; Reset();
}
void Autopilot::Switch() { kind=kind==Kind::Cem ? Kind::Ppo:Kind::Cem; controller.Reset(); error.clear(); }
void Autopilot::Advance(int millis) {
    if(millis<=0 || game.Status()==Game::State::Paused) return;
    if(!enabled) { game.Update(millis); return; }
    if(game.Status()==Game::State::Cleared) {
        clearMillis+=std::min(millis,2000);
        if(clearMillis>=1200) { game.NextLevel(); Reset(); }
        return;
    }
    if(game.Status()!=Game::State::Playing) return;
    // Bound arithmetic without discarding elapsed time while a game is running.
    while(millis>0 && game.Status()==Game::State::Playing) {
        const int step=std::min(millis,TickMillis-pendingMillis);
        millis-=step; pendingMillis+=step;
        if(pendingMillis<TickMillis) continue;
        pendingMillis=0;
        try {
            if(!controller.Active()) controller.Begin(Select(kind,controller.Observe()));
            controller.Tick();
        } catch(const std::exception& failure) {
            // A failed planner hands control back without terminating the user's run.
            error=failure.what(); enabled=false; controller.Reset();
            game.Update(TickMillis); game.Update(millis); break;
        }
    }
}
std::string Autopilot::Status() const {
    if(!error.empty()) return "AI stopped: "+error.substr(0,54)+" | I retry";
    const std::string selected=Name(kind),other=Name(kind==Kind::Cem ? Kind::Ppo:Kind::Cem);
    if(enabled) return "AI: "+selected+" | I or move/fire: take over | O: "+other+" | assisted run";
    return "Control: MANUAL | I: "+selected+" autopilot | O: select "+other+(game.Assisted() ? " | assisted run":"");
}
} }
