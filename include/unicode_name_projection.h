#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace mgo2mt::unicode_name_projection {
inline constexpr std::size_t field_bytes = 16;
enum class Termination { nul_required, full_field_allowed };
struct Projection {
    std::string namePrefix;       // UTF-8 prefix of the full name, without marker.
    std::string displayPrefix;    // Complete marker followed by namePrefix; no NUL.
    std::array<std::uint8_t, field_bytes> field{}; // Exact legacy field, zero padded.
    bool truncated = false;
    bool hasTerminator = false;   // True iff a NUL exists inside field[0..15].
};
namespace detail {
// Validate the ENTIRE input even when only a short prefix fits. Reject embedded
// NUL, overlong encodings, surrogates, out-of-range scalars and incomplete tails.
// This is encoding validation, not the character-creation name policy.
inline std::optional<std::size_t> prefixBoundary(std::string_view text, std::size_t budget) {
    std::size_t end = 0;
    for (std::size_t at = 0; at < text.size();) {
        std::uint32_t cp = static_cast<std::uint8_t>(text[at++]);
        unsigned extra = 0; std::uint32_t minimum = 0;
        if (cp == 0) return std::nullopt;
        if (cp < 0x80) {}
        else if (cp >= 0xc2 && cp <= 0xdf) { cp &= 0x1f; extra = 1; minimum = 0x80; }
        else if (cp >= 0xe0 && cp <= 0xef) { cp &= 0x0f; extra = 2; minimum = 0x800; }
        else if (cp >= 0xf0 && cp <= 0xf4) { cp &= 7; extra = 3; minimum = 0x10000; }
        else return std::nullopt;
        if (extra > text.size() - at) return std::nullopt;
        while (extra--) {
            const auto next = static_cast<std::uint8_t>(text[at++]);
            if ((next & 0xc0) != 0x80) return std::nullopt;
            cp = (cp << 6) | (next & 0x3f);
        }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return std::nullopt;
        if (at <= budget) end = at;
    }
    return end;
}
} // namespace detail
// Display projection only: never use this result as a database name, account
// identifier, uniqueness key or native full-name field. No normalization occurs.
// Both termination and marker are mandatory explicit caller decisions. A '*'
// already in the full name is literal text; it is never inferred to be metadata.
inline std::optional<Projection> project(std::string_view fullNameUtf8,
                                       Termination termination,
                                       std::string_view markerPrefixUtf8) {
    if (termination != Termination::nul_required && termination != Termination::full_field_allowed)
        return std::nullopt;
    const std::size_t budget = field_bytes - (termination == Termination::nul_required ? 1 : 0);
    if (markerPrefixUtf8.size() > budget || !detail::prefixBoundary(markerPrefixUtf8, budget))
        return std::nullopt; // A marker is never silently shortened.
    const auto boundary = detail::prefixBoundary(fullNameUtf8, budget - markerPrefixUtf8.size());
    if (!boundary) return std::nullopt;
    Projection result;
    result.namePrefix.assign(fullNameUtf8.substr(0, *boundary));
    result.displayPrefix.assign(markerPrefixUtf8);
    result.displayPrefix += result.namePrefix;
    for (std::size_t i = 0; i < result.displayPrefix.size(); ++i)
        result.field[i] = static_cast<std::uint8_t>(result.displayPrefix[i]);
    result.truncated = *boundary < fullNameUtf8.size();
    result.hasTerminator = result.displayPrefix.size() < field_bytes;
    return result;
}
} // namespace mgo2mt::unicode_name_projection
