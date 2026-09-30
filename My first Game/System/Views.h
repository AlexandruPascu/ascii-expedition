#pragma once
#include "TermScreen.h"
#include "HighScores.h"
namespace MyGame
{
    void RenderGame(TerminalScreen& screen,const Game& game,const HighScores& scores);
    void RenderMenu(TerminalScreen& screen,Difficulty selected,bool skipTutorial,std::uint32_t seed,const HighScores& scores);
    void RenderHelp(TerminalScreen& screen);
    std::string RunSummary(const Game& game,const HighScores& scores);
}
