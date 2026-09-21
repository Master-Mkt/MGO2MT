// Manual offline XAudio2 smoke, deliberately not an automatic CTest sound.
#include "audio_control.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
int run_audio_probe(int,wchar_t**,const std::atomic_bool*,const mgo2mt::AudioControl*);
int wmain(int argc,wchar_t** argv){
    const std::wstring mode=argc==4?argv[3]:L"";
    if((argc!=3&&argc!=4)||(argc==4&&mode!=L"--near-loop"&&mode!=L"--no-loop"&&mode!=L"--cancel")){
        std::cerr<<"Usage: audio_layers_playback_test normal.wav alternate.wav [--near-loop|--no-loop|--cancel]\n";return 2;
    }
    std::vector<std::wstring> arguments{L"audio",argv[1],L"3.5"};
    if(mode==L"--near-loop"){
        try{
            std::ifstream input(std::filesystem::path(argv[1]),std::ios::binary|std::ios::ate);
            if(!input)throw std::runtime_error("Near-loop WAV open");
            const auto size=input.tellg();
            if(size<44||size>256*1024*1024)throw std::runtime_error("Near-loop WAV size");
            std::vector<unsigned char> bytes(static_cast<size_t>(size));
            input.seekg(0);input.read(reinterpret_cast<char*>(bytes.data()),size);
            if(!input)throw std::runtime_error("Near-loop WAV read");
            const auto wave=mgo2mt::read_pcm_wave(bytes);
            if(!wave.loopEnd||wave.loopEnd-wave.loopBegin<=wave.rate)
                throw std::runtime_error("Near-loop needs an embedded loop longer than one second");
            const auto playBegin=wave.loopEnd-wave.rate;
            arguments.push_back(std::to_wstring(wave.loopBegin));
            arguments.push_back(std::to_wstring(wave.loopEnd));
            arguments.push_back(std::to_wstring(playBegin));
            std::cout<<"{\"near_loop\":true,\"loop_begin\":"<<wave.loopBegin
                     <<",\"loop_end\":"<<wave.loopEnd<<",\"play_begin\":"<<playBegin<<"}\n";
        }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
    }
    mgo2mt::AudioControl control;
    control.stream="layer_smoke";control.loopWhole=mode!=L"--no-loop";control.alternateWave=argv[2];
    control.layerMix={93*.007874f,98*.007874f,500,2000};
    std::atomic_bool cancel{false};
    std::jthread switcher([&]{
        std::this_thread::sleep_for(std::chrono::milliseconds(400));control.alternate=true;
        if(mode==L"--cancel"){
            // Cancel during the 500ms-in/2000ms-out overlap, while the source
            // and both nonzero layer gains are still active.
            std::this_thread::sleep_for(std::chrono::milliseconds(300));cancel=true;return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(800));control.alternate=false;
    });
    std::vector<wchar_t*> args;for(auto& argument:arguments)args.push_back(argument.data());
    return run_audio_probe(static_cast<int>(args.size()),args.data(),mode==L"--cancel"?&cancel:nullptr,&control);
}
