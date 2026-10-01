#include "AI/Agent.h"
#include "TestSupport.h"
#include <cmath>
#include <iostream>

using namespace MyGame;
using GameTest::Check;
int main() {
    try {
        int crates=0, boosted=0;
        for(unsigned seed=0;seed<64;++seed) {
            AI::Episode bulk(seed), replay(seed);
            Check(bulk.Rules().Stage()==Level::TutorialStages && bulk.Rules().Mode()==Difficulty::Normal,
                  "Training uses the real first Normal expedition");
            while(!bulk.Done()) {
                const auto observation=bulk.Observe();
                Check(observation==replay.Observe(),"Seed and action sequence must reproduce observations");
                for(float value:observation) Check(std::isfinite(value) && value>=0,"Finite observation contract");
                // Deliberately exercise bonuses, shooting and speed before collecting stars.
                int action=0;
                for(int candidate:{6,4,5,3,7}) {
                    const int feature=candidate==6 ? 5:candidate==4 ? 3:candidate==5 ? 4:candidate==3 ? 2:6;
                    if(observation[candidate*AI::Features+feature]>0) { action=candidate; break; }
                }
                const auto before=bulk.Rules().Stats().elapsedMillis;
                const double phase=bulk.Rules().Hazards()[1].phase;
                bulk.Step(action);
                replay.Begin(action);
                int frames=0;
                do { ++frames; } while(replay.Tick());
                Check(frames>0,"A skill advances at least one native input tick");
                Check(bulk.Rules().Stats().elapsedMillis>before || bulk.Rules().Status()==Game::State::Cleared,
                      "Actions advance time; an exit contact ends the game immediately");
                if(bulk.Rules().Stats().elapsedMillis-before<500 && !bulk.Done())
                    Check(bulk.Rules().Hazards()[1].phase!=phase,"Patrols must move during AI control");
                Check(bulk.Render()==replay.Render(),"Animated playback and headless training must match");
                if(bulk.Rules().SpeedBoosted()) ++boosted;
            }
            Check(bulk.Rules().Status()==Game::State::Cleared,"Skills should finish generated expeditions");
            Check(bulk.Rules().Stats().hits==0,"Shared pathfinding avoids entire patrol sweep areas");
            Check(bulk.Rules().Stats().shots>=2*bulk.Rules().Stats().crates,"Crates require actual blaster shots");
            crates+=bulk.Rules().Stats().crates;
            Check(!bulk.Truncated(),"Progressing skills must finish before the decision cap");
            bool rejected=false;
            try { bulk.Step(0); } catch(const std::logic_error&) { rejected=true; }
            Check(rejected,"Finished episodes reject further steps");
        }
        Check(crates>100 && boosted>50,"Exercise crate combat and speed boosts on many maps");
        AI::Episode invalid(42);
        bool rejected=false;
        try { invalid.Begin(-1); } catch(const std::invalid_argument&) { rejected=true; }
        Check(rejected && invalid.Decisions()==0,"Invalid actions do not mutate the game");
        std::cout<<"PASS native AI: 64 timed expeditions, replay parity, combat, boosts and action validation\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
