#pragma once
#include "player_motion.h"
#include "source_coordinates.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace mgo2mt::cover {
// Native public action IDs. They are neither PS3 wire values nor PlayerMotion IDs.
enum class Action:uint32_t {stand_right,stand_left,move_right,move_left,
 peek_right_enter,peek_right_hold,peek_right_exit,peek_left_enter,peek_left_hold,peek_left_exit,
 crouch_right,crouch_left,crouch_move_right,crouch_move_left,
 crouch_peek_right_enter,crouch_peek_right_hold,crouch_peek_right_exit,
 crouch_peek_left_enter,crouch_peek_left_hold,crouch_peek_left_exit,
 lean_enter,lean_hold,lean_exit,count};
struct Descriptor {uint32_t sourceIndex,sourceKey,frames;bool loop;};
inline constexpr std::array<Descriptor,23> descriptors{{
 {137,0xBCC36C,100,true},{138,0x1CC36D,100,true},{139,0x48136D,54,true},{140,0xA8136D,54,true},
 {141,0xEA2762,55,false},{142,0xAF9E9E,100,true},{143,0x2B81C9,50,false},
 {144,0xEA5762,55,false},{145,0xAFCE9E,100,true},{146,0x2B81D5,50,false},
 {147,0xB8ADC4,100,true},{148,0x18ADC5,100,true},{149,0x43FDC5,76,true},{150,0xA3FDC5,76,true},
 {151,0x162558,55,false},{152,0xDB9C93,100,true},{153,0xA8CCC8,55,false},
 {154,0x165558,55,false},{155,0xDBCC93,100,true},{156,0xA8CCD4,55,false},
 {257,0x05B0A4,18,false},{258,0x78B8DD,60,true},{259,0x7BC8AB,18,false}}};
inline constexpr const Descriptor* descriptor(Action a){auto i=uint32_t(a);return i<descriptors.size()?&descriptors[i]:nullptr;}
inline constexpr double duration(Action a){auto p=descriptor(a);return p?double(p->frames)/60.:0.;}
struct Sample {MotionPose pose;Action action;std::array<float,3> rootTravel{};uint32_t sourceKey=0,sourceIndex=0;};
// Explicit native digital-lean presentation, NOT the original common forward
// lean clip rotated and relabelled. Original18-frame enter/exit informs the
// caller's0.3s clock; this20-degree upper-spine roll is a native visual policy.
// Bone hash6C02B2 is the checked base rig's upper-body ancestor. Leg/hip/root
// tracks stay unchanged. Camera/shot offsets remain HOST/caller authoritative.
inline constexpr float native_lean_roll_radians=.35f;
inline std::optional<MotionPose> native_side_lean(const MotionPose& aim,int side,float weight){
 if(side < -1||side>1||!std::isfinite(weight)||weight<0||weight>1||!aim.rootBone||!aim.rotations.contains(aim.rootBone)||aim.rotations.empty()||aim.rotations.size()>128)return {};
 for(auto v:aim.root)if(!std::isfinite(v)||std::abs(v)>1e6)return {};
 for(const auto& [key,q]:aim.rotations){float n=0;if(!key)return {};for(auto v:q){if(!std::isfinite(v))return {};n+=v*v;}if(n<.99f||n>1.01f)return {};}
 auto at=aim.rotations.find(0x6C02B2);if(at==aim.rotations.end())return {};
 MotionPose out=aim;if(!side||!weight)return out;
 auto& q=out.rotations.at(0x6C02B2);const auto a=q;const float s=std::sin(-source_screen_x*side*weight*native_lean_roll_radians*.5f),c=std::cos(-source_screen_x*side*weight*native_lean_roll_radians*.5f);
 q={c*a[0]-s*a[1],s*a[0]+c*a[1],c*a[2]+s*a[3],c*a[3]-s*a[2]};float n=0;for(auto v:q)n+=v*v;n=std::sqrt(n);for(auto&v:q)v/=n;
 return out;
}
class CoverMotionBank {
 // Each embedded GWT1 stores one unnamed clip in slot0. The slot's ordinary
 // PlayerMotion::Idle spelling is never exposed as this motion's action meaning.
 std::vector<PlayerMotionBank> clips_;
 static void need(bool ok){if(!ok)throw std::runtime_error("Invalid reviewed GCV1 cover motion bank");}
public:
 explicit CoverMotionBank(std::span<const char> bytes){
  need(bytes.size()>=12&&bytes.size()<=8*1024*1024&&!std::memcmp(bytes.data(),"GCV1",4));size_t at=4;
  auto u=[&](){need(at+4<=bytes.size());uint32_t v=0;for(unsigned n=0;n<4;++n)v|=uint32_t(uint8_t(bytes[at++]))<<(8*n);return v;};
  need(u()==1);const auto count=u();need(count==descriptors.size());clips_.reserve(count);
  for(uint32_t i=0;i<count;++i){need(u()==i);auto size=u();need(size>=44&&size<=bytes.size()-at);clips_.emplace_back(bytes.subspan(at,size));at+=size;
   const auto* c=clips_.back().find(PlayerMotion::Idle);const auto& d=descriptors[i];
   need(clips_.back().size()==1&&c&&c->sourceKey==d.sourceKey&&c->sourceIndex==d.sourceIndex&&c->frames==d.frames&&c->fps==60&&c->loop==d.loop&&c->rootBone==0xA89233&&c->tracks.size()==53);
  }need(at==bytes.size());
 }
 size_t size()const{return clips_.size();}
 // The caller owns physics, full-identity/scene scope and transition blending.
 // Native60fps/source-loop policy. XZ is never applied to pose or world state.
 // Common lean source bends forward; its separate original yaw/weapon layers
 // are not recovered here and must not be called distinct left/right assets.
 std::optional<Sample> sample(Action action,double seconds)const{
  const auto* d=descriptor(action);if(!d||!std::isfinite(seconds)||seconds<0)return {};
  const auto& bank=clips_[uint32_t(action)];auto pose=bank.sample(PlayerMotion::Idle,seconds);if(!pose)return {};
  const auto* clip=bank.find(PlayerMotion::Idle);
  double t=d->loop?std::fmod(seconds,double(d->frames)/60.)*60.:(std::min)(seconds*60.,double(d->frames));
  const auto f=(std::min)(uint32_t(t),d->frames),next=(std::min)(f+1,d->frames);const auto alpha=float(t-f);
  Sample out{std::move(*pose),action,{},d->sourceKey,d->sourceIndex};
  for(unsigned k:{0u,2u})out.rootTravel[k]=clip->roots[f][k]*(1-alpha)+clip->roots[next][k]*alpha-clip->roots[0][k];
  return out;
 }
};
}
