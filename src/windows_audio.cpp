// Windows adapter probe using decoded local game audio, not a PS3 mixer port.
#include <windows.h>
#include <xaudio2.h>
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <syncstream>
#include <stdexcept>
#include <vector>
#include "audio_control.h"
using Microsoft::WRL::ComPtr;
static void check(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("Windows audio HRESULT " + std::to_string(static_cast<unsigned long>(hr))); }
struct Callback final : IXAudio2VoiceCallback {
    std::atomic<unsigned> loops{0}; std::atomic<HRESULT> error{S_OK};
    std::atomic_bool streamEnded{false};
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnStreamEnd() override { streamEnded=true; }
    void STDMETHODCALLTYPE OnBufferStart(void*) override {}
    void STDMETHODCALLTYPE OnBufferEnd(void*) override {}
    void STDMETHODCALLTYPE OnLoopEnd(void*) override { ++loops; }
    void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT hr) override { error = hr; }
};
struct Audio {
    ComPtr<IXAudio2> engine; IXAudio2MasteringVoice* master = nullptr; IXAudio2SourceVoice* source = nullptr;
    ~Audio() { if(source) source->DestroyVoice(); if(master) master->DestroyVoice(); }
};
struct Com { Com(){check(CoInitializeEx(nullptr, COINIT_MULTITHREADED));} ~Com(){CoUninitialize();} };
static uint32_t u32(const std::vector<unsigned char>& b, size_t p) {
    if (p > b.size() || b.size()-p < 4) throw std::runtime_error("Truncated WAV");
    return b[p] | uint32_t(b[p+1])<<8 | uint32_t(b[p+2])<<16 | uint32_t(b[p+3])<<24;
}
int run_audio_probe(int argc, wchar_t** argv, const std::atomic_bool* cancel, const mgo2win::AudioControl* control) {
    try {
        if(argc != 3 && argc != 6) throw std::runtime_error("Usage: mgo2win_audio file.wav seconds [loop_start loop_end play_begin] (sample frames)");
        const double seconds = std::stod(argv[2]);
        if (!(seconds > 0 && seconds <= 600)) throw std::runtime_error("Duration must be 0..600 seconds");
        std::ifstream f(std::filesystem::path(argv[1]), std::ios::binary|std::ios::ate);
        if(!f) throw std::runtime_error("Cannot open WAV");
        const auto size=f.tellg();
        if(size < 12 || size > 256*1024*1024) throw std::runtime_error("Invalid WAV size");
        std::vector<unsigned char> bytes(static_cast<size_t>(size)); f.seekg(0); f.read(reinterpret_cast<char*>(bytes.data()), size);
        if(!f)throw std::runtime_error("Audio read failed");
        bool native=!std::memcmp(bytes.data(),"GWA1",4);
        WAVEFORMATEXTENSIBLE format{}; bool haveFormat=false; size_t dataAt=0; uint32_t dataSize=0;
        uint32_t nativeLoopBegin=0,nativeLoopEnd=0;
        if(native){
            auto qword=[&](size_t p){return uint64_t(u32(bytes,p))|(uint64_t(u32(bytes,p+4))<<32);};
            auto frames=qword(24),begin=qword(32),end=qword(40),pcm=qword(56);
            auto rate=u32(bytes,12),channels=u32(bytes,16),flags=u32(bytes,48);
            if(u32(bytes,4)!=1||u32(bytes,8)!=64||u32(bytes,20)!=16||u32(bytes,52)||flags>1||
               channels<1||channels>8||rate<8000||rate>192000||!frames||frames>UINT32_MAX||
               pcm!=frames*channels*2||pcm!=bytes.size()-64||
               (flags&&!(begin<end&&end<=frames))||(!flags&&(begin||end)))throw std::runtime_error("GWA contract");
            auto& fmt=format.Format;fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=static_cast<WORD>(channels);fmt.nSamplesPerSec=rate;
            fmt.wBitsPerSample=16;fmt.nBlockAlign=static_cast<WORD>(channels*2);fmt.nAvgBytesPerSec=rate*fmt.nBlockAlign;
            dataAt=64;dataSize=static_cast<uint32_t>(pcm);haveFormat=true;nativeLoopBegin=static_cast<uint32_t>(begin);nativeLoopEnd=static_cast<uint32_t>(end);
            if(argc!=3)throw std::runtime_error("GWA uses embedded loop metadata; external loop overrides rejected");
        }else{
        if(std::memcmp(bytes.data(),"RIFF",4) || std::memcmp(bytes.data()+8,"WAVE",4) || uint64_t(u32(bytes,4))+8 != bytes.size()) throw std::runtime_error("Invalid RIFF/WAVE extent");
        for(size_t p=12; p<bytes.size();) {
            if(bytes.size()-p<8) throw std::runtime_error("Truncated chunk header");
            auto n=u32(bytes,p+4); size_t begin=p+8;
            if(n>bytes.size()-begin) throw std::runtime_error("Truncated chunk payload");
            if(!std::memcmp(bytes.data()+p,"fmt ",4)) {
                if(haveFormat || (n!=16 && n!=18 && n!=40)) throw std::runtime_error("Unsupported fmt chunk");
                std::memcpy(&format,bytes.data()+begin,n); haveFormat=true;
            } else if(!std::memcmp(bytes.data()+p,"data",4)) {
                if(dataAt) throw std::runtime_error("Duplicate data chunk");
                dataAt=begin; dataSize=n;
            }
            p=begin+n+(n&1); if(p>bytes.size()) throw std::runtime_error("Missing chunk padding");
        }
        }
        auto& fmt=format.Format;
        const GUID pcm={1,0,0x10,{0x80,0,0,0xaa,0,0x38,0x9b,0x71}};
        if(!haveFormat || !dataAt || !dataSize || fmt.wBitsPerSample!=16 || !fmt.nChannels || fmt.nChannels>8 || !fmt.nSamplesPerSec ||
           fmt.nBlockAlign!=fmt.nChannels*2 || fmt.nAvgBytesPerSec!=fmt.nSamplesPerSec*fmt.nBlockAlign || dataSize%fmt.nBlockAlign ||
           !((fmt.wFormatTag==WAVE_FORMAT_PCM && fmt.cbSize==0) || (fmt.wFormatTag==WAVE_FORMAT_EXTENSIBLE && fmt.cbSize==22 && IsEqualGUID(format.SubFormat,pcm))))
            throw std::runtime_error("Expected valid 16-bit PCM WAV");
        XAUDIO2_BUFFER buffer{}; buffer.AudioBytes=dataSize; buffer.pAudioData=bytes.data()+dataAt; buffer.Flags=XAUDIO2_END_OF_STREAM;
        if(nativeLoopEnd){buffer.LoopBegin=nativeLoopBegin;buffer.LoopLength=nativeLoopEnd-nativeLoopBegin;buffer.LoopCount=XAUDIO2_LOOP_INFINITE;}
        if(argc==6) {
            auto frame=[&](int i){auto n=std::stoull(argv[i]); if(n>dataSize/fmt.nBlockAlign) throw std::runtime_error("Frame outside audio"); return static_cast<UINT32>(n);};
            buffer.LoopBegin=frame(3); const auto end=frame(4); buffer.PlayBegin=frame(5);
            if(end<=buffer.LoopBegin || buffer.PlayBegin>=end) throw std::runtime_error("Invalid loop interval");
            buffer.PlayLength=dataSize/fmt.nBlockAlign-buffer.PlayBegin;
            buffer.LoopLength=end-buffer.LoopBegin; buffer.LoopCount=XAUDIO2_LOOP_INFINITE;
        }
        Com com; Callback callback; Audio audio;
        check(XAudio2Create(&audio.engine)); check(audio.engine->CreateMasteringVoice(&audio.master));
        check(audio.engine->CreateSourceVoice(&audio.source,&fmt,0,XAUDIO2_DEFAULT_FREQ_RATIO,&callback));
        float lastRatio=control?control->frequencyRatio.load():1.f;
        if(!(lastRatio>=.5f&&lastRatio<=2.f))throw std::runtime_error("Audio pitch range");
        check(audio.source->SetFrequencyRatio(lastRatio));
        check(audio.source->SetVolume(0.20f)); check(audio.source->SubmitSourceBuffer(&buffer)); check(audio.source->Start());
        const auto deadline=GetTickCount64()+static_cast<ULONGLONG>(seconds*1000);
        XAUDIO2_VOICE_STATE state{};UINT64 observedSamples=0;
        unsigned volumeUpdates=0;float lastGain=1;
        do { Sleep(20); check(callback.error.load());
            if(control){float gain=control->gain.load();if(!(gain>=0&&gain<=1))throw std::runtime_error("Audio gain range");if(gain!=lastGain){check(audio.source->SetVolume(.2f*gain));lastGain=gain;++volumeUpdates;}}
            if(control){float ratio=control->frequencyRatio.load();if(!(ratio>=.5f&&ratio<=2.f))throw std::runtime_error("Audio pitch range");if(ratio!=lastRatio){check(audio.source->SetFrequencyRatio(ratio));lastRatio=ratio;}}
            audio.source->GetState(&state);
            observedSamples=(std::max)(observedSamples,state.SamplesPlayed);
        } while(GetTickCount64()<deadline && state.BuffersQueued && !(cancel && cancel->load()));
        check(audio.source->Stop()); audio.source->GetState(&state);
        // XAudio2 resets SamplesPlayed on XAUDIO2_END_OF_STREAM. A completed,
        // non-looping buffer has consumed its declared frame count; retain the
        // raw counter and completion callback separately in the evidence log.
        const bool completed=callback.streamEnded.load();
        auto played=completed&&!buffer.LoopCount?UINT64(dataSize/fmt.nBlockAlign):(std::max)(observedSamples,state.SamplesPlayed);
        std::osyncstream(std::cout) << "{\"cue\":"<<(control?control->cue:0)<<",\"stream\":\""<<(control?control->stream:"other")<<"\",\"sample_rate\":"<<fmt.nSamplesPerSec<<",\"channels\":"<<fmt.nChannels<<",\"samples_played\":"<<played<<",\"raw_samples_counter\":"<<state.SamplesPlayed<<",\"stream_completed\":"<<(completed?"true":"false")<<",\"loop_callbacks\":"<<callback.loops.load()<<",\"volume\":0.2}"<<std::endl;
        std::osyncstream(std::cout)<<"{\"stream\":\""<<(control?control->stream:"other")<<"\",\"audio_format\":\""<<(native?"GWA1":"WAV")<<"\",\"gain_updates\":"<<volumeUpdates<<",\"final_gain\":"<<lastGain<<",\"loop_begin\":"<<buffer.LoopBegin<<",\"loop_length\":"<<buffer.LoopLength<<"}"<<std::endl;
        std::osyncstream(std::cout)<<"{\"stream\":\""<<(control?control->stream:"other")<<"\",\"frequency_ratio\":"<<lastRatio<<"}"<<std::endl;
        if(!played && !(cancel&&cancel->load())) throw std::runtime_error("No playback progress");
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<std::endl;return 1;}
}
