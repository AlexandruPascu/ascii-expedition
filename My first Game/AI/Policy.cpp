#include "Policy.h"
#include "PolicyData.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace MyGame { namespace AI {
namespace {
template<std::size_t In,std::size_t Out>
std::array<float,Out> Dense(const std::array<float,In>& input,const float (&weights)[In*Out],
                          const float (&bias)[Out],bool activate) {
    std::array<float,Out> result{};
    for(std::size_t row=0;row<Out;++row) {
        double sum=bias[row];
        for(std::size_t column=0;column<In;++column)
            sum+=static_cast<double>(weights[row*In+column])*input[column];
        result[row]=static_cast<float>(activate ? std::tanh(sum):sum);
    }
    return result;
}
}
const char* Name(Kind kind) { return kind==Kind::Cem ? "CEM":"PPO"; }
std::array<double,Actions> Scores(Kind kind,const std::array<float,ObservationSize>& observation) {
    for(float value:observation) if(!std::isfinite(value)) throw std::invalid_argument("Policy observation must be finite");
    std::array<double,Actions> result{};
    if(kind==Kind::Cem) {
        for(int a=0;a<Actions;++a) for(int f=0;f<Features;++f)
            result[a]+=observation[a*Features+f]*Data::CemWeights[f];
    } else {
        const auto first=Dense(observation,Data::Layer0Weights,Data::Layer0Bias,true);
        const auto second=Dense(first,Data::Layer1Weights,Data::Layer1Bias,true);
        const auto logits=Dense(second,Data::Layer2Weights,Data::Layer2Bias,false);
        std::copy(logits.begin(),logits.end(),result.begin());
    }
    return result;
}
int Select(Kind kind,const std::array<float,ObservationSize>& observation) {
    const auto values=Scores(kind,observation);
    return static_cast<int>(std::max_element(values.begin(),values.end())-values.begin());
}
} }
