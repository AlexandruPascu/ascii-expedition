#include "HighScores.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <memory>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace MyGame
{
    namespace
    {
        std::string Environment(const char* name)
        {
#if defined(_MSC_VER)
            char* value=nullptr;
            std::size_t length=0;
            if(_dupenv_s(&value,&length,name)!=0) return {};
            const std::unique_ptr<char,decltype(&std::free)> owned(value,&std::free);
            return owned ? owned.get() : "";
#else
            const char* value=std::getenv(name);
            return value ? value : "";
#endif
        }
    }
    std::string HighScores::DefaultPath()
    {
#if defined(_WIN32)
        std::string directory=Environment("LOCALAPPDATA");
        if(directory.empty()) directory=Environment("USERPROFILE");
#else
        const std::string directory=Environment("HOME");
#endif
        return (directory.empty() ? "." : directory)+"/.ea-workshop-highscores";
    }
    HighScores::HighScores(const std::string& path) : file(path)
    {
        std::ifstream input(file);
        if(!input) return;
        std::string magic; int version=0;
        std::array<ScoreEntry,6> loaded;
        bool valid=static_cast<bool>(input>>magic>>version) && magic=="EA_WORKSHOP_SCORES" && version==1;
        for(int i=0;valid && i<6;++i)
        {
            int index=-1; long long score=-1; unsigned long long seed=0;
            valid=static_cast<bool>(input>>index>>score>>seed) && index==i && score>=0 &&
                  score<=std::numeric_limits<int>::max() && seed<=std::numeric_limits<std::uint32_t>::max();
            if(valid) { loaded[i].score=static_cast<int>(score); loaded[i].seed=static_cast<std::uint32_t>(seed); }
        }
        std::string extra;
        if(valid && !(input>>extra)) entries=loaded;
        else error="Saved scores could not be read; starting a fresh table.";
    }
    bool HighScores::Save()
    {
        const std::string temporary=file+".tmp";
        {
            std::ofstream output(temporary,std::ios::trunc);
            if(!output) { error="High score could not be saved."; return false; }
            output<<"EA_WORKSHOP_SCORES 1\n";
            for(int i=0;i<6;++i) output<<i<<' '<<entries[i].score<<' '<<entries[i].seed<<'\n';
            output.close();
            if(!output) { std::remove(temporary.c_str()); error="High score could not be saved."; return false; }
        }
#if defined(_WIN32)
        const bool moved=MoveFileExA(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
        const bool moved=std::rename(temporary.c_str(),file.c_str())==0;
#endif
        if(!moved) { std::remove(temporary.c_str()); error="High score could not be saved."; return false; }
        error.clear(); return true;
    }
    bool HighScores::Record(const Game& game)
    {
        ScoreEntry& entry=entries[Index(game.Mode(),game.SkipsTutorial())];
        if(!game.Assisted() && game.Score()>entry.score)
        {
            entry.score=game.Score(); entry.seed=game.Seed();
            dirty=true;
        }
        if(!dirty) return true;
        if(!Save()) return false;
        dirty=false;
        return true;
    }
}
