#pragma once
#include "pcm_wave.h"
#include <array>
#include <cmath>
#include <vector>

namespace mgo2mt {
struct AudioLayerMix {
    float normalGain=1.f, alternateGain=1.f;
    uint32_t fadeInMs=500, fadeOutMs=2000;
};
struct PcmLayerPair {
    uint32_t rate=0, frames=0, loopBegin=0, loopEnd=0;
    std::vector<unsigned char> pcm;
};
// Both stereo pairs share every sample frame, including the effective loop.
// Bound the resulting buffer, not merely each of its input WAVs.
inline PcmLayerPair interleave_audio_layers(std::span<const unsigned char> normal,
                                          std::span<const unsigned char> alternate,
                                          bool loopWhole=false) {
    const auto a=read_pcm_wave(normal), b=read_pcm_wave(alternate);
    const auto aEnd=a.loopEnd?a.loopEnd:(loopWhole?a.dataSize/(a.channels*2):0);
    const auto bEnd=b.loopEnd?b.loopEnd:(loopWhole?b.dataSize/(b.channels*2):0);
    if(a.channels!=2||b.channels!=2||a.rate!=b.rate||a.dataSize!=b.dataSize||
       a.loopBegin!=b.loopBegin||aEnd!=bEnd||uint64_t(a.dataSize)*2>256*1024*1024)
        throw std::runtime_error("Audio layer PCM rate/frames/loop/size mismatch");
    PcmLayerPair result{a.rate,a.dataSize/4,a.loopBegin,aEnd,{}};
    result.pcm.resize(size_t(a.dataSize)*2);
    for(size_t i=0;i<result.frames;++i){
        std::memcpy(result.pcm.data()+i*8,normal.data()+a.dataAt+i*4,4);
        std::memcpy(result.pcm.data()+i*8+4,alternate.data()+b.dataAt+i*4,4);
    }
    return result;
}

// Windows mixer policy: an interrupted transition starts at its current gain,
// then takes the configured in/out duration to reach the new target. Repeating
// a selection never resets that transition. Use a monotonic millisecond clock.
class AudioLayerFader {
    struct Ramp {float from=0,to=0;uint64_t start=0;uint32_t duration=0;};
    AudioLayerMix mix_;
    std::array<Ramp,2> ramps_{};
    bool alternate_=false;
    uint64_t lastTime_=0;
    static float value(const Ramp& r,uint64_t now){
        if(!r.duration||now-r.start>=r.duration)return r.to;
        return r.from+(r.to-r.from)*(float(now-r.start)/r.duration);
    }
public:
    explicit AudioLayerFader(AudioLayerMix mix={},bool alternate=false):mix_(mix),alternate_(alternate){
        if(!std::isfinite(mix.normalGain)||!std::isfinite(mix.alternateGain)||
           mix.normalGain<0||mix.normalGain>1||mix.alternateGain<0||mix.alternateGain>1||
           mix.fadeInMs>600000||mix.fadeOutMs>600000)
            throw std::runtime_error("Audio layer mix range");
        ramps_[0].from=ramps_[0].to=alternate?0:mix.normalGain;
        ramps_[1].from=ramps_[1].to=alternate?mix.alternateGain:0;
    }
    std::array<float,2> update(bool alternate,uint64_t now){
        if(now<lastTime_)now=lastTime_;
        lastTime_=now;
        std::array<float,2> gains{value(ramps_[0],now),value(ramps_[1],now)};
        if(alternate!=alternate_){
            alternate_=alternate;
            const std::array<float,2> targets{alternate?0:mix_.normalGain,alternate?mix_.alternateGain:0};
            for(size_t i=0;i<2;++i){
                ramps_[i]={gains[i],targets[i],now,targets[i]>gains[i]?mix_.fadeInMs:mix_.fadeOutMs};
                gains[i]=value(ramps_[i],now);
            }
        }
        return gains;
    }
};
// XAudio2 uses matrix[source + sourceChannels * destination]. Both source
// stereo pairs go to the same L/R destinations, without speaker-mask remixing.
inline std::array<float,8> audio_layer_matrix(std::array<float,2> gains){
    return {gains[0],0,gains[1],0,0,gains[0],0,gains[1]};
}
}
