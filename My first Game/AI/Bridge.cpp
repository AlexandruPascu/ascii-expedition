#include "Agent.h"
#include <algorithm>
#include <exception>
#include <string>
#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
namespace { thread_local std::string error, frame; }
// Every throwing entry point catches C++ exceptions before crossing the C ABI.
extern "C" {
EXPORT int ae_version() { return 1; }
EXPORT const char* ae_error() { return error.c_str(); }
EXPORT void* ae_create(unsigned int seed) {
    try { error.clear(); return new MyGame::AI::Episode(seed); }
    catch(const std::exception& e) { error=e.what(); return nullptr; }
}
EXPORT void ae_destroy(void* p) { delete static_cast<MyGame::AI::Episode*>(p); }
EXPORT int ae_observe(void* p,float* output) {
    try { const auto& obs=static_cast<MyGame::AI::Episode*>(p)->Observe(); std::copy(obs.begin(),obs.end(),output); return 0; }
    catch(const std::exception& e) { error=e.what(); return -1; }
}
EXPORT int ae_step(void* p,int action) {
    try { static_cast<MyGame::AI::Episode*>(p)->Step(action); return 0; }
    catch(const std::exception& e) { error=e.what(); return -1; }
}
EXPORT int ae_begin(void* p,int action) {
    try { static_cast<MyGame::AI::Episode*>(p)->Begin(action); return 0; }
    catch(const std::exception& e) { error=e.what(); return -1; }
}
EXPORT int ae_tick(void* p) {
    try { return static_cast<MyGame::AI::Episode*>(p)->Tick() ? 1:0; }
    catch(const std::exception& e) { error=e.what(); return -1; }
}
EXPORT void ae_info(void* p,double* output) {
    const auto& e=*static_cast<MyGame::AI::Episode*>(p); const auto& g=e.Rules();
    const double data[]={static_cast<double>(g.Score()),g.Status()==MyGame::Game::State::Cleared ? 1.0:0.0,
        static_cast<double>(g.Stats().elapsedMillis)/1000.0,static_cast<double>(g.Stats().hits),
        static_cast<double>(g.Stats().crates),static_cast<double>(g.Stats().stars),static_cast<double>(g.Lives()),
        e.Done() ? 1.0:0.0,e.Truncated() ? 1.0:0.0,static_cast<double>(e.Decisions())};
    std::copy(data,data+10,output);
}
EXPORT const char* ae_render(void* p) {
    try { frame=static_cast<MyGame::AI::Episode*>(p)->Render(); return frame.c_str(); }
    catch(const std::exception& e) { error=e.what(); return nullptr; }
}
}
