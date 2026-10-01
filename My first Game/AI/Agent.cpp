#include "Agent.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace MyGame { namespace AI {
namespace {
const int Cells=Game::Width*Game::Height;
const Point Directions[]={{1,0},{-1,0},{0,1},{0,-1}};
int Cell(Point p) { return p.y*Game::Width+p.x; }
Point Position(int c) { return {c%Game::Width,c/Game::Width}; }
int Manhattan(Point a,Point b) { return std::abs(a.x-b.x)+std::abs(a.y-b.y); }
struct Routes {
    std::array<int,Cells> previous, distance;
    std::array<bool,Cells> blocked{},danger{};
    explicit Routes(const Game& g) {
        previous.fill(-1); distance.fill(-1);
        // Expand obstacles by the player's 3x3 collision footprint once per decision.
        auto block=[&](const Stone& s,std::array<bool,Cells>& cells) {
            for(int y=std::max(0,s.GetY()-2);y<std::min<int>(Game::Height,s.GetY()+s.GetShape().GetHeight());++y)
                for(int x=std::max(0,s.GetX()-2);x<std::min<int>(Game::Width,s.GetX()+s.GetShape().GetWidth());++x)
                    cells[y*Game::Width+x]=true;
        };
        for(const auto& w:g.Walls()) block(w,blocked);
        for(const auto& c:g.Crates()) if(c.health>0) block(c.body,blocked);
        for(const auto& h:g.Hazards()) block(h.DangerBounds(),danger);
        const int start=Cell({g.Player().GetX(),g.Player().GetY()});
        std::array<int,Cells> queue{};
        int front=0,back=0,origin=start;
        previous[start]=start; distance[start]=0;
        if(danger[start]) {
            std::array<int,Cells> escape;
            escape.fill(-1); escape[start]=start; queue[back++]=start;
            while(front<back) {
                const int c=queue[front++]; const Point p=Position(c);
                if(!danger[c]) { origin=c; break; }
                for(Point d:Directions) {
                    const Point q{p.x+d.x,p.y+d.y};
                    if(q.x<1 || q.y<1 || q.x>Game::Width-4 || q.y>Game::Height-4) continue;
                    const int n=Cell(q);
                    if(blocked[n] || escape[n]!=-1) continue;
                    escape[n]=c; queue[back++]=n;
                }
            }
            if(origin==start) throw std::runtime_error("No route out of this patrol area");
            std::vector<int> prefix;
            for(int c=origin;c!=start;c=escape[c]) prefix.push_back(c);
            std::reverse(prefix.begin(),prefix.end());
            for(int c:prefix) { previous[c]=escape[c]; distance[c]=distance[escape[c]]+1; }
        }
        front=back=0; queue[back++]=origin;
        while(front<back) {
            const int c=queue[front++]; const Point p=Position(c);
            for(Point d:Directions) {
                const Point q{p.x+d.x,p.y+d.y};
                if(q.x<1 || q.y<1 || q.x>Game::Width-4 || q.y>Game::Height-4) continue;
                const int n=Cell(q);
                if(blocked[n] || danger[n] || previous[n]!=-1) continue;
                previous[n]=c; distance[n]=distance[c]+1; queue[back++]=n;
            }
        }
    }
    int Distance(Point p) const {
        if(p.x<0 || p.y<0 || p.x>=Game::Width || p.y>=Game::Height) return -1;
        return danger[Cell(p)] ? -1 : distance[Cell(p)];
    }
    std::vector<Point> Path(Point p) const {
        std::vector<Point> result;
        for(int c=Cell(p);previous[c]!=c;c=previous[c]) result.push_back(Position(c));
        std::reverse(result.begin(),result.end()); return result;
    }
};
}
bool Controller::Reached(const Plan& p) const {
    switch(p.kind) {
        case Plan::Star: return game.Coins()[static_cast<std::size_t>(p.index)].collected;
        case Plan::Box: return game.Crates()[static_cast<std::size_t>(p.index)].health<=0;
        case Plan::Exit: return game.Status()==Game::State::Cleared;
        default: return game.Pickups()[static_cast<std::size_t>(p.index)].collected;
    }
}
void Controller::Prepare() {
    observation.fill(0);
    if(game.Status()!=Game::State::Playing) { ready=true; return; }
    const Routes routes(game);
    std::vector<Plan> options;
    auto add=[&](Plan::Kind kind,int index,Point object,int width,int height) {
        Plan p; p.kind=kind; p.index=index;
        int best=std::numeric_limits<int>::max();
        auto consider=[&](Point q,Point aim) {
            const int distance=routes.Distance(q);
            if(distance>=0 && distance<best) { best=distance; p.target=q; p.aim=aim; }
        };
        if(kind==Plan::Box) {
            for(Point d:Directions) consider({object.x-3*d.x,object.y-3*d.y},d);
        } else {
            for(int y=object.y-2;y<object.y+height;++y)
                for(int x=object.x-2;x<object.x+width;++x) consider({x,y},{0,0});
        }
        if(best==std::numeric_limits<int>::max()) return;
        p.path=routes.Path(p.target);
        p.features[0]=static_cast<float>(best)/100.0f;
        p.features[1]=kind==Plan::Star ? 1.0f:0.0f;
        // Crate drops and hidden reward amounts are deliberately not observations.
        p.features[2]=kind==Plan::Box ? 1.0f:0.0f;
        p.features[3]=kind==Plan::Heart ? (game.Lives()<Game::MaxLives ? 1.0f:0.4f):0.0f;
        p.features[4]=kind==Plan::Time ? 1.0f:0.0f;
        p.features[5]=kind==Plan::Speed ? (game.SpeedBoosted() ? 0.2f:1.0f):0.0f;
        p.features[6]=kind==Plan::Shield ? 1.0f:0.0f;
        p.features[7]=kind==Plan::Exit ? 1.0f:0.0f;
        for(const auto& c:game.Coins()) if(!c.collected && Manhattan(p.target,{c.x,c.y})<=16) p.features[8]+=1.0f/6.0f;
        p.features[9]=static_cast<float>(Manhattan(p.target,{game.Exit().GetX(),game.Exit().GetY()}))/100.0f;
        p.features[10]=game.Timed() ? static_cast<float>(best)/(10.0f*static_cast<float>(std::max(1,game.SecondsLeft()))) : 0.0f;
        p.features[11]=static_cast<float>(game.Coins().size()-game.Collected())/6.0f;
        options.push_back(p);
    };
    for(std::size_t i=0;i<game.Coins().size();++i) {
        const auto& c=game.Coins()[i]; if(!c.collected) add(Plan::Star,static_cast<int>(i),{c.x,c.y},1,1);
    }
    for(std::size_t i=0;i<game.Crates().size();++i) {
        const auto& c=game.Crates()[i]; if(c.health>0) add(Plan::Box,static_cast<int>(i),{c.body.GetX(),c.body.GetY()},3,3);
    }
    for(std::size_t i=0;i<game.Pickups().size();++i) {
        const auto& p=game.Pickups()[i]; if(p.collected) continue;
        Plan::Kind kind=Plan::Shield;
        if(p.power==Power::Heart) kind=Plan::Heart;
        else if(p.power==Power::Time) kind=Plan::Time;
        else if(p.power==Power::Speed) kind=Plan::Speed;
        add(kind,static_cast<int>(i),{p.x,p.y},1,1);
    }
    if(game.ExitOpen()) add(Plan::Exit,0,{game.Exit().GetX(),game.Exit().GetY()},3,3);
    if(options.empty()) throw std::runtime_error("No reachable AI objective");
    const Plan* nearest=nullptr; const Plan* cluster=nullptr; const Plan* furthest=nullptr;
    for(const Plan& p:options) if(p.kind==Plan::Star) {
        if(!nearest || p.path.size()<nearest->path.size()) nearest=&p;
        if(!furthest || p.path.size()>furthest->path.size()) furthest=&p;
        if(!cluster || p.features[8]/(1.0f+p.features[0]*5)>cluster->features[8]/(1.0f+cluster->features[0]*5)) cluster=&p;
    }
    const Plan* fallback=nearest;
    if(!fallback) for(const Plan& p:options) if(p.kind==Plan::Exit) fallback=&p;
    if(!fallback) fallback=&options.front();
    plans.fill(*fallback);
    if(nearest) { plans[0]=*nearest; plans[1]=*cluster; plans[2]=*furthest; }
    const Plan::Kind kinds[]={Plan::Box,Plan::Heart,Plan::Time,Plan::Speed,Plan::Shield,Plan::Exit};
    for(int action=3;action<Actions;++action) {
        const Plan* best=nullptr;
        for(const Plan& p:options) if(p.kind==kinds[action-3] && (!best || p.path.size()<best->path.size())) best=&p;
        if(best) plans[action]=*best;
    }
    for(int a=0;a<Actions;++a) for(int f=0;f<Features;++f) observation[a*Features+f]=plans[a].features[f];
    ready=true;
}
const std::array<float,ObservationSize>& Controller::Observe() { if(!ready) Prepare(); return observation; }
void Controller::Begin(int action) {
    if(action<0 || action>=Actions) throw std::invalid_argument("AI action must be between 0 and 8");
    if(active) throw std::logic_error("Finish the active skill before choosing another");
    if(game.Status()!=Game::State::Playing) throw std::logic_error("The game must be playing before choosing a skill");
    Observe(); current=plans[action]; pathIndex=0; skillTicks=0; active=true; ready=false; ++decisions;
}
bool Controller::Tick() {
    if(!active) return false;
    if(game.Status()==Game::State::Paused) return true;
    if(game.Status()!=Game::State::Playing) { active=false; ready=false; return false; }
    game.MarkAssisted();
    const int hits=game.Stats().hits;
    if(pathIndex<current.path.size() && !Reached(current)) {
        Point p{game.Player().GetX(),game.Player().GetY()},next=current.path[pathIndex];
        const Point direction{next.x-p.x,next.y-p.y};
        bool doubleStep=false;
        if(game.SpeedBoosted() && pathIndex+1<current.path.size()) {
            const Point after=current.path[pathIndex+1];
            doubleStep=after.x-next.x==direction.x && after.y-next.y==direction.y;
        }
        game.Move(direction.x,direction.y,!doubleStep);
        pathIndex+=doubleStep ? 2:1;
    } else if(current.kind==Plan::Box && !Reached(current)) {
        // A blocked move against the crate aims the normal blaster without teleporting.
        game.Move(current.aim.x,current.aim.y,true);
        game.Fire();
    }
    game.Update(TickMillis); ++skillTicks;
    active=game.Status()==Game::State::Playing && !Reached(current) && game.Stats().hits==hits && skillTicks<1600;
    if(!active) ready=false;
    return active;
}
void Controller::Reset() {
    ready=active=false; pathIndex=0; decisions=skillTicks=0;
    current=Plan{}; observation.fill(0);
}
void Episode::Begin(int action) {
    if(Done()) throw std::logic_error("Reset after an episode ends");
    controller.Begin(action);
}
void Episode::Step(int action) { Begin(action); while(Tick()) {} }
void Episode::NextLevel() {
    if(game.Status()!=Game::State::Cleared) throw std::logic_error("Clear the current stage first");
    game.NextLevel(); controller.Reset();
}
std::string Episode::Render() const {
    std::vector<std::string> rows(Game::Height,std::string(Game::Width,' '));
    auto put=[&](int x,int y,char c) { if(x>=0 && y>=0 && x<Game::Width && y<Game::Height) rows[y][x]=c; };
    auto text=[&](int x,int y,const std::string& s) { for(std::size_t i=0;i<s.size();++i) put(x+static_cast<int>(i),y,s[i]); };
    for(int x=0;x<Game::Width;++x) { put(x,0,'#'); put(x,Game::Height-1,'#'); }
    for(int y=0;y<Game::Height;++y) { put(0,y,'#'); put(Game::Width-1,y,'#'); }
    for(const auto& w:game.Walls()) for(int y=0;y<w.GetShape().GetHeight();++y) for(int x=0;x<w.GetShape().GetWidth();++x) put(w.GetX()+x,w.GetY()+y,'#');
    for(const auto& c:game.Crates()) if(c.health>0) {
        text(c.body.GetX(),c.body.GetY(),"+-+"); text(c.body.GetX(),c.body.GetY()+1,c.health==2 ? "|C|":"|c|"); text(c.body.GetX(),c.body.GetY()+2,"+-+");
    }
    for(const auto& h:game.Hazards()) {
        if(h.Charging() || h.Pulsing()) {
            const auto area=h.DangerBounds();
            for(int y=0;y<area.GetShape().GetHeight();++y) for(int x=0;x<area.GetShape().GetWidth();++x) put(area.GetX()+x,area.GetY()+y,h.Pulsing() ? '!':'.');
        }
        const int x=h.body.GetX(),y=h.body.GetY();
        text(x,y," O "); text(x,y+1,h.kind==PatrolKind::Sentry ? "[+]":h.kind==PatrolKind::Scout ? "/!\\":" ^ "); text(x,y+2,h.kind==PatrolKind::Sentry ? "[_]":h.kind==PatrolKind::Scout ? " V ":"/_\\");
    }
    for(const auto& c:game.Coins()) if(!c.collected) put(c.x,c.y,'*');
    for(const auto& p:game.Pickups()) if(!p.collected) put(p.x,p.y,p.power==Power::Heart ? '+':p.power==Power::Time ? 'T':p.power==Power::Speed ? 'B':'S');
    text(game.Exit().GetX(),game.Exit().GetY(),game.ExitOpen() ? "GO!":"[-]");
    for(Point p:game.Beam()) put(p.x,p.y,'.');
    const int x=game.Player().GetX(),y=game.Player().GetY();
    text(x,y," @ "); text(x,y+1,std::string("/")+game.Aim()+"\\"); text(x,y+2,"/ \\");
    std::ostringstream out;
    for(const auto& row:rows) out<<row<<'\n';
    out<<"AI expedition | seed "<<game.Seed()<<" | score "<<game.Score()<<" | lives "<<game.Lives()<<" | stars "<<game.Collected()<<"/"<<game.Coins().size()<<" | time "<<game.SecondsLeft()<<"s\n";
    out<<"Crates "<<game.Stats().crates<<" | hits "<<game.Stats().hits<<" | simulated "<<static_cast<double>(game.Stats().elapsedMillis)/1000.0<<"s | "<<(game.Status()==Game::State::Playing ? "Playing":game.EndReason())<<'\n';
    return out.str();
}
} }
