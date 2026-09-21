#include "combat_authority.h"
#include "combat_ballistics.h"
#include "cover_hit_geometry.h"
#include "material_audio.h"
#include "original_hit_regions.h"
#include "weapon_projectiles.h"
#include "projectile_body_contact.h"
#include "weapon_extension_policy.h"
#include "weapon_visual_policy.h"
#include "combat_burning.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace mgo2mt::combat {
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
Decision Authority::fire(Identity id,const FireRequest&r,uint64_t now,uint32_t subMsNs){return attack(id,r,now,subMsNs,false);}
Decision Authority::melee(Identity id,const FireRequest&r,uint64_t now){return attack(id,r,now,0,true);}
Decision Authority::attack(Identity id,const FireRequest&r,uint64_t now,uint32_t subMsNs,bool punch){
 advance_evade(now);
 Decision out;auto fail=[&](Reject why){out.reject=why;return out;};if(r.epoch!=epoch_)return fail(Reject::generation);auto s=slot(id);if(!s)return fail(Reject::identity);if(!r.life||r.life!=s->state.life)return fail(Reject::generation);
 if(subMsNs>=1000000)return fail(Reject::clock);
 advance_water(*s,now);
 if(s->fireSequenced&&!newer(r.sequence,s->fireSequence))return fail(Reject::sequence);s->fireSequence=r.sequence;s->fireSequenced=true;
 if(!active_)return fail(Reject::not_active);if(!s->state.alive||s->state.stunned)return fail(Reject::dead);if(now<s->poseAt||(s->fired&&(now<s->fireAt||(now==s->fireAt&&subMsNs<s->fireSubMsNs))))return fail(Reject::clock);if(now-s->poseAt>policy_.stalePoseMs)return fail(Reject::invalid_pose);
 if(s->state.specialPc.action!=special_pc::Action::none||s->state.specialPhase!=SpecialPhase::none||s->state.evadeKind!=EvadeKind::none||s->state.ladderAnchor||s->state.flightId)return fail(Reject::unavailable);
 auto* mount=mounted_state(*s);
 if(mount&&mount->type.kind==mounted::Kind::catapult){if(punch)return fail(Reject::unavailable);return launch_catapult(*s,*mount,r,now,subMsNs);}
 if(r.weapon!=s->state.weapon||(!punch&&(special_pc::weapon(r.weapon)!=(s->state.specialPc.kind==special_pc::Kind::gekko)||!weapons_.contains(r.weapon))))return fail(Reject::weapon);
 if(punch&&(s->state.mountedId||r.weapon==1||s->state.specialPc.kind!=special_pc::Kind::human||s->state.aiming))return fail(Reject::unavailable);
 if(s->state.mountedId&&(!mount||mount->type.weapon!=r.weapon))return fail(Reject::weapon);
 Weapon w;if(punch){w.id=r.weapon;w.staminaDamage=250;w.range=1250;w.intervalMs=750;w.meleeAttack=true;}else w=weapons_.at(r.weapon);
 if(w.heldOnly||(w.mountedOnly&&!mount))return fail(Reject::weapon);if(now<s->meleeUntil)return fail(Reject::interval);finish_reload(*s,now);
 if(s->state.reloadUntil)return fail(Reject::reloading);
 // Relative, bounded arithmetic also works near uint64 timestamp exhaustion.
 // Anchor to the last actual shot: a delayed tick cannot create catch-up fire.
 if(s->fired&&now-s->fireAt<=60000){
  const auto elapsed=int64_t((now-s->fireAt)*1000000)+int64_t(subMsNs)-s->fireSubMsNs;
  if(elapsed<int64_t(punch?750000000ull:fireIntervalsNs_.at(w.id)))return fail(Reject::interval);
 }
 if(!w.meleeAttack&&!s->state.ammo)return fail(Reject::no_ammo);
 if(!finite(r.direction))return fail(Reject::invalid_direction);float length=std::sqrt(dot(r.direction,r.direction));if(std::abs(length-1)>1e-3f)return fail(Reject::invalid_direction);
 Vec3 direction=mul(r.direction,1/length);const auto&pose=s->state.pose;Vec3 look{std::sin(pose.yaw)*std::cos(pose.pitch),std::sin(pose.pitch),std::cos(pose.yaw)*std::cos(pose.pitch)};
 if(dot(look,direction)<.999f)return fail(Reject::invalid_direction);
 auto accuracyCandidate=s->accuracy;
 const auto accuracyPolicy=punch?std::nullopt:weapon_accuracy_policy(w);
 const float accuracyScale=accuracyPolicy?accuracy_scale(*s,*accuracyPolicy,now):1.f;
 // The native cone is centered on the accepted HOST pose, not on a request
 // direction anywhere inside the legacy dot-product validation tolerance.
 if(accuracyPolicy){auto ray=accuracyCandidate.direction(*accuracyPolicy,now,look,accuracy_seed(*s),accuracyScale);if(!ray||!accuracyCandidate.accepted(*accuracyPolicy,now))return fail(Reject::clock);direction=*ray;}
 advance_cover();if(!cover::fire_allowed(*movement_,pose.feet,pose.capsule,pose.yaw,s->state.cover))return fail(Reject::obstructed);
 auto origin=cover::eye(pose.feet,pose.capsule,s->state.cover,pose.yaw);
 auto weaponQuery=w.meleeAttack?stage::CollisionQuery{}:stage::query::bullet;
 if(w.nativeProjectile){const auto category=projectile::collision_query(w.id);if(!category)return fail(Reject::weapon);weaponQuery=*category;}
 if(mount){const auto muzzle=mounted::muzzle_position(mount->instance,mount->type,pose.yaw,pose.pitch);const auto delta=sub(muzzle,origin);const float length=std::sqrt(dot(delta,delta));if(length>.01f)for(const auto& collision:{world_,targets_})if(collision)if(auto hit=collision->ray(origin,mul(delta,1/length),length,weaponQuery);hit&&hit->distance<length-4)return fail(Reject::obstructed);origin=muzzle;}
 auto* carried=mount?nullptr:holding(*s,s->state.weapon);if((!carried&&!punch&&!mount)||(carried&&carried->revision==UINT64_MAX))return fail(Reject::weapon);
 if(w.meleeAttack&&now>UINT64_MAX-w.intervalMs)return fail(Reject::clock);
 if(w.nativePlaced){
  if(traps_.size()>=64||now>UINT64_MAX-1000)return fail(Reject::unavailable);
  // A close aimed surface takes precedence; otherwise place on the floor
  // ahead. Surface normals come from the authoritative collision, never input.
  std::optional<stage::CollisionHit> support;
  for(const auto& world:{movement_,targets_})if(world)if(auto hit=world->ray(origin,direction,w.range);hit&&hit->normal[1]>-.5f&&(!support||hit->distance<support->distance))support=hit;
  if(!support){Vec3 trial{pose.feet[0]+std::sin(pose.yaw)*650,pose.feet[1]+500,pose.feet[2]+std::cos(pose.yaw)*650};
   for(const auto& world:{movement_,targets_})if(world)if(auto hit=world->ray(trial,{0,-1,0},1000,stage::query::player_floor);hit&&hit->normal[1]>=.5f&&(!support||hit->distance<support->distance))support=hit;}
  if(!support)return fail(Reject::invalid_pose);
  auto target=add(support->position,mul(support->normal,4));auto sight=sub(target,origin);const auto len=std::sqrt(dot(sight,sight));
  if(len>.01f)for(const auto& world:{movement_,targets_})if(world)if(auto hit=world->ray(origin,mul(sight,1/len),len);hit&&hit->distance<len-5)return fail(Reject::obstructed);
  auto placed=placedItems_.deploy({id.slot,id.instance,id.character,s->state.life},*carried,carried->revision,{target[0],target[1],target[2],pose.yaw,support->normal[0],support->normal[1],support->normal[2]});if(!placed||!placed.entity)return fail(Reject::unavailable);
  // Support charges advance automatically; Reload is the remote activator.
  // Conservation: deploy consumes one, this transfer never creates a charge.
  if(!carried->contents.magazine&&carried->contents.reserve){--carried->contents.reserve;++carried->contents.magazine;}
  s->state.ammo=uint16_t(carried->contents.magazine);s->state.reserve=uint16_t(carried->contents.reserve);s->fireAt=now;s->fireSubMsNs=subMsNs;s->fired=true;++revision_;
  Event event;event.kind=EventKind::shot;event.source=id;event.weapon=w.id;event.position=target;event.normal=direction;emit(out,event);
  traps_.push_back({*placed.entity,{{epoch_,id.slot,id.instance,id.character,s->state.life},w.id,0,s->state.team},event_,now+1000});return out;
 }
 if(w.id==130||w.id==131){
  auto floor=movement_->ray({pose.feet[0],pose.feet[1]+10,pose.feet[2]},{0,-1,0},30,stage::query::player_floor);
  if(!floor||floor->normal[1]<.7f)return fail(Reject::invalid_pose);
  s->state.specialPc.action=special_pc::Action::kick;s->state.specialPc.serial=r.sequence?s->fireSequence:1;s->state.specialPc.elapsedMs=0;
  s->specialPcAt=now;s->specialPcStart=pose.feet;s->specialPcHit=false;s->specialMeleeWeapon=w.id;
  s->fireAt=now;s->fireSubMsNs=subMsNs;s->fired=true;++revision_;return out;
 }
 if(w.nativeProjectile){
  if(mount&&mount->type.kind!=mounted::Kind::mortar)return fail(Reject::weapon);
  if(event_>UINT64_MAX-2)return fail(Reject::sequence);
  auto flight=w.id==129?projectile::native_gekko_missile:w.id==50?projectile::native_rpg7:projectile::native_throw(w.id);flight.range=w.range;
  if(w.id==103){if(!mount||mount->type.kind!=mounted::Kind::mortar)return fail(Reject::weapon);const auto&t=mount->type;if(mount->fired&&now-mount->fireAt<t.cooldownMs)return fail(Reject::interval);flight={103,t.launchSpeed,t.gravity,w.range,t.maxFlightMs,0,true,0,600,t.blastRadius};direction=mounted::launch_direction(t,pose.yaw,pose.pitch);}
  if((w.id!=50&&!projectile::throwable(w.id)&&w.id!=129&&w.id!=103)||projectiles_.scope().epoch!=epoch_)return fail(Reject::unavailable);
  const projectile::Owner owner{id.slot,id.instance,id.character,s->state.life};
  projectile::Shot request{projectiles_.scope(),owner,event_+1,w.id,origin,direction};
  // Same HOST thread: reserve simulation before consuming the magazine.
  // A rejected/capacity-exhausted flight never consumes ammunition or emits shot.
  if(projectiles_.spawn(request,flight,now)!=projectile::Submit::accepted)return fail(Reject::unavailable);
  if(!special_pc::weapon(w.id)&&(!mount||!mount->type.infiniteAmmo))--s->state.ammo;if(mount){mount->ammo=s->state.ammo;mount->fireAt=now;mount->subMs=subMsNs;mount->fired=true;}if(carried){carried->contents.magazine=s->state.ammo;carried->contents.reserve=s->state.reserve;++carried->revision;}
  s->fireAt=now;s->fireSubMsNs=subMsNs;s->fired=true;++revision_;
  Event shot;shot.kind=EventKind::shot;shot.source=id;shot.weapon=w.id;shot.cue=w.shotCue;shot.position=origin;shot.normal=direction;shot.object=w.id==103&&mount?mount->instance.id:0;emit(out,shot);
  shot.kind=EventKind::projectile;shot.cue=0;shot.object=0;emit(out,shot);return out;
 }
 // Trace every pellet against one accepted pose before committing one shot.
 // Each ray has independent HOST randomness and a bounded share of the single
 // configured trigger damage budget. Pellets do not consume extra cartridges.
 const unsigned pelletCount=accuracyPolicy?accuracyPolicy->pellets:1;
 struct RayResult {Vec3 direction{},normal{};float distance=0;uint32_t object=0;bool impact=false;Slot* victim=nullptr;BallisticPath path;uint32_t damage=0,staminaDamage=0;};
 std::vector<RayResult> rays;rays.reserve(pelletCount);
 for(unsigned pellet=0;pellet<pelletCount;++pellet){
  if(pellet){auto ray=s->accuracy.direction(*accuracyPolicy,now,look,accuracy_seed(*s)^weapon_accuracy::mix(pellet),accuracyScale);if(!ray)return fail(Reject::clock);direction=*ray;}
  float distance=w.range;Vec3 normal{};uint32_t object=0;bool impact=false;Slot* victim=nullptr;
 if(!w.nativeAkPenetration&&!w.originalFirearm)for(auto collision:{world_,targets_})if(collision)if(auto hit=collision->ray(origin,direction,distance,weaponQuery)){distance=hit->distance;normal=hit->normal;object=collision->triangles[hit->triangle].object;impact=true;}
 uint8_t hitBone=0;
 for(auto&target:slots_)if(target&&target->state.identity!=id&&target->state.alive){
  std::optional<float> hit;uint8_t bone=0;
  if((w.nativeAkHitRegions||w.originalFirearm)&&target->state.specialPc.kind==special_pc::Kind::human){
   const auto&p=target->state.pose;
   auto stance=target->state.stunned||p.capsule.height==560?host_hit::Stance::prone:p.capsule.height==1100?host_hit::Stance::crouching:host_hit::Stance::standing;
   // Host has no trusted gender or animation phase. Fixed authored male pose
   // is an explicit proxy; it is never selected from informational GWAV data.
   if(auto region=cover::upper_hit(origin,direction,distance,p.feet,target->state.cover.attached?std::remainder(target->state.cover.normalYaw+3.14159265359f,6.28318530718f):p.yaw,stance,cover::eye_offset(target->state.cover,p.yaw))){hit=region->distance;bone=region->bone;}
  }else{hit=ray_capsule(origin,direction,target->state);if(target->state.cover.lean){auto upper=target->state;upper.pose.feet=add(upper.pose.feet,cover::eye_offset(upper.cover,upper.pose.yaw));upper.pose.feet[1]+=upper.pose.capsule.height*.5f;upper.pose.capsule.height*=.5f;upper.pose.capsule.radius=85;if(auto extra=ray_capsule(origin,direction,upper);extra&&(!hit||*extra<*hit))hit=extra;}}
  if(hit&&*hit<distance){distance=*hit;victim=&*target;hitBone=bone;object=0;impact=true;normal=mul(direction,-1);}
 }
 BallisticPath path;uint32_t damage=w.damage,staminaDamage=w.staminaDamage;int32_t hitForce=1000;
 if(w.originalFirearm){const auto* original=original_weapon::find(w.behaviorSourceId?w.behaviorSourceId:w.id);if(!w.ballistics&&!original)return fail(Reject::weapon);const auto bullet=w.ballistics?*w.ballistics:original->bullet;path=trace_ak102(origin,direction,distance,*world_,targets_.get(),bullet.penetration);if(path.blocked){distance=path.distance;victim=nullptr;impact=false;}if(victim){hitForce=original_weapon::force(bullet,distance,path.priorForceCost);damage=uint32_t(std::max(int64_t(0),int64_t(w.damage)*hitForce/1000));staminaDamage=uint32_t(std::max(int64_t(0),int64_t(w.staminaDamage)*hitForce/1000));}}
 if(w.nativeAkPenetration){path=w.ballistics?trace_ak102(origin,direction,distance,*world_,targets_.get(),w.ballistics->penetration):trace_ak102(origin,direction,distance,*world_,targets_.get());if(path.blocked){distance=path.distance;victim=nullptr;impact=false;}if(victim){if(w.ballistics){hitForce=original_weapon::force(*w.ballistics,distance,path.priorForceCost);damage=uint32_t(std::max(int64_t(0),int64_t(w.damage)*hitForce/1000));staminaDamage=uint32_t(std::max(int64_t(0),int64_t(w.staminaDamage)*hitForce/1000));}else{auto force=original_bullet_penetration::target_force(1000,0,distance,path.priorForceCost);auto hp=force?(*force<=0?std::optional<int>(0):original_bullet_penetration::ak102_base_hp(*force)):std::nullopt;if(!hp||*hp<0)return fail(Reject::unavailable);damage=uint32_t(*hp);hitForce=*force;}}}
 if(victim&&(w.nativeAkHitRegions||w.originalFirearm)){
  // Apply after force/base integer truncation. No authenticated original aim
  // state is available, so head/neck use the original non-HS branch.
  auto part=original_hit_regions::ak102_region_damage(int32_t(damage),int32_t(victim->state.maxHp),hitBone,uint16_t(hitForce),false,false);
  if(!part)return fail(Reject::unavailable);damage=uint32_t(part->damage);
  if(!policy_.freeForAll&&s->state.team&&victim->state.team==s->state.team)damage/=2;
 }
  if(pelletCount>1){
   const uint32_t hpBudget=w.damage/pelletCount+(pellet<w.damage%pelletCount),staminaBudget=w.staminaDamage/pelletCount+(pellet<w.staminaDamage%pelletCount);
   damage=(std::min)(hpBudget,damage/pelletCount+(pellet<damage%pelletCount));staminaDamage=(std::min)(staminaBudget,staminaDamage/pelletCount+(pellet<staminaDamage%pelletCount));
  }
  rays.push_back({direction,normal,distance,object,impact,victim,std::move(path),damage,staminaDamage});
 }
 if(!special_pc::weapon(w.id)&&!w.meleeAttack&&(!mount||!mount->type.infiniteAmmo))--s->state.ammo;if(mount){mount->ammo=s->state.ammo;mount->fireAt=now;mount->subMs=subMsNs;mount->fired=true;}if(carried){carried->contents.magazine=s->state.ammo;carried->contents.reserve=s->state.reserve;++carried->revision;}if(w.meleeAttack)s->meleeUntil=now+w.intervalMs;s->fireAt=now;s->fireSubMsNs=subMsNs;s->fired=true;if(accuracyPolicy)s->accuracy=accuracyCandidate;++revision_;advance_accuracy(now);
 Event shot;shot.kind=punch?EventKind::melee:EventKind::shot;shot.source=id;shot.weapon=w.id;shot.cue=w.shotCue;shot.position=origin;shot.normal=rays.front().direction;shot.shotDistance=!w.meleeAttack&&tracer_shot(w.id,uint16_t(s->state.ammo+1))?rays.front().distance:0.f;emit(out,shot);
 for(auto& ray:rays){
  const auto direction=ray.direction,normal=ray.normal;const float distance=ray.distance;const auto object=ray.object;const bool impact=ray.impact;auto* victim=ray.victim;const auto&path=ray.path;const auto damage=ray.damage,staminaDamage=ray.staminaDamage;
 if(w.nativeAkPenetration||w.originalFirearm)for(const auto& hit:path.impacts){Event surface;surface.kind=EventKind::impact;surface.source=id;surface.weapon=w.id;surface.cue=material_audio::bullet_cue(material_audio::stage_for_map(w.materialMap),hit.material.id).value_or(0);surface.position=hit.position;surface.normal=hit.normal;surface.object=hit.object;emit(out,surface);}
 if(!impact)continue;
 Event event;event.kind=EventKind::impact;event.source=id;if(victim)event.target=victim->state.identity;event.weapon=w.id;event.cue=victim&&w.bodyCue?w.bodyCue:w.impactCue;event.position=add(origin,mul(direction,distance));event.normal=normal;event.object=object;emit(out,event);
 if(!victim||!victim->state.alive||(s->state.team&&s->state.team==victim->state.team&&!policy_.friendlyFire&&!policy_.freeForAll))continue;
 regenerate(*victim,now);auto&v=victim->state;event.kind=EventKind::damage;event.cue=0;event.hpDamage=std::min(damage,v.hp);event.staminaDamage=std::min(staminaDamage,v.stamina);v.hp-=event.hpDamage;v.stamina-=event.staminaDamage;v.alive=v.hp!=0;v.stunned=v.alive&&v.stamina==0;event.hp=v.hp;event.stamina=v.stamina;++revision_;emit(out,event);
 if(!v.alive||v.stunned){release_special_pc(v.identity);v.specialPc.action=special_pc::Action::none;v.specialPc.serial=0;v.specialPc.elapsedMs=0;v.cover={};victim->accuracy.reset();victim->sopView.spreadMilliRadians=0;v.evadeKind=EvadeKind::none;v.evadeSerial=0;v.evadeElapsedMs=0;victim->ladderState.reset();v.ladderAnchor=0;}
 if(!v.alive){victim->regeneration.reset();clear_sop_slot(v.identity.slot);refresh_sop_views();}
 else if(v.stunned){v.specialPhase=SpecialPhase::none;victim->specialHeld=false;victim->specialAt=now;}
 if(!v.alive){if(auto& score=scores_[v.identity.slot];score&&score->deaths<1000000000)++score->deaths;
  if((policy_.freeForAll||!s->state.team||s->state.team!=v.team)&&scores_[id.slot]&&scores_[id.slot]->kills<1000000000)++scores_[id.slot]->kills;
  v.reloadUntil=0;v.reloadLevel=0;victim->reloadRefillAt=0;victim->burn.clear();v.burning=false;event.kind=EventKind::death;event.hpDamage=event.staminaDamage=0;emit(out,event);}
 }
 return out;
}
Decision Authority::advance_traps(uint64_t now,std::optional<Identity> detonate,uint16_t weapon){
 Decision out;const auto entities=placedItems_.state();std::vector<Trap> keep;keep.reserve(64);bool remoteConsumed=false;
 for(const auto& trap:traps_){
  auto found=std::find_if(entities.entities.begin(),entities.entities.end(),[&](const items::Entity& e){return e.key==trap.entity.key&&e.revision==trap.entity.revision;});
  if(found==entities.entities.end())continue; // Recovered/removed entities cannot trigger stale traps.
  const Identity owner{trap.source.actor.slot,trap.source.actor.instance,trap.source.actor.character};const auto* source=slot(owner);
  if(!active_||!source||source->state.life!=trap.source.actor.life){placedItems_.erase_deployed(trap.entity.key,trap.entity.revision);continue;}
  if(now<trap.armedAt){keep.push_back(trap);continue;}
  const uint16_t id=trap.source.weapon;const auto profile=weapons_.find(id);if(profile==weapons_.end())continue;const auto& configured=profile->second;const auto& p=trap.entity.position;Vec3 position{p.x,p.y,p.z};
  const bool remote=id==66||id==67;bool trigger=remote&&!remoteConsumed&&detonate&&*detonate==owner&&weapon==id;
  Slot* reader=nullptr;
  if(!remote)for(auto& target:slots_)if(target&&target->state.alive&&target->state.identity!=owner&&(policy_.freeForAll||!trap.source.team||target->state.team!=trap.source.team)){
   const auto delta=sub(target->state.pose.feet,position);const float distance=std::sqrt(dot(delta,delta));
   if(distance>(id==69?850.f:1500.f))continue;
   // Native directional mine cone and bounded one-second arming delay.
   if(id!=69&&distance>1&&dot(delta,std::abs(p.ny)<.5f?Vec3{p.nx,p.ny,p.nz}:Vec3{std::sin(p.yaw),0,std::cos(p.yaw)})/distance<.34f)continue;
   burning::Blast area{trap.source,trap.serial,position,1600,1,false};
   if(!burning::exposed(area,target->state.pose.feet,target->state.pose.capsule,world_.get(),targets_.get()))continue;
   trigger=true;reader=&*target;break;
  }
  if(!trigger){keep.push_back(trap);continue;}
  if(!placedItems_.erase_deployed(trap.entity.key,trap.entity.revision)){keep.push_back(trap);continue;}
  if(remote)remoteConsumed=true;
  ++revision_;
  if(id==69&&reader){reader->grenadeStamina=std::max(reader->grenadeStamina,reader->state.stamina);reader->state.stamina=0;reader->state.stunned=true;reader->grenadeStunUntil=now<=UINT64_MAX-5000?now+5000:UINT64_MAX;release_special_pc(reader->state.identity);reader->state.cover={};reader->state.specialPhase=SpecialPhase::none;reader->specialHeld=false;reader->state.evadeKind=EvadeKind::none;reader->state.evadeSerial=0;reader->state.evadeElapsedMs=0;reader->state.reloadUntil=0;reader->state.reloadLevel=0;reader->reloadRefillAt=0;reader->ladderState.reset();reader->state.ladderAnchor=0;continue;}
  burning::Blast blast{trap.source,trap.serial,position,5000,configured.damage,false,configured.staminaDamage};
  auto effect=explode(blast,now);out.events.insert(out.events.end(),effect.events.begin(),effect.events.end());
 }
 traps_=std::move(keep);return out;
}
Decision Authority::advance_projectiles(uint64_t now){
 Decision result=advance_traps(now);
 // Conservative native incarnation boundary. No delayed damage from a former
 // room/life, disconnected owner, or a round that is no longer active.
 if(!active_||projectiles_.scope().epoch!=epoch_){projectiles_.reset(projectiles_.scope());return result;}
 for(auto owner:projectiles_.owners()){
  auto source=slot({owner.slot,owner.instance,owner.character});
  if(!source||source->state.life!=owner.life||!source->state.alive)projectiles_.remove(owner);
 }
 auto step=projectiles_.advance_typed(now,[&](Vec3 origin,Vec3 direction,float maximum,projectile::Owner source,uint16_t weapon)->std::optional<projectile::Contact>{
  std::optional<projectile::Contact> nearest;float distance=maximum;
  const auto category=projectile::collision_query(weapon);if(!category)throw std::logic_error("Unsupported accepted projectile category");
  for(const auto&world:{world_,targets_})if(world)if(auto hit=world->ray(origin,direction,distance,*category)){
   nearest=projectile::Contact{hit->distance,hit->normal,{},world->triangles[hit->triangle].object};distance=hit->distance;}
  for(const auto&target:slots_)if(target&&target->state.alive&&target->state.identity!=Identity{source.slot,source.instance,source.character}){
   if(auto hit=ray_capsule(origin,direction,target->state);hit&&*hit<distance){const auto&id=target->state.identity;
    const auto&pose=target->state.pose;const auto contact=add(origin,mul(direction,*hit));
    nearest=projectile::Contact{*hit,projectile::capsule_surface_normal(contact,pose.feet,pose.capsule,direction),projectile::Owner{id.slot,id.instance,id.character,target->state.life},0};distance=*hit;}
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
  const auto profile=weapons_.find(hit.weapon);if(profile==weapons_.end())continue;const auto& configured=profile->second;
  Event event;event.kind=EventKind::impact;event.source=id;event.weapon=hit.weapon;event.position=hit.position;event.normal=hit.normal;event.object=hit.object;
  if(hit.target){Identity target{hit.target->slot,hit.target->instance,hit.target->character};auto current=slot(target);if(current&&current->state.life==hit.target->life)event.target=target;}
  emit(result,event);
  if(hit.weapon!=50&&!projectile::throwable(hit.weapon)&&hit.weapon!=129&&hit.weapon!=103)continue;
  if(hit.weapon>=56&&hit.weapon<=59){Event smoke;smoke.kind=EventKind::smoke;smoke.source=id;smoke.weapon=hit.weapon;smoke.position=hit.position;emit(result,smoke);continue;}
  if(hit.weapon==55||hit.weapon==63){
   burning::Blast area{{{epoch_,id.slot,id.instance,id.character,hit.owner.life},hit.weapon,0,source->state.team},hit.acceptedShotId,hit.position,5000,1,false};
   for(auto& target:slots_)if(target&&target->state.alive&&burning::exposed(area,target->state.pose.feet,target->state.pose.capsule,world_.get(),targets_.get())){
    if(hit.weapon==55){sop_jam(target->state.identity,true,now);target->grenadeJamUntil=now<=UINT64_MAX-12000?now+12000:UINT64_MAX;}
    else if(target->state.specialPc.kind==special_pc::Kind::gekko){target->grenadeStamina=std::max(target->grenadeStamina,target->state.stamina);target->state.stamina=0;target->state.stunned=true;target->grenadeStunUntil=now<=UINT64_MAX-5000?now+5000:UINT64_MAX;release_special_pc(target->state.identity);++revision_;}
   }
   Event pulse;pulse.kind=EventKind::explosion;pulse.source=id;pulse.weapon=hit.weapon;pulse.position=hit.position;emit(result,pulse);continue;
  }
  const burning::Key key{epoch_,id.slot,id.instance,id.character,hit.owner.life};
  // Shift an impact just off the surface toward the incoming side, so the
  // occlusion test starts outside the solid instead of intersecting at zero.
  auto center=add(hit.position,mul(hit.normal,2));
  burning::Blast blast{{key,hit.weapon,0,source->state.team},hit.acceptedShotId,center,
      hit.blastRadius>0?hit.blastRadius:hit.weapon!=53?5000.f:3500.f,configured.damage,hit.weapon==53,configured.staminaDamage};
  auto damage=explode(blast,now);result.events.insert(result.events.end(),damage.events.begin(),damage.events.end());
 }
 return result;
}
}
