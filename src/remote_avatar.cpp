#include "remote_avatar.h"
#include "source_coordinates.h"
#include <algorithm>
#include <cmath>
namespace mgo2mt::remote {
namespace {
constexpr float pi=3.14159265358979323846f;
float turn(float from,float to,float t){return std::remainder(from+std::remainder(to-from,2*pi)*t,2*pi);}
float fraction(uint64_t now,uint64_t at){return now>=at?std::min(1.f,float(now-at)/100.f):0.f;}
PlayerMotion action(const combat::Player&p,bool moving,float speed){
 if(!p.alive||p.stunned)return PlayerMotion::PlayDeadProne;
 if(p.pose.capsule.height==560)return moving?PlayerMotion::ProneForward:PlayerMotion::ProneIdle;
 if(p.pose.capsule.height==1100)return moving?PlayerMotion::CrouchWalk:PlayerMotion::CrouchIdle;
 if(p.reloadUntil)return PlayerMotion::Reload;
 return moving?(speed>3000?PlayerMotion::Run:PlayerMotion::Walk):PlayerMotion::Idle;
}
}
void Scene::clear(){replica_.clear();tracks_={};revision_=epoch_=lastNow_=0;self_={};}
bool Scene::update(const combat::Snapshot&s,const host::Roster&r,combat::Identity self,uint64_t now){
 if(self.slot>=24||!self.instance||!self.character||now<lastNow_)return false;
 if(epoch_&&self_!=self){clear();}
 if(!replica_.snapshot(s))return false;
 const bool epochChanged=epoch_!=s.epoch,newState=epochChanged||revision_!=s.revision;
 if(epochChanged)tracks_={};
 for(unsigned i=0;i<24;++i){auto&track=tracks_[i];const auto&p=s.players[i];const auto&roster=r.slots[i];
  if(!r.complete||!p||p->identity.character==self.character||p->identity.slot==self.slot||!roster||roster->instance!=p->identity.instance||roster->character!=p->identity.character||!roster->appearance){track.reset();continue;}
  bool fresh=!track||track->avatar.identity!=p->identity||track->avatar.life!=p->life;
  if(fresh){Track t;t.avatar.identity=p->identity;t.avatar.life=p->life;t.from=t.target=p->pose.feet;t.fromYaw=t.targetYaw=p->pose.yaw;t.at=t.motionAt=now;t.player=*p;track=t;}
  auto&t=*track;t.avatar.appearance=*roster->appearance;t.avatar.alive=p->alive;t.avatar.stunned=p->stunned;t.avatar.cover=p->cover;
  if(newState&&!fresh){float blend=fraction(now,t.at);for(unsigned j=0;j<3;++j)t.from[j]=t.from[j]+(t.target[j]-t.from[j])*blend;t.fromYaw=turn(t.fromYaw,t.targetYaw,blend);
   float dx=p->pose.feet[0]-t.target[0],dz=p->pose.feet[2]-t.target[2];float distance=std::sqrt(dx*dx+dz*dz);auto elapsed=now>=t.at?now-t.at:0;
   const auto wallYaw=p->cover.normalYaw+pi;const auto along=source_screen_x*(dx*std::cos(wallYaw)-dz*std::sin(wallYaw));t.avatar.coverMove=p->cover.attached&&std::abs(along)>1?(along<0?-1:1):0;
   t.moving=elapsed>0&&elapsed<=500&&distance>1;t.speed=t.moving?distance*1000/float(elapsed):0;if(t.moving)t.movedAt=now;
   t.target=p->pose.feet;t.targetYaw=p->pose.yaw;t.at=now;
   // Snap discontinuities; never sweep a new life or teleport through walls.
   if(distance>1200||!p->alive){t.from=t.target;t.fromYaw=t.targetYaw;t.moving=false;}
  }
  if(fresh||p->specialPhase!=t.avatar.specialPhase){t.avatar.specialPhase=p->specialPhase;t.specialAt=now;}
  if(fresh||newState){
   double elapsed=double(p->evadeElapsedMs)/1000.;
   // Snapshots carry HOST age, while sample() extrapolates from receipt time.
   // Variable transport delay must not rewind the same action/clip phase.
   if(!fresh&&p->evadeKind!=combat::EvadeKind::none&&p->evadeKind==t.avatar.evadeKind&&p->evadeSerial==t.avatar.evadeSerial)
    elapsed=std::max(elapsed,t.avatar.evadeSeconds+(now>=t.evadeAt?double(now-t.evadeAt)/1000.:0.));
   t.avatar.evadeKind=p->evadeKind;t.avatar.evadeSerial=p->evadeSerial;t.avatar.evadeSeconds=elapsed;t.evadeAt=now;
  }
  if(fresh||newState){double elapsed=double(p->specialPc.elapsedMs)/1000.;if(!fresh&&p->specialPc.serial&&p->specialPc.serial==t.avatar.specialPc.serial&&p->specialPc.action==t.avatar.specialPc.action)elapsed=std::max(elapsed,t.avatar.specialPcSeconds+(now>=t.specialPcAt?double(now-t.specialPcAt)/1000.:0.));t.avatar.specialPc=p->specialPc;t.avatar.specialPcSeconds=elapsed;t.specialPcAt=now;}
  t.player=*p;auto motion=action(*p,t.moving,t.speed);if(fresh||motion!=t.avatar.motion){t.avatar.motion=motion;t.motionAt=now;}
 }
 epoch_=s.epoch;revision_=s.revision;self_=self;lastNow_=now;return true;
}
std::vector<Avatar> Scene::sample(uint64_t now)const{
 std::vector<Avatar> out;
 for(const auto&track:tracks_)if(track){const auto&t=*track;auto a=t.avatar;auto f=fraction(now,t.at);
  for(unsigned j=0;j<3;++j)a.origin[j]=t.from[j]+(t.target[j]-t.from[j])*f;a.yaw=turn(t.fromYaw,t.targetYaw,f);
  bool moving=t.moving&&now>=t.movedAt&&now-t.movedAt<500;a.motion=action(t.player,moving,t.speed);
  if(a.cover.attached)a.yaw=std::remainder(a.cover.normalYaw+pi,2*pi);if(!moving)a.coverMove=0;
  if(a.alive&&!a.stunned&&a.evadeKind!=combat::EvadeKind::none)a.yaw=t.targetYaw;
  a.seconds=now>=t.motionAt?double(now-t.motionAt)/1000.:0;
  a.specialSeconds=now>=t.specialAt?double(now-t.specialAt)/1000.:0;
  if(a.evadeKind!=combat::EvadeKind::none&&now>=t.evadeAt)a.evadeSeconds+=double(now-t.evadeAt)/1000.;
  if(a.specialPc.action!=special_pc::Action::none&&now>=t.specialPcAt)a.specialPcSeconds+=double(now-t.specialPcAt)/1000.;
  if(a.motion==PlayerMotion::Reload)a.seconds*=original::rifle_reload_rates[t.player.reloadLevel];
  // This is an explicit static downed representation, not recovered death or
  // stun animation timing. Raw gameplay root Y is retained by the motion bank.
  if(!a.alive||a.stunned)a.seconds=1000;
  out.push_back(a);
 }
 return out;
}
}
