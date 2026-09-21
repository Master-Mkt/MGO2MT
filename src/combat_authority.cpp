#include "combat_authority.h"
#include "combat_ballistics.h"
#include "cover_hit_geometry.h"
#include "material_audio.h"
#include "original_mastery_policy.h"
#include "host_hit_geometry.h"
#include "original_hit_regions.h"
#include "original_weapon_reload.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
namespace mgo2mt::combat {
namespace {
float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vec3 sub(Vec3 a,Vec3 b){for(int i=0;i<3;++i)a[i]-=b[i];return a;}
Vec3 add(Vec3 a,Vec3 b){for(int i=0;i<3;++i)a[i]+=b[i];return a;}
Vec3 mul(Vec3 a,float b){for(auto&x:a)x*=b;return a;}
bool finite(Vec3 a){return std::all_of(a.begin(),a.end(),[](float f){return std::isfinite(f)&&std::abs(f)<1000000;});}
bool valid(Identity id){return id.slot<24&&id.instance&&id.character;}
bool valid(const Pose&p){return finite(p.feet)&&std::isfinite(p.yaw)&&std::abs(p.yaw)<=3.14159274f&&std::isfinite(p.pitch)&&std::abs(p.pitch)<=1.4f&&(p.capsule.radius==260||p.capsule.radius==350||p.capsule.radius==800)&&p.capsule.skin==2&&p.capsule.height>=p.capsule.radius*2&&(p.capsule.height==1700||p.capsule.height==1100||p.capsule.height==560||p.capsule.height==4200);}
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
// A grounded client walks horizontally before gravity is applied. One HOST
// packet can therefore contain two clear legs while its diagonal chord cuts
// into the departure floor. This fallback validates that actual bounded path;
// it never changes the endpoint, stance, elapsed time or speed allowance.
std::optional<Vec3> falling_corner(const stage::Collision& world,const Player& player,const Pose& next){
 const auto& previous=player.pose;
 if(player.specialPc.kind!=special_pc::Kind::human||player.mountedId||player.flightId||player.ladderAnchor||player.cover.attached||player.cover.lean||player.specialPhase!=SpecialPhase::none||
    previous.capsule.radius!=next.capsule.radius||previous.capsule.height!=next.capsule.height||previous.capsule.skin!=next.capsule.skin||next.feet[1]>=previous.feet[1]-.01f||std::hypot(next.feet[0]-previous.feet[0],next.feet[2]-previous.feet[2])<.01f)return {};
 const auto support=world.sweep(previous.feet,{0,-4,0},previous.capsule,stage::query::floor);
 if(!support||support->normal[1]<.70710678f)return {};
 auto corner=next.feet;corner[1]=previous.feet[1];
 if(!world.clear(corner,previous.capsule))return {};
 for(const auto& leg:{std::pair{previous.feet,corner},std::pair{corner,next.feet}})
  if(auto hit=world.sweep(leg.first,sub(leg.second,leg.first),previous.capsule);hit&&hit->fraction<.9999f)return {};
 return corner;
}
Player expanded(const Player&other,stage::Capsule moving){auto p=other;p.pose.feet[1]-=moving.height;p.pose.capsule.radius+=moving.radius;p.pose.capsule.height+=moving.height;return p;}
}
std::optional<WeaponReloadTiming> weapon_reload_timing(const Weapon&w,const Player&p){
 WeaponReloadTiming out{w.reloadRefillMs?w.reloadRefillMs:w.reloadMs,w.reloadMs,0};
 if(!valid_weapon_tuning(w))return {};
 if(w.reloadMotion){auto timing=original::reload_timing(*w.reloadMotion);if(!timing)return {};out.refillMs=timing->refillMs;out.endMs=timing->endMs;}
 if(w.reloadMotion&&(w.originalFirearm||w.nativePrimaryMastery)){
  const auto source=w.behaviorSourceId?w.behaviorSourceId:w.id;const auto baseline=original_weapon::reload_motion(source,0);
  if(baseline&&baseline->baseTick==w.reloadMotion->baseTick&&baseline->intervals==w.reloadMotion->intervals&&baseline->refillTick==w.reloadMotion->refillTick)
   if(auto motion=original_weapon::reload_motion(source,p.pose.capsule.height==560?2:p.pose.capsule.height==1100?1:0)){motion->rate=w.reloadMotion->rate;auto timing=original::reload_timing(*motion);if(!timing)return {};out.endMs=timing->endMs;out.refillMs=timing->refillMs;}
 }
 if(w.nativePrimaryMastery&&p.verifiedSkills){
  if(!w.reloadMotion)return {};out.level=p.masteryLevel;auto motion=*w.reloadMotion;
  const auto rate=original_mastery::ak102_reload_rate(w.id,out.level,0);if(!rate)return {};motion.rate=*rate;auto timing=original::reload_timing(motion);if(!timing)return {};out.endMs=timing->endMs;out.refillMs=timing->refillMs;
 }
 out.endMs=uint32_t(std::ceil(double(out.endMs)/weapon_reload_scale(w)));out.refillMs=uint32_t(std::ceil(double(out.refillMs)/weapon_reload_scale(w)));
 if(!out.endMs||out.endMs>60000||!out.refillMs||out.refillMs>out.endMs)return {};return out;
}
Authority::Authority(Policy policy):policy_(policy){if(!std::isfinite(policy.maxHorizontalSpeed)||policy.maxHorizontalSpeed<=0||policy.maxHorizontalSpeed>10000||!std::isfinite(policy.maxVerticalSpeed)||policy.maxVerticalSpeed<=0||policy.maxVerticalSpeed>30000||!policy.stalePoseMs||policy.stalePoseMs>5000)throw std::invalid_argument("Combat policy");items::DropPolicy nativeAk;nativeAk.drop=items::DropOverride::allow;itemPolicies_.emplace(25,nativeAk);}
items::HeldSlot* Authority::holding(Slot& s,uint16_t weapon){if(!weapon)return nullptr;for(auto& h:s.inventory)if(h.contents.domain==items::Domain::weapon&&h.contents.item==weapon&&h.contents.quantity)return &h;return nullptr;}
Authority::Slot* Authority::slot(Identity id){return valid(id)&&slots_[id.slot]&&slots_[id.slot]->state.identity==id?&*slots_[id.slot]:nullptr;}
const Authority::Slot* Authority::slot(Identity id)const{return valid(id)&&slots_[id.slot]&&slots_[id.slot]->state.identity==id?&*slots_[id.slot]:nullptr;}
void Authority::begin(uint64_t epoch,std::shared_ptr<const stage::Collision> world,std::span<const Weapon> weapons,std::shared_ptr<const stage::Collision> targets){
 if(!epoch||epoch<=epoch_||!world||weapons.size()>128)throw std::invalid_argument("Combat world/epoch");
 std::map<uint16_t,Weapon> checked;std::map<uint16_t,uint64_t> intervals;for(auto w:weapons){
  if(!valid_weapon_tuning(w))throw std::invalid_argument("Combat weapon tuning");
  if(w.blastMotion&&(!blast_motion::valid(*w.blastMotion)||(!w.nativeProjectile&&!w.nativePlaced)||(!w.damage&&!w.staminaDamage)))throw std::invalid_argument("Combat blast motion profile");
  if((w.nativePlaced||w.nativeProjectile||w.meleeAttack)&&w.behaviorSourceId&&w.behaviorSourceId!=w.id)throw std::invalid_argument("Unsupported non-firearm behavior alias");
  if(w.ballistics){const auto&b=*w.ballistics;if(!std::isfinite(b.range)||b.range<=0||b.range>1000000||!std::isfinite(b.speed)||b.speed<=0||!std::isfinite(b.decayStart)||!std::isfinite(b.decayEnd)||b.decayStart<0||b.decayEnd<b.decayStart||b.minimumForce<0||b.minimumForce>1000||b.penetration<0||b.penetration>1000000)throw std::invalid_argument("Combat ballistics profile");}
  if(w.mountedOnly&&(w.heldOnly||w.meleeAttack||(w.nativeProjectile&&w.id!=103)||w.nativePlaced))throw std::invalid_argument("Mounted-only weapon profile");
  if(w.nativePlaced&&(w.heldOnly||(w.id!=64&&w.id!=65&&w.id!=66&&w.id!=67&&w.id!=69)))throw std::invalid_argument("Placed weapon profile");
  if(w.nativeProjectile&&(w.heldOnly||(w.id!=50&&!projectile::throwable(w.id)&&w.id!=129&&w.id!=103)))throw std::invalid_argument("Native projectile profile");
  if(w.heldOnly){
   if(!w.id||w.damage||w.staminaDamage||w.intervalMs||w.reloadMs||w.magazine||w.reserve||w.range||w.shotCue||w.impactCue||w.bodyCue||w.automatic||w.reloadRefillMs||w.reloadMotion||w.fireIntervalTicks||w.nativePrimaryMastery||w.nativeAkPenetration||w.nativeAkHitRegions||w.nativeAkAccuracy||w.originalFirearm||w.meleeAttack||w.nativePlaced||!checked.emplace(w.id,w).second)throw std::invalid_argument("Held-only weapon profile");
   continue;
  }
  uint64_t interval=uint64_t(w.intervalMs)*1000000;
  if(w.fireIntervalTicks){
   auto original=original::fire_interval_ns(*w.fireIntervalTicks);
   if(!original||w.intervalMs)throw std::invalid_argument("Combat fire interval profile");
   interval=*original;w.intervalMs=uint32_t((interval+999999)/1000000);
  }
  if(w.originalFirearm&&!w.ballistics&&!original_weapon::find(w.behaviorSourceId?w.behaviorSourceId:w.id))throw std::invalid_argument("Original firearm identity");
  if(w.meleeAttack&&(w.id!=1&&w.id!=73))throw std::invalid_argument("Melee weapon identity");
  if(w.nativeAkAccuracy&&(w.behaviorSourceId?w.behaviorSourceId:w.id)!=25)throw std::invalid_argument("Native AK accuracy profile");
  if(w.nativeAkPenetration&&((w.behaviorSourceId?w.behaviorSourceId:w.id)!=25||(!w.ballistics&&(w.damage!=275||w.staminaDamage||w.range!=200000))))throw std::invalid_argument("Native AK penetration profile");
  if(w.nativeAkHitRegions&&(!w.nativeAkPenetration||(w.behaviorSourceId?w.behaviorSourceId:w.id)!=25))throw std::invalid_argument("Native AK hit region profile");
  if(w.nativePrimaryMastery&&(w.id!=25||!w.reloadMotion||w.reloadMotion->baseTick!=5||w.reloadMotion->intervals!=210||w.reloadMotion->refillTick!=650||w.reloadMotion->rate!=1.f))throw std::invalid_argument("Combat native mastery profile");
  if(w.reloadMotion){
   auto timing=original::reload_timing(*w.reloadMotion);
   if(!timing||w.reloadMs||w.reloadRefillMs)throw std::invalid_argument("Combat reload motion profile");
   w.reloadMs=timing->endMs;w.reloadRefillMs=timing->refillMs;
  }
  if(std::ceil(double(w.reloadMs)/weapon_reload_scale(w))>60000)throw std::invalid_argument("Combat tuned reload duration");
  if(!w.id||(!w.damage&&!w.staminaDamage&&!(w.nativeProjectile&&projectile::throwable(w.id))&&!w.nativePlaced)||w.damage>1000000||w.staminaDamage>1000000||!w.intervalMs||w.intervalMs>60000||!w.reloadMs||w.reloadMs>60000||w.reloadRefillMs>w.reloadMs||w.reserve>10000||!w.magazine||w.magazine>1000||!std::isfinite(w.range)||w.range<=0||w.range>1000000||!checked.emplace(w.id,w).second)throw std::invalid_argument("Combat weapon profile");
  intervals.emplace(w.id,interval);
 }
 mounted_.clear();mountedRegistry_={};mountedMap_=0;traps_.clear();traps_.reserve(64);slots_={};scores_={};skills_={};sopGroups_.reset();spawnLife_.reset();water_.reset();waterRatio_=.65f;weapons_=std::move(checked);fireIntervalsNs_=std::move(intervals);movement_=stage::movement_collision(world);world_=std::move(world);targets_=std::move(targets);epoch_=epoch;revision_=1;event_=0;active_=false;itemGeneration_=1;placedItems_.reset({epoch_,itemGeneration_});explosionReplay_={};ladders_.clear();projectiles_.reset({epoch_,++projectileScene_});
}
bool Authority::world(std::shared_ptr<const stage::Collision> world,std::shared_ptr<const stage::Collision> targets){if(!world||!epoch_)return false;for(auto&s:slots_)if(s){cancel_catapult(*s,true);cancel_catapult(*s,false);release_mounted(s->state.identity);}mounted_.clear();mountedRegistry_={};mountedMap_=0;movement_=stage::movement_collision(world);world_=std::move(world);targets_=std::move(targets);projectiles_.reset({epoch_,++projectileScene_});++revision_;return true;}
bool Authority::object_world(std::shared_ptr<const stage::Collision> world,std::shared_ptr<const stage::Collision> targets){
 if(!world||!world_||!epoch_||projectiles_.scope().epoch!=epoch_)return false;
 if(mountedMap_)world=std::make_shared<const stage::Collision>(mounted::with_collision(*world,mountedRegistry_,mountedMap_));
 auto movement=stage::movement_collision(world);movement_=std::move(movement);world_=std::move(world);targets_=std::move(targets);++revision_;return true;
}
bool Authority::install_loadout(const host_skills::Verified&v){
 const auto&s=v.scope();Identity id{s.slot,s.instance,s.character};
 if(s.epoch!=epoch_||!valid(id)||slot(id))return false;
 auto&entry=skills_[id.slot];if(entry&&(entry->id!=id||entry->frozen||entry->verified))return false;
 entry=Skills{id,v,false};return true;
}
bool Authority::join(Identity id,uint8_t team,const Pose&p,uint32_t hp,uint32_t stamina,std::span<const uint16_t> inventory,uint64_t now){
 if(!epoch_||!world_||!valid(id)||team>2||(policy_.freeForAll&&team!=0)||!valid(p)||p.capsule.radius==800||p.capsule.height==4200||!hp||hp>1000000||!stamina||stamina>1000000||inventory.empty()||inventory.size()>3||slots_[id.slot]||!movement_->clear(p.feet,p.capsule))return false;
 for(const auto&s:slots_)if(s&&s->state.identity.character==id.character)return false;
 for(const auto&s:slots_)if(s&&s->state.alive){auto hit=ray_capsule(p.feet,{0,1,0},expanded(s->state,p.capsule));if(hit&&*hit==0)return false;}
 std::set<uint16_t> ids;for(auto w:inventory)if(special_pc::weapon(w)||!weapons_.contains(w)||weapons_.at(w).mountedOnly||!ids.insert(w).second)return false;
 auto&frozen=skills_[id.slot];if(frozen&&frozen->id!=id)return false;
 if(!frozen)frozen=Skills{id,{},false};frozen->frozen=true;
 Slot s;const auto&initial=weapons_.at(inventory.front());s.state={id,team,p,hp,hp,stamina,stamina,inventory.front(),uint16_t(initial.meleeAttack?0:initial.magazine),uint16_t(initial.meleeAttack?0:initial.reserve),0,true,false};for(size_t i=0;i<inventory.size();++i){const auto w=inventory[i];s.inventory[i].contents={w,1,uint32_t(weapons_.at(w).meleeAttack?0:weapons_.at(w).magazine),uint32_t(weapons_.at(w).meleeAttack?0:weapons_.at(w).reserve),0,(weapons_.at(w).heldOnly||weapons_.at(w).meleeAttack)?items::Resource::durable:items::Resource::ammunition};}if(frozen->verified){s.state.verifiedSkills=true;s.state.masteryLevel=frozen->verified->level(3);s.state.surveyorLevel=frozen->verified->level(7);s.state.hawkeyeLevel=frozen->verified->level(6);}
 if(weapons_.contains(1)&&!ids.contains(1))s.inventory[items::knife_slot].contents={1,1,0,0,0,items::Resource::durable,items::Domain::weapon};
 s.falling=falling::Tracker(healthRules_.falling);s.oxygen.reset(oxygenPolicy_);s.waterAt=now;s.waterClockArmed=true;s.poseAt=now;s.sopView.recipient=id;s.sopView.life=s.state.life;if(!spawnLife_&&!placedItems_.admit({id.slot,id.instance,id.character,s.state.life}))return false;if(!scores_[id.slot]||scores_[id.slot]->id!=id)scores_[id.slot]=Score{id};slots_[id.slot]=std::move(s);if(!spawnLife_){sopGroups_.clear(id.slot);refresh_sop_views();}if(auto angle=accuracy(id,now))slots_[id.slot]->sopView.spreadMilliRadians=*angle;++revision_;return true;
}
bool Authority::leave(Identity id){if(valid(id)&&skills_[id.slot]&&skills_[id.slot]->id==id)skills_[id.slot].reset();if(!slot(id))return false;projectiles_.remove({id.slot,id.instance,id.character,slot(id)->state.life});placedItems_.remove({id.slot,id.instance,id.character,slot(id)->state.life});slots_[id.slot].reset();sopGroups_.clear(id.slot);refresh_sop_views();++revision_;return true;}
bool Authority::respawn(Identity id,uint32_t nextLife,const std::function<bool()>&grant){
 auto current=slot(id);if(!current||current->state.alive||!nextLife||current->state.life==std::numeric_limits<uint32_t>::max()||nextLife!=current->state.life+1||!grant||spawnLife_)return false;
 const auto previousSkills=skills_[id.slot];auto previous=std::move(slots_[id.slot]);const auto revision=revision_;const auto event=event_;slots_[id.slot].reset();spawnLife_=std::pair{id,nextLife};
 auto restore=[&]{skills_[id.slot]=previousSkills;slots_[id.slot]=std::move(previous);revision_=revision;event_=event;spawnLife_.reset();};
 try{if(!grant()||!slot(id)||!slot(id)->state.alive){restore();return false;}}catch(...){restore();throw;}
 slots_[id.slot]->state.life=nextLife;slots_[id.slot]->itemSequence=0;placedItems_.admit({id.slot,id.instance,id.character,nextLife});spawnLife_.reset();clear_sop_slot(id.slot);refresh_sop_views();return true;
}
Reject Authority::pose(Identity id,uint64_t epoch,uint32_t sequence,const Pose&requested,uint64_t now,uint32_t life){
 Pose p=requested;
 advance_evade(now);
 if(epoch!=epoch_)return Reject::generation;auto s=slot(id);if(!s)return Reject::identity;if(!life||life!=s->state.life)return Reject::generation;if(!valid(p))return Reject::invalid_pose;
 if(s->state.flightId){
  if(!s->flight||!s->state.alive||(s->state.stunned&&!s->state.blastFlight))return Reject::unavailable;
  if(s->poseSequenced&&!newer(sequence,s->poseSequence))return Reject::sequence;if(now<s->poseAt)return Reject::clock;
  // The packet acknowledges input/life only. Position, view and stance remain
  // the HOST flight state, including after packet delay or attempted teleport.
  s->poseAt=now;s->poseSequence=sequence;s->poseSequenced=true;s->sopView.inputSequence=sequence;s->sopView.inputSequenced=true;return Reject::none;
 }
 if(s->state.mountedId){auto*m=mounted_state(*s);if(!m||!s->state.alive||s->state.stunned)return Reject::unavailable;if(p.capsule.height!=1700||p.capsule.radius!=s->state.pose.capsule.radius)return Reject::invalid_pose;if(!mounted_pose(*s,p))return Reject::unavailable;}
 if(s->state.specialPc.kind==special_pc::Kind::gekko){if(p.capsule.radius!=special_pc::native_gekko.capsule.radius||p.capsule.height!=special_pc::native_gekko.capsule.height)return Reject::invalid_pose;if(s->state.specialPc.action!=special_pc::Action::none&&(p.feet!=s->state.pose.feet||p.yaw!=s->state.pose.yaw||p.pitch!=s->state.pose.pitch))return Reject::unavailable;}else if(p.capsule.radius==800||p.capsule.height==4200)return Reject::invalid_pose;
 if(s->state.ladderAnchor&&(p.feet!=s->state.pose.feet||p.capsule.height!=s->state.pose.capsule.height||p.capsule.radius!=s->state.pose.capsule.radius||p.capsule.skin!=s->state.pose.capsule.skin||p.yaw!=s->state.pose.yaw))return Reject::unavailable;
 if(s->poseSequenced&&!newer(sequence,s->poseSequence))return Reject::sequence;if(now<s->poseAt)return Reject::clock;
 auto delta=sub(p.feet,s->state.pose.feet);const float seconds=float(std::min<uint64_t>(now-s->poseAt,250))/1000;
 if(s->state.cover.attached){const auto projected=cover::projected_destination(*movement_,s->state.pose.feet,p.feet,p.capsule,s->state.cover);if(dot(sub(projected,p.feet),sub(projected,p.feet))>.01f||std::hypot(delta[0],delta[2])>cover::native_policy.slideSpeed*seconds+.1f)return Reject::too_fast;}
 if(s->state.specialPhase!=SpecialPhase::none&&(dot(delta,delta)>0||p.capsule.radius!=s->state.pose.capsule.radius||p.capsule.height!=s->state.pose.capsule.height))return Reject::unavailable;
 if(s->state.evadeKind!=EvadeKind::none&&(p.capsule.radius!=s->state.pose.capsule.radius||p.capsule.height!=1700||std::abs(std::remainder(p.yaw-s->evadeYaw,6.28318530718f))>.001f))return Reject::unavailable;
 if(std::hypot(delta[0],delta[2])>.001f&&(water_blocks(s->state.pose)||water_blocks(p)))return Reject::too_fast;
 const float carriedScale=!s->state.cover.attached&&s->state.evadeKind==EvadeKind::none&&weapons_.contains(s->state.weapon)?weapon_move_scale(weapons_.at(s->state.weapon)):1.f;
 if(!s->state.mountedId&&(std::hypot(delta[0],delta[2])>(s->state.specialPc.kind==special_pc::Kind::gekko?special_pc::native_gekko.runSpeed:policy_.maxHorizontalSpeed*carriedScale)*std::max(water_scale(s->state.pose),water_scale(p))*seconds+.1f||std::abs(delta[1])>policy_.maxVerticalSpeed*seconds+.1f))return Reject::too_fast;
 if((!s->state.alive||s->state.stunned)&&dot(delta,delta)>.01f)return Reject::dead;
 if(!s->state.mountedId&&!s->state.ladderAnchor&&std::hypot(delta[0],delta[2])>.001f&&movement_->fall_prevention_fraction(s->state.pose.feet,{delta[0],0,delta[2]},s->state.pose.capsule)<.9999f)return Reject::obstructed;
 if(!movement_->clear(p.feet,p.capsule))return Reject::obstructed;
 if(!s->state.mountedId){
  std::array<Vec3,2> ends{p.feet,p.feet};unsigned count=1;
  if(auto hit=movement_->sweep(s->state.pose.feet,delta,s->state.pose.capsule);hit&&hit->fraction<.9999f){
   const auto corner=falling_corner(*movement_,s->state,p);if(!corner)return Reject::obstructed;ends[0]=*corner;count=2;
  }
  auto start=s->state.pose.feet;
  for(unsigned segment=0;segment<count;++segment){const auto travel=sub(ends[segment],start);const float length=std::sqrt(dot(travel,travel));
   if(length>.001f)for(const auto&other:slots_)if(other&&other->state.alive&&other->state.identity!=id){
    auto obstacle=expanded(other->state,p.capsule);auto collision=ray_capsule(start,mul(travel,1/length),obstacle);
    if(collision&&*collision<length&&(*collision>.001f||dot(travel,sub(ends[segment],other->state.pose.feet))<=0))return Reject::obstructed;
   }
   start=ends[segment];
  }
 }
 if(!s->state.mountedId&&seconds>0&&std::hypot(delta[0],delta[2])>.1f){s->movementSpeed=std::hypot(delta[0],delta[2])/seconds;s->movedAt=now;}
 const bool wasAlive=s->state.alive;advance_water(*s,now);if(wasAlive&&!s->state.alive)return Reject::dead;
 s->previousApprovedVelocity=s->approvedVelocity;s->previousApprovedVelocityAt=s->approvedVelocityAt;s->approvedVelocity={};const float evidenceSeconds=float(double(now-s->poseAt)/1000.);if(evidenceSeconds>0){s->approvedVelocity[0]=delta[0]/evidenceSeconds;s->approvedVelocity[2]=delta[2]/evidenceSeconds;}s->approvedVelocityAt=now;
 s->specialRecovery.reset();s->state.pose=p;advance_water(*s,now);s->poseAt=now;s->poseSequence=sequence;s->poseSequenced=true;s->sopView.inputSequence=sequence;s->sopView.inputSequenced=true;++revision_;advance_cover();advance_accuracy(now);return Reject::none;
}
void Authority::finish_reload(Slot&s,uint64_t now){
 if(!s.state.reloadUntil){s.state.reloadElapsedMs=0;return;}
 if(now>=s.reloadAt){auto age=uint16_t(std::min<uint64_t>(60000,now-s.reloadAt)/25*25);if(age!=s.state.reloadElapsedMs){s.state.reloadElapsedMs=age;++revision_;}}
 auto* current=holding(s,s.state.weapon);if(!current||!weapons_.contains(s.state.weapon)||current->revision==UINT64_MAX){s.state.reloadUntil=0;s.state.reloadLevel=0;s.reloadRefillAt=0;return;}
 if(s.reloadRefillAt&&now>=s.reloadRefillAt){
  auto count=std::min<unsigned>(s.reloadCapacity>s.state.ammo?s.reloadCapacity-s.state.ammo:0,s.state.reserve);
  s.state.ammo+=uint16_t(count);s.state.reserve-=uint16_t(count);s.reloadRefillAt=0;
  current->contents.magazine=s.state.ammo;current->contents.reserve=s.state.reserve;++current->revision;++revision_;
 }
 if(now>=s.state.reloadUntil){s.state.reloadUntil=0;s.state.reloadLevel=0;++revision_;}
}
Reject Authority::equip(Identity id,uint64_t epoch,uint16_t weapon,uint64_t now,uint32_t life){
 if(epoch!=epoch_)return Reject::generation;auto s=slot(id);if(!s)return Reject::identity;if(!life||life!=s->state.life)return Reject::generation;if(!s->state.alive)return Reject::dead;
 if(s->state.mountedId||s->state.flightId||s->state.ladderAnchor||s->state.specialPc.action!=special_pc::Action::none)return Reject::unavailable;
 if(special_pc::weapon(weapon)!=(s->state.specialPc.kind==special_pc::Kind::gekko))return Reject::weapon;
 if(weapon==s->state.weapon)return Reject::none;
 if(s->state.specialPhase!=SpecialPhase::none||s->state.evadeKind!=EvadeKind::none)return Reject::unavailable;
 auto* selected=holding(*s,weapon);if(weapon&&!selected)return Reject::weapon;finish_reload(*s,now);if(s->state.reloadUntil)return Reject::reloading;
 auto* previous=holding(*s,s->state.weapon);
 if((selected&&selected->revision==UINT64_MAX)||(previous&&previous->revision==UINT64_MAX))return Reject::sequence;
 release_cover(id);if(previous)++previous->revision;if(selected)++selected->revision;
 s->accuracy.reset();s->state.weapon=weapon;s->state.ammo=selected?uint16_t(selected->contents.magazine):0;s->state.reserve=selected?uint16_t(selected->contents.reserve):0;++revision_;advance_accuracy(now);return Reject::none;
}
void Authority::emit(Decision&out,Event e){e.epoch=epoch_;e.id=++event_;if(auto source=slot(e.source))e.sourceLife=source->state.life;if(auto target=slot(e.target))e.targetLife=target->state.life;out.events.push_back(e);}
Decision Authority::reload(Identity id,uint64_t epoch,uint64_t now,uint32_t life){
 advance_evade(now);
 Decision out;auto fail=[&](Reject r){out.reject=r;return out;};if(epoch!=epoch_)return fail(Reject::generation);auto s=slot(id);if(!s)return fail(Reject::identity);if(!life||life!=s->state.life)return fail(Reject::generation);if(!active_)return fail(Reject::not_active);if(!s->state.alive||s->state.stunned)return fail(Reject::dead);finish_reload(*s,now);
 if(s->state.mountedId||s->state.flightId||s->state.ladderAnchor||s->state.specialPc.kind!=special_pc::Kind::human||s->state.specialPhase!=SpecialPhase::none||s->state.evadeKind!=EvadeKind::none)return fail(Reject::unavailable);
 if(s->state.reloadUntil)return fail(Reject::reloading);if(!s->state.weapon||!weapons_.contains(s->state.weapon)||!holding(*s,s->state.weapon))return fail(Reject::weapon);const auto&w=weapons_.at(s->state.weapon);if(w.heldOnly||w.meleeAttack)return fail(Reject::weapon);if(now<s->meleeUntil)return fail(Reject::unavailable);

 // Remote support weapons activate one earliest surviving deployment on Reload.
 // The same accepted action clock prevents repeated edges bypassing cadence.
 if((w.id==66||w.id==67)&&std::any_of(traps_.begin(),traps_.end(),[&](const Trap&t){return t.source.actor==burning::Key{epoch_,id.slot,id.instance,id.character,s->state.life}&&t.source.weapon==w.id;})){
  if(now<s->poseAt||now-s->poseAt>policy_.stalePoseMs)return fail(Reject::invalid_pose);
  if(s->fired&&(now<s->fireAt||now-s->fireAt<w.intervalMs))return fail(Reject::interval);
  auto next=std::find_if(traps_.begin(),traps_.end(),[&](const Trap&t){return t.source.actor==burning::Key{epoch_,id.slot,id.instance,id.character,s->state.life}&&t.source.weapon==w.id;});
  if(next==traps_.end()||now<next->armedAt)return fail(Reject::unavailable);
  auto action=advance_traps(now,id,w.id);s->fireAt=now;s->fireSubMsNs=0;s->fired=true;return action;
 }
 const auto capacity=weapon_reload_capacity(w,s->state.ammo);if(s->state.ammo>=capacity)return out;if(!s->state.reserve)return fail(Reject::no_ammo);
 const auto timing=weapon_reload_timing(w,s->state);if(!timing)return fail(Reject::weapon);const auto endMs=timing->endMs,refillMs=timing->refillMs;const auto level=timing->level;
 if(now>std::numeric_limits<uint64_t>::max()-endMs)return fail(Reject::clock);release_cover(id);s->accuracy.reset();s->reloadCapacity=capacity;s->state.reloadLevel=level;s->state.reloadUntil=now+endMs;s->reloadAt=now;s->state.reloadElapsedMs=0;s->reloadRefillAt=now+refillMs;++revision_;Event e;e.kind=EventKind::reload;e.source=id;e.weapon=w.id;e.position=eye(s->state);emit(out,e);advance_accuracy(now);return out;
}
void Authority::advance(uint64_t now){advance_catapults(now);advance_mounted(now);for(auto&s:slots_)if(s){if(s->grenadeJamUntil&&now>=s->grenadeJamUntil){s->grenadeJamUntil=0;sop_jam(s->state.identity,false,now);}if(s->grenadeStunUntil&&now>=s->grenadeStunUntil){s->grenadeStunUntil=0;if(s->state.alive)s->state.stamina=std::min(s->state.maxStamina,std::max(s->state.stamina,s->grenadeStamina));s->grenadeStamina=0;s->state.stunned=s->state.alive&&s->state.stamina==0;++revision_;}advance_water(*s,now);if(s->state.alive)finish_reload(*s,now);}advance_sop(now);advance_evade(now);advance_accuracy(now);advance_cover();}
void Authority::active(bool enabled){
 if(!enabled){projectiles_.reset({epoch_,++projectileScene_});for(auto&s:slots_)if(s){if(s->state.burning||s->state.ladderAnchor)++revision_;s->burn.clear();s->state.burning=false;s->ladderState.reset();s->state.ladderAnchor=0;}}
 if(!enabled)for(auto&s:slots_)if(s){cancel_catapult(*s,true);cancel_catapult(*s,false);release_mounted(s->state.identity);release_cover(s->state.identity);release_special_pc(s->state.identity);}
 if(active_!=enabled)for(auto&s:slots_)if(s)s->waterClockArmed=false;
 active_=enabled;if(enabled){advance_accuracy(0);for(auto& s:slots_)if(s){sample_fall(*s,s->poseAt);regenerate(*s,s->poseAt);}}else for(auto& s:slots_)if(s){s->falling.clear();s->regeneration.reset();}
 if(!enabled)for(auto&s:slots_)if(s&&s->state.evadeKind!=EvadeKind::none){s->state.evadeKind=EvadeKind::none;s->state.evadeSerial=0;s->state.evadeElapsedMs=0;++revision_;}
 if(!enabled){for(auto&s:slots_)if(s)s->accuracy.reset();bool changed=false;sopGroups_.reset();for(auto&s:slots_)if(s){
  SopView reset{s->state.identity,s->state.life,0,0,{},false,s->poseSequence,s->poseSequenced};reset.coverRequest=s->coverRequest;reset.specialPcRequest=s->specialPcRequest;
  changed|=s->state.specialPhase!=SpecialPhase::none||s->sopView!=reset;
  s->state.specialPhase=SpecialPhase::none;s->specialHeld=false;s->sopJammed=false;s->sopView=reset;
 }if(changed)++revision_;}
}
bool Authority::configure_sop(uint32_t startMs,uint32_t endMs){
 if(startMs>60000||endMs>60000)return false;
 specialStartMs_=startMs;specialEndMs_=endMs;sopEnabled_=true;return true;
}
bool Authority::configure_evade(EvadeProfile roll,EvadeProfile backstep){
 if(!valid_evade_profile(roll)||!valid_evade_profile(backstep)||std::any_of(slots_.begin(),slots_.end(),[](const auto&s){return s&&s->state.evadeKind!=EvadeKind::none;}))return false;
 evadeProfiles_={roll,backstep};return true;
}
Reject Authority::evade(Identity id,uint64_t epoch,uint32_t sequence,EvadeKind kind,uint32_t request,uint64_t now,uint32_t life){
 if(epoch!=epoch_)return Reject::generation;auto*s=slot(id);if(!s)return Reject::identity;if(!life||life!=s->state.life)return Reject::generation;
 if(s->state.mountedId||s->state.flightId||s->state.ladderAnchor)return Reject::unavailable;
 if(!request||kind==EvadeKind::none||!valid_evade_kind(kind))return Reject::unavailable;
 if(!s->poseSequenced||sequence!=s->poseSequence||(s->evadeSequenced&&!newer(request,s->evadeRequest)))return Reject::sequence;
 if(now<s->poseAt||now<s->evadeAt)return Reject::clock;
 // ACK rejected requests too; a client must not replay a denied short press
 // when conditions improve. Sequence/life-invalid traffic receives no ACK.
 s->evadeSequenced=true;s->evadeRequest=request;s->sopView.evadeRequest=request;++revision_;
 advance_evade(now);
 if(!active_)return Reject::not_active;if(!s->state.alive||s->state.stunned)return Reject::dead;
 if(s->state.specialPc.kind!=special_pc::Kind::human||s->state.cover.attached||s->state.cover.lean||s->state.evadeKind!=EvadeKind::none||s->state.specialPhase!=SpecialPhase::none)return Reject::unavailable;
 finish_reload(*s,now);if(s->state.reloadUntil)return Reject::reloading;
 const auto profile=evadeProfiles_[kind==EvadeKind::backstep?1:0];if(!valid_evade_profile(profile))return Reject::unavailable;
 if(s->state.pose.capsule.height!=1700)return Reject::invalid_pose;
 // Lateral rolling, like backstep, is available from standing. Only the
 // original forward-roll adapter requires running evidence.
 // Native admission proxy for running. Retain the last moving sample briefly
 // so a same-position packet/coalescing cannot erase a just-observed run.
 const float carriedScale=weapons_.contains(s->state.weapon)?weapon_move_scale(weapons_.at(s->state.weapon)):1.f;
 if(kind==EvadeKind::roll&&(now<s->movedAt||now-s->movedAt>policy_.stalePoseMs||s->movementSpeed<3000*carriedScale*water_scale(s->state.pose)))return Reject::too_fast;
 s->state.evadeKind=kind;s->state.evadeSerial=request;s->state.evadeElapsedMs=0;s->evadeAt=now;s->evadeYaw=s->state.pose.yaw;++revision_;return Reject::none;
}
bool Authority::release_evade(Identity id,uint64_t now){
 auto*s=slot(id);if(!s||now<s->evadeAt||now<s->poseAt)return false;
 if(s->state.evadeKind!=EvadeKind::none){s->state.evadeKind=EvadeKind::none;s->state.evadeSerial=0;s->state.evadeElapsedMs=0;++revision_;}return true;
}
void Authority::advance_evade(uint64_t now){
 for(auto&s:slots_)if(s&&s->state.evadeKind!=EvadeKind::none&&now>=s->evadeAt){
  const auto elapsed=now-s->evadeAt;const auto duration=evadeProfiles_[s->state.evadeKind==EvadeKind::backstep?1:0].durationMs;
  if(elapsed<s->state.evadeElapsedMs)continue;
  if(!active_||!s->state.alive||s->state.stunned||!duration||elapsed>=duration){s->state.evadeKind=EvadeKind::none;s->state.evadeSerial=0;s->state.evadeElapsedMs=0;++revision_;}
  else if(s->state.evadeElapsedMs!=elapsed){s->state.evadeElapsedMs=uint16_t(elapsed);++revision_;}
 }
}
void Authority::clear_sop_slot(uint8_t index){
 if(index>=slots_.size())return;sopGroups_.clear(index);
 if(auto&s=slots_[index]){s->state.specialPhase=SpecialPhase::none;s->specialHeld=false;s->sopJammed=false;s->specialSequenced=false;s->specialAt=0;const auto accuracy=s->state.alive&&!s->state.stunned?s->sopView.spreadMilliRadians:uint16_t(0);s->sopView={s->state.identity,s->state.life,0,0,{},false,s->poseSequence,s->poseSequenced};s->sopView.spreadMilliRadians=accuracy;s->sopView.coverRequest=s->coverRequest;s->sopView.specialPcRequest=s->specialPcRequest;}
}
void Authority::refresh_sop_views(){
 sopGroups_.dissolve_singletons();
 for(size_t i=0;i<slots_.size();++i)if(auto&s=slots_[i]){
  uint32_t mask=0;const auto& p=s->state;const int group=sopGroups_.group(int(i));
  if(active_&&!policy_.freeForAll&&p.specialPc.kind==special_pc::Kind::human&&p.alive&&!s->sopJammed&&p.team>=1&&p.team<=2&&group>=0)
   for(size_t j=0;j<slots_.size();++j)if(i!=j)if(const auto&t=slots_[j];t&&t->state.alive&&t->state.specialPc.kind==special_pc::Kind::human&&!t->sopJammed&&t->state.team==p.team&&sopGroups_.group(int(j))==group)mask|=uint32_t(1)<<j;
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
 if(s->state.specialPc.kind!=special_pc::Kind::human||s->state.cover.attached||s->state.cover.lean||s->state.evadeKind!=EvadeKind::none)return Reject::unavailable;
 if(s->state.mountedId||s->state.flightId||s->state.ladderAnchor)return Reject::unavailable;
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
  for(size_t j=0;j<slots_.size();++j)if(i!=j)if(auto&t=slots_[j];t&&t->state.alive&&t->state.specialPc.kind==special_pc::Kind::human&&!t->sopJammed&&t->state.team==p.team&&sopGroups_.value[i]!=sopGroups_.value[j]){
   const auto&a=p.pose.feet;const auto&b=t->state.pose.feet;
   if(!original_sop::geometry({a[0],a[1],a[2]},{b[0],b[1],b[2]},yaw,original_sop::Branch::status22))continue;
   if(sopGroups_.merge(int(i),int(j)))refresh_sop_views();break;
  }
 }
 for(size_t i=0;i<slots_.size();++i)if(auto&s=slots_[i];s&&s->state.specialPhase==SpecialPhase::hold&&!s->specialHeld){s->state.specialPhase=SpecialPhase::end;s->specialAt=now;++revision_;}
 refresh_sop_views();
}
void Authority::aiming(Identity id,bool aiming){auto*s=slot(id);if(!s)return;auto&p=s->state;aiming=aiming&&p.alive&&!p.stunned&&!p.flightId&&p.weapon&&!p.reloadUntil&&p.specialPhase==SpecialPhase::none&&p.evadeKind==EvadeKind::none&&!p.ladderAnchor;if(p.aiming!=aiming){p.aiming=aiming;++revision_;}}
Snapshot Authority::snapshot()const{Snapshot out{epoch_,revision_,event_,{}};for(size_t i=0;i<slots_.size();++i)if(slots_[i]){auto p=slots_[i]->state;p.aiming=p.aiming&&p.alive&&!p.stunned&&!p.flightId&&p.weapon&&!p.reloadUntil&&p.specialPhase==SpecialPhase::none&&p.evadeKind==EvadeKind::none&&!p.ladderAnchor;if(!p.reloadUntil)p.reloadElapsedMs=0;out.players[i]=p;}return out;}
}
