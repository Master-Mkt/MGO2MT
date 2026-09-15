#include "combat_burning.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace mgo2win::combat::burning {
namespace {
bool finite(Vec3 p){for(float x:p)if(!std::isfinite(x)||std::abs(x)>=1000000)return false;return true;}
bool capsule(stage::Capsule c){return std::isfinite(c.radius)&&std::isfinite(c.height)&&c.radius>0&&c.radius<=2000&&c.height>=2*c.radius&&c.height<=10000;}
}
bool valid(Key k)noexcept{return k.epoch&&k.slot<24&&k.instance&&k.character&&k.life;}
bool valid(Policy p)noexcept{return p.durationMs>0&&p.durationMs<=60000&&p.damagePermillePerSecond>0&&p.damagePermillePerSecond<=1000;}
void State::bind(Key k){if(k==target_)return;*this=State{};target_=k;}
void State::clear(){until_=0;remainder_=0;}
bool State::ignite(Key target,Source source,uint64_t event,uint64_t now,Policy p){
 if(!valid(target)||target!=target_||!valid(source.actor)||source.actor.epoch!=target.epoch||source.team>2||!source.weapon||!valid(p)||!event||(event==event_&&source==source_)||(clock_&&now<at_)||now>UINT64_MAX-p.durationMs)return false;
 // Caller advances the previous effect to this clock before refreshing it.
 // Never silently discard unpaid elapsed damage on a live repeated ignition.
 if(until_&&(!clock_||at_!=now))return false;
 if(!until_){remainder_=0;at_=now;clock_=true;}
 until_=(std::max)(until_,now+p.durationMs);event_=event;source_=source;policy_=p;return true;
}
Step State::advance(Key target,uint64_t now,uint32_t maxHp,bool alive,bool enabled,bool submerged){
 if(target!=target_||!valid(target)||!maxHp||(clock_&&now<at_))return {0,until_&&at_<until_?uint32_t(until_-at_):0,active(),false};
 const bool was=active();
 if(!alive||!enabled||submerged){clear();at_=now;clock_=true;return {0,0,false,was};}
 uint64_t elapsed=clock_&&until_&&at_<until_?(std::min)(now,until_)-at_:0;
 at_=now;clock_=true;
 // Carry fractional HP across updates; cadence-independent native policy.
 // Bounds: maxHp UINT32_MAX * 1000 * 60000 < UINT64_MAX.
 uint64_t amount=uint64_t(maxHp)*policy_.damagePermillePerSecond*elapsed+remainder_;
 const auto damage=static_cast<uint32_t>((std::min)(uint64_t(UINT32_MAX),amount/1000000));remainder_=amount%1000000;
 if(until_&&now>=until_)clear();
 return {damage,until_?uint32_t(until_-now):0,active(),was!=active()||damage!=0};
}
bool Replay::accept(Source source,uint64_t value){
 if(!valid(source.actor)||!value||source.team>2)return false;
 if(epoch&&source.actor.epoch<epoch)return false;
 if(epoch!=source.actor.epoch){channels={};epoch=source.actor.epoch;}
 for(auto& s:channels)if(s.serial&&s.source.actor==source.actor&&s.source.weapon==source.weapon&&s.source.object==source.object){if(value<=s.serial)return false;s.serial=value;return true;}
 for(auto& s:channels)if(!s.serial){s={source,value};return true;}
 return false; // No eviction/replay resurrection on an exhausted epoch ledger.
}
bool valid(const Blast& b)noexcept{return valid(b.source.actor)&&b.source.team<=2&&b.serial&&b.source.weapon&&finite(b.position)&&std::isfinite(b.radius)&&b.radius>0&&b.radius<=20000&&b.damage<=1000000&&(b.damage||b.ignite);}
bool exposed(const Blast& b,Vec3 feet,stage::Capsule c,const stage::Collision* world,const stage::Collision* objects){
 if(!valid(b)||!finite(feet)||!capsule(c))return false;
 Vec3 axis=feet;axis[1]=(std::clamp)(b.position[1],feet[1]+c.radius,feet[1]+c.height-c.radius);
 Vec3 d{axis[0]-b.position[0],axis[1]-b.position[1],axis[2]-b.position[2]};const float length=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
 const float distance=(std::max)(0.f,length-c.radius);if(distance>=b.radius)return false;if(distance<=.01f)return true;
 for(auto& x:d)x/=length;
 for(const auto* collision:{world,objects})if(collision){auto hit=collision->ray(b.position,d,distance);if(hit&&hit->distance<distance-.01f)return false;}
 return true;
}
std::vector<Line> flames(Key key,bool burning,Vec3 feet,stage::Capsule c,uint64_t now){
 if(!burning||!valid(key)||!finite(feet)||!capsule(c))return {};
 std::vector<Line> result;result.reserve(32);
 // Explicit native procedural rising orange/yellow tongues; original flame
 // texture, bone attachment, sound/pitch and particle coefficients unresolved.
 for(unsigned i=0;i<16;++i){const auto seed=uint32_t(key.character*1664525u+key.life*1013904223u+i*747796405u);const float phase=float((now+seed%1000)%900)/900.f;
  const float angle=float(i)*2.39996323f+float(seed%100)*.01f;const float radius=c.radius*(.4f+.35f*std::sin(phase*3.14159265f));
  Vec3 a{feet[0]+std::cos(angle)*radius,feet[1]+phase*c.height*.85f,feet[2]+std::sin(angle)*radius};
  Vec3 b{a[0]+std::sin(angle+phase*5)*50,a[1]+140+(1-phase)*120,a[2]+std::cos(angle+phase*5)*50};
  result.push_back({a,b,{1,.22f+.5f*(1-phase),.015f,.9f*(1-phase)}});
  a[1]+=30;Vec3 mid{(a[0]+b[0])*.5f,(a[1]+b[1])*.5f,(a[2]+b[2])*.5f};result.push_back({a,mid,{1,.9f,.25f,.75f*(1-phase)}});
 }
 return result;
}
}
