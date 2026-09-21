#include "combat_authority.h"
#include <algorithm>
#include <cmath>
namespace mgo2mt::combat {
namespace {
float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vec3 add(Vec3 a,Vec3 b){for(unsigned i=0;i<3;++i)a[i]+=b[i];return a;}
Vec3 mul(Vec3 a,float n){for(auto&x:a)x*=n;return a;}
bool finite(Vec3 a){for(auto x:a)if(!std::isfinite(x)||std::abs(x)>=999000)return false;return true;}
// Conservative swept upright body against a peer. Horizontal circles and a
// vertical interval prevent a fast launch crossing an intervening player.
std::optional<stage::CapsuleHit> body_hit(Vec3 feet,Vec3 delta,stage::Capsule moving,const Pose& other){
 const float radius=moving.radius+other.capsule.radius+moving.skin;
 const float x=feet[0]-other.feet[0],z=feet[2]-other.feet[2];
 const float A=delta[0]*delta[0]+delta[2]*delta[2],B=x*delta[0]+z*delta[2],C=x*x+z*z-radius*radius;
 float lo=0,hi=1;Vec3 normal{};
 if(A<.000001f){if(C>0)return {};}
 else{const float disc=B*B-A*C;if(disc<0)return {};const float enter=(-B-std::sqrt(disc))/A,leave=(-B+std::sqrt(disc))/A;lo=(std::max)(lo,enter);hi=(std::min)(hi,leave);if(lo>hi)return {};if(enter>=0){normal={x+delta[0]*lo,0,z+delta[2]*lo};const float len=std::hypot(normal[0],normal[2]);if(len>0)normal=mul(normal,1/len);}}
 const float bottom=other.feet[1]-moving.height-moving.skin,top=other.feet[1]+other.capsule.height+moving.skin;
 if(std::abs(delta[1])<.000001f){if(feet[1]<bottom||feet[1]>top)return {};}
 else{float enter=(bottom-feet[1])/delta[1],leave=(top-feet[1])/delta[1];if(enter>leave)std::swap(enter,leave);if(enter>lo){lo=enter;normal={0,delta[1]>0?-1.f:1.f,0};}hi=(std::min)(hi,leave);}
 if(lo>hi||hi<0||lo>1)return {};if(dot(normal,normal)<.1f){normal=mul(delta,-1);float len=std::sqrt(dot(normal,normal));if(len<.001f)return {};normal=mul(normal,1/len);}
 if(dot(normal,delta)>=0)return {};return stage::CapsuleHit{(std::max)(0.f,lo),normal,0};
}
}
Decision Authority::launch_catapult(Slot& s,MountedState& m,const FireRequest& request,uint64_t now,uint32_t subMs){
 Decision out;auto reject=[&](Reject why){out.reject=why;return out;};auto&p=s.state;
 if(request.weapon!=p.weapon||m.type.kind!=mounted::Kind::catapult||s.flight)return reject(Reject::weapon);
 if(m.fired&&(now<m.fireAt||(now==m.fireAt&&subMs<m.subMs)))return reject(Reject::clock);
 if(m.fired&&now-m.fireAt<m.type.cooldownMs)return reject(Reject::interval);
 if(event_==UINT64_MAX)return reject(Reject::sequence);
 if(now>UINT64_MAX-m.type.maxFlightMs)return reject(Reject::clock);
 if(!finite(request.direction)||std::abs(dot(request.direction,request.direction)-1)>.002f)return reject(Reject::invalid_direction);
 Vec3 look{std::sin(p.pose.yaw)*std::cos(p.pose.pitch),std::sin(p.pose.pitch),std::cos(p.pose.yaw)*std::cos(p.pose.pitch)};
 if(dot(look,request.direction)<.999f)return reject(Reject::invalid_direction);
 if(!movement_||!movement_->clear(p.pose.feet,p.pose.capsule)||(targets_&&!targets_->clear(p.pose.feet,p.pose.capsule,stage::query::player)))return reject(Reject::obstructed);
 const auto velocity=mul(mounted::launch_direction(m.type,p.pose.yaw,p.pose.pitch),m.type.launchSpeed);
 if(!finite(velocity)||velocity[1]<=0)return reject(Reject::invalid_direction);
 const auto id=p.mountedId;const auto safe=s.mountedPreviousFeet;
 m.fireAt=now;m.subMs=subMs;m.fired=true;
 release_mounted(p.identity,true); // Restores the exact carried weapon and its fire clock.
 s.flight=CatapultFlight{velocity,safe,now,now,m.type.gravity,m.type.maxFlightMs};
 p.flightId=id;p.flightElapsedMs=0;p.blastFlight=false;p.aiming=false;s.falling.clear();
 s.approvedVelocity={};s.previousApprovedVelocity={};s.movementSpeed=0;++revision_;
 Event launched;launched.kind=EventKind::catapultLaunch;launched.source=p.identity;launched.object=id;launched.cue=948;launched.position=p.pose.feet;launched.normal=mounted::launch_direction(m.type,p.pose.yaw,p.pose.pitch);emit(out,launched);return out;
}
std::optional<Vec3> Authority::blast_velocity(const Slot& s,const burning::Blast& blast,const blast_motion::Policy& policy)const{
 if(!policy.enabled||!blast_motion::valid(policy)||s.state.specialPc.kind!=special_pc::Kind::human)return {};
 const auto& p=s.state;
 auto nearest=p.pose.feet;nearest[1]=(std::clamp)(blast.position[1],p.pose.feet[1]+p.pose.capsule.radius,p.pose.feet[1]+p.pose.capsule.height-p.pose.capsule.radius);
 const Vec3 delta{nearest[0]-blast.position[0],nearest[1]-blast.position[1],nearest[2]-blast.position[2]};
 const float distance=(std::max)(0.f,std::sqrt(dot(delta,delta))-p.pose.capsule.radius);
 const float scale=policy.minimumScale+(1-policy.minimumScale)*(1-(std::clamp)(distance/blast.radius,0.f,1.f));
 const float horizontal=std::hypot(delta[0],delta[2]);
 Vec3 velocity{0,policy.upwardSpeed*scale,0};
 if(horizontal>.001f){velocity[0]=delta[0]/horizontal*policy.horizontalSpeed*scale;velocity[2]=delta[2]/horizontal*policy.horizontalSpeed*scale;}
 // A second genuine explosion can add momentum; replayed explosions never
 // reach here. Apply the same bounded final velocity to living bodies/corpses.
 if(s.flight)velocity=add(velocity,s.flight->velocity);
 const float speed=std::sqrt(dot(velocity,velocity));if(!finite(velocity)||!std::isfinite(speed)||speed<.001f)return {};
 if(speed>blast_motion::maximum_velocity)velocity=mul(velocity,blast_motion::maximum_velocity/speed);
 return velocity;
}
bool Authority::launch_blast(Slot& s,Vec3 velocity,const blast_motion::Policy& policy,uint64_t now){
 auto& p=s.state;
 if(!p.alive||p.specialPc.kind!=special_pc::Kind::human||!movement_||
    !movement_->clear(p.pose.feet,p.pose.capsule)||(targets_&&!targets_->clear(p.pose.feet,p.pose.capsule,stage::query::player)))return false;
 // Release transport at the actual blast position, never teleport a seated
 // operator back to the pre-mount ground position before applying the impulse.
 release_mounted(p.identity,true);
 p.cover={};p.evadeKind=EvadeKind::none;p.evadeSerial=p.evadeElapsedMs=0;
 p.specialPhase=SpecialPhase::none;s.specialHeld=false;s.specialAt=now;
 s.ladderState.reset();p.ladderAnchor=0;p.reloadUntil=0;p.reloadLevel=0;p.reloadElapsedMs=0;s.reloadRefillAt=0;
 p.aiming=false;s.accuracy.reset();s.sopView.spreadMilliRadians=0;
 s.blastSerial=uint16_t(s.blastSerial%32767+1);
 s.flight=CatapultFlight{velocity,p.pose.feet,now,now,policy.gravity,policy.maxFlightMs};
 p.flightId=s.blastSerial;p.flightElapsedMs=0;p.blastFlight=true;s.poseAt=now;
 s.approvedVelocity={};s.previousApprovedVelocity={};s.movementSpeed=0;
 s.falling.clear();++revision_;return true;
}
bool Authority::recover_catapult(Slot& s,Vec3 safe){
 auto&p=s.state;if(!movement_)return false;
  auto clear=[&](Vec3 candidate){
   if(!movement_->clear(candidate,p.pose.capsule)||(targets_&&!targets_->clear(candidate,p.pose.capsule,stage::query::player)))return false;
   for(const auto& other:slots_)if(other&&other->state.alive&&other->state.identity!=p.identity){const auto&q=other->state.pose;if(std::hypot(candidate[0]-q.feet[0],candidate[2]-q.feet[2])<p.pose.capsule.radius+q.capsule.radius&&candidate[1]<q.feet[1]+q.capsule.height&&q.feet[1]<candidate[1]+p.pose.capsule.height)return false;}
   return true;
  };
  bool found=false;
  for(unsigned n=0;n<33&&!found;++n){auto candidate=safe;if(n){const float angle=float((n-1)%8)*.78539816339f,radius=float((n-1)/8+1)*600;candidate[0]+=std::sin(angle)*radius;candidate[2]+=std::cos(angle)*radius;auto probe=candidate;probe[1]+=1000;auto floor=movement_->ray(probe,{0,-1,0},2000,stage::query::floor);if(!floor||floor->normal[1]<.7071f)continue;candidate[1]=floor->position[1]+p.pose.capsule.skin;}if(clear(candidate)){p.pose.feet=candidate;found=true;}}
 return found;
}
void Authority::cancel_catapult(Slot& s,bool recover){
 if(!s.flight&&!s.state.flightId)return;auto&p=s.state;
 if(recover&&s.flight&&p.alive&&movement_){
  const bool found=recover_catapult(s,s.flight->safeFeet);
  // If a changed scene blocks every recovery point, remain HOST-controlled
  // and descend vertically; never hand an airborne stale pose to the client.
  if(!found&&active_){s.flight->velocity={};s.flight->born=s.flight->at;p.flightElapsedMs=0;return;}
 }
 s.flight.reset();p.flightId=p.flightElapsedMs=0;p.blastFlight=false;p.aiming=false;s.approvedVelocity={};s.previousApprovedVelocity={};++revision_;
}
void Authority::advance_catapults(uint64_t now){
 for(auto& entry:slots_)if(entry&&entry->flight){auto&s=*entry;auto&p=s.state;
  if(!active_||!p.alive||(p.stunned&&!p.blastFlight)){cancel_catapult(s,!active_);continue;}
  if(p.blastFlight&&now<s.flight->at)continue; // A backward HOST clock cannot rewind/relaunch an impulse.
  if(now<s.flight->at||now-s.flight->at>1000||now-s.flight->born>=s.flight->maximumMs){
   if(p.blastFlight){
    // No catch-up teleport or return to the grenade origin. Stop horizontal
    // travel and retain HOST gravity until checked landing or normal fall death.
    s.flight->velocity={};s.flight->born=s.flight->at=now;p.flightElapsedMs=0;++revision_;
   }else{s.flight->at=now;cancel_catapult(s,true);}
   continue;
  }
  bool landed=false;
  while(s.flight&&s.flight->at<now&&!landed){auto&f=*s.flight;const auto ms=(std::min)(uint64_t(20),now-f.at);const float dt=float(ms)*.001f;
   Vec3 displacement=mul(f.velocity,dt);displacement[1]-=.5f*f.gravity*dt*dt;f.velocity[1]-=f.gravity*dt;f.at+=ms;
   if(!finite(displacement)||!finite(add(p.pose.feet,displacement))){cancel_catapult(s,true);break;}
   // Up to three swept contacts let wall/ceiling collisions lose normal
   // velocity while gravity continues; remaining travel never crosses a wall.
   for(unsigned pass=0;pass<3&&dot(displacement,displacement)>.00001f;++pass){
    std::optional<stage::CapsuleHit> nearest;bool floorContact=false;
    for(const auto& geometry:{movement_,targets_})if(geometry)if(auto hit=geometry->sweep(p.pose.feet,displacement,p.pose.capsule,stage::query::player);hit&&(!nearest||hit->fraction<nearest->fraction)){nearest=hit;floorContact=hit->triangle<geometry->triangles.size()&&stage::query::floor.matches(geometry->triangles[hit->triangle].attribute);}
    bool peerContact=false;
    for(const auto& other:slots_)if(other&&other->state.alive&&other->state.identity!=p.identity)if(auto hit=body_hit(p.pose.feet,displacement,p.pose.capsule,other->state.pose);hit&&(!nearest||hit->fraction<nearest->fraction)){nearest=hit;peerContact=true;}
    if(!nearest){p.pose.feet=add(p.pose.feet,displacement);break;}
    const float fraction=(std::clamp)(nearest->fraction,0.f,1.f);p.pose.feet=add(p.pose.feet,mul(displacement,(std::max)(0.f,fraction-.0001f)));
    const auto normal=nearest->normal;const float approach=dot(f.velocity,normal);if(approach<0)f.velocity=add(f.velocity,mul(normal,-approach));
    if(!peerContact&&floorContact&&normal[1]>=.70710678f&&displacement[1]<0){landed=true;break;}
    displacement=mul(displacement,1-fraction);const float inward=dot(displacement,normal);if(inward<0)displacement=add(displacement,mul(normal,-inward));
   }
  }
  if(!s.flight)continue;
  if(landed){s.flight.reset();p.flightId=p.flightElapsedMs=0;p.blastFlight=false;s.approvedVelocity={};s.previousApprovedVelocity={};s.movementSpeed=0;}
  else p.flightElapsedMs=uint16_t((std::min)(uint64_t(31750),now-s.flight->born)/250*250);
  ++revision_;
 }
}
}
