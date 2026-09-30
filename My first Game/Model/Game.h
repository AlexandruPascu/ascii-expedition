#pragma once

#include "Personaj.h"
#include "Level.h"
#include <string>

namespace MyGame
{
    enum class FeedbackTone { Info, Reward, Danger };
    struct Feedback { std::string text; FeedbackTone tone; int remainingMillis; };
    struct RunStats
    {
        int expeditionsCleared=0, stars=0, crates=0, hits=0, shots=0, stuns=0;
        long long elapsedMillis=0;
    };

    class Game
    {
    public:
        enum { Width = Level::Width, Height = Level::Height, DurationMillis = 120000, MaxLives = 5 };
        enum class State { Playing, Paused, Cleared, Lost };

        Game();
        explicit Game(std::uint32_t seed, bool skipTutorial = false, Difficulty difficulty = Difficulty::Normal);
        void Reset();
        void NewRun();
        void NextLevel();
        void Move(int dx, int dy, bool precise = false);
        void Fire();
        void Update(int elapsedMillis);
        void TogglePause();

        const Personaj& Player() const { return player; }
        const Level& Map() const { return level; }
        const std::vector<Stone>& Walls() const { return level.walls; }
        const std::vector<Hazard>& Hazards() const { return level.hazards; }
        const std::vector<Coin>& Coins() const { return level.coins; }
        const std::vector<Crate>& Crates() const { return level.crates; }
        const std::vector<Pickup>& Pickups() const { return level.pickups; }
        const std::vector<Point>& Beam() const { return beam; }
        const Stone& Exit() const { return level.exit; }
        State Status() const { return state; }
        int Lives() const { return lives; }
        int Score() const { return score; }
        int SecondsLeft() const { return (remainingMillis + 999) / 1000; }
        int Stage() const { return stage; }
        std::uint32_t Seed() const { return seed; }
        int Collected() const;
        Difficulty Mode() const { return difficulty; }
        bool SkipsTutorial() const { return firstStage != 0; }
        bool Timed() const { return !Tutorial() && RulesFor(difficulty).durationMillis > 0; }
        const RunStats& Stats() const { return stats; }
        const std::vector<Feedback>& Messages() const { return feedback; }
        bool HitFlash() const { return hitFlashMillis > 0; }
        std::string EndReason() const;
        bool Tutorial() const { return stage < Level::TutorialStages; }
        bool ExitOpen() const { return Collected() == static_cast<int>(level.coins.size()); }
        bool Invulnerable() const { return immunityMillis > 0; }
        bool WeaponUnlocked() const { return stage >= Level::TutorialStages - 1; }
        bool WeaponReady() const { return WeaponUnlocked() && !cooldownMillis; }
        bool SpeedBoosted() const { return speedMillis > 0; }
        int SpeedSecondsLeft() const { return (speedMillis + 999) / 1000; }
        char Aim() const;
        std::string Title() const;
        std::string Hint() const;

    private:
        Personaj player;
        Level level;
        std::vector<Point> beam;
        State state;
        Difficulty difficulty;
        RunStats stats;
        std::vector<Feedback> feedback;
        int hitFlashMillis;
        std::uint32_t seed;
        int firstStage, stage, lives, score, remainingMillis, immunityMillis;
        int cooldownMillis, beamMillis, speedMillis, aimX, aimY;
        void Notice(const std::string& text, FeedbackTone tone = FeedbackTone::Info);
        void LoadLevel();
        void ResolveContacts();
        void Respawn();
        void TakePickup(Pickup& pickup);
    };
}
