#include "stage_fog.h"
#include <bit>
#include <cmath>
#include <stdexcept>

namespace mgo2mt::stage {
FogRenderConstants decode_fog_values(const FogScriptValues& v) {
    // Deliberate native bounds, not claimed original engine validation.
    if (v.nearDistance < -10000000 || v.nearDistance > 10000000 ||
        v.farDistance < -10000000 || v.farDistance > 10000000 ||
        v.farDistance <= v.nearDistance || v.limit[0] < 0 ||
        v.limit[1] > 1000 || v.limit[0] > v.limit[1])
        throw std::invalid_argument("unsupported fog distance/limit");
    FogRenderConstants out;
    constexpr float rgbScale = std::bit_cast<float>(0x3e828f5du);
    constexpr float limitScale = std::bit_cast<float>(0x3a83126fu);
    for (std::size_t i = 0; i < v.rgb.size(); ++i) {
        if (v.rgb[i] < 0 || v.rgb[i] > 1000)
            throw std::invalid_argument("unsupported fog RGB");
        // Original uses integer->float, fmuls, then fctiwz/byte extraction.
        out.rgbBytes[i] = static_cast<std::uint8_t>(
            static_cast<std::int32_t>(static_cast<float>(v.rgb[i]) * rgbScale));
    }
    const float nearValue = static_cast<float>(v.nearDistance);
    const float span = static_cast<float>(v.farDistance) - nearValue;
    const float reciprocal = 1.0f / span;
    // Standard float division replaces PPC fres + two refinements; not a
    // promise of bit-identical RSX constants on every input.
    out.constant21 = {reciprocal, -nearValue * reciprocal,
        static_cast<float>(v.limit[0]) * limitScale,
        static_cast<float>(v.limit[1]) * limitScale};
    return out;
}
float fog_transition_seconds(std::int32_t frames) {
    if (frames < 0 || frames > 360000)
        throw std::invalid_argument("unsupported fog transition duration");
    return static_cast<float>(frames) * std::bit_cast<float>(0x3c888889u);
}
}
