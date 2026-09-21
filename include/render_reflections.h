#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <iosfwd>

namespace mgo2mt::render_reflections {
// Native Windows material settings. These are not recovered original RSX values.
struct Rule {
 uint32_t shader=0;
 float reflectivity=0,roughness=1;
 bool operator==(const Rule&) const = default;
};
struct Profile {
 std::array<Rule,32> rules{};
 unsigned count=0;
 bool operator==(const Profile&) const = default;
};
Profile defaults() noexcept;
bool valid(const Profile&) noexcept;
// Strict version 1 JSON, at most 64 KiB. Invalid content throws; no partial result.
Profile read(std::istream&);
// A missing optional file uses defaults. An existing invalid/unreadable file throws.
Profile load(const std::filesystem::path&);
// Configure only validated profiles. Unmatched shaders are nonreflective.
std::array<float,2> material(const Profile&,uint32_t shader) noexcept;
}
