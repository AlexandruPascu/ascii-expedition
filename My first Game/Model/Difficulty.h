#pragma once

namespace MyGame
{
    enum class Difficulty { Relaxed, Normal, Hard };
    struct Rules { int lives, durationMillis; double patrolSpeed; int extraPatrols; };
    inline Rules RulesFor(Difficulty difficulty)
    {
        switch (difficulty)
        {
            case Difficulty::Relaxed: return {5, 0, 0.75, 0};
            case Difficulty::Hard: return {2, 90000, 1.35, 1};
            default: return {3, 120000, 1.0, 0};
        }
    }
    inline const char* DifficultyName(Difficulty difficulty)
    {
        return difficulty == Difficulty::Relaxed ? "Relaxed" : difficulty == Difficulty::Hard ? "Hard" : "Normal";
    }
}
