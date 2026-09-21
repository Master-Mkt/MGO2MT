#include "player_motion.h"
#include "original_reload_timing.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace mgo2mt {
namespace {
void require(bool condition){if(!condition)throw std::runtime_error("Invalid GWT1 player motion bank");}
struct Reader {
 std::span<const char> b;size_t at=4;
 uint32_t u(){require(at+4<=b.size());uint32_t value=0;for(unsigned i=0;i<4;++i)value|=uint32_t(uint8_t(b[at++]))<<(8*i);return value;}
 float f(){auto bits=u();float value;std::memcpy(&value,&bits,4);require(std::isfinite(value)&&std::abs(value)<=1000000.f);return value;}
};
}
PlayerMotionBank::PlayerMotionBank(std::span<const char>b){
 require(b.size()>=12&&b.size()<=32*1024*1024&&!std::memcmp(b.data(),"GWT1",4));Reader r{b};require(r.u()==1);auto count=r.u();require(count&&count<=uint32_t(PlayerMotion::Count));uint64_t values=0;
 for(uint32_t i=0;i<count;++i){PlayerMotionClip clip;auto action=r.u();require(action<uint32_t(PlayerMotion::Count));clip.action=PlayerMotion(action);require(!clips_.contains(clip.action));clip.sourceKey=r.u();clip.sourceIndex=r.u();clip.frames=r.u();clip.fps=r.u();auto flags=r.u();auto tracks=r.u();clip.rootBone=r.u();
  require(clip.sourceKey&&clip.sourceIndex<4096&&clip.frames&&clip.frames<=3600&&clip.fps==60&&flags<=1&&tracks&&tracks<=128&&clip.rootBone);clip.loop=flags!=0;
  auto samples=uint64_t(clip.frames)+1;values+=samples*(3+uint64_t(tracks)*4);require(values<=8*1024*1024&&samples*12+uint64_t(tracks)*(4+samples*16)<=b.size()-r.at);
  clip.roots.reserve(size_t(samples));for(uint64_t f=0;f<samples;++f){std::array<float,3> root{};for(auto&v:root)v=r.f();clip.roots.push_back(root);}
  for(uint32_t t=0;t<tracks;++t){auto key=r.u();require(key&&!clip.tracks.contains(key));auto&rotations=clip.tracks[key];rotations.reserve(size_t(samples));for(uint64_t f=0;f<samples;++f){std::array<float,4> q{};float norm=0;for(auto&v:q){v=r.f();norm+=v*v;}require(norm>.99f&&norm<1.01f);rotations.push_back(q);}}
  require(clip.tracks.contains(clip.rootBone));clips_.emplace(clip.action,std::move(clip));
 }require(r.at==b.size());
}
const PlayerMotionClip* PlayerMotionBank::find(PlayerMotion action)const{auto at=clips_.find(action);return at==clips_.end()?nullptr:&at->second;}
std::optional<MotionPose> PlayerMotionBank::sample(PlayerMotion action,double seconds)const{
 auto*clip=find(action);if(!clip)return {};if(!std::isfinite(seconds)||seconds<0)seconds=0;
 // Only this reviewed standing AK archive/selector has an established end
 // boundary. Other clips keep their existing inspection playback policy.
 const bool akReload=action==PlayerMotion::Reload&&clip->sourceKey==0x7c56c7&&clip->sourceIndex==3&&clip->frames==210&&!clip->loop;
 const double fps=akReload?original::nominal_motion_fps:clip->fps;
 const double lastFrame=akReload?209:clip->frames;
 double frames=seconds*fps;
 // Avoid overflow from finite but very large caller timestamps.
 double time=clip->loop?std::fmod(seconds,double(clip->frames)/fps)*fps:std::min(frames,lastFrame);
 auto frame=std::min(uint32_t(time),clip->frames),next=std::min(frame+1,clip->frames);float alpha=float(time-frame);MotionPose out;out.rootBone=clip->rootBone;
 out.root[1]=clip->roots[frame][1]*(1-alpha)+clip->roots[next][1]*alpha;
 for(const auto&[key,track]:clip->tracks){const auto&a=track[frame];const auto&b=track[next];float dot=0;for(unsigned i=0;i<4;++i)dot+=a[i]*b[i];float sign=dot<0?-1.f:1.f;std::array<float,4>q{};float norm=0;for(unsigned i=0;i<4;++i){q[i]=a[i]*(1-alpha)+b[i]*sign*alpha;norm+=q[i]*q[i];}norm=std::sqrt(norm);for(auto&v:q)v/=norm;out.rotations.emplace(key,q);}
 return out;
}
}
