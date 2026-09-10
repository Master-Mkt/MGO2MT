#include "audio_fade.h"
#include <iostream>
void require(bool v){if(!v)throw std::runtime_error("Fade contract failed");}
int main(){try{
 mgo2win::AudioFade f;f.stop(24);require(f.remaining()==144);
 for(unsigned i=0;i<72;++i)f.advance(1);require(std::abs(f.gain()-.5f)<.00001f);
 for(unsigned i=0;i<72;++i)f.advance(1);require(!f.remaining()&&f.gain()==0);
 mgo2win::AudioFade cap;cap.stop(10);cap.advance(100);require(cap.remaining()==57);
 cap.stop(0);require(cap.gain()==0&&!cap.remaining());
 mgo2win::AudioFade interrupted;interrupted.stop(24);for(unsigned i=0;i<72;++i)interrupted.advance(1);interrupted.stop(10);require(interrupted.remaining()>=29&&interrupted.remaining()<=30);
 bool rejected=false;try{interrupted.stop(-1);}catch(...){rejected=true;}require(rejected);
 std::cout<<"normal BGM fade 144 frames, midpoint, completion, delta clamp, interruption and range passed\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
