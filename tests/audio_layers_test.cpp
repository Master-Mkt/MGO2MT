#include "audio_layers.h"
#include <iostream>
#include <limits>

using namespace mgo2mt;
static void require(bool ok){if(!ok)throw std::runtime_error("audio layers test failed");}
static void near(float a,float b){require(std::abs(a-b)<.0001f+std::abs(b)*.000001f);}
template<class F> static void reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}require(rejected);}
static void put(std::vector<unsigned char>& b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b[at+i]=static_cast<unsigned char>(n>>(i*8));}
static std::vector<unsigned char> wave(std::initializer_list<int16_t> samples,uint32_t rate=48000,uint32_t begin=0,uint32_t end=0){
    std::vector<unsigned char> b(44+samples.size()*2+(end?68:0));
    std::memcpy(b.data(),"RIFF",4);put(b,4,static_cast<uint32_t>(b.size()-8));std::memcpy(b.data()+8,"WAVEfmt ",8);
    put(b,16,16);put(b,20,1|(2<<16));put(b,24,rate);put(b,28,rate*4);put(b,32,4|(16<<16));
    std::memcpy(b.data()+36,"data",4);put(b,40,static_cast<uint32_t>(samples.size()*2));
    size_t at=44;for(auto sample:samples){b[at++]=static_cast<unsigned char>(sample);b[at++]=static_cast<unsigned char>(uint16_t(sample)>>8);}
    if(end){std::memcpy(b.data()+at,"smpl",4);put(b,at+4,60);put(b,at+8+28,1);put(b,at+8+44,begin);put(b,at+8+48,end-1);}
    return b;
}
static float mixed(const PcmLayerPair& p,size_t frame,size_t destination,std::array<float,2> gains){
    const auto matrix=audio_layer_matrix(gains);float value=0;
    for(size_t source=0;source<4;++source){const size_t at=frame*8+source*2;
        const auto sample=static_cast<int16_t>(uint16_t(p.pcm[at])|(uint16_t(p.pcm[at+1])<<8));
        value+=sample*matrix[source+4*destination];
    }return value;
}
int main(){try{
    auto a=wave({1000,-2000,3000,-4000}),b=wave({10000,-12000,14000,-16000});
    auto pair=interleave_audio_layers(a,b);require(pair.frames==2&&pair.rate==48000&&pair.pcm.size()==16&&!pair.loopEnd);
    require(interleave_audio_layers(a,b,true).loopEnd==2);
    // Whole-file fallback and an explicit equivalent loop are the same playback.
    auto whole=wave({10000,-12000,14000,-16000},48000,0,2);
    require(interleave_audio_layers(a,whole,true).loopEnd==2);
    reject([&]{interleave_audio_layers(a,whole);});
    reject([&]{interleave_audio_layers(a,wave({1,2}));});
    reject([&]{interleave_audio_layers(a,wave({1,2,3,4},44100));});
    reject([&]{interleave_audio_layers(a,wave({1,2,3,4},48000,1,2),true);});
    auto mono=b;put(mono,20,1|(1<<16));put(mono,28,48000*2);put(mono,32,2|(16<<16));
    reject([&]{interleave_audio_layers(a,mono);});
    auto invalid=b;invalid[0]=0;reject([&]{interleave_audio_layers(a,invalid);});
    reject([&]{AudioLayerFader f({std::numeric_limits<float>::quiet_NaN(),1,500,2000});});
    reject([&]{AudioLayerFader f({1,1,600001,2000});});
    const auto savedPcm=pair.pcm;
    AudioLayerFader f({.8f,.6f,500,2000});
    auto gains=f.update(false,0);near(mixed(pair,0,0,gains),800);near(mixed(pair,0,1,gains),-1600);
    f.update(true,100);
    gains=f.update(true,350);near(gains[0],.7f);near(gains[1],.3f);
    // Unequal fades overlap: at 250 ms both tracks contribute to the same
    // sample position, and neither left signal leaks into the right output.
    near(mixed(pair,1,0,gains),6300);near(mixed(pair,1,1,gains),-7600);
    gains=f.update(true,600);near(gains[0],.6f);near(gains[1],.6f);
    auto interrupted=f.update(false,600);require(gains==interrupted);
    gains=f.update(false,850);near(gains[0],.7f);near(gains[1],.525f);
    require(f.update(false,700)==gains); // Backward timestamps cannot reverse fade.
    gains=f.update(false,2600);near(gains[0],.8f);near(gains[1],0);
    require(pair.pcm==savedPcm); // No re-read, seek or PCM mutation on switches.
    AudioLayerFader instant({1,1,0,0});gains=instant.update(true,0);near(gains[0],0);near(gains[1],1);
    AudioLayerFader urgent({.8f,.6f,500,2000},true);gains=urgent.update(true,0);near(gains[0],0);near(gains[1],.6f);
    std::cout<<"audio_layers_test: PCM pairing, effective loops, stereo matrix and interrupted independent fades passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
