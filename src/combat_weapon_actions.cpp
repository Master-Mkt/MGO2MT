#include "combat_authority.h"
#include "combat_ballistics.h"
#include "cover_hit_geometry.h"
#include "material_audio.h"
#include "original_hit_regions.h"
#include "weapon_projectiles.h"
#include "weapon_extension_policy.h"
#include "weapon_visual_policy.h"
#include "combat_burning.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace mgo2win::combat {
namespace {
float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vec3 sub(Vec3 a,Vec3 b){for(int i=0;i<3;++i)a[i]-=b[i];return a;}
Vec3 add(Vec3 a,Vec3 b){for(int i=0;i<3;++i)a[i]+=b[i];return a;}
Vec3 mul(Vec3 a,float b){for(auto&x:a)x*=b;return a;}
bool finite(Vec3 a){return std::all_of(a.begin(),a.end(),[](float f){return std::isfinite(f)&&std::abs(f)<1000000;});}
bool newer(uint32_t a,uint32_t b){return a!=b&&uint32_t(a-b)<0x80000000u;}
std::optional<float> ray_capsule(Vec3 ro,Vec3 rd,const Player&p){
 const float radius=p.pose.capsule.radius;auto a=p.pose.feet,b=a;a[1]+=radius;b[1]+=p.pose.capsule.height-radius;
 auto ba=sub(b,a),oa=sub(ro,a);float baba=dot(ba,ba),bard=dot(ba,rd),baoa=dot(ba,oa),rdoa=dot(rd,oa),oaoa=dot(oa,oa);
 float nearest=std::numeric_limits<float>::infinity();
 float segment=std::clamp(baoa/baba,0.f,1.f);auto closest=add(a,mul(ba,segment));if(dot(sub(ro,closest),sub(ro,closest))<=radius*radius)return 0.f;
 float A=baba-bard*bard,B=baba*rdoa-baoa*bard,C=baba*oaoa-baoa*baoa-radius*radius*baba;
 float discriminant=B*B-A*C;if(A>1e-5f&&discriminant>=0){float t=(-B-std::sqrt(discriminant))/A,y=baoa+t*bard;if(t>=0&&y>=0&&y<=baba)nearest=t;}
 for(auto center:{a,b}){auto oc=sub(ro,center);float q=dot(oc,rd),c=dot(oc,oc)-radius*radius,h=q*q-c;if(h>=0){float t=-q-std::sqrt(h);if(t>=0)nearest=std::min(nearest,t);}}
 return std::isfinite(nearest)?std::optional(nearest):std::nullopt;
}
}
Decision Authority::fire(Identity id,const FireRequest&r,uint64_t now,uint32_t subMsNs){
 advance_evade(now);
 Decision out;auto fail=[&](Reject why){out.reject=why;return out;};if(r.epoch!=epoch_)return fail(Reject::generation);auto s=slot(id);if(!s)return fail(Reject::identity);if(!r.life||r.life!=s->state.life)return fail(Reject::generation);
 if(subMsNs>=1000000)return fail(Reject::clock);
 advance_water(*s,now);
 if(s->fireSequenced&&!newer(r.sequence,s->fireSequence))return fail(Reject::sequence);s->fireSequence=r.sequence;s->fireSequenced=true;
 if(!active_)return fail(Reject::not_active);if(!s->state.alive||s->state.stunned)return fail(Reject::dead);if(now<s->poseAt||(s->fired&&(now<s->fireAt||(now==s->fireAt&&subMsNs<s->fireSubMsNs))))return fail(Reject::clock);if(now-s->poseAt>policy_.stalePoseMs)return fail(Reject::invalid_pose);
 if(s->state.specialPc.action!=special_pc::Action::none||s->state.specialPhase!=SpecialPhase::none||s->state.evadeKind!=EvadeKind::none||s->state.ladderAnchor)return fail(Reject::unavailable);
 if(special_pc::weapon(r.weapon)!=(s->state.specialPc.kind==special_pc::Kind::gekko)||r.weapon!=s->state.weapon||!weapons_.contains(r.weapon))return fail(Reject::weapon);const auto&w=weapons_.at(r.weapon);if(w.heldOnly)return fail(Reject::weapon);finish_reload(*s,now);
 if(s->state.reloadUntil)return fail(Reject::reloading);
 // Relative, bounded arithmetic also works near uint64 timestamp exhaustion.
 // Anchor to the last actual shot: a delayed tick cannot create catch-up fire.
 if(s->fired&&now-s->fireAt<=60000){
  const auto elapsed=int64_t((now-s->fireAt)*1000000)+int64_t(subMsNs)-s->fireSubMsNs;
  if(elapsed<int64_t(fireIntervalsNs_.at(w.id)))return fail(Reject::interval);
 }
 if(!s->state.ammo)return fail(Reject::no_ammo);
 if(!finite(r.direction))return fail(Reject::invalid_direction);float length=std::sqrt(dot(r.direction,r.direction));if(std::abs(length-1)>1e-3f)return fail(Reject::invalid_direction);
 Vec3 direction=mul(r.direction,1/length);const auto&pose=s->state.pose;Vec3 look{std::sin(pose.yaw)*std::cos(pose.pitch),std::sin(pose.pitch),std::cos(pose.yaw)*std::cos(pose.pitch)};
 if(dot(look,direction)<.999f)return fail(Reject::invalid_direction);
 auto accuracyCandidate=s->accuracy;
 // The native cone is centered on the accepted HOST pose, not on a request
 // direction anywhere inside the legacy dot-product validation tolerance.
 if(w.nativeAkAccuracy){auto ray=accuracyCandidate.direction(weapon_accuracy::native_ak,now,look,accuracy_seed(*s));if(!ray||!accuracyCandidate.accepted(weapon_accuracy::native_ak,now))return fail(Reject::clock);direction=*ray;}
 advance_cover();if(!cover::fire_allowed(*movement_,pose.feet,pose.capsule,pose.yaw,s->state.cover))return fail(Reject::obstructed);
 auto origin=cover::eye(pose.feet,pose.capsule,s->state.cover,pose.yaw);float distance=w.range;Vec3 normal{};uint32_t object=0;bool impact=false;Slot* victim=nullptr;
 auto* carried=holding(*s,s->state.weapon);if(!carried||carried->revision==UINT64_MAX)return fail(Reject::weapon);
 if(w.id==130||w.id==131){
  auto floor=movement_->ray({pose.feet[0],pose.feet[1]+10,pose.feet[2]},{0,-1,0},30);
  if(!floor||floor->normal[1]<.7f)return fail(Reject::invalid_pose);
  s->state.specialPc.action=special_pc::Action::kick;s->state.specialPc.serial=r.sequence?s->fireSequence:1;s->state.specialPc.elapsedMs=0;
  s->specialPcAt=now;s->specialPcStart=pose.feet;s->specialPcHit=false;s->specialMeleeWeapon=w.id;
  s->fireAt=now;s->fireSubMsNs=subMsNs;s->fired=true;++revision_;return out;
 }
 if(w.nativeProjectile){
  if(event_>UINT64_MAX-2)return fail(Reject::sequence);
  const auto&flight=w.id==129?projectile::native_gekko_missile:w.id==50?projectile::native_rpg7:projectile::native_white_phosphorus;
  if((w.id!=50&&w.id!=53&&w.id!=129)||projectiles_.scope().epoch!=epoch_)return fail(Reject::unavailable);
  const projectile::Owner owner{id.slot,id.instance,id.character,s->state.life};
  projectile::Shot request{projectiles_.scope(),owner,event_+1,w.id,origin,direction};
  // Same HOST thread: reserve simulation before consuming the magazine.
  // A rejected/capacity-exhausted flight never consumes ammunition or emits shot.
  if(projectiles_.spawn(request,flight,now)!=projectile::Submit::accepted)return fail(Reject::unavailable);
  if(!special_pc::weapon(w.id))--s->state.ammo;carried->contents.magazine=s->state.ammo;carried->contents.reserve=s->state.reserve;++carried->revision;
  s->fireAt=now;s->fireSubMsNs=subMsNs;s->fired=true;++revision_;
  Event shot;shot.kind=EventKind::shot;shot.source=id;shot.weapon=w.id;shot.cue=w.shotCue;shot.position=origin;shot.normal=direction;emit(out,shot);
  shot.kind=EventKind::projectile;shot.cue=0;emit(out,shot);return out;
 }
 if(!w.nativeAkPenetration)for(auto collision:{world_,targets_})if(collision)if(auto hit=collision->ray(origin,direction,distance)){distance=hit->distance;normal=hit->normal;object=collision->triangles[hit->triangle].object;impact=true;}
 uint8_t hitBone=0;
 for(auto&target:slots_)if(target&&target->state.identity!=id&&target->state.alive){
  std::optional<float> hit;uint8_t bone=0;
  if(w.nativeAkHitRegions&&target->state.specialPc.kind==special_pc::Kind::human){
   const auto&p=target->state.pose;
   auto stance=target->state.stunned||p.capsule.height==560?host_hit::Stance::prone:p.capsule.height==1100?host_hit::Stance::crouching:host_hit::Stance::standing;
   // Host has no trusted gender or animation phase. Fixed authored male pose
   // is an explicit proxy; it is never selected from informational GWAV data.
   if(auto region=cover::upper_hit(origin,direction,distance,p.feet,target->state.cover.attached?std::remainder(target->state.cover.normalYaw+3.14159265359f,6.28318530718f):p.yaw,stance,cover::eye_offset(target->state.cover,p.yaw))){hit=region->distance;bone=region->bone;}
  }else{hit=ray_capsule(origin,direction,target->state);if(target->state.cover.lean){auto upper=target->state;upper.pose.feet=add(upper.pose.feet,cover::eye_offset(upper.cover,upper.pose.yaw));upper.pose.feet[1]+=upper.pose.capsule.height*.5f;upper.pose.capsule.height*=.5f;upper.pose.capsule.radius=85;if(auto extra=ray_capsule(origin,direction,upper);extra&&(!hit||*extra<*hit))hit=extra;}}
  if(hit&&*hit<distance){distance=*hit;victim=&*target;hitBone=bone;object=0;impact=true;normal=mul(direction,-1);}
 }
 BallisticPath path;uint32_t damage=w.damage;int32_t hitForce=1000;
 if(w.nativeAkPenetration){path=trace_ak102(origin,direction,distance,*world_,targets_.get());if(path.blocked){distance=path.distance;victim=nullptr;impact=false;}if(victim){auto force=original_bullet_penetration::target_force(1000,0,distance,path.priorForceCost);auto hp=force?(*force<=0?std::optional<int>(0):original_bullet_penetration::ak102_base_hp(*force)):std::nullopt;if(!hp||*hp<0)return fail(Reject::unavailable);damage=uint32_t(*hp);hitForce=*force;}}
 if(victim&&w.nativeAkHitRegions){
  // Apply after force/base integer truncation. No authenticated original aim
  // state is available, so head/neck use the original non-HS branch.
  auto part=original_hit_regions::ak102_region_damage(int32_t(damage),int32_t(victim->state.maxHp),hitBone,uint16_t(hitForce),false,false);
  if(!part)return fail(Reject::unavailable);damage=uint32_t(part->damage);
  if(!policy_.freeForAll&&s->state.team&&victim->state.team==s->state.team)damage/=2;
 }
 if(!special_pc::weapon(w.id))--s->state.ammo;carried->contents.magazine=s->state.ammo;carried->contents.reserve=s->state.reserve;++carried->revision;s->fireAt=now;s->fireSubMsNs=subMsNs;s->fired=true;if(w.nativeAkAccuracy)s->accuracy=accuracyCandidate;++revision_;advance_accuracy(now);
 Event shot;shot.kind=EventKind::shot;shot.source=id;shot.weapon=w.id;shot.cue=w.shotCue;shot.position=origin;shot.normal=direction;shot.shotDistance=tracer_shot(w.id,uint16_t(s->state.ammo+1))?distance:0.f;emit(out,shot);
 if(w.nativeAkPenetration)for(const auto& hit:path.impacts){Event surface;surface.kind=EventKind::impact;surface.source=id;surface.weapon=w.id;surface.cue=material_audio::bullet_cue(material_audio::stage_for_map(w.materialMap),hit.material.id).value_or(0);surface.position=hit.position;surface.normal=hit.normal;surface.object=hit.object;emit(out,surface);}
 if(!impact)return out;
 Event event;event.kind=EventKind::impact;event.source=id;if(victim)event.target=victim->state.identity;event.weapon=w.id;event.cue=victim&&w.bodyCue?w.bodyCue:w.impactCue;event.position=add(origin,mul(direction,distance));event.normal=normal;event.object=object;emit(out,event);
 if(!victim||(s->state.team&&s->state.team==victim->state.team&&!policy_.friendlyFire&&!policy_.freeForAll))return out;
 regenerate(*victim,now);auto&v=victim->state;event.kind=EventKind::damage;event.cue=0;event.hpDamage=std::min(damage,v.hp);event.staminaDamage=std::min(w.staminaDamage,v.stamina);v.hp-=event.hpDamage;v.stamina-=event.staminaDamage;v.alive=v.hp!=0;v.stunned=v.alive&&v.stamina==0;event.hp=v.hp;event.stamina=v.stamina;++revision_;emit(out,event);
 if(!v.alive||v.stunned){release_special_pc(v.identity);v.specialPc.action=special_pc::Action::none;v.specialPc.serial=0;v.specialPc.elapsedMs=0;v.cover={};victim->accuracy.reset();victim->sopView.spreadMilliRadians=0;v.evadeKind=EvadeKind::none;v.evadeSerial=0;v.evadeElapsedMs=0;victim->ladderState.reset();v.ladderAnchor=0;}
 if(!v.alive){victim->regeneration.reset();clear_sop_slot(v.identity.slot);refresh_sop_views();}
 else if(v.stunned){v.specialPhase=SpecialPhase::none;victim->specialHeld=false;victim->specialAt=now;}
 if(!v.alive){if(auto& score=scores_[v.identity.slot];score&&score->deaths<1000000000)++score->deaths;
  if((policy_.freeForAll||!s->state.team||s->state.team!=v.team)&&scores_[id.slot]&&scores_[id.slot]->kills<1000000000)++scores_[id.slot]->kills;
  v.reloadUntil=0;v.reloadLevel=0;victim->reloadRefillAt=0;victim->burn.clear();v.burning=false;event.kind=EventKind::death;event.hpDamage=event.staminaDamage=0;emit(out,event);}return out;
}
Decision Authority::advance_projectiles(uint64_t now){
 Decision result;
 // Conservative native incarnation boundary. No delayed damage from a former
 // room/life, disconnected owner, or a round that is no longer active.
 if(!active_||projectiles_.scope().epoch!=epoch_){projectiles_.reset(projectiles_.scope());return result;}
 for(auto owner:projectiles_.owners()){
  auto source=slot({owner.slot,owner.instance,owner.character});
  if(!source||source->state.life!=owner.life||!source->state.alive)projectiles_.remove(owner);
 }
 auto step=projectiles_.advance(now,[&](Vec3 origin,Vec3 direction,float maximum,projectile::Owner source)->std::optional<projectile::Contact>{
  std::optional<projectile::Contact> nearest;float distance=maximum;
  for(const auto&world:{world_,targets_})if(world)if(auto hit=world->ray(origin,direction,distance)){
   nearest=projectile::Contact{hit->distance,hit->normal,{},world->triangles[hit->triangle].object};distance=hit->distance;}
  for(const auto&target:slots_)if(target&&target->state.alive&&target->state.identity!=Identity{source.slot,source.instance,source.character}){
   if(auto hit=ray_capsule(origin,direction,target->state);hit&&*hit<distance){const auto&id=target->state.identity;
    nearest=projectile::Contact{*hit,mul(direction,-1),projectile::Owner{id.slot,id.instance,id.character,target->state.life},0};distance=*hit;}
  }
  return nearest;
 });
 for(const auto&trail:step.trails){
  const Identity id{trail.owner.slot,trail.owner.instance,trail.owner.character};auto source=slot(id);
  if(!source||source->state.life!=trail.owner.life||!source->state.alive)continue;
  Event event;event.kind=EventKind::projectileTrail;event.source=id;event.weapon=trail.weapon;event.position=trail.position;emit(result,event);
 }
 for(const auto&hit:step.impacts){
  const Identity id{hit.owner.slot,hit.owner.instance,hit.owner.character};auto source=slot(id);
  if(!source||source->state.life!=hit.owner.life||!source->state.alive||hit.scope!=projectiles_.scope())continue;
  Event event;event.kind=EventKind::impact;event.source=id;event.weapon=hit.weapon;event.position=hit.position;event.normal=hit.normal;event.object=hit.object;
  if(hit.target){Identity target{hit.target->slot,hit.target->instance,hit.target->character};auto current=slot(target);if(current&&current->state.life==hit.target->life)event.target=target;}
  emit(result,event);
  if(hit.weapon!=50&&hit.weapon!=53&&hit.weapon!=129)continue;
  const burning::Key key{epoch_,id.slot,id.instance,id.character,hit.owner.life};
  // Shift an impact just off the surface toward the incoming side, so the
  // occlusion test starts outside the solid instead of intersecting at zero.
  auto center=add(hit.position,mul(hit.normal,2));
  burning::Blast blast{{key,hit.weapon,0,source->state.team},hit.acceptedShotId,center,
      hit.weapon!=53?5000.f:3500.f,hit.weapon!=53?1125u:0u,hit.weapon==53};
  auto damage=explode(blast,now);result.events.insert(result.events.end(),damage.events.begin(),damage.events.end());
 }
 return result;
}
}
