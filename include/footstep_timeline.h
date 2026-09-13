#pragma once
#include <array>
#include <cstdint>

namespace mgo2win::combat::footsteps {
struct Input {
    uint64_t epoch{}, scene{}, actor{}, life{};
    uint32_t sourceKey{}, sourceIndex{};
    double seconds{}; // Unwrapped native displayed clip clock, 60 frames/second.
    bool grounded{}, eligible{};
};
struct Event { uint32_t cue{}, bone{}; double seconds{}; }; // bone is original name HASH, never a skeleton index.
struct Events { std::array<Event, 2> values{}; uint32_t count{}; };
struct Policy { double maxAdvanceSeconds; }; // Native backlog suppression, not an original constant.
class Timeline {
public:
    explicit Timeline(Policy policy);
    Events advance(const Input& input) noexcept;
    void reset() noexcept;
private:
    Policy policy_;
    Input previous_{};
    bool initialized_{};
};
// Reviewed snake.mtsq primary foot events only. Source identity includes BOTH key and index.
// First observation, scope/clip change, pause, ineligible interval or clock rewind establishes
// a new baseline without playing old events. Caller must also gate water/unknown surfaces.
// Material variant selection is separate; resolve(... cue ...) currently selects tableId 0.
}
