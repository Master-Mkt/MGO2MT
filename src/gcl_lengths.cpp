#include "mgo2mt/gcl_lengths.hpp"

namespace mgo2mt::gcl {
// Recovered from MGO2 PPU 0x000D6928. See outputs/focused/000D6928.txt.
std::optional<Length> block_length(std::span<const std::uint8_t> bytes) {
    if (bytes.empty()) return std::nullopt;
    const auto tag = static_cast<std::uint32_t>(bytes[0] & 0x0f);
    if (tag <= 12) return Length{tag, 1};
    const std::size_t count = tag - 12;
    if (bytes.size() < count + 1) return std::nullopt;
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < count; ++i)
        value |= static_cast<std::uint32_t>(bytes[i + 1]) << (i * 8);
    return Length{value, count + 1};
}

// Recovered from MGO2 PPU 0x000D69C0; unlike block lengths this is big-endian.
std::optional<Length> command_length(std::span<const std::uint8_t> bytes) {
    if (bytes.empty()) return std::nullopt;
    if ((bytes[0] & 0x80) == 0) return Length{bytes[0], 1};
    if (bytes.size() < 2) return std::nullopt;
    return Length{(static_cast<std::uint32_t>(bytes[0] & 0x7f) << 8) | bytes[1], 2};
}
}
