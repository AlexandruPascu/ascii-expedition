#include "Level.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <queue>
#include <random>
#include <stdexcept>

namespace MyGame
{
    bool Overlaps(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2)
    {
        return x1 < x2 + w2 && x2 < x1 + w1 && y1 < y2 + h2 && y2 < y1 + h1;
    }
    bool Touches(int x, int y, int width, int height, const Stone& stone)
    {
        return Overlaps(x,y,width,height,stone.GetX(),stone.GetY(),stone.GetShape().GetWidth(),stone.GetShape().GetHeight());
    }
    Hazard::Hazard(Point start, Point end, double velocity, double initialPhase, PatrolKind type)
        : body(start.x,start.y,3,3), from(start), to(end), speed(velocity), phase(initialPhase),
          stunnedMillis(0), cycleMillis(0), kind(type) { Update(0); }
    Stone Hazard::DangerBounds() const
    {
        if (kind == PatrolKind::Sentry) return Stone(from.x-2,from.y-2,7,7);
        return Stone(std::min(from.x,to.x),std::min(from.y,to.y),std::abs(from.x-to.x)+3,std::abs(from.y-to.y)+3);
    }
    bool Hazard::Hurts(int x,int y,int width,int height) const
    {
        return !stunnedMillis && Touches(x,y,width,height,Pulsing() ? DangerBounds() : body);
    }
    void Hazard::Update(int millis)
    {
        if (millis < 0) return;
        const int frozen=std::min(millis,stunnedMillis);
        stunnedMillis-=frozen; millis-=frozen;
        cycleMillis=(cycleMillis+millis)%3000;
        const double span=std::abs(to.x-from.x)+std::abs(to.y-from.y);
        if (span<=0) return;
        phase=std::fmod(phase+speed*millis/1000.0,2*span);
        const double offset=phase<=span ? phase : 2*span-phase;
        body.SetX(from.x+static_cast<int>(std::round((to.x-from.x)*offset/span)));
        body.SetY(from.y+static_cast<int>(std::round((to.y-from.y)*offset/span)));
    }
    namespace
    {
        enum { Columns=19, Rows=7, Count=Columns*Rows, Tile=4 };
        using Grid=std::array<bool,Count>;
        Point Anchor(int cell) { return {1+(cell%Columns)*Tile,1+(cell/Columns)*Tile}; }
        int Random(std::mt19937& rng,int lo,int hi) { return std::uniform_int_distribution<int>(lo,hi)(rng); }
        std::vector<int> Neighbors(int cell)
        {
            std::vector<int> out;
            if(cell%Columns>0) out.push_back(cell-1);
            if(cell%Columns+1<Columns) out.push_back(cell+1);
            if(cell>=Columns) out.push_back(cell-Columns);
            if(cell+Columns<Count) out.push_back(cell+Columns);
            return out;
        }
        std::array<int,Count> Distances(const Grid& blocked,int start)
        {
            std::array<int,Count> distances; distances.fill(-1);
            std::queue<int> pending;
            if(!blocked[start]) { distances[start]=0; pending.push(start); }
            while(!pending.empty())
            {
                const int cell=pending.front(); pending.pop();
                for(int next:Neighbors(cell)) if(!blocked[next] && distances[next]<0)
                { distances[next]=distances[cell]+1; pending.push(next); }
            }
            return distances;
        }
        bool Connected(const Grid& blocked)
        {
            int first=0; while(first<Count && blocked[first]) ++first;
            if(first==Count) return false;
            const auto distances=Distances(blocked,first);
            for(int i=0;i<Count;++i) if(!blocked[i] && distances[i]<0) return false;
            return true;
        }
        void CarveRoom(Level& level,Grid& blocked,int x,int y,int width,int height,bool bonus=false)
        {
            level.rooms.push_back({1+x*Tile,1+y*Tile,width*Tile,height*Tile,bonus});
            for(int row=y;row<y+height;++row) for(int col=x;col<x+width;++col) blocked[row*Columns+col]=false;
        }
        void Corridor(Grid& blocked,Point a,Point b,bool horizontalFirst)
        {
            blocked[a.y*Columns+a.x]=false;
            auto horizontal=[&] { while(a.x!=b.x) { a.x+=a.x<b.x ? 1:-1; blocked[a.y*Columns+a.x]=false; } };
            auto vertical=[&] { while(a.y!=b.y) { a.y+=a.y<b.y ? 1:-1; blocked[a.y*Columns+a.x]=false; } };
            if(horizontalFirst) { horizontal(); vertical(); } else { vertical(); horizontal(); }
        }
        void Layout(Level& level,Grid& blocked,Grid& bonus,std::mt19937& rng)
        {
            blocked.fill(true);
            const int theme=Random(rng,0,2);
            if(theme==0)
            {
                level.layoutName="Relay rooms";
                std::vector<Point> centers;
                for(int section=0;section<3;++section)
                {
                    const int width=Random(rng,4,5), height=Random(rng,3,5);
                    const int x=section*7, y=Random(rng,0,Rows-height);
                    CarveRoom(level,blocked,x,y,width,height);
                    centers.push_back({x+width/2,y+height/2});
                }
                Corridor(blocked,centers[0],centers[1],Random(rng,0,1));
                Corridor(blocked,centers[1],centers[2],Random(rng,0,1));
            }
            else if(theme==1)
            {
                level.layoutName="Crossroads";
                const int left=Random(rng,0,1), right=Random(rng,12,13);
                CarveRoom(level,blocked,left,0,6,2); CarveRoom(level,blocked,left,5,6,2);
                CarveRoom(level,blocked,right,0,6,2); CarveRoom(level,blocked,right,5,6,2);
                CarveRoom(level,blocked,8,2,3,3);
                Corridor(blocked,{left+2,0},{left+2,6},true);
                Corridor(blocked,{right+2,0},{right+2,6},true);
                Corridor(blocked,{left+2,3},{right+2,3},true);
                // A second connection makes a loop instead of four forced dead ends.
                Corridor(blocked,{left+2,Random(rng,0,1)},{right+2,1},true);
            }
            else
            {
                level.layoutName="Courtyard";
                CarveRoom(level,blocked,6,1,7,5);
                const int leftY=Random(rng,0,4),rightY=Random(rng,0,4);
                CarveRoom(level,blocked,0,leftY,4,3); CarveRoom(level,blocked,15,rightY,4,3);
                Corridor(blocked,{2,leftY+1},{9,3},Random(rng,0,1));
                Corridor(blocked,{16,rightY+1},{9,3},Random(rng,0,1));
            }
            // Carve an optional stash off existing floor; required objectives stay outside it.
            std::vector<int> pockets;
            for(int y=0;y<Rows-1;++y) for(int x=0;x<Columns-1;++x)
            {
                const int c=y*Columns+x;
                if(!blocked[c] || !blocked[c+1] || !blocked[c+Columns] || !blocked[c+Columns+1]) continue;
                bool adjacent=false;
                for(int p:{c,c+1,c+Columns,c+Columns+1})
                    for(int n:Neighbors(p)) adjacent|=!blocked[n];
                if(adjacent) pockets.push_back(c);
            }
            if(!pockets.empty())
            {
                const int c=pockets[Random(rng,0,static_cast<int>(pockets.size())-1)];
                CarveRoom(level,blocked,c%Columns,c/Columns,2,2,true);
                for(int p:{c,c+1,c+Columns,c+Columns+1}) bonus[p]=true;
            }
        }
        bool Candidate(Level& level,std::mt19937& rng,int stage,Difficulty difficulty)
        {
            Grid blocked{},bonus{};
            Layout(level,blocked,bonus,rng);
            if(!Connected(blocked)) return false;
            // Merge wall tiles horizontally; the renderer fills their full collision footprint.
            for(int y=0;y<Rows;++y) for(int x=0;x<Columns;)
            {
                if(!blocked[y*Columns+x]) { ++x; continue; }
                const int start=x;
                while(x<Columns && blocked[y*Columns+x]) ++x;
                level.walls.emplace_back(1+start*Tile,1+y*Tile,(x-start)*Tile,Tile);
            }
            const int progress=std::max(0,stage-Level::TutorialStages);
            const Rules rules=RulesFor(difficulty);
            std::vector<PatrolKind> kinds;
            if(stage==1) kinds={PatrolKind::Guard,PatrolKind::Guard};
            if(stage>=Level::TutorialStages)
            {
                kinds={PatrolKind::Sentry,PatrolKind::Guard,PatrolKind::Scout};
                for(int i=0;i<std::min(progress/3,2)+rules.extraPatrols;++i) kinds.push_back(i%2 ? PatrolKind::Guard:PatrolKind::Scout);
            }
            for(PatrolKind kind:kinds)
            {
                bool placed=false;
                for(int tries=0;tries<600 && !placed;++tries)
                {
                    const bool vertical=Random(rng,0,1)!=0;
                    const int length=kind==PatrolKind::Sentry ? 2:Random(rng,2,3);
                    const int x=Random(rng,0,Columns-(kind==PatrolKind::Sentry ? 2:vertical ? 1:length));
                    const int y=Random(rng,0,Rows-(kind==PatrolKind::Sentry ? 2:vertical ? length:1));
                    std::vector<int> lane;
                    if(kind==PatrolKind::Sentry) lane={y*Columns+x,y*Columns+x+1,(y+1)*Columns+x,(y+1)*Columns+x+1};
                    else for(int i=0;i<length;++i) lane.push_back((y+(vertical ? i:0))*Columns+x+(vertical ? 0:i));
                    if(std::any_of(lane.begin(),lane.end(),[&](int c) { return blocked[c] || bonus[c]; })) continue;
                    for(int c:lane) blocked[c]=true;
                    if(!Connected(blocked)) { for(int c:lane) blocked[c]=false; continue; }
                    Point from=Anchor(lane.front()),to=Anchor(lane.back());
                    double speed=0,phase=0;
                    if(kind==PatrolKind::Sentry) { from.x+=2; from.y+=2; to=from; }
                    else
                    {
                        speed=(kind==PatrolKind::Scout ? 9.0:4.0)+std::min(progress,6)*0.3;
                        speed*=rules.patrolSpeed;
                        phase=Random(rng,0,2*(length-1)*Tile-1);
                    }
                    level.hazards.emplace_back(from,to,speed,phase,kind);
                    placed=true;
                }
                if(!placed) return false;
            }
            std::vector<int> crateCells;
            const int crateCount=stage<Level::TutorialStages ? 0:Random(rng,4,6)+std::min(progress,2);
            for(int tries=0;tries<1000 && static_cast<int>(crateCells.size())<crateCount;++tries)
            {
                const int c=Random(rng,0,Count-1);
                if(blocked[c] || (tries<200 && !bonus[c])) continue;
                blocked[c]=true; crateCells.push_back(c);
                const bool accessible=std::all_of(crateCells.begin(),crateCells.end(),[&](int crate) {
                    const auto adjacent=Neighbors(crate);
                    return std::any_of(adjacent.begin(),adjacent.end(),[&](int n) { return !blocked[n]; });
                });
                if(!accessible || !Connected(blocked)) { blocked[c]=false; crateCells.pop_back(); continue; }
                const Point p=Anchor(c);
                level.crates.push_back({Stone(p.x,p.y,3,3),2,Random(rng,1,2)*75,static_cast<Power>(Random(rng,0,4))});
            }
            if(static_cast<int>(crateCells.size())!=crateCount) return false;
            if(crateCount && !std::any_of(crateCells.begin(),crateCells.end(),[&](int c) { return bonus[c]; })) return false;
            std::vector<int> available;
            for(int c=0;c<Count;++c) if(!blocked[c] && !bonus[c]) available.push_back(c);
            const int coinCount=stage==0 ? 3:stage==1 ? 4:6+std::min(progress/2,3);
            if(static_cast<int>(available.size())<coinCount+8) return false;
            std::shuffle(available.begin(),available.end(),rng);
            const int spawn=available.back(); available.pop_back(); level.spawn=Anchor(spawn);
            const auto distances=Distances(blocked,spawn);
            const auto furthest=std::max_element(available.begin(),available.end(),[&](int a,int b) { return distances[a]<distances[b]; });
            const Point gate=Anchor(*furthest); level.exit=Stone(gate.x,gate.y,3,3); available.erase(furthest);
            for(int i=0;i<coinCount;++i)
            {
                const Point p=Anchor(available.back()); available.pop_back(); level.coins.push_back({p.x+1,p.y+1,false});
            }
            if(stage>=Level::TutorialStages)
                for(Power power:{Power::Shield,Power::Heart,Power::Time,Power::Speed})
                {
                    const Point p=Anchor(available.back()); available.pop_back(); level.pickups.push_back({p.x+1,p.y+1,power,false});
                }
            return true;
        }
    }
    Level GenerateLevel(std::uint32_t seed,int stage,Difficulty difficulty)
    {
        if(stage<0) throw std::invalid_argument("Stage cannot be negative.");
        for(unsigned attempt=0;attempt<128;++attempt)
        {
            std::seed_seq sequence{seed,static_cast<std::uint32_t>(stage),static_cast<std::uint32_t>(difficulty),attempt,0xEA2u};
            std::mt19937 rng(sequence);
            Level level;
            if(Candidate(level,rng,stage,difficulty)) return level;
        }
        throw std::runtime_error("Could not generate a connected map for this seed.");
    }
}
