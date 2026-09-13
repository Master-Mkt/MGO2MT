#pragma once
#include <atomic>
#include <filesystem>
#include "audio_layers.h"
namespace mgo2win {
struct AudioControl {
    std::atomic<float> gain{1.f},frequencyRatio{1.f};
    unsigned cue=0; const char* stream="other";bool loopWhole=false;
    // Configure these before starting the audio thread; only alternate changes
    // during playback. An absent or invalid second layer keeps normal playback.
    std::filesystem::path alternateWave;
    AudioLayerMix layerMix;
    std::atomic_bool alternate{false};
};
}
