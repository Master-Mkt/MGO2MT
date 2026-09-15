#pragma once
#include "gekko_motion.h"

namespace mgo2win::special_pc {
// Inspected original poses selected for native traversal. Original action
// dispatch is unverified. Turn candidates are stored for research, not enabled.
inline constexpr std::array<GekkoClip,5> gekko_traversal_clips{{
 {19,0xA21FF0,87,false},{20,0xA21FEA,88,false},
 {21,0x43FC80,124,false},{22,0x43FC86,124,false},
 {63,0xB2E3CF,54,false}}};
class GekkoTraversalMotionBank {
 std::vector<PlayerMotionBank> clips_;
 static void need(bool ok){if(!ok)throw std::runtime_error("Invalid reviewed GKT1 traversal bank");}
 static void physical_root(MotionPose& p){
  // Physical feet/bodyYaw own displacement and heading. Retain root pitch/roll
  // and all child animation; use the original ordinary standing root height.
  p.root={0,3160.f-gekko_model_feet_offset,0};
  auto& q=p.rotations.at(p.rootBone);
  const double yaw=std::atan2(2.*(double(q[3])*q[1]+double(q[0])*q[2]),1.-2.*(double(q[0])*q[0]+double(q[1])*q[1]));
  const double c=std::cos(yaw*.5),s=std::sin(yaw*.5);
  const std::array<double,4> r{c*q[0]-s*q[2],c*q[1]-s*q[3],c*q[2]+s*q[0],c*q[3]+s*q[1]};
  double n=0;for(auto x:r)n+=x*x;n=std::sqrt(n);
  for(size_t k=0;k<4;++k)q[k]=float(r[k]/n);
 }
public:
 explicit GekkoTraversalMotionBank(std::span<const char> bytes){
  need(bytes.size()>=12&&bytes.size()<=8*1024*1024&&!std::memcmp(bytes.data(),"GKT1",4));size_t at=4;
  auto u=[&](){need(at+4<=bytes.size());uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(uint8_t(bytes[at++]))<<(8*i);return v;};
  need(u()==1&&u()==gekko_traversal_clips.size());clips_.reserve(5);
  for(uint32_t i=0;i<5;++i){need(u()==i);auto n=u();need(n>=44&&n<=bytes.size()-at);clips_.emplace_back(bytes.subspan(at,n));at+=n;
   const auto* c=clips_.back().find(PlayerMotion::Idle);const auto& d=gekko_traversal_clips[i];
   need(clips_.back().size()==1&&c&&c->sourceKey==d.sourceKey&&c->sourceIndex==d.sourceIndex&&c->frames==d.frames&&c->fps==60&&!c->loop&&c->rootBone==0xA89233&&c->tracks.size()==56&&c->tracks.contains(c->rootBone));
   if(i)need(c->tracks.size()==clips_[0].find(PlayerMotion::Idle)->tracks.size());
   for(const auto& [bone,track]:c->tracks){(void)track;need(clips_[0].find(PlayerMotion::Idle)->tracks.contains(bone));}
  }need(at==bytes.size());
 }
 size_t size()const{return clips_.size();}
 // Candidate access does not assign a gameplay turn action.
 std::optional<GekkoSample> sample_candidate(uint32_t slot,double seconds)const{
  if(slot>=clips_.size()||!std::isfinite(seconds)||seconds<0)return {};
  const auto& d=gekko_traversal_clips[slot];const double t=(std::min)(seconds,double(d.frames)/60);
  auto p=clips_[slot].sample(PlayerMotion::Idle,t);if(!p)return {};physical_root(*p);
  return GekkoSample{std::move(*p),d.sourceKey,d.sourceIndex,slot,t,0};
 }
 // HOST native climb lasts 2600ms: rise1600/cross800/settle200. Source air7
 // and candidate63 are retimed independently. Include BOTH sourceKey and phase
 // in MotionBlend's key: cross and settle share a source clip. All transitions
 // require the existing blend. No authored translation/yaw moves the actor.
 std::optional<GekkoSample> sample_climb(const GekkoMotionBank& base,double seconds)const{
  if(!std::isfinite(seconds)||seconds<0)return {};
  const double t=(std::min)(seconds,2.6);std::optional<GekkoSample> p;
  if(t<1.6){
   const double sourceTime=(std::min)(.6+(t/1.6)*(59./60),std::nextafter(95./60,0.));
   p=base.sample_detail(GekkoMotion::jump,sourceTime);
   if(p){p->phase=0;p->authoredAirHeight=0;physical_root(p->pose);}
  }else if(t<2.4){p=sample_candidate(4,(t-1.6)/.8*(42./60));if(p)p->phase=1;
  }else{p=sample_candidate(4,42./60+(t-2.4)/.2*(12./60));if(p)p->phase=2;}
  return p;
 }
};
}
