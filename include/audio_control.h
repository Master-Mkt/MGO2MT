#pragma once
#include <atomic>
namespace mgo2win {
struct AudioControl {std::atomic<float> gain{1.f},frequencyRatio{1.f}; unsigned cue=0; const char* stream="other";bool loopWhole=false;};
}
