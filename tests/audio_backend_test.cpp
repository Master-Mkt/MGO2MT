#include "multi_ui_audio.h"
#include <windows.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt::multi_ui;
int main(int argc,char**argv){try{
 if(argc<2||argc>3)throw std::runtime_error("fixture directory and optional audio file required");auto dir=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(dir);auto file=dir/"authored.wav";
 {std::ofstream f(file,std::ios::binary);auto u16=[&](uint16_t v){f.put(char(v));f.put(char(v>>8));};auto u32=[&](uint32_t v){u16(uint16_t(v));u16(uint16_t(v>>16));};f.write("RIFF",4);u32(36+44100);f.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(44100);u32(88200);u16(2);u16(16);f.write("data",4);u32(44100);for(int i=0;i<22050;++i)u16(uint16_t(int16_t(std::sin(double(i)*6.283185307*660/44100)*1000)));}
 if(argc==3)file=std::filesystem::absolute(argv[2]);auto audio=make_audio_backend();audio->mute(true);std::string error;if(!audio->start(file,1,77,error))throw std::runtime_error(error);bool started=false,ended=false;auto deadline=GetTickCount64()+15000;
 while(GetTickCount64()<deadline&&!ended){for(auto&e:audio->poll()){if(e.token!=77)throw std::runtime_error("wrong token");if(e.result==AudioResult::failed)throw std::runtime_error(e.message);if(e.result==AudioResult::started)started=true;if(e.result==AudioResult::completed){if(!started)throw std::runtime_error("completed before started");ended=true;}}Sleep(5);}
 if(!ended)throw std::runtime_error("no actual playback ended notification");std::cout<<"PASS actual Media Foundation muted "<<file.extension().string()<<" started -> playback ended\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
