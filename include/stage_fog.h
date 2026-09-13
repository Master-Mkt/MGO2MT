#pragma once
#include <array>
#include <cstdint>

namespace mgo2win::stage {
// Literal GCX values accepted by 228BF0/229338/22A580. Distances retain
// original world units. This is a data contract, not a weather renderer.
struct FogScriptValues {
    std::int32_t nearDistance{};
    std::int32_t farDistance{};
    std::array<std::int32_t, 3> rgb{};
    std::array<std::int32_t, 2> limit{};
};
struct FogRenderConstants {
    std::array<std::uint8_t, 3> rgbBytes{};
    // PPC F9738..97E8: reciprocal span, negative near/span, low, high.
    // No assumption here about the shader's distance input or blend equation.
    std::array<float, 4> constant21{};
};
// Rejects malformed/native unsupported bounds with invalid_argument. Negative
// near values are valid (CC sandstorm). No stage is enabled by this function.
FogRenderConstants decode_fog_values(const FogScriptValues& values);
// 229338 frame option: literal frame count multiplied by exact ELF float 1/60.
float fog_transition_seconds(std::int32_t frames);
}
