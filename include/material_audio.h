#pragma once
#include <cstdint>
#include <optional>
namespace mgo2mt::combat::material_audio {
enum class Stage : uint8_t { unknown, n022a, n001a, n004a, n023a };
Stage stage_for_map(uint8_t map) noexcept;
// Verified GCX conversion, first value selected by current 0x77028.
// nullopt means unreviewed material/base/stage: native fail-closed, not original fallback.
// Value 0 means explicitly silent in the original table. Never play cue zero.
// Caller must establish the original base cue and whether an impact/step occurred.
// Material is 44-byte material record word0, not its index or effect callback word5.
std::optional<uint32_t> resolve(Stage stage, uint32_t baseCue, uint32_t materialHash) noexcept;
std::optional<uint32_t> bullet_cue(Stage stage, uint32_t materialHash) noexcept;
}
