#include "Game.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace MyGame
{
    Game::Game() : Game(std::random_device{}()) {}
    Game::Game(std::uint32_t initialSeed, bool skipTutorial, Difficulty mode)
        : difficulty(mode), seed(initialSeed), firstStage(skipTutorial ? Level::TutorialStages : 0) { Reset(); }

    void Game::Reset()
    {
        stage = firstStage;
        lives = RulesFor(difficulty).lives;
        score = 0;
        stats = RunStats{};
        assisted = false;
        LoadLevel();
    }

    void Game::NewRun()
    {
        seed = std::random_device{}();
        Reset();
    }

    void Game::LoadLevel()
    {
        level = GenerateLevel(seed, stage, difficulty);
        remainingMillis = RulesFor(difficulty).durationMillis;
        hitFlashMillis = 0;
        feedback.clear();
        cooldownMillis = beamMillis = 0;
        beam.clear();
        state = State::Playing;
        Respawn();
    }

    void Game::NextLevel()
    {
        if (state != State::Cleared) return;
        ++stage;
        LoadLevel();
    }

    void Game::Notice(const std::string& text, FeedbackTone tone)
    {
        if (feedback.size() == 3) feedback.erase(feedback.begin());
        feedback.push_back({text, tone, 2600});
    }

    std::string Game::EndReason() const
    {
        if (state == State::Lost) return lives == 0 ? "No lives left" : "Time ran out";
        return state == State::Cleared ? "Stage complete" : "Run ended";
    }

    void Game::Respawn()
    {
        player.SetX(level.spawn.x);
        player.SetY(level.spawn.y);
        player.SetOrientare(Personaj::SPRE_DREAPTA);
        aimX = 1; aimY = 0;
        immunityMillis = 1200;
        speedMillis = 0;
    }

    int Game::Collected() const
    {
        return static_cast<int>(std::count_if(level.coins.begin(), level.coins.end(),
                                             [](const Coin& c) { return c.collected; }));
    }

    char Game::Aim() const { return aimX ? (aimX > 0 ? '>' : '<') : (aimY > 0 ? 'v' : '^'); }

    std::string Game::Title() const
    {
        if (stage == 0) return "TUTORIAL 1/2: COLLECT STARS";
        if (stage == 1) return "TUTORIAL 2/2: PATROLS AND BLASTER";
        return "EXPEDITION " + std::to_string(stage - Level::TutorialStages + 1);
    }

    std::string Game::Hint() const
    {
        if (state == State::Paused) return "PAUSED | P to resume. The timer and patrols are frozen.";
        if (state == State::Lost) return "GAME OVER | R replays this seed; N starts a new random run.";
        if (state == State::Cleared) return "STAGE CLEAR! Bonus awarded. Press ENTER for the next map.";
        if (stage == 0) return "Arrows/WASD move. Collect every * to open the gate. No timer or patrols.";
        if (stage == 1) return "Red triangle patrols hurt. F/Space stuns them; aim with arrows/WASD.";
        return "Fire at crates for bonuses; hit patrols to stun. All stars unlock the exit.";
    }

    void Game::Move(int dx, int dy, bool precise)
    {
        if (state != State::Playing || std::abs(dx) + std::abs(dy) != 1) return;
        aimX = dx; aimY = dy;
        if (dx) player.SetOrientare(dx < 0 ? Personaj::SPRE_STANGA : Personaj::SPRE_DREAPTA);
        const int steps = SpeedBoosted() && !precise ? 2 : 1;
        const int w = player.GetShape().GetWidth(), h = player.GetShape().GetHeight();
        for (int step = 0; step < steps; ++step)
        {
            const int x = player.GetX() + dx, y = player.GetY() + dy;
            if (x < 1 || y < 1 || x + w > Width - 1 || y + h > Height - 1) return;
            for (const Stone& wall : level.walls) if (Touches(x, y, w, h, wall)) return;
            for (const Crate& crate : level.crates)
                if (crate.health > 0 && Touches(x, y, w, h, crate.body)) return;
            // Resolve each cell so a boost cannot skip collisions, pickups, or the exit.
            const int livesBefore = lives;
            player.SetX(x);
            player.SetY(y);
            ResolveContacts();
            if (state != State::Playing || lives < livesBefore) return;
        }
    }

    void Game::Fire()
    {
        if (state != State::Playing || !WeaponReady()) return;
        ++stats.shots;
        cooldownMillis = 300;
        beamMillis = 150;
        beam.clear();
        const int centerX = player.GetX() + 1, centerY = player.GetY() + 1;
        for (int distance = 2; distance <= 9; ++distance)
        {
            const Point p{centerX + aimX * distance, centerY + aimY * distance};
            if (p.x <= 0 || p.y <= 0 || p.x >= Width - 1 || p.y >= Height - 1) break;
            if (std::any_of(level.walls.begin(), level.walls.end(),
                            [&](const Stone& wall) { return Touches(p.x, p.y, 1, 1, wall); })) break;
            beam.push_back(p);
            for (Crate& crate : level.crates)
                if (crate.health > 0 && Touches(p.x, p.y, 1, 1, crate.body))
                {
                    if (--crate.health == 0)
                    {
                        score += crate.points;
                        ++stats.crates;
                        Notice("Crate broken: +" + std::to_string(crate.points) + " points!", FeedbackTone::Reward);
                        if (crate.drop != Power::None)
                            level.pickups.push_back({crate.body.GetX() + 1, crate.body.GetY() + 1, crate.drop, false});
                    }
                    else Notice("Crate hit! One more shot breaks it.");
                    return;
                }
            for (Hazard& hazard : level.hazards)
                if (Touches(p.x, p.y, 1, 1, hazard.body))
                {
                    if (!hazard.stunnedMillis) ++stats.stuns;
                    hazard.stunnedMillis = 2000;
                    Notice("Patrol stunned: safe to pass for 2 seconds!", FeedbackTone::Reward);
                    return;
                }
        }
    }

    void Game::TakePickup(Pickup& pickup)
    {
        pickup.collected = true;
        switch (pickup.power)
        {
            case Power::Shield: immunityMillis = std::max(immunityMillis, 5000); Notice("Shield acquired: 5 seconds of protection.", FeedbackTone::Reward); break;
            case Power::Heart:
                if (lives < MaxLives) { ++lives; Notice("Extra life acquired!", FeedbackTone::Reward); }
                else { score += 100; Notice("Lives full: +100 points!", FeedbackTone::Reward); }
                break;
            case Power::Time:
                if (Timed()) { remainingMillis = std::min(180000, remainingMillis + 15000); Notice("Time bonus: +15 seconds!", FeedbackTone::Reward); }
                else { score += 100; Notice("Untimed bonus: +100 points!", FeedbackTone::Reward); }
                break;
            case Power::Speed: speedMillis = 6000; Notice("Speed boost acquired: double movement for 6 seconds!", FeedbackTone::Reward); break;
            case Power::None: break;
        }
    }

    void Game::ResolveContacts()
    {
        const int x = player.GetX(), y = player.GetY();
        const int w = player.GetShape().GetWidth(), h = player.GetShape().GetHeight();
        if (!Invulnerable())
            for (const Hazard& hazard : level.hazards)
                if (hazard.Hurts(x, y, w, h))
                {
                    speedMillis = 0;
                    --lives;
                    ++stats.hits;
                    if (!lives) state = State::Lost;
                    else Respawn();
                    hitFlashMillis = 500;
                    Notice(!lives ? "Last life lost!" : "Life lost! Respawned with a brief shield.", FeedbackTone::Danger);
                    return;
                }
        for (Coin& coin : level.coins)
            if (!coin.collected && Overlaps(x, y, w, h, coin.x, coin.y, 1, 1))
            {
                coin.collected = true;
                score += 100;
                ++stats.stars;
                Notice("Star collected: +100 points!", FeedbackTone::Reward);
                if (ExitOpen()) Notice("Exit unlocked! Find the gate marked GO!", FeedbackTone::Reward);
            }
        for (Pickup& pickup : level.pickups)
            if (!pickup.collected && Overlaps(x, y, w, h, pickup.x, pickup.y, 1, 1)) TakePickup(pickup);
        if (ExitOpen() && Touches(x, y, w, h, level.exit))
        {
            state = State::Cleared;
            const int bonus = Tutorial() ? 250 : lives * 250 + (Timed() ? SecondsLeft() * 10 : 0);
            score += bonus;
            if (!Tutorial()) ++stats.expeditionsCleared;
            Notice("Stage clear: +" + std::to_string(bonus) + " bonus points!", FeedbackTone::Reward);
        }
    }

    void Game::Update(int elapsedMillis)
    {
        // Bound each collision step so fast-moving patrols cannot skip over the player.
        while (elapsedMillis > 0 && state == State::Playing)
        {
            const int step = std::min(elapsedMillis, 20);
            elapsedMillis -= step;
            stats.elapsedMillis += step;
            if (Timed()) remainingMillis = std::max(0, remainingMillis - step);
            hitFlashMillis = std::max(0, hitFlashMillis - step);
            for (Feedback& message : feedback) message.remainingMillis -= step;
            feedback.erase(std::remove_if(feedback.begin(), feedback.end(), [](const Feedback& message) {
                return message.remainingMillis <= 0;
            }), feedback.end());
            immunityMillis = std::max(0, immunityMillis - step);
            speedMillis = std::max(0, speedMillis - step);
            cooldownMillis = std::max(0, cooldownMillis - step);
            beamMillis = std::max(0, beamMillis - step);
            if (!beamMillis) beam.clear();
            for (Hazard& hazard : level.hazards) hazard.Update(step);
            ResolveContacts();
            if (Timed() && !remainingMillis && state == State::Playing)
            {
                state = State::Lost;
                Notice("Time ran out!", FeedbackTone::Danger);
            }
        }
    }

    void Game::TogglePause()
    {
        if (state == State::Playing || state == State::Cleared)
        {
            beforePause = state;
            state = State::Paused;
        }
        else if (state == State::Paused) state = beforePause;
    }
}
