#include "Views.h"
#include "Geometry/Patrol.h"
#include <sstream>

namespace MyGame
{
    namespace
    {
        void Panel(TerminalScreen& screen,int x,int y,int width,int height,Color color)
        {
            screen.Text(x,y,"+"+std::string(width-2,'-')+"+",color);
            for(int row=1;row<height-1;++row) screen.Text(x,y+row,"|"+std::string(width-2,' ')+"|",color);
            screen.Text(x,y+height-1,"+"+std::string(width-2,'-')+"+",color);
        }
        std::string Clock(long long millis)
        {
            const long long seconds=millis/1000;
            return std::to_string(seconds/60)+":"+(seconds%60<10 ? "0":"")+std::to_string(seconds%60);
        }
        Color Tone(FeedbackTone tone)
        {
            return tone==FeedbackTone::Danger ? Color::Red:tone==FeedbackTone::Reward ? Color::Green:Color::Yellow;
        }
        void SummaryPanel(TerminalScreen& screen,const Game& game,const HighScores& scores,bool autoPlaying)
        {
            const RunStats& stats=game.Stats();
            const bool cleared=game.Status()==Game::State::Cleared;
            const Color color=cleared ? Color::Green:Color::Red;
            Panel(screen,10,5,60,20,color);
            screen.Text(13,7,cleared ? "STAGE CLEAR - RUN PROGRESS":"RUN OVER",color);
            screen.Text(13,9,game.EndReason()+" | "+DifficultyName(game.Mode()));
            screen.Text(13,10,"Score: "+std::to_string(game.Score())+"    Best: "+std::to_string(scores.Best(game.Mode(),game.SkipsTutorial()).score),Color::Yellow);
            screen.Text(13,12,"Expeditions cleared: "+std::to_string(stats.expeditionsCleared));
            screen.Text(13,13,"Stars: "+std::to_string(stats.stars)+"    Crates broken: "+std::to_string(stats.crates));
            screen.Text(13,14,"Hits taken: "+std::to_string(stats.hits)+"    Patrols stunned: "+std::to_string(stats.stuns));
            screen.Text(13,15,"Active time: "+Clock(stats.elapsedMillis)+"    Lives: "+std::to_string(game.Lives()));
            screen.Text(13,17,"Seed: "+std::to_string(game.Seed())+(game.SkipsTutorial() ? " | Direct expedition":" | With tutorial"),Color::Cyan);
            screen.Text(13,19,cleared ? (autoPlaying ? "AI continues shortly | I: take over":"ENTER: next stage, keep your lives and points"):"R: replay this seed   N: new random run");
            screen.Text(13,20,cleared ? "R: replay   N: new run   M: menu   Q: quit":"M: difficulty menu   Q: quit",Color::Dim);
            if(game.Assisted()) screen.Text(13,21,"AI-assisted: personal best is not saved.",Color::Cyan);
            if(!scores.Error().empty()) screen.Text(13,22,scores.Error().substr(0,54),Color::Yellow);
        }
    }
    void RenderMenu(TerminalScreen& screen,Difficulty selected,bool skipTutorial,std::uint32_t seed,const HighScores& scores,const std::string& aiStatus)
    {
        screen.Clear();
        Panel(screen,5,3,70,29,Color::Cyan);
        screen.Text(9,5,"ASCII EXPEDITION",Color::Cyan);
        screen.Text(9,7,"Choose a difficulty. Each mode keeps its own high score.");
        const char* details[]={"5 lives | no timer | slower patrols","3 lives | 2 minutes | balanced patrols","2 lives | 90 seconds | faster, extra patrols"};
        for(int i=0;i<3;++i)
        {
            const Difficulty mode=static_cast<Difficulty>(i);
            const Color color=mode==selected ? Color::Yellow:Color::Default;
            screen.Text(9,10+i*3,std::string(mode==selected ? "> ":"  ")+std::to_string(i+1)+". "+DifficultyName(mode)+"  | Best: "+std::to_string(scores.Best(mode,skipTutorial).score),color);
            screen.Text(14,11+i*3,details[i],Color::Dim);
        }
        screen.Text(9,21,std::string("Tutorial: ")+(skipTutorial ? "OFF (straight to expedition)":"ON (stars, then patrols and firing)"));
        screen.Text(9,23,"Seed: "+std::to_string(seed),Color::Cyan);
        screen.Text(9,25,"Arrows/W/S choose | ENTER starts | 1/2/3 quick start");
        screen.Text(9,27,"T tutorial | N seed | H/? help | Q quit",Color::Dim);
        screen.Text(9,28,aiStatus,Color::Cyan);
        if(!scores.Error().empty()) screen.Text(9,29,scores.Error().substr(0,62),Color::Yellow);
        screen.Present();
    }
    void RenderGame(TerminalScreen& screen,const Game& game,const HighScores& scores,const std::string& aiStatus,bool autoPlaying)
    {
        screen.Clear();
        Panel(screen,0,0,Game::Width,Game::Height,game.HitFlash() ? Color::Red:Color::Blue);
        for(const Stone& wall:game.Walls())
            for(int y=0;y<wall.GetShape().GetHeight();++y)
                screen.Text(wall.GetX(),wall.GetY()+y,std::string(wall.GetShape().GetWidth(),'#'),Color::Dim);
        for(const Crate& crate:game.Crates()) if(crate.health>0)
        {
            const int x=crate.body.GetX(),y=crate.body.GetY();
            screen.Text(x,y,"+-+",Color::Yellow);
            screen.Text(x,y+1,crate.health==2 ? "|C|":"|c|",Color::Yellow);
            screen.Text(x,y+2,"+-+",Color::Yellow);
        }
        for(const Hazard& hazard:game.Hazards())
        {
            if(hazard.Charging() || hazard.Pulsing())
            {
                const Stone area=hazard.DangerBounds();
                for(int y=0;y<area.GetShape().GetHeight();++y)
                    screen.Text(area.GetX(),area.GetY()+y,std::string(area.GetShape().GetWidth(),hazard.Pulsing() ? '!':'.'),hazard.Pulsing() ? Color::Red:Color::Yellow);
            }
            const Color color=hazard.stunnedMillis ? Color::Cyan:hazard.kind==PatrolKind::Scout ? Color::Magenta:hazard.kind==PatrolKind::Sentry ? Color::Yellow:Color::Red;
            screen.Draw(Patrol(hazard.stunnedMillis>0,hazard.kind),hazard.body.GetX(),hazard.body.GetY(),color);
        }
        for(const Coin& coin:game.Coins()) if(!coin.collected) screen.Text(coin.x,coin.y,"*",Color::Yellow);
        for(const Pickup& pickup:game.Pickups()) if(!pickup.collected)
        {
            const char glyph=pickup.power==Power::Heart ? '+':pickup.power==Power::Time ? 'T':pickup.power==Power::Speed ? 'B':'S';
            screen.Text(pickup.x,pickup.y,std::string(1,glyph),Color::Magenta);
        }
        const Stone& gate=game.Exit(); const Color gateColor=game.ExitOpen() ? Color::Green:Color::Dim;
        screen.Text(gate.GetX(),gate.GetY(),game.ExitOpen() ? "GO!":"[-]",gateColor);
        screen.Text(gate.GetX(),gate.GetY()+1,"| |",gateColor);
        screen.Text(gate.GetX(),gate.GetY()+2,"+-+",gateColor);
        for(Point p:game.Beam()) screen.Text(p.x,p.y,".",Color::Yellow);
        screen.Draw(game.Player().GetShape(),game.Player().GetX(),game.Player().GetY(),game.HitFlash() ? Color::Red:game.Invulnerable() ? Color::Cyan:Color::Default);
        screen.Text(0,30,game.Title()+" | "+game.Map().layoutName+" | "+DifficultyName(game.Mode()),Color::Cyan);
        screen.Text(0,31,"Lives: "+std::to_string(game.Lives())+"  Stars: "+std::to_string(game.Collected())+"/"+std::to_string(game.Coins().size())+
                    "  Score: "+std::to_string(game.Score())+"  Time: "+(game.Timed() ? std::to_string(game.SecondsLeft())+"s":"OFF")+
                    "  Best: "+std::to_string(scores.Best(game.Mode(),game.SkipsTutorial()).score));
        screen.Text(0,32,"Arrows/WASD move | F/Space fire | H help | P pause | R/N runs | M menu | Q quit",Color::Dim);
        const auto& messages=game.Messages();
        if(!messages.empty() && game.Status()!=Game::State::Paused)
            screen.Text(0,33,messages.back().text,Tone(messages.back().tone));
        else screen.Text(0,33,game.Hint());
        if(game.Tutorial())
        {
            screen.Text(0,34,game.WeaponUnlocked() ? "Shoot a red triangle guard to stun it for two seconds.":"Next lesson: aim with arrows/WASD; F/Space fires the blaster.",Color::Dim);
        }
        else screen.Text(0,34,"Red triangle: guard | Magenta V: scout | Yellow box: sentry (. warns, ! hits)",Color::Dim);
        std::string status=game.WeaponUnlocked() ? std::string("Blaster ")+game.Aim()+(game.WeaponReady() ? " READY":" recharging"):"Blaster unlocks in tutorial 2";
        if(game.Invulnerable()) status+=" | SHIELD";
        if(game.SpeedBoosted()) status+=" | SPEED x2 ("+std::to_string(game.SpeedSecondsLeft())+"s)";
        status+=" | Seed: "+std::to_string(game.Seed());
        screen.Text(0,35,status,Color::Yellow);
        if(messages.size()>1 && game.Status()!=Game::State::Paused)
            screen.Text(0,36,messages[messages.size()-2].text,Tone(messages[messages.size()-2].tone));
        else screen.Text(0,36,game.SpeedBoosted() ? "Shift+WASD: precise steps | B speed | S shield | + life | T time":"C crates: two shots, bonus loot | B speed | S shield | + life | T time",Color::Dim);
        if(!aiStatus.empty()) screen.Text(0,36,std::string(80,' '),Color::Dim);
        if(!aiStatus.empty()) screen.Text(0,36,aiStatus,Color::Cyan);
        if(game.Status()==Game::State::Cleared || game.Status()==Game::State::Lost) SummaryPanel(screen,game,scores,autoPlaying);
        if(game.Status()==Game::State::Paused)
        {
            Panel(screen,14,9,52,11,Color::Cyan);
            screen.Text(18,11,"PAUSED",Color::Yellow);
            screen.Text(18,13,"P: resume   H/?: controls and game guide");
            screen.Text(18,15,"R: replay this seed   N: new random run");
            screen.Text(18,17,"M: difficulty menu   Q: quit",Color::Dim);
        }
        screen.Present();
    }
    void RenderHelp(TerminalScreen& screen)
    {
        screen.Clear();
        Panel(screen,4,1,72,35,Color::Cyan);
        screen.Text(8,3,"HOW TO PLAY",Color::Yellow);
        screen.Text(8,5,"Collect every * star, then find the open GO! gate.");
        screen.Text(8,6,"Crates and power-ups are optional bonuses.");
        screen.Text(8,8,"MOVE AND FIRE",Color::Cyan);
        screen.Text(8,10,"Arrows / WASD    Move and aim in your last direction");
        screen.Text(8,11,"Shift + WASD    Precise steps while speed-boosted");
        screen.Text(8,12,"F / Space       Fire from tutorial 2 onward");
        screen.Text(8,13,"Crates take two shots; patrols are stunned for 2 seconds.");
        screen.Text(8,15,"BONUSES",Color::Cyan);
        screen.Text(8,17,"B: speed x2 for 6s    S: shield for 5s    +: extra life");
        screen.Text(8,18,"T: +15s on the clock, or +100 points in Relaxed mode");
        screen.Text(8,20,"PATROLS",Color::Cyan);
        screen.Draw(Patrol(false,PatrolKind::Guard),9,22,Color::Red);
        screen.Draw(Patrol(false,PatrolKind::Scout),29,22,Color::Magenta);
        screen.Draw(Patrol(false,PatrolKind::Sentry),49,22,Color::Yellow);
        screen.Text(14,23,"Guard",Color::Red);
        screen.Text(34,23,"Scout",Color::Magenta);
        screen.Text(54,23,"Sentry",Color::Yellow);
        screen.Text(8,26,"Guards patrol; scouts move faster. Contact costs a life.");
        screen.Text(8,27,"Sentries warn with yellow dots, then pulse red ! marks.");
        screen.Text(8,29,"P: pause | ENTER: next stage | R: replay | N: new run");
        screen.Text(8,30,"M: end run and return to the difficulty menu");
        screen.Text(8,31,"I: autopilot | O: CEM/PPO | Move/fire: take over",Color::Cyan);
        screen.Text(8,32,"H / ? / ENTER: close guide    Q: quit",Color::Yellow);
        screen.Text(8,33,"The game stays frozen while this guide is open.",Color::Dim);
        screen.Present();
    }

    std::string RunSummary(const Game& game,const HighScores& scores)
    {
        std::ostringstream out;
        out<<"Run summary - "<<DifficultyName(game.Mode())<<" | Seed "<<game.Seed()<<'\n'
           <<"Score: "<<game.Score()<<" | Best: "<<scores.Best(game.Mode(),game.SkipsTutorial()).score<<'\n'
           <<"Expeditions cleared: "<<game.Stats().expeditionsCleared<<" | Stars: "<<game.Stats().stars<<" | Crates: "<<game.Stats().crates<<'\n'
           <<"Hits: "<<game.Stats().hits<<" | Patrols stunned: "<<game.Stats().stuns<<" | Active time: "<<Clock(game.Stats().elapsedMillis)<<'\n';
        if(game.Assisted()) out<<"AI-assisted run: personal best not saved.\n";
        if(!scores.Error().empty()) out<<scores.Error()<<'\n';
        return out.str();
    }
}
