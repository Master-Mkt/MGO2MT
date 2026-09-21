#pragma once

#include <cmath>
#include <cstdint>
#include <optional>

namespace mgo2mt {

// Native diagnostic of successful Present completions, not simulation ticks or
// physical display scanout. Feed monotonic wall-clock seconds after Present.
class DebugFrameRate {
public:
    static constexpr double SampleWindowSeconds = 0.5;

    void reset() noexcept {
        started_ = false;
        windowStart_ = 0.0;
        lastPresent_ = 0.0;
        intervals_ = 0;
        fps_.reset();
    }

    void present(double nowSeconds) noexcept {
        if (!std::isfinite(nowSeconds)) {
            reset();
            return;
        }
        // A first presentation is an anchor, not an elapsed frame interval.
        // Clock discontinuities also invalidate the previous displayed result.
        if (!started_ || nowSeconds <= lastPresent_) {
            restart(nowSeconds);
            return;
        }
        const double elapsed = nowSeconds - windowStart_;
        if (!std::isfinite(elapsed)) {
            restart(nowSeconds);
            return;
        }
        lastPresent_ = nowSeconds;
        ++intervals_;
        if (elapsed >= SampleWindowSeconds) {
            const double measured = static_cast<double>(intervals_) / elapsed;
            if (std::isfinite(measured) && measured > 0.0) {
                fps_ = measured;
            } else {
                fps_.reset();
            }
            // Retain this completed presentation as the next interval anchor.
            windowStart_ = nowSeconds;
            intervals_ = 0;
        }
    }

    [[nodiscard]] std::optional<double> fps() const noexcept { return fps_; }

private:
    void restart(double nowSeconds) noexcept {
        reset();
        started_ = true;
        windowStart_ = nowSeconds;
        lastPresent_ = nowSeconds;
    }

    bool started_ = false;
    double windowStart_ = 0.0;
    double lastPresent_ = 0.0;
    std::uint64_t intervals_ = 0;
    std::optional<double> fps_;
};

}
