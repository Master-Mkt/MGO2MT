#include "debug_frame_rate.h"

#include <cmath>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

void requireFps(const mgo2mt::DebugFrameRate& meter, double expected,
                std::string_view message) {
    require(meter.fps().has_value(), "FPS sample missing");
    require(std::abs(*meter.fps() - expected) < 0.000001, message);
}

void firstFrameAndRegularRates() {
    mgo2mt::DebugFrameRate meter;
    require(!meter.fps(), "Fresh meter must be unavailable");
    meter.present(1000.0);
    require(!meter.fps(), "First completion must only start measurement");
    for (int i = 1; i < 30; ++i) meter.present(1000.0 + i / 60.0);
    require(!meter.fps(), "Do not publish a partial half-second sample");
    meter.present(1000.5);
    requireFps(meter, 60.0, "Thirty intervals, not thirty-one frames, are 60 FPS");
    for (int i = 31; i <= 60; ++i) meter.present(1000.0 + i / 60.0);
    requireFps(meter, 60.0, "Adjacent windows must share the boundary anchor");

    meter.reset();
    meter.present(2000.0);
    for (int i = 1; i <= 72; ++i) meter.present(2000.0 + i / 144.0);
    requireFps(meter, 144.0, "144Hz must not be capped to simulation frequency");
}

void variableIntervalsAndWallClockStalls() {
    mgo2mt::DebugFrameRate meter;
    for (const double now : {0.0, 0.1, 0.15, 0.35, 0.5}) meter.present(now);
    requireFps(meter, 8.0, "Use interval count/elapsed, not mean reciprocal dt");
    meter.present(0.65);
    requireFps(meter, 8.0, "Keep last complete sample during next window");
    meter.present(1.1);
    requireFps(meter, 2.0 / 0.6, "Use actual elapsed window, not fixed 0.5 denominator");

    meter.reset();
    meter.present(10.0);
    meter.present(12.0);
    requireFps(meter, 0.5, "A two-second successful-Present gap must not be clamped");
    meter.reset(); // Caller handles minimized, occluded and recreated surfaces.
    require(!meter.fps(), "Reset must discard previous visible FPS");
    meter.present(100.0);
    require(!meter.fps(), "Resume starts a new anchor without counting hidden time");
    meter.present(100.5);
    requireFps(meter, 2.0, "New post-reset interval must be counted once");
}

void invalidAndNonMonotonicTimes() {
    mgo2mt::DebugFrameRate meter;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double invalid : {nan, inf, -inf}) {
        meter.present(0.0);
        meter.present(0.5);
        requireFps(meter, 2.0, "Prepare a visible valid sample");
        meter.present(invalid);
        require(!meter.fps(), "Non-finite clock must invalidate stale FPS");
        meter.present(1.0);
        require(!meter.fps(), "A valid sample after invalid clock is an anchor");
        meter.reset();
    }

    meter.present(4.0);
    meter.present(4.5);
    requireFps(meter, 2.0, "Prepare duplicate-clock case");
    meter.present(4.5);
    require(!meter.fps(), "Duplicate timestamp must not fabricate infinite FPS");
    meter.present(5.0);
    requireFps(meter, 2.0, "Duplicate finite timestamp becomes a new anchor");
    meter.present(3.0);
    require(!meter.fps(), "Clock reversal must discard old FPS and interval count");
    meter.present(3.5);
    requireFps(meter, 2.0, "Clock reversal must recover from its new anchor");

    meter.reset();
    meter.present(-2.0);
    meter.present(-1.5);
    requireFps(meter, 2.0, "Finite monotonic times do not require a positive epoch");
    meter.present(std::numeric_limits<double>::max());
    meter.present(-std::numeric_limits<double>::max());
    meter.present(std::numeric_limits<double>::max());
    require(!meter.fps(), "Overflowing elapsed time must not publish NaN or infinity");
}
}

int main() {
    try {
        firstFrameAndRegularRates();
        variableIntervalsAndWallClockStalls();
        invalidAndNonMonotonicTimes();
        std::cout << "Present FPS: first frame, 60/144 Hz, variable intervals, stalls, reset and invalid clocks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
