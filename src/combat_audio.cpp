#include "combat_audio.h"
#include "pcm_wave.h"
#include <fstream>
#include <cmath>
#include <algorithm>
namespace mgo2win::combat {
bool Effects::load(const std::filesystem::path&root){
 try{auto path=root/"combat.txt";if(!std::filesystem::is_regular_file(path))return false;if(std::filesystem::file_size(path)>16384)return false;std::ifstream in(path);std::string magic;unsigned version,count;if(!(in>>magic>>version>>count)||magic!="MGO2WIN.COMBAT_AUDIO"||version!=1||count>128)return false;std::map<uint32_t,std::filesystem::path> checked;
  for(unsigned i=0;i<count;++i){uint32_t cue;std::string name;if(!(in>>cue>>name)||!cue||name.size()>96||name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")!=std::string::npos||name.find("..")!=std::string::npos||!name.ends_with(".wav"))return false;auto f=root/name;auto base=std::filesystem::weakly_canonical(root);if(std::filesystem::weakly_canonical(f).parent_path()!=base)return false;
   if(!std::filesystem::is_regular_file(f)||std::filesystem::file_size(f)>16*1024*1024)return false;std::ifstream audio(f,std::ios::binary|std::ios::ate);auto size=audio.tellg();if(size<44)return false;std::vector<unsigned char>b(static_cast<size_t>(size));audio.seekg(0);if(!audio.read(reinterpret_cast<char*>(b.data()),size))return false;auto pcm=read_pcm_wave(b);if(pcm.channels>2||pcm.loopEnd||double(pcm.dataSize)/(pcm.rate*pcm.channels*2)>30)return false;if(!checked.emplace(cue,f).second)return false;
  }std::string tail;if(in>>tail)return false;files_=std::move(checked);return true;
 }catch(...){return false;}
}
bool Effects::play_cue(uint32_t cue,Vec3 origin,Vec3 listener,const std::function<void(const Sound&)>&play){
 if(!cue||!play)return false;float distance=0;for(unsigned i=0;i<3;++i){if(!std::isfinite(origin[i])||!std::isfinite(listener[i]))return false;distance+=(origin[i]-listener[i])*(origin[i]-listener[i]);}if(!std::isfinite(distance))return false;
 auto f=files_.find(cue);if(f==files_.end()){++missing_;return false;}
 // Existing native distance gain; original SCE spatial/reverb processing remains separate.
 float gain=1.f/(1.f+distance/(12000.f*12000.f));if(gain<.001f)return false;play({cue,f->second,gain,origin});return true;
}
void Effects::dispatch(std::span<const Event>events,Vec3 listener,const std::function<void(const Sound&)>&play){
 if(!play||!std::all_of(listener.begin(),listener.end(),[](float x){return std::isfinite(x);}))return;
 for(const auto&e:events){if(!e.epoch||!e.id||e.epoch<epoch_)continue;if(e.epoch!=epoch_){epoch_=e.epoch;played_=0;}if(e.id<=played_)continue;played_=e.id;play_cue(e.cue,e.position,listener,play);}
}
}
