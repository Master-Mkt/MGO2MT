#pragma once
#include "player_motion.h"
#include "gekko_jump_curve.h"
#include "special_pc.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace mgo2win::special_pc {
// Native action selection from inspected original poses. These are not original
// dispatcher IDs or ordinary human PlayerMotion IDs.
enum class GekkoMotion:uint32_t {idle,walk,run,jump,kick};
struct GekkoClip {uint32_t sourceIndex,sourceKey,frames;bool loop;};
inline constexpr std::array<GekkoClip,7> gekko_clips{{
 {0,0x5FFEB3,160,true},{3,0xC06DF3,102,true},{61,0xDF5481,72,true},
 {6,0x6FAAF5,36,false},{7,0xA16DDA,59,false},{8,0x10232A,94,false},
 {16,0x7C7002,125,false}}};
inline constexpr float gekko_model_feet_offset=2926.016f;
inline constexpr double duration(GekkoMotion a){switch(a){case GekkoMotion::idle:return 160./60;case GekkoMotion::walk:return 102./60;case GekkoMotion::run:return 72./60;case GekkoMotion::jump:return 189./60;case GekkoMotion::kick:return 125./60;}return 0;}
struct GekkoSample {MotionPose pose;uint32_t sourceKey=0,sourceIndex=0,phase=0;double phaseSeconds=0;float authoredAirHeight=0;};
class GekkoMotionBank {
 std::vector<PlayerMotionBank> clips_;
 static void need(bool ok){if(!ok)throw std::runtime_error("Invalid reviewed GKG1 Gekko motion bank");}
public:
 explicit GekkoMotionBank(std::span<const char> bytes){
  need(bytes.size()>=12&&bytes.size()<=8*1024*1024&&!std::memcmp(bytes.data(),"GKG1",4));size_t at=4;
  auto u=[&](){need(at+4<=bytes.size());uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(uint8_t(bytes[at++]))<<(8*i);return v;};
  need(u()==1&&u()==gekko_clips.size());clips_.reserve(7);
  for(uint32_t i=0;i<7;++i){need(u()==i);auto n=u();need(n>=44&&n<=bytes.size()-at);clips_.emplace_back(bytes.subspan(at,n));at+=n;
   const auto* c=clips_.back().find(PlayerMotion::Idle);const auto& d=gekko_clips[i];
   need(clips_.back().size()==1&&c&&c->sourceKey==d.sourceKey&&c->sourceIndex==d.sourceIndex&&c->frames==d.frames&&c->fps==60&&c->loop==d.loop&&c->rootBone==0xA89233&&c->tracks.size()==56);
  }need(at==bytes.size());
 }
 size_t size()const{return clips_.size();}
 // seconds follows authored 60fps clips. Jump selects start/air/landing in that
 // order. Caller must include sourceKey in its blend source and blend every
 // phase change. Native gameplay may scale action time to its HOST duration.
 // Origin contract: actor origin = world feet + gekko_model_feet_offset.
 // This sample subtracts that offset, and (for jump only) authored airborne
 // height from the shared native adapted source-root curve, so HOST displacement is
 // not added twice. No source X/Z is applied. Physics remains authoritative.
 std::optional<GekkoSample> sample_detail(GekkoMotion action,double seconds)const{
  if(!std::isfinite(seconds)||seconds<0||!duration(action))return {};
  uint32_t i=0;double t=seconds;
  switch(action){case GekkoMotion::idle:i=0;break;case GekkoMotion::walk:i=1;break;case GekkoMotion::run:i=2;break;case GekkoMotion::kick:i=6;break;
   case GekkoMotion::jump:if(t<36./60)i=3;else if(t<95./60){i=4;t-=36./60;}else{i=5;t-=95./60;}break;}
  const auto& d=gekko_clips[i];
  auto p=clips_[i].sample(PlayerMotion::Idle,t);if(!p)return {};
  const float air=action==GekkoMotion::jump?gekko_jump_height_seconds(seconds):0;
  p->root[1]-=gekko_model_feet_offset+air;
  return GekkoSample{std::move(*p),d.sourceKey,d.sourceIndex,i,t,air};
 }
 std::optional<MotionPose> sample(GekkoMotion a,double seconds)const{auto p=sample_detail(a,seconds);if(!p)return {};return std::move(p->pose);}
 // The native ten-metre trajectory extends only the airborne source clip.
 // Preparation and landing retain their authored frame rate. Root removal must
 // sample the same source time, never subtract the native ten-metre height.
 std::optional<GekkoSample> sample_gameplay(GekkoMotion a,double seconds)const{
  if(!std::isfinite(seconds)||seconds<0)return {};
  if(a==GekkoMotion::jump){
   constexpr double prepare=.6,land=3.458,sourceLand=95./60;
   if(seconds>=land)seconds=sourceLand+(seconds-land);
   else if(seconds>prepare)seconds=prepare+(seconds-prepare)*(sourceLand-prepare)/(land-prepare);
  }
  return sample_detail(a,seconds);
 }
};
}
