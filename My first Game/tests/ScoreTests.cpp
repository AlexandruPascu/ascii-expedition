#include "TestSupport.h"
#include "System/HighScores.h"
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace MyGame;
using namespace GameTest;

namespace
{
    struct Fixture
    {
        std::string path;
        explicit Fixture(const std::string& file) : path(file) { Clean(); }
        ~Fixture() { Clean(); }
        void Clean()
        {
            std::remove(path.c_str());
#if defined(_WIN32)
            _rmdir((path + ".tmp").c_str());
#else
            rmdir((path + ".tmp").c_str());
#endif
            std::remove((path + ".tmp").c_str());
        }
    };
    std::string Read(const std::string& path)
    {
        std::ifstream input(path);
        return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }
}

int main(int argc, char** argv)
{
    if (argc != 2) { std::cerr << "Supply a test-only score file path.\n"; return 1; }
    const std::string path = argv[1];
    int failed = 0;
    auto test = [&](const char* name, const std::function<void()>& run)
    {
        try { run(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failed; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    };
    test("scores survive reload and separate difficulty and tutorial starts", [&] {
        Fixture fixture(path);
        HighScores scores(path);
        Check(scores.Error().empty(), "A missing score file is a normal first run");
        std::uint32_t seed = 42;
        for (Difficulty mode : {Difficulty::Relaxed, Difficulty::Normal, Difficulty::Hard})
            for (bool skip : {false, true})
            {
                Check(scores.Best(mode, skip).score == 0, "A different mode must begin with its own record");
                Game game(seed++, skip, mode); Clear(game);
                Check(scores.Record(game), "Completed run must save");
                const HighScores loaded(path);
                Check(loaded.Error().empty() && loaded.Best(mode, skip).score == game.Score() && loaded.Best(mode, skip).seed == game.Seed(), "Reload must retain score and replay seed");
            }
        const std::string original = Read(path);
        Check(scores.Record(Game(99)), "A lower score must be accepted without replacing the best");
        Check(Read(path) == original, "A lower score must leave the table unchanged");
        Game improved(77); Clear(improved); improved.NextLevel(); Clear(improved);
        Check(scores.Record(improved) && HighScores(path).Best(Difficulty::Normal, false).score == improved.Score(), "A later personal best must replace the record");
    });
    test("malformed saved data is reported and recoverable", [&] {
        Fixture fixture(path);
        const std::string validTail = "1 0 0\n2 0 0\n3 0 0\n4 0 0\n5 0 0\n";
        for (const std::string& contents : {
                std::string("broken"),
                "EA_WORKSHOP_SCORES 1\n0 -1 0\n" + validTail,
                "EA_WORKSHOP_SCORES 1\n0 2147483648 0\n" + validTail,
                "EA_WORKSHOP_SCORES 1\n0 42 4294967296\n" + validTail,
                "EA_WORKSHOP_SCORES 1\n0 42 0\n" + validTail + "unexpected"})
        {
            { std::ofstream output(path); output << contents; }
            HighScores scores(path);
            Check(!scores.Error().empty() && scores.Best(Difficulty::Relaxed, false).score == 0, "Invalid values must not become high scores");
            Game game(42); Clear(game);
            Check(scores.Record(game) && scores.Error().empty() && HighScores(path).Best(Difficulty::Normal, false).score == game.Score(), "A new score must recover an invalid table");
        }
    });
    test("failed saves preserve the previous file and can be retried", [&] {
        Fixture fixture(path);
        HighScores scores(path);
        Game game(42); Clear(game);
        Check(scores.Record(game), "Initial score must save");
        const int original = game.Score();
        game.NextLevel(); Clear(game);
#if defined(_WIN32)
        Check(_mkdir((path + ".tmp").c_str()) == 0, "Create a blocked temporary-file path");
#else
        Check(mkdir((path + ".tmp").c_str(), 0700) == 0, "Create a blocked temporary-file path");
#endif
        Check(!scores.Record(game) && !scores.Error().empty(), "Save failure must be nonfatal and visible");
        Check(HighScores(path).Best(Difficulty::Normal, false).score == original, "Failed replacement must preserve the previous file");
        Check(scores.Best(Difficulty::Normal, false).score == game.Score(), "Session best must remain available despite disk failure");
#if defined(_WIN32)
        _rmdir((path + ".tmp").c_str());
#else
        rmdir((path + ".tmp").c_str());
#endif
        Check(scores.Record(game) && scores.Error().empty(), "The same score must retry after storage recovers");
        Check(HighScores(path).Best(Difficulty::Normal, false).score == game.Score(), "Retried score must survive reload");
    });
    return failed ? 1 : 0;
}
