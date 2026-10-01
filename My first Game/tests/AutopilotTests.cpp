#include "AI/Autopilot.h"
#include "System/HighScores.h"
#include "TestSupport.h"
#include <cstdio>
#include <fstream>
#include <iostream>

using namespace MyGame;
using GameTest::Check;
namespace {
void Finish(Game& game,AI::Autopilot& pilot) {
    for(int ticks=0;game.Status()==Game::State::Playing && ticks<4000;++ticks) pilot.Advance(100);
    Check(game.Status()==Game::State::Cleared,"Autopilot must finish: seed "+std::to_string(game.Seed())+
          ", stage "+std::to_string(game.Stage())+", "+DifficultyName(game.Mode())+", "+pilot.Status());
}
void Same(const Game& a,const Game& b) {
    Check(a.Player().GetX()==b.Player().GetX() && a.Player().GetY()==b.Player().GetY() &&
          a.Score()==b.Score() && a.Lives()==b.Lives() && a.Stats().elapsedMillis==b.Stats().elapsedMillis &&
          a.Stats().shots==b.Stats().shots && GameTest::Fingerprint(a.Map())==GameTest::Fingerprint(b.Map()),
          "Frame cadence must not change simulation or movement");
}
}
int main(int argc,char** argv) {
    try {
        int cleared=0;
        for(AI::Kind kind:{AI::Kind::Cem,AI::Kind::Ppo})
        for(Difficulty mode:{Difficulty::Relaxed,Difficulty::Normal,Difficulty::Hard})
        for(unsigned seed=0;seed<8;++seed) {
            Game game(seed,false,mode);
            AI::Autopilot pilot(game,kind,true);
            for(int stage=0;stage<14;++stage) {
                Check(game.Stage()==stage,"AI must progress through tutorials and expeditions in order");
                Finish(game,pilot); ++cleared;
                Check(game.Assisted() && game.Stats().hits==0,"Generated starts have safe AI routes");
                Check(game.ExitOpen(),"AI must collect all stars before finishing");
                const int score=game.Score(),lives=game.Lives();
                game.TogglePause(); pilot.Advance(5000);
                Check(game.Status()==Game::State::Paused && game.Stage()==stage,"Pause must freeze automatic stage continuation");
                game.TogglePause();
                Check(game.Status()==Game::State::Cleared,"Resume a paused clear screen without restarting play");
                pilot.Advance(1199);
                Check(game.Stage()==stage,"Show the clear screen before advancing");
                pilot.Advance(1);
                Check(game.Stage()==stage+1 && game.Score()==score && game.Lives()==lives && game.Assisted(),
                      "Automatic advancement must preserve score, lives and assistance");
            }
        }
        Check(cleared==672,"Cover both policies, all difficulties and capped late-game mechanics");
        for(AI::Kind kind:{AI::Kind::Cem,AI::Kind::Ppo}) {
            Game a(42,true),b(42,true);
            AI::Autopilot first(a,kind,true),second(b,kind,true);
            first.Advance(1000);
            for(int i=0;i<20;++i) second.Advance(50);
            Same(a,b);
            a.TogglePause(); const auto elapsed=a.Stats().elapsedMillis;
            first.Advance(10000);
            Check(a.Stats().elapsedMillis==elapsed,"AI pause must freeze the entire game");
            a.TogglePause(); first.Enable(false);
            const int x=a.Player().GetX(),y=a.Player().GetY();
            first.Advance(200);
            Check(a.Player().GetX()==x && a.Player().GetY()==y,"Disabling AI returns movement to the player");
            Check(a.Assisted(),"Taking over must not erase run assistance");
            const int score=a.Score(); const int stage=a.Stage();
            first.Switch(); first.Enable(true);
            Check(a.Score()==score && a.Stage()==stage,"Switching agents must preserve the live run");
            Finish(a,first);
            a.Reset(); first.Reset();
            Check(!a.Assisted() && a.Score()==0,"Restart clears assistance until another AI input");
            first.Advance(100);
            Check(a.Assisted(),"AI remains available after restart");
            a.NewRun(); first.Reset(); Finish(a,first);
        }
        // Take over a human position inside a patrol sweep, where the old safe-only flood was stranded.
        for(AI::Kind kind:{AI::Kind::Cem,AI::Kind::Ppo}) {
            Game game(42,true,Difficulty::Hard);
            const auto reachable=GameTest::Flood(game.Map(),game.Map().spawn,false);
            bool found=false;
            for(int y=1;y<Game::Height-3 && !found;++y) for(int x=1;x<Game::Width-3 && !found;++x)
                if(reachable[y*Game::Width+x]!=-1 && GameTest::Blocked(game.Map(),x,y,true)) {
                    GameTest::WalkTo(game,x,y,false); found=true;
                }
            Check(found,"Fixture must find a reachable patrol-lane takeover");
            AI::Autopilot pilot(game,kind,true); Finish(game,pilot);
            // Fixture: the player is caught by a guard at the next collision resolution.
            Game hit(42,true,Difficulty::Hard);
            hit.Update(1500);
            const auto& guard=hit.Hazards()[1].body;
            auto& player=const_cast<Personaj&>(hit.Player());
            player.SetX(guard.GetX()); player.SetY(guard.GetY());
            AI::Autopilot recovery(hit,kind,true); Finish(hit,recovery);
            Check(hit.Stats().hits==1,"AI must replan from respawn after damage, not follow stale coordinates");
        }
        Check(argc==2,"Provide an isolated score-file path");
        const std::string file=argv[1]; std::remove(file.c_str());
        HighScores scores(file);
        Game human(42); GameTest::Clear(human);
        Check(scores.Record(human),"Save a human score first");
        const int personal=scores.Best(Difficulty::Normal,false).score;
        Game assisted(42); AI::Autopilot pilot(assisted,AI::Kind::Ppo,true);
        Finish(assisted,pilot); pilot.Advance(1200); Finish(assisted,pilot); pilot.Enable(false);
        Check(assisted.Score()>personal && scores.Record(assisted),"Assisted scores must not fail persistence");
        Check(HighScores(file).Best(Difficulty::Normal,false).score==personal,"AI-assisted runs must preserve human high scores");
        std::remove(file.c_str());
        std::cout<<"PASS 672 AI stages, frame timing, pause, handover, agent switching, restart, lane escape, respawn and score isolation\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
