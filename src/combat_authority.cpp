#include "combat_authority.h"
#include "combat_ballistics.h"
#include "material_audio.h"
#include "original_mastery_policy.h"
#include "host_hit_geometry.h"
#include "original_hit_regions.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
namespace mgo2win::combat {
namespace {
float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vec3 sub(Vec3 a,Vec3 b){for(int i=0;i<3;++i)a[i]-=b[i];return a;}
Vec3 add(Vec3 a,Vec3 b){for(int i=0;i<3;++i)a[i]+=b[i];return a;}
Vec3 mul(Vec3 a,float b){for(auto&x:a)x*=b;return a;}
bool finite(Vec3 a){return std::all_of(a.begin(),a.end(),[](float f){return std::isfinite(f)&&std::abs(f)<1000000;});}
bool valid(Identity id){return id.slot<24&&id.instance&&id.character;}
bool valid(const Pose&p){return finite(p.feet)&&std::isfinite(p.yaw)&&std::abs(p.yaw)<=3.14159274f&&std::isfinite(p.pitch)&&std::abs(p.pitch)<=1.4f&&(p.capsule.radius==260||p.capsule.radius==350)&&p.capsule.skin==2&&p.capsule.height>=p.capsule.radius*2&&(p.capsule.height==1700||p.capsule.height==1100||p.capsule.height==560);}
bool newer(uint32_t a,uint32_t b){return a!=b&&uint32_t(a-b)<0x80000000u;}
Vec3 eye(const Player&p){auto v=p.pose.feet;v[1]+=p.pose.capsule.height-150;return v;}
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
Player expanded(const Player&other,stage::Capsule moving){auto p=other;p.pose.feet[1]-=moving.height;p.pose.capsule.radius+=moving.radius;p.pose.capsule.height+=moving.height;return p;}
}
Authority::Authority(Policy policy):policy_(policy){if(!std::isfinite(policy.maxHorizontalSpeed)||policy.maxHorizontalSpeed<=0||policy.maxHorizontalSpeed>10000||!std::isfinite(policy.maxVerticalSpeed)||policy.maxVerticalSpeed<=0||policy.maxVerticalSpeed>30000||!policy.stalePoseMs||policy.stalePoseMs>5000)throw std::invalid_argument("Combat policy");items::DropPolicy nativeAk;nativeAk.drop=items::DropOverride::allow;itemPolicies_.emplace(25,nativeAk);}
items::HeldSlot* Authority::holding(Slot& s,uint16_t weapon){if(!weapon)return nullptr;for(auto& h:s.inventory)if(h.contents.domain==items::Domain::weapon&&h.contents.item==weapon&&h.contents.quantity)return &h;return nullptr;}
Authority::Slot* Authority::slot(Identity id){return valid(id)&&slots_[id.slot]&&slots_[id.slot]->state.identity==id?&*slots_[id.slot]:nullptr;}
const Authority::Slot* Authority::slot(Identity id)const{return valid(id)&&slots_[id.slot]&&slots_[id.slot]->state.identity==id?&*slots_[id.slot]:nullptr;}
void Authority::begin(uint64_t epoch,std::shared_ptr<const stage::Collision> world,std::span<const Weapon> weapons,std::shared_ptr<const stage::Collision> targets){
 if(!epoch||epoch<=epoch_||!world||weapons.size()>128)throw std::invalid_argument("Combat world/epoch");
 std::map<uint16_t,Weapon> checked;std::map<uint16_t,uint64_t> intervals;for(auto w:weapons){
  uint64_t interval=uint64_t(w.intervalMs)*1000000;
  if(w.fireIntervalTicks){
   auto original=original::fire_interval_ns(*w.fireIntervalTicks);
   if(!original||w.intervalMs)throw std::invalid_argument("Combat fire interval profile");
   interval=*original;w.intervalMs=uint32_t((interval+999999)/1000000);
  }
  if(w.nativeAkPenetration&&(w.id!=25||w.damage!=275||w.staminaDamage||w.range!=200000))throw std::invalid_argument("Native AK penetration profile");
  if(w.nativeAkHitRegions&&(!w.nativeAkPenetration||w.id!=25))throw std::invalid_argument("Native AK hit region profile");
  if(w.nativePrimaryMastery&&(w.id!=25||!w.reloadMotion||w.reloadMotion->baseTick!=5||w.reloadMotion->intervals!=210||w.reloadMotion->refillTick!=650||w.reloadMotion->rate!=1.f))throw std::invalid_argument("Combat native mastery profile");
  if(w.reloadMotion){
   auto timing=original::reload_timing(*w.reloadMotion);
   if(!timing||w.reloadMs||w.reloadRefillMs)throw std::invalid_argument("Combat reload motion profile");
   w.reloadMs=timing->endMs;w.reloadRefillMs=timing->refillMs;
  }
  if(!w.id||(!w.damage&&!w.staminaDamage)||w.damage>1000000||w.staminaDamage>1000000||!w.intervalMs||w.intervalMs>60000||!w.reloadMs||w.reloadMs>60000||w.reloadRefillMs>w.reloadMs||w.reserve>10000||!w.magazine||w.magazine>1000||!std::isfinite(w.range)||w.range<=0||w.range>1000000||!checked.emplace(w.id,w).second)throw std::invalid_argument("Combat weapon profile");
  intervals.emplace(w.id,interval);
 }
 slots_={};scores_={};skills_={};sopGroups_.reset();spawnLife_.reset();water_.reset();waterRatio_=.65f;weapons_=std::move(checked);fireIntervalsNs_=std::move(intervals);movement_=stage::movement_collision(world);world_=std::move(world);targets_=std::move(targets);epoch_=epoch;revision_=1;event_=0;active_=false;itemGeneration_=1;placedItems_.reset({epoch_,itemGeneration_});
}
bool Authority::world(std::shared_ptr<const stage::Collision> world,std::shared_ptr<const stage::Collision> targets){if(!world||!epoch_)return false;movement_=stage::movement_collision(world);world_=std::move(world);targets_=std::move(targets);++revision_;return true;}
bool Authority::water(std::shared_ptr<const stage::Water> water,float ratio){if(!std::isfinite(ratio)||ratio<=0||ratio>1)return false;water_=std::move(water);waterRatio_=ratio;return true;}
float Authority::water_scale(const Pose&p)const{
 if(!water_||!movement_)return 1;auto origin=p.feet;origin[1]+=p.capsule.skin*2;auto floor=movement_->ray(origin,{0,-1,0},p.capsule.height+p.capsule.skin*4);
 if(!floor||floor->normal[1]<.70710678f)return 1;auto level=water_->control_level(p.feet,floor->position[1]);return level&&stage::Water::classify(*level,floor->position[1],p.feet[1])==stage::WaterFoot::inWater?waterRatio_:1.f;
}
bool Authority::install_loadout(const host_skills::Verified&v){
 const auto&s=v.scope();Identity id{s.slot,s.instance,s.character};
 if(s.epoch!=epoch_||!valid(id)||slot(id))return false;
 auto&entry=skills_[id.slot];if(entry&&(entry->id!=id||entry->frozen||entry->verified))return false;
 entry=Skills{id,v,false};return true;
}
bool Authority::join(Identity id,uint8_t team,const Pose&p,uint32_t hp,uint32_t stamina,std::span<const uint16_t> inventory,uint64_t now){
 if(!epoch_||!world_||!valid(id)||team>2||(policy_.freeForAll&&team!=0)||!valid(p)||!hp||hp>1000000||!stamina||stamina>1000000||inventory.empty()||inventory.size()>3||slots_[id.slot]||!movement_->clear(p.feet,p.capsule))return false;
 for(const auto&s:slots_)if(s&&s->state.identity.character==id.character)return false;
 for(const auto&s:slots_)if(s&&s->state.alive){auto hit=ray_capsule(p.feet,{0,1,0},expanded(s->state,p.capsule));if(hit&&*hit==0)return false;}
 std::set<uint16_t> ids;for(auto w:inventory)if(!weapons_.contains(w)||!ids.insert(w).second)return false;
 auto&frozen=skills_[id.slot];if(frozen&&frozen->id!=id)return false;
 if(!frozen)frozen=Skills{id,{},false};frozen->frozen=true;
 Slot s;const auto&initial=weapons_.at(inventory.front());s.state={id,team,p,hp,hp,stamina,stamina,inventory.front(),initial.magazine,initial.reserve,0,true,false};for(size_t i=0;i<inventory.size();++i){const auto w=inventory[i];s.inventory[i].contents={w,1,weapons_.at(w).magazine,weapons_.at(w).reserve,0,items::Resource::ammunition};}if(frozen->verified){s.state.verifiedSkills=true;s.state.masteryLevel=frozen->verified->level(3);s.state.surveyorLevel=frozen->verified->level(7);}
 s.poseAt=now;s.sopView.recipient=id;s.sopView.life=s.state.life;if(!spawnLife_&&!placedItems_.admit({id.slot,id.instance,id.character,s.state.life}))return false;if(!scores_[id.slot]||scores_[id.slot]->id!=id)scores_[id.slot]=Score{id};slots_[id.slot]=std::move(s);if(!spawnLife_){sopGroups_.clear(id.slot);refresh_sop_views();}++revision_;return true;
}
bool Authority::leave(Identity id){if(valid(id)&&skills_[id.slot]&&skills_[id.slot]->id==id)skills_[id.slot].reset();if(!slot(id))return false;placedItems_.remove({id.slot,id.instance,id.character,slot(id)->state.life});slots_[id.slot].reset();sopGroups_.clear(id.slot);refresh_sop_views();++revision_;return true;}
bool Authority::respawn(Identity id,uint32_t nextLife,const std::function<bool()>&grant){
 auto current=slot(id);if(!current||current->state.alive||!nextLife||current->state.life==std::numeric_limits<uint32_t>::max()||nextLife!=current->state.life+1||!grant||spawnLife_)return false;
 const auto previousSkills=skills_[id.slot];auto previous=std::move(slots_[id.slot]);const auto revision=revision_;const auto event=event_;slots_[id.slot].reset();spawnLife_=std::pair{id,nextLife};
 auto restore=[&]{skills_[id.slot]=previousSkills;slots_[id.slot]=std::move(previous);revision_=revision;event_=event;spawnLife_.reset();};
 try{if(!grant()||!slot(id)||!slot(id)->state.alive){restore();return false;}}catch(...){restore();throw;}
 slots_[id.slot]->state.life=nextLife;slots_[id.slot]->itemSequence=0;placedItems_.admit({id.slot,id.instance,id.character,nextLife});spawnLife_.reset();clear_sop_slot(id.slot);refresh_sop_views();return true;
}
Reject Authority::pose(Identity id,uint64_t epoch,uint32_t sequence,const Pose&p,uint64_t now,uint32_t life){
 if(epoch!=epoch_)return Reject::generation;auto s=slot(id);if(!s)return Reject::identity;if(!life||life!=s->state.life)return Reject::generation;if(!valid(p))return Reject::invalid_pose;
 if(s->poseSequenced&&!newer(sequence,s->poseSequence))return Reject::sequence;if(now<s->poseAt)return Reject::clock;
 auto delta=sub(p.feet,s->state.pose.feet);const float seconds=float(std::min<uint64_t>(now-s->poseAt,250))/1000;
 if(s->state.specialPhase!=SpecialPhase::none&&(dot(delta,delta)>0||p.capsule.radius!=s->state.pose.capsule.radius||p.capsule.height!=s->state.pose.capsule.height))return Reject::unavailable;
 if(std::hypot(delta[0],delta[2])>policy_.maxHorizontalSpeed*std::max(water_scale(s->state.pose),water_scale(p))*seconds+.1f||std::abs(delta[1])>policy_.maxVerticalSpeed*seconds+.1f)return Reject::too_fast;
 if((!s->state.alive||s->state.stunned)&&dot(delta,delta)>.01f)return Reject::dead;
 if(!movement_->clear(p.feet,p.capsule))return Reject::obstructed;
 auto hit=movement_->sweep(s->state.pose.feet,delta,s->state.pose.capsule);if(hit&&hit->fraction<.9999f)return Reject::obstructed;
 float length=std::sqrt(dot(delta,delta));if(length>.001f)for(const auto&other:slots_)if(other&&other->state.alive&&other->state.identity!=id){
  auto obstacle=expanded(other->state,p.capsule);auto collision=ray_capsule(s->state.pose.feet,mul(delta,1/length),obstacle);
  if(collision&&*collision<length&&(*collision>.001f||dot(delta,sub(p.feet,other->state.pose.feet))<=0))return Reject::obstructed;
 }
 s->state.pose=p;s->poseAt=now;s->poseSequence=sequence;s->poseSequenced=true;s->sopView.inputSequence=sequence;s->sopView.inputSequenced=true;++revision_;return Reject::none;
}
void Authority::finish_reload(Slot&s,uint64_t now){
 if(!s.state.reloadUntil)return;
 auto* current=holding(s,s.state.weapon);if(!current||!weapons_.contains(s.state.weapon)||current->revision==UINT64_MAX){s.state.reloadUntil=0;s.state.reloadLevel=0;s.reloadRefillAt=0;return;}
 if(s.reloadRefillAt&&now>=s.reloadRefillAt){
  auto count=std::min<unsigned>(weapons_.at(s.state.weapon).magazine-s.state.ammo,s.state.reserve);
  s.state.ammo+=uint16_t(count);s.state.reserve-=uint16_t(count);s.reloadRefillAt=0;
  current->contents.magazine=s.state.ammo;current->contents.reserve=s.state.reserve;++current->revision;++revision_;
 }
 if(now>=s.state.reloadUntil){s.state.reloadUntil=0;s.state.reloadLevel=0;++revision_;}
}
Reject Authority::equip(Identity id,uint64_t epoch,uint16_t weapon,uint64_t now,uint32_t life){
 if(epoch!=epoch_)return Reject::generation;auto s=slot(id);if(!s)return Reject::identity;if(!life||life!=s->state.life)return Reject::generation;if(!s->state.alive)return Reject::dead;
 if(weapon==s->state.weapon)return Reject::none;
 if(s->state.specialPhase!=SpecialPhase::none)return Reject::unavailable;
 auto* selected=holding(*s,weapon);if(weapon&&!selected)return Reject::weapon;finish_reload(*s,now);if(s->state.reloadUntil)return Reject::reloading;
 auto* previous=holding(*s,s->state.weapon);
 if((selected&&selected->revision==UINT64_MAX)||(previous&&previous->revision==UINT64_MAX))return Reject::sequence;
 if(previous)++previous->revision;if(selected)++selected->revision;
 s->state.weapon=weapon;s->state.ammo=selected?uint16_t(selected->contents.magazine):0;s->state.reserve=selected?uint16_t(selected->contents.reserve):0;++revision_;return Reject::none;
}
void Authority::emit(Decision&out,Event e){e.epoch=epoch_;e.id=++event_;if(auto source=slot(e.source))e.sourceLife=source->state.life;if(auto target=slot(e.target))e.targetLife=target->state.life;out.events.push_back(e);}
Decision Authority::reload(Identity id,uint64_t epoch,uint64_t now,uint32_t life){
 Decision out;auto fail=[&](Reject r){out.reject=r;return out;};if(epoch!=epoch_)return fail(Reject::generation);auto s=slot(id);if(!s)return fail(Reject::identity);if(!life||life!=s->state.life)return fail(Reject::generation);if(!active_)return fail(Reject::not_active);if(!s->state.alive||s->state.stunned)return fail(Reject::dead);finish_reload(*s,now);
 if(s->state.specialPhase!=SpecialPhase::none)return fail(Reject::unavailable);
 if(s->state.reloadUntil)return fail(Reject::reloading);if(!s->state.weapon||!weapons_.contains(s->state.weapon)||!holding(*s,s->state.weapon))return fail(Reject::weapon);const auto&w=weapons_.at(s->state.weapon);if(s->state.ammo==w.magazine)return out;if(!s->state.reserve)return fail(Reject::no_ammo);
 uint32_t endMs=w.reloadMs,refillMs=w.reloadRefillMs?w.reloadRefillMs:w.reloadMs;uint8_t level=0;
 if(w.nativePrimaryMastery&&s->state.verifiedSkills){
  level=s->state.masteryLevel;auto motion=*w.reloadMotion;
  // Native primary-only inventory has no GP30. Do not apply this adapter to
  // an unknown attack-state byte or an underbarrel reload (original mask 0x40).
  motion.rate=*original_mastery::ak102_reload_rate(w.id,level,0);
  auto timing=original::reload_timing(motion);if(!timing)return fail(Reject::weapon);
  endMs=timing->endMs;refillMs=timing->refillMs;
 }
 if(now>std::numeric_limits<uint64_t>::max()-endMs)return fail(Reject::clock);s->state.reloadLevel=level;s->state.reloadUntil=now+endMs;s->reloadRefillAt=now+refillMs;++revision_;Event e;e.kind=EventKind::reload;e.source=id;e.weapon=w.id;e.position=eye(s->state);emit(out,e);return out;
}
Decision Authority::fire(Identity id,const FireRequest&r,uint64_t now,uint32_t subMsNs){
 Decision out;auto fail=[&](Reject why){out.reject=why;return out;};if(r.epoch!=epoch_)return fail(Reject::generation);auto s=slot(id);if(!s)return fail(Reject::identity);if(!r.life||r.life!=s->state.life)return fail(Reject::generation);
 if(subMsNs>=1000000)return fail(Reject::clock);
 if(s->fireSequenced&&!newer(r.sequence,s->fireSequence))return fail(Reject::sequence);s->fireSequence=r.sequence;s->fireSequenced=true;
 if(!active_)return fail(Reject::not_active);if(!s->state.alive||s->state.stunned)return fail(Reject::dead);if(now<s->poseAt||(s->fired&&(now<s->fireAt||(now==s->fireAt&&subMsNs<s->fireSubMsNs))))return fail(Reject::clock);if(now-s->poseAt>policy_.stalePoseMs)return fail(Reject::invalid_pose);
 if(s->state.specialPhase!=SpecialPhase::none)return fail(Reject::unavailable);
 if(r.weapon!=s->state.weapon||!weapons_.contains(r.weapon))return fail(Reject::weapon);const auto&w=weapons_.at(r.weapon);finish_reload(*s,now);
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
 auto origin=eye(s->state);float distance=w.range;Vec3 normal{};uint32_t object=0;bool impact=false;Slot* victim=nullptr;
 if(!w.nativeAkPenetration)for(auto collision:{world_,targets_})if(collision)if(auto hit=collision->ray(origin,direction,distance)){distance=hit->distance;normal=hit->normal;object=collision->triangles[hit->triangle].object;impact=true;}
 uint8_t hitBone=0;
 for(auto&target:slots_)if(target&&target->state.identity!=id&&target->state.alive){
  std::optional<float> hit;uint8_t bone=0;
  if(w.nativeAkHitRegions){
   const auto&p=target->state.pose;
   auto stance=target->state.stunned||p.capsule.height==560?host_hit::Stance::prone:p.capsule.height==1100?host_hit::Stance::crouching:host_hit::Stance::standing;
   // Host has no trusted gender or animation phase. Fixed authored male pose
   // is an explicit proxy; it is never selected from informational GWAV data.
   if(auto region=host_hit::query(origin,direction,distance,p.feet,p.yaw,0,stance)){hit=region->distance;bone=region->bone;}
  }else hit=ray_capsule(origin,direction,target->state);
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
 auto* carried=holding(*s,s->state.weapon);if(!carried||carried->revision==UINT64_MAX)return fail(Reject::weapon);--s->state.ammo;carried->contents.magazine=s->state.ammo;carried->contents.reserve=s->state.reserve;++carried->revision;s->fireAt=now;s->fireSubMsNs=subMsNs;s->fired=true;++revision_;
 Event shot;shot.kind=EventKind::shot;shot.source=id;shot.weapon=w.id;shot.cue=w.shotCue;shot.position=origin;emit(out,shot);
 if(w.nativeAkPenetration)for(const auto& hit:path.impacts){Event surface;surface.kind=EventKind::impact;surface.source=id;surface.weapon=w.id;surface.cue=material_audio::bullet_cue(material_audio::stage_for_map(w.materialMap),hit.material.id).value_or(0);surface.position=hit.position;surface.normal=hit.normal;surface.object=hit.object;emit(out,surface);}
 if(!impact)return out;
 Event event;event.kind=EventKind::impact;event.source=id;if(victim)event.target=victim->state.identity;event.weapon=w.id;event.cue=victim&&w.bodyCue?w.bodyCue:w.impactCue;event.position=add(origin,mul(direction,distance));event.normal=normal;event.object=object;emit(out,event);
 if(!victim||(s->state.team&&s->state.team==victim->state.team&&!policy_.friendlyFire&&!policy_.freeForAll))return out;
 auto&v=victim->state;event.kind=EventKind::damage;event.cue=0;event.hpDamage=std::min(damage,v.hp);event.staminaDamage=std::min(w.staminaDamage,v.stamina);v.hp-=event.hpDamage;v.stamina-=event.staminaDamage;v.alive=v.hp!=0;v.stunned=v.alive&&v.stamina==0;event.hp=v.hp;event.stamina=v.stamina;++revision_;emit(out,event);
 if(!v.alive){clear_sop_slot(v.identity.slot);refresh_sop_views();}
 else if(v.stunned){v.specialPhase=SpecialPhase::none;victim->specialHeld=false;victim->specialAt=now;}
 if(!v.alive){if(auto& score=scores_[v.identity.slot];score&&score->deaths<1000000000)++score->deaths;
  if((policy_.freeForAll||!s->state.team||s->state.team!=v.team)&&scores_[id.slot]&&scores_[id.slot]->kills<1000000000)++scores_[id.slot]->kills;
  v.reloadUntil=0;v.reloadLevel=0;victim->reloadRefillAt=0;event.kind=EventKind::death;event.hpDamage=event.staminaDamage=0;emit(out,event);}return out;
}
void Authority::advance(uint64_t now){for(auto&s:slots_)if(s&&s->state.alive)finish_reload(*s,now);advance_sop(now);}
void Authority::active(bool enabled){
 active_=enabled;
 if(!enabled){bool changed=false;sopGroups_.reset();for(auto&s:slots_)if(s){
  const SopView reset{s->state.identity,s->state.life,0,0,{},false,s->poseSequence,s->poseSequenced};
  changed|=s->state.specialPhase!=SpecialPhase::none||s->sopView!=reset;
  s->state.specialPhase=SpecialPhase::none;s->specialHeld=false;s->sopJammed=false;s->sopView=reset;
 }if(changed)++revision_;}
}
bool Authority::configure_sop(uint32_t startMs,uint32_t endMs){
 if(startMs>60000||endMs>60000)return false;
 specialStartMs_=startMs;specialEndMs_=endMs;sopEnabled_=true;return true;
}
void Authority::clear_sop_slot(uint8_t index){
 if(index>=slots_.size())return;sopGroups_.clear(index);
 if(auto&s=slots_[index]){s->state.specialPhase=SpecialPhase::none;s->specialHeld=false;s->sopJammed=false;s->specialSequenced=false;s->specialAt=0;s->sopView={s->state.identity,s->state.life,0,0,{},false,s->poseSequence,s->poseSequenced};}
}
void Authority::refresh_sop_views(){
 sopGroups_.dissolve_singletons();
 for(size_t i=0;i<slots_.size();++i)if(auto&s=slots_[i]){
  uint32_t mask=0;const auto& p=s->state;const int group=sopGroups_.group(int(i));
  if(active_&&!policy_.freeForAll&&p.alive&&!s->sopJammed&&p.team>=1&&p.team<=2&&group>=0)
   for(size_t j=0;j<slots_.size();++j)if(i!=j)if(const auto&t=slots_[j];t&&t->state.alive&&!t->sopJammed&&t->state.team==p.team&&sopGroups_.group(int(j))==group)mask|=uint32_t(1)<<j;
  auto view=s->sopView;view.recipient=p.identity;view.life=p.life;view.jammed=s->sopJammed;
  if(mask&~view.visibleMask){if(view.activation==UINT32_MAX)view.activation=1;else ++view.activation;view.origin=p.pose.feet;} // Native scan starts at each recipient, not a recovered shader origin.
  view.visibleMask=mask;view.inputSequence=s->poseSequence;view.inputSequenced=s->poseSequenced;
  if(view!=s->sopView){s->sopView=view;++revision_;}
 }
}
Reject Authority::special(Identity id,uint64_t epoch,uint32_t sequence,bool pressed,bool held,uint64_t now,uint32_t life){
 if(epoch!=epoch_)return Reject::generation;auto*s=slot(id);if(!s)return Reject::identity;if(!life||life!=s->state.life)return Reject::generation;
 if(!s->poseSequenced||s->poseSequence!=sequence||(s->specialSequenced&&!newer(sequence,s->specialSequence)))return Reject::sequence;
 if(now<s->poseAt||now<s->specialAt)return Reject::clock;
 s->specialSequence=sequence;s->specialSequenced=true;s->specialHeld=held;
 if(!active_)return Reject::not_active;if(!s->state.alive||s->state.stunned)return Reject::dead;
 if(!sopEnabled_)return Reject::unavailable;
 if(s->state.specialPhase==SpecialPhase::hold&&!held){s->state.specialPhase=SpecialPhase::end;s->specialAt=now;++revision_;}
 if(s->state.specialPhase!=SpecialPhase::none||!pressed)return Reject::none;
 if(s->state.pose.capsule.height<1700)return Reject::invalid_pose;
 finish_reload(*s,now);if(s->state.reloadUntil)return Reject::reloading;
 s->state.specialPhase=SpecialPhase::start;s->specialAt=now;++revision_;return Reject::none;
}
bool Authority::release_special(Identity id,uint64_t now){
 auto*s=slot(id);if(!s||now<s->specialAt||now<s->poseAt)return false;s->specialHeld=false;
 if(s->state.specialPhase==SpecialPhase::start||s->state.specialPhase==SpecialPhase::hold){s->state.specialPhase=SpecialPhase::end;s->specialAt=now;++revision_;}
 return true;
}
bool Authority::sop_jam(Identity id,bool enabled,uint64_t now){
 auto*s=slot(id);if(!s||now<s->poseAt)return false;if(s->sopJammed==enabled)return true;
 s->sopJammed=enabled;if(enabled){sopGroups_.clear(id.slot);s->specialHeld=false;s->state.specialPhase=SpecialPhase::none;s->specialAt=now;}
 ++revision_;refresh_sop_views();return true;
}
std::optional<SopView> Authority::sop_view(Identity id)const{const auto*s=slot(id);if(!s)return {};return s->sopView;}
void Authority::advance_sop(uint64_t now){
 if(!active_||!sopEnabled_)return;
 std::array<bool,24> firstHold{};
 for(size_t i=0;i<slots_.size();++i)if(auto&s=slots_[i]){
  if(!s->state.alive||s->state.stunned){if(s->state.specialPhase!=SpecialPhase::none){s->state.specialPhase=SpecialPhase::none;s->specialHeld=false;++revision_;}continue;}
  if(now<s->specialAt)continue;
  if(s->state.specialPhase==SpecialPhase::start&&now-s->specialAt>=specialStartMs_){s->state.specialPhase=SpecialPhase::hold;s->specialAt=now;firstHold[i]=true;++revision_;}
  else if(s->state.specialPhase==SpecialPhase::end&&now-s->specialAt>=specialEndMs_){s->state.specialPhase=SpecialPhase::none;s->specialAt=now;++revision_;}
 }
 if(!policy_.freeForAll)for(size_t i=0;i<slots_.size();++i)if(auto&s=slots_[i];s&&s->state.alive&&!s->state.stunned&&!s->sopJammed&&s->state.specialPhase==SpecialPhase::hold&&(s->specialHeld||firstHold[i])&&s->state.team>=1&&s->state.team<=2){
  const auto& p=s->state;const auto yaw=static_cast<uint16_t>(static_cast<int>(p.pose.yaw*10430.3779296875f));
  for(size_t j=0;j<slots_.size();++j)if(i!=j)if(auto&t=slots_[j];t&&t->state.alive&&!t->sopJammed&&t->state.team==p.team&&sopGroups_.value[i]!=sopGroups_.value[j]){
   const auto&a=p.pose.feet;const auto&b=t->state.pose.feet;
   if(!original_sop::geometry({a[0],a[1],a[2]},{b[0],b[1],b[2]},yaw,original_sop::Branch::status22))continue;
   if(sopGroups_.merge(int(i),int(j)))refresh_sop_views();break;
  }
 }
 for(size_t i=0;i<slots_.size();++i)if(auto&s=slots_[i];s&&s->state.specialPhase==SpecialPhase::hold&&!s->specialHeld){s->state.specialPhase=SpecialPhase::end;s->specialAt=now;++revision_;}
 refresh_sop_views();
}
Snapshot Authority::snapshot()const{Snapshot out{epoch_,revision_,event_,{}};for(size_t i=0;i<slots_.size();++i)if(slots_[i])out.players[i]=slots_[i]->state;return out;}
bool Authority::configure_items(uint64_t generation,items::Capacity capacity,float range){
 if(!epoch_||!generation||!std::isfinite(range)||range<=0||range>10000||generation<itemGeneration_)return false;
 if(!placedItems_.configure(capacity))return false;
 if(generation!=itemGeneration_){if(!placedItems_.reset({epoch_,generation}))return false;itemGeneration_=generation;for(const auto& s:slots_)if(s)placedItems_.admit({s->state.identity.slot,s->state.identity.instance,s->state.identity.character,s->state.life});}
 itemRange_=range;return true;
}
void Authority::item_policies(const items::DropPolicies& source){std::map<uint32_t,items::DropPolicy> candidate;for(const auto&[key,e]:source.entries())if(e.domain==items::Domain::weapon)candidate.emplace(e.id,e.policy);itemPolicies_.swap(candidate);}
bool Authority::item_policy(uint32_t weapon,const items::DropPolicy& policy){if(!weapon||policy.drop>items::DropOverride::allow)return false;itemPolicies_.insert_or_assign(weapon,policy);return true;}
std::optional<items::wire::Held> Authority::item_held(Identity id,uint64_t token)const{
 const auto* s=slot(id);if(!s||!token)return {};
 items::wire::Held out;out.header={{epoch_,itemGeneration_},token,{id.slot,id.instance,id.character,s->state.life},0};out.slots=s->inventory;
 for(uint8_t i=0;i<out.slots.size();++i)if(s->state.weapon&&out.slots[i].contents.item==s->state.weapon)out.selectedSlot=i;return out;
}
items::Result Authority::item_action(Identity admitted,const items::wire::Command& command,uint64_t now){
 using items::ResultCode;using items::wire::Action;
 const auto& h=command.header;auto* s=slot(admitted);
 if(!s||h.actor.slot!=admitted.slot||h.actor.instance!=admitted.instance||h.actor.character!=admitted.character||h.actor.life!=s->state.life)return {ResultCode::identity};
 if(h.scope!=items::Scope{epoch_,itemGeneration_})return {ResultCode::scope};
 if(!items::wire::encode(command))return {ResultCode::invalid};
 if(command.header.sequence<=s->itemSequence)return {ResultCode::replay};s->itemSequence=command.header.sequence;
 if(!active_||!s->state.alive||s->state.stunned||!world_||now<s->poseAt||now-s->poseAt>policy_.stalePoseMs)return {ResultCode::unauthorized};
 if(s->state.specialPhase!=SpecialPhase::none)return {ResultCode::unauthorized};
 finish_reload(*s,now);if(s->state.reloadUntil)return {ResultCode::unauthorized};
 items::Request request{h.scope,h.actor,h.sequence,true};items::Result result;
 auto clearSight=[&](Vec3 target){auto origin=eye(s->state);auto delta=sub(target,origin);float distance=std::sqrt(dot(delta,delta));if(distance<.01f)return true;auto direction=mul(delta,1/distance);for(const auto& collision:{movement_,targets_})if(collision)if(auto hit=collision->ray(origin,direction,distance);hit&&hit->distance<distance-4)return false;return true;};
 if(command.action==Action::drop||command.action==Action::install){
  if(command.heldSlot>=s->inventory.size())return {ResultCode::invalid};auto& held=s->inventory[command.heldSlot];
  // Deliberately native AK102 only. Metadata for other weapons is not a fire,
  // trap, explosive, or mounted-weapon implementation.
  if(held.contents.domain!=items::Domain::weapon||held.contents.item!=25||!weapons_.contains(25))return {ResultCode::policy};
  Vec3 origin=s->state.pose.feet;origin[0]+=std::sin(s->state.pose.yaw)*650;origin[2]+=std::cos(s->state.pose.yaw)*650;origin[1]+=500;
  std::optional<stage::CollisionHit> ground;for(const auto& collision:{movement_,targets_})if(collision)if(auto hit=collision->ray(origin,{0,-1,0},1000);hit&&std::abs(hit->normal[1])>=.5f&&(!ground||hit->distance<ground->distance))ground=hit;
  if(!ground)return {ResultCode::unauthorized};auto target=ground->position;target[1]+=2;auto sight=target;sight[1]+=40;if(!clearSight(sight))return {ResultCode::unauthorized};
  items::Position position{target[0],target[1],target[2],s->state.pose.yaw};items::DropPolicy policy;auto entry=itemPolicies_.find(25);if(entry!=itemPolicies_.end())policy=entry->second;
  if(command.action==Action::drop)result=placedItems_.drop(request,held,command.heldRevision,position,policy);
  else result=placedItems_.install(request,held,command.heldRevision,command.amount,position,policy);
  if(result&&s->state.weapon==25&&!holding(*s,25)){s->state.weapon=0;s->state.ammo=s->state.reserve=0;s->state.reloadUntil=0;s->state.reloadLevel=0;s->reloadRefillAt=0;}
 }else{
  const auto state=placedItems_.state();auto entity=std::find_if(state.entities.begin(),state.entities.end(),[&](const auto& e){return e.key.id==command.entity;});if(entity==state.entities.end())return {ResultCode::not_found};
  if(entity->contents.domain!=items::Domain::weapon||entity->contents.item!=25||!weapons_.contains(25)||entity->contents.quantity!=1||entity->contents.resource!=items::Resource::ammunition||entity->contents.magazine>weapons_.at(25).magazine||entity->contents.reserve>weapons_.at(25).reserve)return {ResultCode::policy};
  const bool recover=command.action==Action::recover||command.action==Action::use;
  if((recover&&entity->kind!=items::PlacementKind::installed)||(!recover&&entity->kind!=items::PlacementKind::dropped))return {ResultCode::invalid};
  if(recover&&!itemRecoverOthers_&&entity->owner.character!=admitted.character)return {ResultCode::unauthorized};
  Vec3 target{entity->position.x,entity->position.y,entity->position.z};auto delta=sub(target,s->state.pose.feet);if(dot(delta,delta)>itemRange_*itemRange_)return {ResultCode::unauthorized};target[1]+=40;if(!clearSight(target))return {ResultCode::unauthorized};
  auto selected=command.action==Action::use?uint8_t(0):command.heldSlot;if(selected>=s->inventory.size()||holding(*s,25))return {ResultCode::occupied};
  auto& held=s->inventory[selected];const auto expected=command.action==Action::use?held.revision:command.heldRevision;
  result=placedItems_.pickup(request,entity->key,command.entityRevision,held,expected);
  if(result){s->state.weapon=25;s->state.ammo=uint16_t(held.contents.magazine);s->state.reserve=uint16_t(held.contents.reserve);s->state.reloadUntil=0;s->state.reloadLevel=0;s->reloadRefillAt=0;}
 }
 if(result)++revision_;return result;
}
}
