// Generate safe key sequences using the same build and standard library as the game.
#include "TestSupport.h"
#include <iostream>
using namespace MyGame;
using namespace GameTest;
std::string commands;
void Route(Game& g, int x, int y, bool finish = false)
{
    const auto previous = Flood(g.Map(), {g.Player().GetX(), g.Player().GetY()}, true, !finish && g.ExitOpen());
    const int start = g.Player().GetY() * Game::Width + g.Player().GetX(), target = y * Game::Width + x;
    Check(previous[target] != -1, "Destination must be reachable");
    std::vector<int> path;
    for (int cell = target; cell != start; cell = previous[cell]) path.push_back(cell);
    std::reverse(path.begin(), path.end());
    for (int cell : path)
    {
        const int dx = cell % Game::Width - g.Player().GetX(), dy = cell / Game::Width - g.Player().GetY();
        commands += dx == 1 ? 'D' : dx == -1 ? 'A' : dy == 1 ? 'S' : 'W';
        g.Move(dx, dy, true);
        if (finish && g.Status() == Game::State::Cleared) return;
    }
}
int main()
{
    Game g(42);
    for (int stage = 0; stage < 2; ++stage)
    {
        commands.clear();
        const auto coins = g.Coins();
        for (std::size_t i = 0; i < coins.size(); ++i)
            if (!g.Coins()[i].collected) Route(g, coins[i].x - 1, coins[i].y - 1);
        Route(g, g.Exit().GetX(), g.Exit().GetY(), true);
        std::cout << "tutorial" << stage << '\t' << commands << '\n';
        g.NextLevel();
    }
    Game expedition(42, true);
    const Stone body=expedition.Crates()[0].body;
    const auto reachable=Flood(expedition.Map(),expedition.Map().spawn);
    for(Point aim:{Point{1,0},Point{-1,0},Point{0,1},Point{0,-1}})
    {
        const int x=body.GetX()-4*aim.x,y=body.GetY()-4*aim.y;
        if(Blocked(expedition.Map(),x,y) || reachable[y*Game::Width+x]==-1) continue;
        commands.clear(); Route(expedition,x,y);
        commands+=aim.x==1 ? 'D':aim.x==-1 ? 'A':aim.y==1 ? 'S':'W';
        std::cout<<"crate\t"<<commands<<'\n'; break;
    }
    expedition.Reset();
    const auto pickup=std::find_if(expedition.Pickups().begin(),expedition.Pickups().end(),[](const Pickup& p) { return p.power==Power::Speed; });
    commands.clear(); Route(expedition,pickup->x-1,pickup->y-1);
    std::cout<<"boost\t"<<commands<<'\n';
    const int x=expedition.Player().GetX(),y=expedition.Player().GetY();
    for(Point d:{Point{1,0},Point{-1,0},Point{0,1},Point{0,-1}})
        if(!Blocked(expedition.Map(),x+d.x,y+d.y) && !Blocked(expedition.Map(),x+2*d.x,y+2*d.y))
        { std::cout<<"step\t"<<x<<' '<<y<<' '<<d.x<<' '<<d.y<<'\n'; break; }
}
