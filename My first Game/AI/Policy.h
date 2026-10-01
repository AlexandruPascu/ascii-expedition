#pragma once
#include "Agent.h"

namespace MyGame { namespace AI {
enum class Kind { Cem, Ppo };
const char* Name(Kind kind);
std::array<double,Actions> Scores(Kind kind,const std::array<float,ObservationSize>& observation);
int Select(Kind kind,const std::array<float,ObservationSize>& observation);
} }
