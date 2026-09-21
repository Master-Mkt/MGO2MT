#include "product_identity.h"
#include "combat_audio.h"
#include "pcm_wave.h"
#include <fstream>
#include <cmath>
#include <algorithm>
namespace mgo2mt::combat {
namespace {
uint32_t original_blast_cue(uint16_t weapon){
 // Original7B44E0 switch tails select subtype bits,7D84A8 selects these
 // dry/nearest cues. Water and distance-tier routing remain native adapter work.
 switch(weapon){case 52:return 12006;case 53:return 12016;case 54:return 12025;case 55:return 12032;default:return 0;}
}
}
bool Effects::load(const std::filesystem::path&root){return load_manifest(root/"combat.txt");}
bool Effects::load_manifest(const std::filesystem::path&path){
 const auto root=path.parent_path();
 try{if(!std::filesystem::is_regular_file(path))return false;if(std::filesystem::file_size(path)>16384)return false;std::ifstream in(path);std::string magic;unsigned version,count;if(!(in>>magic>>version>>count)||magic!=mgo2mt::brand::Format{"MGO2MT.COMBAT_AUDIO"}||version!=1||count>256)return false;std::map<uint32_t,std::filesystem::path> checked;
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
 for(const auto&e:events){if(!e.epoch||!e.id||e.epoch<epoch_)continue;if(e.epoch!=epoch_){epoch_=e.epoch;floor_=0;played_.clear();}if(e.id<=floor_||!played_.insert(e.id).second)continue;if(played_.size()>4096){floor_=*played_.begin();played_.erase(played_.begin());}auto cue=e.cue?e.cue:e.kind==EventKind::explosion?original_blast_cue(e.weapon):0;
  // Original 7D84A8 bit0x40 dry explosion distance tiers. Water subtype is
  // not in the native explosion event and remains a separate adapter task.
  if(!cue&&e.kind==EventKind::explosion&&e.weapon==103){const float distance=std::hypot(e.position[0]-listener[0],e.position[1]-listener[1],e.position[2]-listener[2]);cue=distance<15000?12003:distance<35000?12004:12005;}
  play_cue(cue,e.position,listener,play);}
}
}
