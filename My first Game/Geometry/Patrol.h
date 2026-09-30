#pragma once

#include "Shape.h"
#include "Model/Level.h"

namespace MyGame
{
    class Patrol : public Shape
    {
        bool stunned;
        PatrolKind kind;
    public:
        Patrol(bool isStunned,PatrolKind type=PatrolKind::Guard) : stunned(isStunned),kind(type) {}
        void FillPixels(PixelMatrix& output) const override
        {
            const char* rows[]={stunned ? "zOz" : " O "," ^ ","/_\\"};
            if(kind==PatrolKind::Scout) { rows[0]=stunned ? "zoz":" o "; rows[1]="/!\\"; rows[2]=" V "; }
            if(kind==PatrolKind::Sentry) { rows[0]=stunned ? "zOz":" O "; rows[1]="[+]"; rows[2]="[_]"; }
            output.SetWidth(3); output.SetHeight(3);
            for(int y=0;y<3;++y) for(int x=0;x<3;++x) output.SetPixelAt(x,y,rows[y][x]);
        }
        int GetWidth() const override { return 3; }
        int GetHeight() const override { return 3; }
    };
}
