#include "weapon_effect_audio.h"
#include "pcm_wave.h"
#include <fstream>
#include <cmath>
#include <algorithm>
namespace mgo2mt::weapon_effect {
namespace {
bool point(combat::Vec3 p){return std::all_of(p.begin(),p.end(),[](float x){return std::isfinite(x)&&std::abs(x)<1000000;});}
bool current(const combat::Snapshot&s,combat::Identity id,uint32_t life){return id.slot<24&&life&&s.players[id.slot]&&s.players[id.slot]->identity==id&&s.players[id.slot]->life==life;}
}
bool validate_sound_file(const std::filesystem::path&path,std::string& error){try{std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("Cannot open effect WAV");auto n=in.tellg();if(n<44||n>16*1024*1024)throw std::runtime_error("Effect WAV size limit");std::vector<uint8_t>b(static_cast<size_t>(n));in.seekg(0);if(!in.read(reinterpret_cast<char*>(b.data()),n))throw std::runtime_error("Cannot read effect WAV");auto w=read_pcm_wave(b);if(w.channels>2||w.loopEnd||double(w.dataSize)/(w.rate*w.channels*2)>30)throw std::runtime_error("Effect WAV requires mono/stereo 16-bit PCM, no loop, at most 30 seconds");error.clear();return true;}catch(const std::exception&e){error=e.what();return false;}}
bool Audio::configure(std::shared_ptr<const Config> config,const std::filesystem::path&root,const combat::Effects& original,std::string& error){
 try{std::map<std::string,std::filesystem::path,std::less<>> files;auto base=std::filesystem::weakly_canonical(root);
  if(config)for(unsigned id=1;id<=511;++id)for(auto name:{"shot","reload","click","casing","explosion"})if(auto sound=config->sound(uint16_t(id),name);sound&&sound->enabled){
   if(!sound->path.empty()&&!files.contains(sound->path)){auto path=std::filesystem::weakly_canonical(base/std::filesystem::u8path(sound->path));if(!relative_path(path.lexically_relative(base).generic_string()))throw std::runtime_error("Effect sound escapes data directory");std::string e;if(!validate_sound_file(path,e))throw std::runtime_error(sound->path+": "+e);files.emplace(sound->path,path);}
   else if(sound->path.empty()&&sound->cue&&!original.files().contains(sound->cue))throw std::runtime_error("Effect sound cue is absent from combat audio manifest: "+std::to_string(sound->cue));
  }
  config_=std::move(config);files_=std::move(files);clear();error.clear();return true;
 }catch(const std::exception&e){error=e.what();return false;}
}
void Audio::clear(){pending_.clear();reloads_={};seen_.clear();contacts_.clear();epoch_=scene_=floor_=now_=lastClick_=0;}
void Audio::synchronize(uint64_t epoch,uint64_t scene,uint64_t watermark,uint64_t now){if(!epoch||!scene){clear();return;}if(epoch_!=epoch||scene_!=scene){clear();epoch_=epoch;scene_=scene;floor_=watermark;}if(now<now_){pending_.clear();lastClick_=0;}now_=now;}
void Audio::queue(const Sound&sound,combat::Identity id,uint32_t life,uint16_t weapon,combat::Vec3 origin,uint64_t now,uint64_t reload,bool world){if(!sound.enabled||!point(origin)||pending_.size()>=256||UINT64_MAX-now<sound.delayMs)return;pending_.push_back({sound,id,life,weapon,now+sound.delayMs,reload,origin,world});}
std::vector<combat::Event> Audio::dispatch(std::span<const combat::Event>events,const combat::Snapshot&s,uint64_t now){
 std::vector<combat::Event> legacy;if(!epoch_||s.epoch!=epoch_)return legacy;
 for(const auto&e:events){if(e.epoch!=epoch_||!e.id||e.id<=floor_||e.id>s.eventWatermark||!seen_.insert(e.id).second)continue;if(seen_.size()>4096){floor_=*seen_.begin();seen_.erase(seen_.begin());}
  const char* name=e.kind==combat::EventKind::shot?"shot":e.kind==combat::EventKind::explosion?"explosion":nullptr;
  const auto* sound=name&&config_?config_->sound(e.weapon,name):nullptr;
  if(!sound){legacy.push_back(e);continue;}
  const bool world=e.kind==combat::EventKind::explosion;
  if(world?e.source.slot>=24||!e.source.instance||!e.source.character||!e.sourceLife:!current(s,e.source,e.sourceLife))continue;
  queue(*sound,e.source,e.sourceLife,e.weapon,e.position,now,0,world);
 }return legacy;
}
void Audio::reloads(const combat::Snapshot&s,uint64_t now){
 if(!epoch_||s.epoch!=epoch_)return;
 for(unsigned slot=0;slot<24;++slot){const auto&p=s.players[slot];auto& prior=reloads_[slot];if(!p||!p->alive||p->stunned||!p->reloadUntil){prior={};continue;}
  const bool changed=prior.owner!=p->identity||prior.life!=p->life||prior.weapon!=p->weapon||prior.deadline!=p->reloadUntil;
  prior={p->identity,p->life,p->weapon,p->reloadUntil};
  if(changed&&config_)if(auto sound=config_->sound(p->weapon,"reload");sound&&(p->reloadElapsedMs<=250||sound->delayMs>=p->reloadElapsedMs)){auto position=p->pose.feet;position[1]+=1100;queue(*sound,p->identity,p->life,p->weapon,position,now-(std::min)(now,uint64_t(p->reloadElapsedMs)),p->reloadUntil);}
 }
}
void Audio::click(const combat::Player&p,uint64_t now){if(!config_||!epoch_||!p.alive||p.stunned||p.reloadUntil||p.ammo||!p.aiming||!p.weapon||(lastClick_&&now>=lastClick_&&now-lastClick_<150))return;if(auto sound=config_->sound(p.weapon,"click")){lastClick_=now;auto point=p.pose.feet;point[1]+=p.pose.capsule.height*.75f;queue(*sound,p.identity,p.life,p.weapon,point,now);}}
void Audio::casing(const combat::Event&e,const combat::Snapshot&s,uint64_t now){if(!epoch_||s.epoch!=epoch_||e.epoch!=epoch_||!e.id||e.id<=floor_||e.id>s.eventWatermark||!current(s,e.source,e.sourceLife)||!contacts_.insert(e.id).second)return;if(contacts_.size()>4096)contacts_.erase(contacts_.begin());if(config_)if(auto sound=config_->sound(e.weapon,"casing"))queue(*sound,e.source,e.sourceLife,e.weapon,e.position,now);}
void Audio::sample(const combat::Snapshot&s,uint64_t now,combat::Vec3 listener,combat::Effects&original,const std::function<void(const combat::Sound&)>&play){
 if(!epoch_||s.epoch!=epoch_||!point(listener)||!play){pending_.clear();return;}if(now<now_){pending_.clear();now_=now;return;}now_=now;
 std::erase_if(pending_,[&](const Pending&p){if(!p.world&&!current(s,p.owner,p.life))return true;if(p.reload){const auto&owner=*s.players[p.owner.slot];if(!owner.alive||owner.stunned||owner.weapon!=p.weapon||owner.reloadUntil!=p.reload)return true;}if(now<p.due)return false;if(now-p.due>250)return true;
  auto deliver=[&](const combat::Sound&sound){auto adjusted=sound;adjusted.gain*=p.sound.gain;if(adjusted.gain>=.001f)play(adjusted);};
  if(p.sound.path.empty())original.play_cue(p.sound.cue,p.origin,listener,deliver);
  else if(auto file=files_.find(p.sound.path);file!=files_.end()){float distance=0;for(unsigned i=0;i<3;++i)distance+=(p.origin[i]-listener[i])*(p.origin[i]-listener[i]);deliver({0xff0002,file->second,1.f/(1.f+distance/144000000.f),p.origin});}
  return true;
 });
}
}
