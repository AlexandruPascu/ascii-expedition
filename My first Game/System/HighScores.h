#pragma once

#include "Model/Game.h"
#include <array>
#include <string>

namespace MyGame
{
    struct ScoreEntry { int score=0; std::uint32_t seed=0; };
    class HighScores
    {
        std::string file, error;
        std::array<ScoreEntry,6> entries;
        bool dirty = false;
        int Index(Difficulty mode,bool skip) const { return static_cast<int>(mode)*2+(skip ? 1:0); }
        bool Save();
    public:
        explicit HighScores(const std::string& path);
        static std::string DefaultPath();
        ScoreEntry Best(Difficulty mode,bool skip) const { return entries[Index(mode,skip)]; }
        bool Record(const Game& game);
        const std::string& Error() const { return error; }
    };
}
