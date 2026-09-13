#pragma once
#include <array>
#include <cmath>
#include <span>
#include <stdexcept>

namespace mgo2win {
inline constexpr size_t maximum_dynamic_lights=8;
struct DynamicPointLight {
    std::array<float,3> position{},color{1,1,1};
    float radius=0,intensity=0;
};
// Bounded native rendering contract. Units match the world/model transform.
// This supplies no shadows, occlusion, original emitter placement or lifetime.
inline void validate_dynamic_lights(std::span<const DynamicPointLight> lights){
    if(lights.size()>maximum_dynamic_lights)throw std::invalid_argument("Dynamic light count");
    for(const auto& light:lights){
        for(float value:light.position)if(!std::isfinite(value)||std::abs(value)>=1e7f)throw std::invalid_argument("Dynamic light position");
        for(float value:light.color)if(!std::isfinite(value)||value<0||value>1)throw std::invalid_argument("Dynamic light color");
        if(!std::isfinite(light.radius)||light.radius<1||light.radius>1e6f||
           !std::isfinite(light.intensity)||light.intensity<0||light.intensity>16)
            throw std::invalid_argument("Dynamic light radius/intensity");
    }
}
}
