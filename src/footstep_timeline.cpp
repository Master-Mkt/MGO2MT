#include "footstep_timeline.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mgo2mt::combat::footsteps {
namespace {
struct Clip { double duration; uint32_t cue; std::array<double, 2> times; };
const Clip* clip(const Input& in) noexcept {
    static constexpr Clip walk{1., 8071, {16. / 60., 46. / 60.}};
    static constexpr Clip run{40. / 60., 8007, {18. / 60., 38. / 60.}};
    if (in.sourceKey == 0x4c078d && in.sourceIndex == 9) return &walk;
    if (in.sourceKey == 0x0460c5 && in.sourceIndex == 10) return &run;
    return nullptr;
}
bool same(const Input& a, const Input& b) noexcept {
    return a.epoch == b.epoch && a.scene == b.scene && a.actor == b.actor && a.life == b.life
        && a.sourceKey == b.sourceKey && a.sourceIndex == b.sourceIndex;
}
bool valid(const Input& in) noexcept {
    return in.epoch && in.scene && in.actor && in.life && std::isfinite(in.seconds)
        && in.seconds >= 0. && in.seconds <= 86400.; // Native bounded elapsed clip clock.
}
}
Timeline::Timeline(Policy policy) : policy_(policy) {
    if (!std::isfinite(policy.maxAdvanceSeconds) || policy.maxAdvanceSeconds <= 0.
        || policy.maxAdvanceSeconds > .5) throw std::invalid_argument("footstep advance bound");
}
void Timeline::reset() noexcept { initialized_ = false; previous_ = {}; }
Events Timeline::advance(const Input& in) noexcept {
    Events out;
    const auto* c = clip(in);
    if (!valid(in) || !c) { reset(); return out; }
    const auto before = previous_;
    const bool process = initialized_ && same(before, in) && before.eligible && before.grounded
        && in.eligible && in.grounded && in.seconds > before.seconds
        && in.seconds - before.seconds <= policy_.maxAdvanceSeconds;
    previous_ = in;
    initialized_ = true;
    if (!process) return out;
    // At most two primary events in <=0.5 seconds for these two exact clips.
    for (unsigned i = 0; i < 2; ++i) {
        const double cycle = std::floor((before.seconds - c->times[i]) / c->duration) + 1.;
        const double at = cycle * c->duration + c->times[i];
        if (at > before.seconds && at <= in.seconds && out.count < out.values.size())
            out.values[out.count++] = {c->cue, i == 0 ? 0x5B4A33u : 0xFB4232u, at};
    }
    if (out.count == 2 && out.values[1].seconds < out.values[0].seconds)
        std::swap(out.values[0], out.values[1]);
    return out;
}
}
