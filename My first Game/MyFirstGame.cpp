#include "System/Views.h"
#include "System/KbInput.h"
#include <algorithm>
#include <chrono>
#include <thread>
#include <csignal>
#include <cctype>
#include <iostream>
#include <limits>
#include <memory>
#include <random>

namespace
{
    volatile std::sig_atomic_t stopped=0;
    void Stop(int) { stopped=1; }
}
int main(int argc,char** argv)
{
    std::signal(SIGINT,Stop); std::signal(SIGTERM,Stop);
    try
    {
        bool skipTutorial=false,difficultyProvided=false;
        MyGame::Difficulty selected=MyGame::Difficulty::Normal;
        std::uint32_t seed=std::random_device{}();
        std::string scoresFile=MyGame::HighScores::DefaultPath();
        for(int i=1;i<argc;++i)
        {
            const std::string arg=argv[i];
            if(arg=="--help")
            {
                std::cout<<"Usage: MyFirstGame [--seed N] [--skip-tutorial]\n"
                         <<"  [--difficulty relaxed|normal|hard] [--scores-file PATH]\n"
                         <<"Without --difficulty, a difficulty menu opens first.\n"
                         <<"Use an ANSI terminal at least 80 columns by 37 rows.\n"
                         <<"Arrows/WASD move; Shift+WASD precise steps; F/Space fire;\n"
                         <<"H/? guide; P pause; Enter next stage; R replay; N new run; M menu; Q quit.\n";
                return 0;
            }
            if(arg=="--skip-tutorial") skipTutorial=true;
            else if(arg=="--seed" && i+1<argc)
            {
                const std::string value=argv[++i];
                if(value.empty() || value.size()>10 || !std::all_of(value.begin(),value.end(),[](char c){ return c>='0' && c<='9'; }))
                    throw std::runtime_error("Seed must be an integer from 0 to 4294967295.");
                const auto parsed=std::stoull(value);
                if(parsed>std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("Seed must be an integer from 0 to 4294967295.");
                seed=static_cast<std::uint32_t>(parsed);
            }
            else if(arg=="--difficulty" && i+1<argc)
            {
                const std::string value=argv[++i];
                if(value=="relaxed") selected=MyGame::Difficulty::Relaxed;
                else if(value=="normal") selected=MyGame::Difficulty::Normal;
                else if(value=="hard") selected=MyGame::Difficulty::Hard;
                else throw std::runtime_error("Difficulty must be relaxed, normal, or hard.");
                difficultyProvided=true;
            }
            else if(arg=="--scores-file" && i+1<argc)
            {
                scoresFile=argv[++i];
                if(scoresFile.empty()) throw std::runtime_error("Score file path cannot be empty.");
            }
            else throw std::runtime_error("Unknown or incomplete option: "+arg+". Use --help.");
        }
        MyGame::HighScores scores(scoresFile);
        std::unique_ptr<MyGame::Game> game;
        bool menu=!difficultyProvided, stageRecorded=false;
        bool helpOpen=false, resumeAfterHelp=false;
        std::string summary;
        auto begin=[&] { game.reset(new MyGame::Game(seed,skipTutorial,selected)); menu=false; stageRecorded=false; };
        auto record=[&] { if(game) { scores.Record(*game); summary=MyGame::RunSummary(*game,scores); } };
        if(!menu) begin();
        {
            MyGame::KeyboardInput input;
            MyGame::TerminalScreen screen;
            using Clock=std::chrono::steady_clock;
            auto previous=Clock::now();
            MyGame::TerminalSize lastNotice{0,0};
            while(!stopped)
            {
                const auto now=Clock::now();
                const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(now-previous);
                previous+=elapsed;
                const MyGame::TerminalSize size=screen.Size();
                if(!size.FitsGame())
                {
                    if(game && game->Status()==MyGame::Game::State::Playing) game->TogglePause();
                    resumeAfterHelp=false; // Resizing requires an explicit resume after the window is usable.
                }
                else if(!menu && !helpOpen) game->Update(static_cast<int>(elapsed.count()));
                char key;
                for(int count=0;count<64 && input.Read(key);++count)
                {
                    const bool precise=std::isupper(static_cast<unsigned char>(key))!=0;
                    const char command=static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
                    if(command=='q') { stopped=1; break; }
                    if(!size.FitsGame()) continue;
                    if(helpOpen)
                    {
                        if(command=='h' || command=='?' || command=='\r' || command=='\n')
                        {
                            helpOpen=false;
                            if(resumeAfterHelp && game && game->Status()==MyGame::Game::State::Paused) game->TogglePause();
                            resumeAfterHelp=false;
                        }
                        continue;
                    }
                    if(command=='h' || command=='?')
                    {
                        helpOpen=true;
                        resumeAfterHelp=game && game->Status()==MyGame::Game::State::Playing;
                        if(resumeAfterHelp) game->TogglePause();
                        continue;
                    }
                    if(menu)
                    {
                        if(command=='q') stopped=1;
                        else if(command=='w') selected=static_cast<MyGame::Difficulty>((static_cast<int>(selected)+2)%3);
                        else if(command=='s') selected=static_cast<MyGame::Difficulty>((static_cast<int>(selected)+1)%3);
                        else if(command=='t') skipTutorial=!skipTutorial;
                        else if(command=='n') seed=std::random_device{}();
                        else if(command>='1' && command<='3') { selected=static_cast<MyGame::Difficulty>(command-'1'); begin(); }
                        else if(command=='\r' || command=='\n') begin();
                    }
                    else switch(command)
                    {
                        case 'w': game->Move(0,-1,precise); break;
                        case 'a': game->Move(-1,0,precise); break;
                        case 's': game->Move(0,1,precise); break;
                        case 'd': game->Move(1,0,precise); break;
                        case 'f': case ' ': game->Fire(); break;
                        case '\r': case '\n': game->NextLevel(); stageRecorded=false; break;
                        case 'p': game->TogglePause(); break;
                        case 'r': record(); game->Reset(); stageRecorded=false; break;
                        case 'n': record(); game->NewRun(); seed=game->Seed(); stageRecorded=false; break;
                        case 'm': record(); seed=game->Seed(); selected=game->Mode(); skipTutorial=game->SkipsTutorial(); menu=true; game.reset(); break;
                        case 'q': stopped=1; break;
                    }
                    if(stopped) break;
                }
                if(!size.FitsGame())
                {
                    if(size.columns!=lastNotice.columns || size.rows!=lastNotice.rows)
                        screen.ResizeNotice(size);
                    lastNotice=size;
                }
                else if(helpOpen) MyGame::RenderHelp(screen);
                else if(menu) MyGame::RenderMenu(screen,selected,skipTutorial,seed,scores);
                else
                {
                    const bool ended=game->Status()==MyGame::Game::State::Cleared || game->Status()==MyGame::Game::State::Lost;
                    if(ended && !stageRecorded) { scores.Record(*game); stageRecorded=true; }
                    if(!ended) stageRecorded=false;
                    MyGame::RenderGame(screen,*game,scores);
                }
                if(size.FitsGame()) lastNotice={0,0};
                std::this_thread::sleep_until(now+std::chrono::milliseconds(50));
            }
            record();
        }
        if(!summary.empty()) std::cout<<summary;
    }
    catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
