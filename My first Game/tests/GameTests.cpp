#include "TestSupport.h"
#include "Geometry/PixelMatrix.h"
#include <functional>
#include <iostream>
#include <set>

using namespace MyGame;
using namespace GameTest;

int main()
{
    int failed = 0;
    auto test = [&failed](const char* name, const std::function<void()>& run)
    {
        try { run(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& e) { ++failed; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
    };
    test("tutorial starts with stars then introduces patrols and firing", [] {
        Game g(17);
        Check(g.Stage() == 0 && g.Coins().size() == 3 && g.Hazards().empty() &&
              g.Crates().empty() && !g.ExitOpen(), "First lesson must introduce stars, not exit-only movement");
        Check(g.Title() == "TUTORIAL 1/2: COLLECT STARS", "Tutorial numbering must begin at one of two");
        g.NextLevel(); Check(g.Stage() == 0, "Cannot advance an unfinished stage");
        g.Update(130000); Check(g.Status() == Game::State::Playing && g.SecondsLeft() == 120, "Tutorial must be untimed");
        g.Fire(); Check(g.Beam().empty(), "Blaster must remain locked in the first lesson");
        WalkTo(g, g.Exit().GetX(), g.Exit().GetY(), true, true);
        Check(g.Status() == Game::State::Playing, "Locked exit must not clear stage");
        Clear(g); const int firstScore = g.Score(); g.NextLevel();
        Check(g.Score() == firstScore && g.Lives() == 3, "Progression preserves points and lives");
        Check(g.Stage() == 1 && g.Coins().size() == 4 && !g.Hazards().empty() && g.Crates().empty(),
              "Second lesson must introduce patrols");
        Check(g.Title() == "TUTORIAL 2/2: PATROLS AND BLASTER" && g.WeaponReady(),
              "Second lesson must unlock the blaster for practice");
        Clear(g); g.NextLevel();
        Check(!g.Tutorial() && g.Stage() == Level::TutorialStages && g.Title() == "EXPEDITION 1" &&
              !g.Crates().empty() && g.Pickups().size() == 4 && g.WeaponReady(),
              "Two tutorials must lead directly into the first expedition");
    });
    test("generated maps keep full-size character routes safe", [] {
        int checked = 0;
        for (Difficulty mode : {Difficulty::Relaxed, Difficulty::Normal, Difficulty::Hard})
        for (std::uint32_t seed = 0; seed < 64; ++seed)
            for (int stage : {0, 1, 2, 3, 5, 11})
            {
                const Level map = GenerateLevel(seed, stage, mode);
                const auto reachable = Flood(map, map.spawn);
                auto reaches = [&](int x, int y) { return !Blocked(map, x, y) && reachable[y * Game::Width + x] != -1; };
                const std::string context = " (seed " + std::to_string(seed) + ", stage " + std::to_string(stage) + ", " + DifficultyName(mode) + ")";
                Check(reaches(map.spawn.x, map.spawn.y), "Spawn must be safe" + context);
                Check(reaches(map.exit.GetX(), map.exit.GetY()), "Exit must be safely reachable" + context);
                for (const Coin& c : map.coins) Check(reaches(c.x - 1, c.y - 1), "Every star must be reachable" + context);
                for (const Pickup& p : map.pickups) Check(reaches(p.x - 1, p.y - 1), "Every power-up must be reachable" + context);
                for (const Crate& c : map.crates)
                {
                    bool accessible = false;
                    for (Point delta : {Point{-4, 0}, Point{4, 0}, Point{0, -4}, Point{0, 4}})
                    {
                        const int x = c.body.GetX() + delta.x, y = c.body.GetY() + delta.y;
                        accessible |= reaches(x, y);
                    }
                    Check(accessible, "Every crate must have a safe approach" + context);
                }
                if (stage >= 1) Check(map.hazards.size() >= 2, "Patrol generation must not silently omit lesson" + context);
                for (const Hazard& h : map.hazards)
                {
                    const Stone area = Sweep(h);
                    for (const Stone& wall : map.walls)
                        Check(!Touches(area.GetX(), area.GetY(), area.GetShape().GetWidth(), area.GetShape().GetHeight(), wall), "Patrols must not cross walls" + context);
                    for (const Crate& c : map.crates)
                        Check(!Touches(area.GetX(), area.GetY(), area.GetShape().GetWidth(), area.GetShape().GetHeight(), c.body), "Patrols must not cross crates" + context);
                }
                ++checked;
            }
        Check(checked == 1152, "Exercise all three difficulties across 1,152 generated maps");
    });
    test("seeds reproduce maps and randomize every element", [] {
        std::set<std::string> maps, spawns, exits, walls, stars, patrols, crates, pickups;
        for (std::uint32_t seed = 0; seed < 16; ++seed)
        {
            const Level a = GenerateLevel(seed, Level::TutorialStages), b = GenerateLevel(seed, Level::TutorialStages);
            Check(Fingerprint(a) == Fingerprint(b), "Same seed must reproduce map");
            maps.insert(Fingerprint(a));
            auto point = [](int x, int y) { return std::to_string(x) + "," + std::to_string(y); };
            spawns.insert(point(a.spawn.x, a.spawn.y)); exits.insert(point(a.exit.GetX(), a.exit.GetY()));
            walls.insert(point(a.walls.front().GetX(), a.walls.front().GetY()));
            stars.insert(point(a.coins.front().x, a.coins.front().y));
            patrols.insert(point(a.hazards.front().from.x, a.hazards.front().from.y));
            crates.insert(point(a.crates.front().body.GetX(), a.crates.front().body.GetY()));
            pickups.insert(point(a.pickups.front().x, a.pickups.front().y));
        }
        Check(maps.size() == 16 && spawns.size() > 1 && exits.size() > 1 && walls.size() > 1 &&
              stars.size() > 1 && patrols.size() > 1 && crates.size() > 1 && pickups.size() > 1, "All map elements must vary across seeds");
        Game g(42, true); const std::string original = Fingerprint(g.Map());
        Clear(g); const int score = g.Score(), lives = g.Lives(); g.NextLevel();
        Check(g.Stage() == Level::TutorialStages + 1 && g.Score() == score && g.Lives() == lives && Fingerprint(g.Map()) != original, "Next expedition must generate a new map and carry rewards");
        g.Reset();
        Check(g.Stage() == Level::TutorialStages && g.Score() == 0 && g.Lives() == 3 && Fingerprint(g.Map()) == original, "Replay must reset run using its original seed");
    });
    test("rooms offer optional stash detours and distinct patrol roles", [] {
        std::set<std::string> themes;
        for (std::uint32_t seed = 0; seed < 32; ++seed)
        {
            const Level map = GenerateLevel(seed, Level::TutorialStages);
            themes.insert(map.layoutName);
            Check(map.rooms.size() >= 4, "Expeditions must contain rooms and a stash");
            bool bonusCrate = false;
            for (const Room& room : map.rooms) if (room.bonus)
            {
                auto inside = [&](int x, int y) { return Overlaps(x, y, 1, 1, room.x, room.y, room.width, room.height); };
                Check(!inside(map.spawn.x, map.spawn.y) && !inside(map.exit.GetX(), map.exit.GetY()), "Stash must be optional");
                for (const Coin& c : map.coins) Check(!inside(c.x, c.y), "Required stars must stay outside bonus rooms");
                for (const Crate& c : map.crates) bonusCrate |= inside(c.body.GetX(), c.body.GetY());
            }
            Check(bonusCrate, "Each expedition must offer a stash crate");
            double guardSpeed = 0, scoutSpeed = 0;
            int sentries = 0;
            for (const Hazard& h : map.hazards)
            {
                if (h.kind == PatrolKind::Guard) guardSpeed = h.speed;
                if (h.kind == PatrolKind::Scout) scoutSpeed = h.speed;
                if (h.kind == PatrolKind::Sentry)
                {
                    ++sentries;
                    Check(h.from.x == h.to.x && h.from.y == h.to.y, "Sentries must stay stationary");
                }
            }
            Check(guardSpeed > 0 && scoutSpeed > guardSpeed && sentries == 1, "Each expedition needs a guard, faster scout, and sentry");
        }
        Check(themes.size() == 3, "Seeds must explore all three room layouts");
    });
    test("sentry warns before pulsing and becomes harmless when stunned", [] {
        Hazard h({10, 10}, {10, 10}, 0, 0, PatrolKind::Sentry);
        Check(h.Hurts(10, 10, 1, 1) && !h.Hurts(8, 8, 1, 1), "An idle sentry only hurts on body contact");
        h.Update(1599);
        Check(!h.Charging() && !h.Pulsing(), "Sentry must wait before warning");
        h.Update(1);
        Check(h.Charging() && !h.Pulsing() && !h.Hurts(8, 8, 1, 1), "Warning must give time to leave the blast area");
        h.Update(799);
        Check(h.Charging(), "Warning must last 800 milliseconds");
        h.Update(1);
        Check(h.Pulsing() && h.Hurts(8, 8, 1, 1) && h.Hurts(14, 14, 1, 1) && !h.Hurts(15, 14, 1, 1), "Pulse must match the displayed seven-cell area");
        h.stunnedMillis = 2000;
        Check(!h.Pulsing() && !h.Hurts(10, 10, 3, 3), "Stun must disable both contact and pulse damage");
        h.Update(1999);
        Check(h.cycleMillis == 2400 && !h.Pulsing(), "Stun must freeze the pulse cycle");
        h.Update(601);
        Check(!h.Pulsing() && !h.Charging() && h.cycleMillis == 0, "Pulse must finish after the remaining active time");
    });
    test("difficulty controls lives, time, speed and extra patrols", [] {
        Game relaxed(42, true, Difficulty::Relaxed), normal(42, true), hard(42, true, Difficulty::Hard);
        Check(relaxed.Lives() == 5 && !relaxed.Timed(), "Relaxed runs must have five lives and no deadline");
        relaxed.Update(240000);
        Check(relaxed.Status() == Game::State::Playing, "Untimed runs must survive beyond normal time limits");
        Check(normal.Lives() == 3 && normal.SecondsLeft() == 120 && normal.Timed(), "Normal must retain three lives and two minutes");
        Check(hard.Lives() == 2 && hard.SecondsLeft() == 90 && hard.Timed(), "Hard must start with two lives and ninety seconds");
        Check(hard.Hazards().size() == normal.Hazards().size() + 1, "Hard must add a patrol");
        auto speed = [](const Game& g) {
            for (const Hazard& h : g.Hazards()) if (h.kind == PatrolKind::Guard) return h.speed;
            return 0.0;
        };
        Check(speed(relaxed) < speed(normal) && speed(normal) < speed(hard), "Patrol speed must scale with difficulty");
        hard.Update(90000);
        Check(hard.Status() == Game::State::Lost && hard.EndReason() == "Time ran out", "Hard must end at its own deadline");
        for (Difficulty mode : {Difficulty::Relaxed, Difficulty::Normal, Difficulty::Hard})
        {
            Game tutorial(42, false, mode);
            tutorial.Update(180000);
            Check(!tutorial.Timed() && tutorial.Status() == Game::State::Playing, "Tutorials must be untimed on every difficulty");
        }
        const auto clock = std::find_if(relaxed.Pickups().begin(), relaxed.Pickups().end(), [](const Pickup& p) { return p.power == Power::Time; });
        const int before = relaxed.Score(), stars = relaxed.Stats().stars;
        WalkTo(relaxed, clock->x - 1, clock->y - 1);
        Check(relaxed.Score() == before + (relaxed.Stats().stars - stars) * 100 + 100, "Time pickup must award points in untimed mode");
        hard.Reset();
        Check(hard.Mode() == Difficulty::Hard && hard.Lives() == 2 && hard.Stats().elapsedMillis == 0, "Replay must preserve difficulty and reset the run");
    });
    test("feedback and run summaries follow rewards, pause and progression", [] {
        Game g(42);
        const Coin first = g.Coins().front();
        WalkTo(g, first.x - 1, first.y - 1);
        Check(g.Stats().stars == g.Collected() && !g.Messages().empty() && g.Messages().back().tone == FeedbackTone::Reward, "Collecting stars must give feedback and update totals");
        const auto messages = g.Messages();
        g.TogglePause(); g.Update(5000);
        Check(g.Stats().elapsedMillis == 0 && g.Messages().back().remainingMillis == messages.back().remainingMillis, "Pause must freeze messages and active run time");
        g.TogglePause(); g.Update(2600);
        Check(g.Messages().empty() && g.Stats().elapsedMillis == 2600, "Feedback must expire during active play");
        g.Reset(); g.Update(2600); Clear(g);
        Check(g.Stats().stars == 3 && g.Stats().expeditionsCleared == 0, "Tutorials must not count as expeditions");
        bool unlocked = false;
        for (const Feedback& message : g.Messages()) unlocked |= message.text.find("Exit unlocked") != std::string::npos;
        Check(unlocked && g.Messages().size() <= 3, "Unlock feedback must survive stage completion without an unbounded message queue");
        g.Update(5000);
        Check(g.Stats().elapsedMillis == 2600, "Summary screens must not count as active time");
        g.NextLevel(); Clear(g); g.NextLevel(); Clear(g);
        const int stars = g.Stats().stars;
        Check(g.Stats().expeditionsCleared == 1 && stars > 7, "Totals must carry across tutorials and expeditions");
        g.NextLevel();
        Check(g.Stats().stars == stars && g.Messages().empty(), "A new stage must retain totals and clear old notices");
        g.Reset();
        Check(g.Stats().stars == 0 && g.Stats().expeditionsCleared == 0 && g.Stats().elapsedMillis == 0 && g.Messages().empty(), "Replay must start a fresh summary");
    });
    test("patrol overshoot and vertical movement", [] {
        Hazard h({1, 1}, {11, 1}, 25); h.Update(1000);
        Check(h.body.GetX() == 6, "Overshoot must reflect beyond both endpoints");
        h.Update(400); Check(h.body.GetX() == 6, "Return leg must preserve speed");
        Hazard v({1, 1}, {1, 11}, 5); v.Update(1000);
        Check(v.body.GetX() == 1 && v.body.GetY() == 6, "Vertical patrols must advance along Y");
        h.Update(100000); Check(h.body.GetX() >= 1 && h.body.GetX() <= 11, "Large time steps must remain bounded");
    });
    test("walls and crate collision respect character size", [] {
        Check(!Overlaps(0, 0, 3, 3, 3, 0, 3, 3) && Overlaps(0, 0, 3, 3, 2, 2, 1, 1), "Touching edges are not overlap");
        Game g(42, true); FaceCrate(g, 0);
        const int x = g.Player().GetX(), y = g.Player().GetY();
        const int dx = g.Aim() == '>' ? 1 : g.Aim() == '<' ? -1 : 0;
        const int dy = g.Aim() == 'v' ? 1 : g.Aim() == '^' ? -1 : 0;
        g.Move(dx, dy);
        Check(g.Player().GetX() == x && g.Player().GetY() == y, "An intact crate must block movement");
        g.Move(5, 0); Check(g.Player().GetX() == x, "Large moves must not tunnel through obstacles");
    });
    test("blaster cooldown, crate rewards and drops", [] {
        Game g(42, true); FaceCrate(g, 0);
        const int before = g.Score(), reward = g.Crates()[0].points;
        const Power drop = g.Crates()[0].drop;
        const auto pickupCount = g.Pickups().size();
        g.Fire(); Check(g.Crates()[0].health == 1 && g.Score() == before && !g.Beam().empty(), "First hit damages crate only");
        Check(g.Messages().back().text.find("One more shot") != std::string::npos, "Crate damage must explain the remaining hit");
        g.Fire(); Check(g.Crates()[0].health == 1, "Weapon cooldown prevents instant double hit");
        g.Update(300); g.Fire();
        Check(g.Crates()[0].health == 0 && g.Score() == before + reward, "Second hit breaks crate and awards bonus");
        Check(g.Stats().crates == 1 && g.Stats().shots == 2 && g.Messages().back().tone == FeedbackTone::Reward, "Crate summary counts accepted shots and completed breaks");
        Check(g.Pickups().size() == pickupCount + (drop == Power::None ? 0 : 1), "Crate emits its predetermined bonus drop");
        const Stone body = g.Crates()[0].body;
        WalkTo(g, body.GetX(), body.GetY());
        Check(g.Player().GetX() == body.GetX(), "Destroyed crate must open its tile");
        if (drop != Power::None) Check(g.Pickups().back().collected, "Walking over crate drop collects it");
    });
    test("walls stop blaster shots", [] {
        for (std::uint32_t seed = 0; seed < 32; ++seed)
        {
            Game g(seed, true);
            const auto reachable = Flood(g.Map(), g.Map().spawn);
            for (std::size_t index = 0; index < g.Crates().size(); ++index)
                for (Point direction : {Point{1, 0}, Point{-1, 0}, Point{0, 1}, Point{0, -1}})
                {
                    const Stone& c = g.Crates()[index].body;
                    const int x = c.GetX() - 8 * direction.x, y = c.GetY() - 8 * direction.y;
                    if (Blocked(g.Map(), x, y) || reachable[y * Game::Width + x] == -1) continue;
                    const int midX = x + 4 * direction.x, midY = y + 4 * direction.y;
                    bool wall = false;
                    for (const Stone& w : g.Walls()) wall |= Touches(midX, midY, 3, 3, w);
                    if (!wall) continue;
                    WalkTo(g, x, y); g.Move(direction.x, direction.y); g.Fire();
                    Check(g.Crates()[index].health == 2, "A shot must not damage a crate through a wall");
                    return;
                }
        }
        throw std::runtime_error("No wall-occlusion scenario found");
    });
    test("tutorial and expedition blasters stun patrol temporarily", [] {
        auto stun = [](Game g, PatrolKind kind) {
            for (std::size_t i = 0; i < g.Hazards().size(); ++i)
            {
                if (g.Hazards()[i].kind != kind) continue;
                const Stone body = g.Hazards()[i].body;
                const auto reachable = Flood(g.Map(), g.Map().spawn);
                for (Point aim : {Point{1, 0}, Point{-1, 0}, Point{0, 1}, Point{0, -1}})
                {
                    const int distance = kind == PatrolKind::Sentry ? 6 : 4;
                    const int x = body.GetX() - distance * aim.x, y = body.GetY() - distance * aim.y;
                    if (Blocked(g.Map(), x, y) || reachable[y * Game::Width + x] == -1) continue;
                    WalkTo(g, x, y); g.Move(aim.x, aim.y, true); g.Fire();
                    Check(g.Hazards()[i].stunnedMillis == 2000, "Blaster must stun patrol for two seconds");
                    const double phase = g.Hazards()[i].phase;
                    const int cycle = g.Hazards()[i].cycleMillis;
                    Check(g.Stats().stuns == 1 && g.Stats().shots == 1, "A successful stun must appear in run stats");
                    g.Update(2000);
                    Check(g.Hazards()[i].phase == phase && g.Hazards()[i].cycleMillis == cycle, "Stunned patrol must freeze movement and pulse timing");
                    g.Update(500);
                    Check(g.Hazards()[i].cycleMillis != cycle && (kind == PatrolKind::Sentry || g.Hazards()[i].phase != phase), "Patrol must resume after stun expires");
                    return;
                }
            }
            throw std::runtime_error("No safe shot at a patrol found");
        };
        Game tutorial(42);
        Clear(tutorial); tutorial.NextLevel();
        stun(tutorial, PatrolKind::Guard);
        for (PatrolKind kind : {PatrolKind::Guard, PatrolKind::Scout, PatrolKind::Sentry})
            stun(Game(42, true), kind);
    });
    test("power-ups apply life, time and temporary shield", [] {
        for (Power power : {Power::Heart, Power::Time, Power::Shield})
        {
            Game g(42, true); g.Update(1200);
            const auto found = std::find_if(g.Pickups().begin(), g.Pickups().end(), [&](const Pickup& p) { return p.power == power; });
            const int x = found->x, y = found->y;
            const int before = g.SecondsLeft();
            WalkTo(g, x - 1, y - 1);
            if (power == Power::Heart) Check(g.Lives() == 4, "Heart must add one life");
            if (power == Power::Time) Check(g.SecondsLeft() == before + 15, "Clock must add fifteen seconds");
            if (power == Power::Shield)
            {
                Check(g.Invulnerable(), "Shield pickup must protect player");
                g.Update(4999); Check(g.Invulnerable(), "Shield lasts five seconds");
                g.Update(1); Check(!g.Invulnerable(), "Shield must expire");
            }
        }
    });
    test("speed boost doubles movement, supports precision and expires", [] {
        Game g(42, true);
        const auto pickup = std::find_if(g.Pickups().begin(), g.Pickups().end(),
                                         [](const Pickup& p) { return p.power == Power::Speed; });
        Check(pickup != g.Pickups().end(), "Every expedition must include a speed pickup");
        WalkTo(g, pickup->x - 1, pickup->y - 1);
        Check(g.SpeedBoosted() && g.SpeedSecondsLeft() == 6, "Speed pickup must activate for six seconds");
        const int x = g.Player().GetX(), y = g.Player().GetY();
        Point direction{0, 0};
        for (Point d : {Point{1, 0}, Point{-1, 0}, Point{0, 1}, Point{0, -1}})
            if (!Blocked(g.Map(), x + d.x, y + d.y) && !Blocked(g.Map(), x + 2 * d.x, y + 2 * d.y))
            { direction = d; break; }
        Check(direction.x || direction.y, "Speed pickup must have room for movement");
        g.Move(direction.x, direction.y);
        Check(g.Player().GetX() == x + 2 * direction.x && g.Player().GetY() == y + 2 * direction.y,
              "Boosted movement must travel two cells per keypress");
        g.Move(-direction.x, -direction.y, true);
        Check(g.Player().GetX() == x + direction.x && g.Player().GetY() == y + direction.y,
              "Precision movement must travel one cell while boosted");
        g.TogglePause(); g.Update(7000);
        Check(g.SpeedSecondsLeft() == 6, "Pause must freeze boost duration");
        g.TogglePause(); g.Update(5999);
        Check(g.SpeedBoosted(), "Boost must remain active until its full duration elapses");
        g.Update(1); Check(!g.SpeedBoosted(), "Speed boost must expire at six seconds");
        g.Move(-direction.x, -direction.y);
        Check(g.Player().GetX() == x && g.Player().GetY() == y, "Normal one-cell movement must return after expiry");
    });
    test("speed boost respects crates and clears between stages", [] {
        Game g(42, true);
        const auto pickup = std::find_if(g.Pickups().begin(), g.Pickups().end(),
                                         [](const Pickup& p) { return p.power == Power::Speed; });
        WalkTo(g, pickup->x - 1, pickup->y - 1);
        FaceCrate(g, 0);
        Check(g.SpeedBoosted(), "Collision test must exercise boosted movement");
        const int dx = g.Aim() == '>' ? 1 : g.Aim() == '<' ? -1 : 0;
        const int dy = g.Aim() == 'v' ? 1 : g.Aim() == '^' ? -1 : 0;
        const int x = g.Player().GetX(), y = g.Player().GetY();
        g.Move(-dx, -dy, true);
        g.Move(dx, dy);
        Check(g.Player().GetX() == x && g.Player().GetY() == y && g.Crates()[0].health == 2,
              "Boost must stop after its first step if the second would hit a crate");
        Clear(g); g.NextLevel();
        Check(!g.SpeedBoosted(), "New stage must clear the speed buff");
        const auto nextPickup = std::find_if(g.Pickups().begin(), g.Pickups().end(),
                                             [](const Pickup& p) { return p.power == Power::Speed; });
        WalkTo(g, nextPickup->x - 1, nextPickup->y - 1);
        Check(g.SpeedBoosted(), "Next map must also offer speed boosts");
        g.Reset(); Check(!g.SpeedBoosted(), "Restart must clear speed boost");
    });
    test("moving patrol hits stationary player and respawns safely", [] {
        Game g(42, true); g.Update(6000);
        const auto reachable = Flood(g.Map(), g.Map().spawn, false);
        for (const Hazard& hazard : g.Hazards())
            for (Point target : {hazard.from, hazard.to})
            {
                if (Blocked(g.Map(), target.x, target.y, false) || reachable[target.y * Game::Width + target.x] == -1) continue;
                WalkTo(g, target.x, target.y, false);
                // Routes can collect a shield, so let it expire before observing the hit.
                const int lives = g.Lives();
                for (int elapsed = 0; elapsed < 20000 && g.Lives() == lives; elapsed += 20) g.Update(20);
                Check(g.Stats().hits == 1 && g.HitFlash() && g.Messages().back().tone == FeedbackTone::Danger, "A hit must flash, show a warning, and enter the summary");
                Check(g.Lives() == lives - 1, "Patrol must damage stationary player exactly once");
                Check(g.Player().GetX() == g.Map().spawn.x && g.Player().GetY() == g.Map().spawn.y, "Hit must return player to safe spawn");
                return;
            }
        throw std::runtime_error("No patrol endpoint reachable for collision test");
    });
    test("pause freezes movement, shooting and time", [] {
        Game g(42, true); g.TogglePause();
        const std::string map = Fingerprint(g.Map());
        g.Move(1, 0); g.Fire(); g.Update(5000);
        Check(g.SecondsLeft() == 120 && g.Player().GetX() == g.Map().spawn.x && g.Beam().empty() &&
              Fingerprint(g.Map()) == map, "Paused state must freeze gameplay");
        g.TogglePause(); g.Update(1000); Check(g.SecondsLeft() == 119, "Resume must restore timer");
    });
    test("simulation uses elapsed time and timeout ends round", [] {
        Game a(42, true), b(42, true); a.Update(1000);
        for (int i = 0; i < 50; ++i) b.Update(20);
        Check(Fingerprint(a.Map()) == Fingerprint(b.Map()) && a.SecondsLeft() == b.SecondsLeft(), "Frame size must not change simulation");
        a.Update(Game::DurationMillis);
        Check(a.Status() == Game::State::Lost && a.SecondsLeft() == 0, "Expedition must end on timeout");
        a.NextLevel(); Check(a.Stage() == Level::TutorialStages, "Losing must not advance stage");
        a.Reset(); Check(a.Status() == Game::State::Playing && a.Score() == 0 && a.Lives() == 3, "Replay resets lost run");
    });
    test("full tutorial-to-expedition playthrough preserves progression", [] {
        for (std::uint32_t seed : {0u, 17u, 42u, 999u})
        {
            Game g(seed);
            for (int stage = 0; stage < 5; ++stage)
            {
                Clear(g);
                const int score = g.Score(), lives = g.Lives();
                g.Update(10000); g.Fire(); g.Move(1, 0);
                Check(g.Score() == score && g.Status() == Game::State::Cleared, "Cleared stage must freeze and award bonus only once");
                g.NextLevel();
                Check(g.Stage() == stage + 1 && g.Score() == score && g.Lives() == lives && g.Collected() == 0,
                      "Stage progression must carry lives/score and reset objectives");
            }
        }
    });
    test("pixel buffer rejects invalid coordinates", [] {
        PixelMatrix pixels;
        bool rejected = false;
        try { pixels.SetPixelAt(100, 0, 'x'); } catch (const std::out_of_range&) { rejected = true; }
        Check(rejected, "Renderer must reject out-of-bounds writes");
    });
    return failed ? 1 : 0;
}
