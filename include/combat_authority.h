#pragma once
#include "combat_health_rules.h"
#include "item_drop_physics.h"
#include "stage_collision.h"
#include "stage_water.h"
#include "original_reload_timing.h"
#include "original_fire_timing.h"
#include "host_skill_wire.h"
#include "world_inventory_wire.h"
#include "original_sop.h"
#include "evade_action.h"
#include "water_oxygen.h"
#include "weapon_accuracy.h"
#include "cover_policy.h"
#include "special_pc.h"
#include "gekko_jump.h"
#include "gekko_climb.h"
#include "combat_burning.h"
#include "weapon_projectiles.h"
#include "ladder_action.h"
#include "original_weapon_parameters.h"
#include "mounted_weapons.h"
#include "blast_motion.h"
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>
#include <functional>
namespace mgo2mt::combat {
using Vec3=stage::Vec3;
struct Identity {uint8_t slot=255;uint16_t instance=0;uint32_t character=0;bool operator==(const Identity&)const=default;};
enum class SpecialPhase:uint8_t {none,start,hold,end};
struct SopView {Identity recipient;uint32_t life=0,visibleMask=0,activation=0;Vec3 origin{};bool jammed=false;uint32_t inputSequence=0;bool inputSequenced=false;uint32_t evadeRequest=0;uint16_t spreadMilliRadians=0;uint32_t coverRequest=0,specialPcRequest=0;bool operator==(const SopView&)const=default;};
struct Pose {Vec3 feet{};float yaw=0,pitch=0;stage::Capsule capsule{260,1700,2};bool operator==(const Pose&p)const{return feet==p.feet&&yaw==p.yaw&&pitch==p.pitch&&capsule.radius==p.capsule.radius&&capsule.height==p.capsule.height&&capsule.skin==p.capsule.skin;}};
// Values must come from a reviewed weapon profile supplied by the host. There
// is no generic "unknown weapon = rifle" or client-supplied damage fallback.
struct WeaponTuning {
 float weightKg=0,moveSpeedScale=1,reloadSpeedScale=1;
 uint8_t chamberCapacity=0;
 std::optional<weapon_accuracy::Policy> accuracy;
};
struct Weapon {
 uint16_t id=0;uint32_t damage=0,staminaDamage=0,intervalMs=0,reloadMs=0;
 uint16_t magazine=0,reserve=0;float range=0;uint32_t shotCue=0,impactCue=0;
 bool automatic=false;uint32_t bodyCue=0;
 // Separate magazine-fill event from motion completion. Zero retains the
 // single-event profile contract. Real motion ticks require reviewed conversion.
 uint32_t reloadRefillMs=0;
 // Reviewed original motion may replace the two millisecond durations. The
 // host chooses the rate; client packets cannot supply a reload skill/rate.
 std::optional<original::ReloadMotion> reloadMotion;
 // Alternative to intervalMs, resolved once by the authority. Never supplied
 // by a client. Legacy integer-ms profiles keep their previous behavior.
 std::optional<uint32_t> fireIntervalTicks;
 // Reviewed native AK102 primary magazine, no GP30/underbarrel branch.
 bool nativePrimaryMastery=false;
 uint8_t materialMap=0; // Host-selected stage material/audio table.
 bool nativeAkPenetration=false; // Reviewed AK102 material budget / force only.
 // Original 12 bone BOX definitions, fixed male stance pose native adapter.
 // Headshot aim-state flags are not present in the wire; no automatic HS.
 bool nativeAkHitRegions=false;
 // Identity/selection support only, until this weapon's original attack is
 // reviewed. Zero ballistics are mandatory; never borrow another gun's profile.
 bool heldOnly=false;
 bool nativeAkAccuracy=false; // Explicit native burst cone; synthetic profiles default off.
 bool nativeProjectile=false;
 bool originalFirearm=false; // ID-specific recovered damage/ballistics; native instantaneous ray.
 bool meleeAttack=false; // Durable knife/shield attack; never consumes ammunition.
 bool mountedOnly=false; // HOST emplacement profile; never granted to carried inventory.
 bool nativePlaced=false; // HOST installed mine/C4/book adapter.
 // Explicit JSON-provided ballistic data; originalFirearm uses this when
 // present so configured damage/range are not replaced by a compiled table.
 std::optional<original_weapon::Ballistic> ballistics;
 // Zero means this ID. A nonzero value deliberately aliases an existing
 // action implementation; it never supplies damage or missing parameters.
 uint16_t behaviorSourceId=0;
 std::optional<blast_motion::Policy> blastMotion;
 WeaponTuning tuning;
};
// Explicit native weight response, not a recovered original kg conversion.
inline float weapon_move_scale(const Weapon&w){return w.tuning.moveSpeedScale/(1.f+.02f*w.tuning.weightKg);}
inline float weapon_reload_scale(const Weapon&w){return w.tuning.reloadSpeedScale;}
inline uint16_t weapon_chamber_rounds(const Weapon&w,uint16_t total){return (std::min)(total,uint16_t(w.tuning.chamberCapacity));}
inline uint16_t weapon_magazine_rounds(const Weapon&w,uint16_t total){return uint16_t(total-weapon_chamber_rounds(w,total));}
inline uint16_t weapon_reload_capacity(const Weapon&w,uint16_t total){return uint16_t(w.magazine+weapon_chamber_rounds(w,total));}
inline bool valid_weapon_tuning(const Weapon&w){const auto&t=w.tuning;return std::isfinite(t.weightKg)&&t.weightKg>=0&&t.weightKg<=100&&std::isfinite(t.moveSpeedScale)&&t.moveSpeedScale>=.1f&&t.moveSpeedScale<=2&&std::isfinite(t.reloadSpeedScale)&&t.reloadSpeedScale>=.1f&&t.reloadSpeedScale<=5&&t.chamberCapacity<=8&&unsigned(w.magazine)+t.chamberCapacity<=1000&&(!t.accuracy||weapon_accuracy::valid(*t.accuracy))&&(!(t.accuracy||t.chamberCapacity)||(!w.heldOnly&&!w.meleeAttack&&!w.nativePlaced&&!w.nativeProjectile&&w.id!=130&&w.id!=131));}
inline std::optional<weapon_accuracy::Policy> weapon_accuracy_policy(const Weapon&w){return w.tuning.accuracy?w.tuning.accuracy:w.nativeAkAccuracy?std::optional(weapon_accuracy::native_ak):std::nullopt;}
struct Player {
 Identity identity;uint8_t team=0;Pose pose;uint32_t hp=0,maxHp=0,stamina=0,maxStamina=0;
 uint16_t weapon=0,ammo=0,reserve=0;uint64_t reloadUntil=0;bool alive=false,stunned=false;uint32_t life=1;
 // Frozen at first successful spawn; reloadLevel is latched for one motion.
 uint8_t reloadLevel=0,masteryLevel=0,surveyorLevel=0;bool verifiedSkills=false;
 SpecialPhase specialPhase=SpecialPhase::none;
 EvadeKind evadeKind=EvadeKind::none;uint32_t evadeSerial=0;uint16_t evadeElapsedMs=0;
 uint16_t oxygen=water_gameplay::Oxygen::full;bool faceSubmerged=false;
 cover::State cover;
 special_pc::State specialPc;
 bool burning=false;uint16_t ladderAnchor=0;bool aiming=false;
 uint16_t reloadElapsedMs=0; // HOST age, present only while reloading on GWCB19.
 uint16_t mountedId=0; // HOST-owned emplacement occupancy, zero means on foot.
 uint16_t flightId=0,flightElapsedMs=0; // HOST flight instance/serial; age quantized to 250ms.
 uint8_t hawkeyeLevel=0; // Skill 6 from the authenticated, frozen host loadout.
 bool blastFlight=false; // HOST blast flight, distinct from a catapult instance.
 bool operator==(const Player&)const=default;
};
struct WeaponReloadTiming {uint32_t refillMs=0,endMs=0;uint8_t level=0;};
// Shared HOST/presentation resolver; accepts both raw JSON and resolved profiles.
std::optional<WeaponReloadTiming> weapon_reload_timing(const Weapon&,const Player&);
inline bool valid_skills(const Player&p){return p.reloadLevel<=3&&p.masteryLevel<=3&&p.surveyorLevel<=3&&p.hawkeyeLevel<=3&&(p.verifiedSkills||(!p.reloadLevel&&!p.masteryLevel&&!p.surveyorLevel&&!p.hawkeyeLevel))&&(p.reloadUntil||!p.reloadLevel)&&(!p.reloadLevel||(p.weapon==25&&p.reloadLevel==p.masteryLevel));}
inline bool valid_evade(const Player&p){return valid_evade_kind(p.evadeKind)&&(p.evadeKind==EvadeKind::none?(!p.evadeSerial&&!p.evadeElapsedMs):(p.evadeSerial&&p.evadeElapsedMs<10000&&p.alive&&!p.stunned&&!p.reloadUntil&&p.specialPhase==SpecialPhase::none&&p.pose.capsule.height==1700));}
inline bool valid_cover(const Player&p){return cover::valid(p.cover)&&((!p.cover.attached&&!p.cover.lean)||(p.alive&&!p.stunned&&!p.reloadUntil&&p.specialPhase==SpecialPhase::none&&p.evadeKind==EvadeKind::none&&!p.faceSubmerged&&p.pose.capsule.height>=1100));}
inline bool valid_special_pc(const Player&p){return special_pc::valid(p.specialPc)&&(p.specialPc.kind==special_pc::Kind::human?((p.pose.capsule.radius==260||p.pose.capsule.radius==350)&&p.pose.capsule.height!=4200):(p.pose.capsule.radius==special_pc::native_gekko.capsule.radius&&p.pose.capsule.height==special_pc::native_gekko.capsule.height&&!p.reloadUntil&&!p.cover.attached&&!p.cover.lean&&p.specialPhase==SpecialPhase::none&&p.evadeKind==EvadeKind::none&&(!p.specialPc.serial||(p.alive&&!p.stunned))));}

inline bool valid_mounted(const Player&p){return !p.mountedId||(p.mountedId<=32767&&p.alive&&!p.stunned&&!p.flightId&&p.specialPc.kind==special_pc::Kind::human&&!p.ladderAnchor&&!p.cover.attached&&!p.cover.lean&&p.evadeKind==EvadeKind::none&&p.specialPhase==SpecialPhase::none&&p.pose.capsule.height==1700&&!p.reloadUntil);}

inline bool valid_flight(const Player&p){return p.flightId?(p.flightId<=32767&&p.flightElapsedMs<=31750&&p.flightElapsedMs%250==0&&p.alive&&(!p.stunned||p.blastFlight)&&!p.mountedId&&!p.ladderAnchor&&!p.cover.attached&&!p.cover.lean&&!p.reloadUntil&&!p.aiming&&p.evadeKind==EvadeKind::none&&p.specialPhase==SpecialPhase::none&&p.specialPc.kind==special_pc::Kind::human&&(p.blastFlight||p.pose.capsule.height==1700)):!p.flightElapsedMs&&!p.blastFlight;}

enum class Reject {none,unavailable,identity,generation,sequence,invalid_pose,obstructed,too_fast,not_active,dead,weapon,interval,reloading,no_ammo,invalid_direction,clock};
enum class EventKind:uint8_t {shot,impact,damage,death,reload,projectile,projectileTrail,itemPickup,explosion,melee,smoke,catapultLaunch,knockback};
struct Event {
 uint64_t epoch=0,id=0;EventKind kind=EventKind::shot;Identity source,target;
 uint16_t weapon=0;uint32_t cue=0,hp=0,stamina=0,hpDamage=0,staminaDamage=0,object=0;
 Vec3 position{},normal{};
 uint32_t sourceLife=1,targetLife=0;
 // HOST-resolved normal shot ray length; zero disables tracer (including RPG/WP).
 float shotDistance=0;
 bool operator==(const Event&)const=default;
};
inline bool valid_shot_distance(const Event& e){
 if(!std::isfinite(e.shotDistance)||e.shotDistance<0||e.shotDistance>1000000)return false;
 if(e.kind!=EventKind::shot)return e.shotDistance==0;
 if(!e.shotDistance)return true;
 const auto& n=e.normal;const float squared=n[0]*n[0]+n[1]*n[1]+n[2]*n[2];
 return std::isfinite(squared)&&std::abs(squared-1.f)<=.002f;
}
inline bool valid_catapult_event(const Event&e){return e.kind!=EventKind::catapultLaunch||(!e.weapon&&e.object&&e.object<=32767&&(e.cue==0||e.cue==948)&&!e.hp&&!e.stamina&&!e.hpDamage&&!e.staminaDamage&&e.target.slot==255&&!e.target.instance&&!e.target.character&&!e.targetLife&&std::abs(e.normal[0]*e.normal[0]+e.normal[1]*e.normal[1]+e.normal[2]*e.normal[2]-1.f)<.002f);}
inline bool valid_knockback_event(const Event&e){
 if(e.kind!=EventKind::knockback)return true;
 const auto& v=e.normal;const double squared=double(v[0])*v[0]+double(v[1])*v[1]+double(v[2])*v[2];
 return e.weapon&&e.target.slot<24&&e.target.instance&&e.target.character&&e.targetLife&&
        !e.cue&&!e.hp&&!e.stamina&&!e.hpDamage&&!e.staminaDamage&&
        std::isfinite(squared)&&squared>0&&squared<=double(blast_motion::maximum_velocity)*blast_motion::maximum_velocity*1.000001;
}
struct Snapshot {uint64_t epoch=0,revision=0,eventWatermark=0;std::array<std::optional<Player>,24> players{};bool operator==(const Snapshot&)const=default;};
struct FireRequest {uint64_t epoch=0;uint32_t sequence=0;uint16_t weapon=0;Vec3 direction{};uint32_t life=1;};
struct Decision {Reject reject=Reject::none;std::vector<Event> events;explicit operator bool()const{return reject==Reject::none;}};
struct Score {Identity id;uint32_t kills=0,deaths=0;bool operator==(const Score&)const=default;};
struct Policy {bool friendlyFire=false;float maxHorizontalSpeed=6000,maxVerticalSpeed=15000;uint32_t stalePoseMs=500;bool freeForAll=false;};
class Authority {
 struct CatapultFlight {Vec3 velocity{},safeFeet{};uint64_t born=0,at=0;float gravity=9800;uint32_t maximumMs=10000;};
 struct Slot {Player state;std::optional<CatapultFlight> flight;uint16_t blastSerial=0;Vec3 mountedPreviousFeet{};uint16_t mountedPreviousWeapon=0;uint32_t mountedRequest=0;bool mountedSequenced=false;uint64_t carriedFireAt=0;uint32_t carriedFireSubMs=0;bool carriedFired=false;std::array<items::HeldSlot,items::held_slot_count> inventory;
  std::array<items::Contents,items::held_slot_count> humanInventory{};uint16_t humanWeapon=0,specialMeleeWeapon=130;uint8_t humanEquipment=255;
  uint8_t selectedEquipment=255;uint64_t reloadAt=0;uint64_t poseAt=0,fireAt=0,reloadRefillAt=0,itemSequence=0,meleeUntil=0,grenadeJamUntil=0,grenadeStunUntil=0;uint32_t fireSubMsNs=0,fireSequence=0,poseSequence=0;bool fired=false,fireSequenced=false,poseSequenced=false;
  uint64_t specialAt=0;uint32_t specialSequence=0;bool specialSequenced=false,specialHeld=false,sopJammed=false;SopView sopView;
  uint32_t grenadeStamina=0;
  weapon_accuracy::State accuracy;uint16_t reloadCapacity=0;
  water_gameplay::Oxygen oxygen;uint64_t waterAt=0;bool waterClockArmed=false;
  uint64_t evadeAt=0,movedAt=0;float movementSpeed=0,evadeYaw=0;uint32_t evadeRequest=0;bool evadeSequenced=false;
  uint32_t coverSequence=0,coverRequest=0;bool coverSequenced=false;
  uint32_t specialPcRequest=0,specialPcSequence=0;bool specialPcSequenced=false,specialPcHit=false;uint64_t specialPcAt=0;Vec3 specialPcStart{};
  Vec3 approvedVelocity{},previousApprovedVelocity{};uint64_t approvedVelocityAt=0,previousApprovedVelocityAt=0;
  std::optional<special_pc::Jump> specialJump;
  std::optional<special_pc::Climb> specialClimb;
  std::optional<special_pc::RecoveryFall> specialRecovery;
  burning::State burn;
  falling::Tracker falling;
  special_pc::regeneration::State regeneration;
  std::optional<ladder::State> ladderState;uint64_t ladderAt=0;uint32_t ladderSequence=0;bool ladderSequenced=false;
  uint32_t humanHp=0,humanStamina=0;stage::Capsule humanCapsule{};
 };
 std::array<EvadeProfile,2> evadeProfiles_{};
 std::vector<ladder::Anchor> ladders_;
 struct MountedState {mounted::Instance instance;mounted::Type type;uint16_t ammo=0;uint64_t fireAt=0;uint32_t subMs=0;bool fired=false;};
 std::map<uint16_t,MountedState> mounted_;mounted::Registry mountedRegistry_;uint8_t mountedMap_=0;
 MountedState* mounted_state(const Slot&);const MountedState* mounted_state(const Slot&)const;
 void advance_mounted(uint64_t);
 void advance_catapults(uint64_t);
 bool recover_catapult(Slot&,Vec3);
 void cancel_catapult(Slot&,bool recover);
 std::optional<Vec3> blast_velocity(const Slot&,const burning::Blast&,const blast_motion::Policy&)const;
 bool launch_blast(Slot&,Vec3,const blast_motion::Policy&,uint64_t);
 Decision launch_catapult(Slot&,MountedState&,const FireRequest&,uint64_t,uint32_t);
 bool mounted_pose(const Slot&,Pose&)const;
 burning::Policy burnPolicy_{};std::array<burning::Replay,24> explosionReplay_{};
 projectile::Pool projectiles_{128};uint64_t projectileScene_=1;
 struct Trap {items::Entity entity;burning::Source source;uint64_t serial=0,armedAt=0;};
 std::vector<Trap> traps_;
 Decision advance_traps(uint64_t now,std::optional<Identity> detonate={},uint16_t weapon=0);
 void environmental_damage(Slot&,uint32_t,const burning::Source&,Decision&,uint64_t now);
 HealthRules healthRules_;void regenerate(Slot&,uint64_t now);Decision sample_fall(Slot&,uint64_t now);
 items::DropPhysics itemPhysics_{};
 void advance_evade(uint64_t);
 void advance_cover();
 void advance_accuracy(uint64_t);uint64_t accuracy_seed(const Slot&)const;float accuracy_scale(const Slot&,weapon_accuracy::Policy,uint64_t)const;
 water_gameplay::OxygenPolicy oxygenPolicy_;void advance_water(Slot&,uint64_t);bool water_blocks(const Pose&)const;
 original_sop::Groups sopGroups_;bool sopEnabled_=false;uint32_t specialStartMs_=0,specialEndMs_=0;
 void clear_sop_slot(uint8_t);void refresh_sop_views();void advance_sop(uint64_t);
 items::WorldInventory placedItems_{{64,64}};uint64_t itemGeneration_=1;float itemRange_=1500;
 std::map<uint64_t,items::DropPolicy> itemPolicies_;bool itemRecoverOthers_=true;
 static items::HeldSlot* holding(Slot&,uint16_t);
 std::map<uint16_t,uint64_t> fireIntervalsNs_;
 std::array<std::optional<Score>,24> scores_{};
 std::array<std::optional<Slot>,24> slots_{};std::map<uint16_t,Weapon> weapons_;
 std::optional<std::pair<Identity,uint32_t>> spawnLife_;
 struct Skills {Identity id;std::optional<host_skills::Verified> verified;bool frozen=false;};
 std::array<std::optional<Skills>,24> skills_{};
 std::shared_ptr<const stage::Collision> world_,targets_,movement_;std::shared_ptr<const stage::Water> water_;float waterRatio_=.65f;float water_scale(const Pose&)const;Policy policy_;uint64_t epoch_=0,revision_=0,event_=0;bool active_=false;
 Slot* slot(Identity);const Slot* slot(Identity)const;void emit(Decision&,Event);void finish_reload(Slot&,uint64_t);
 Decision attack(Identity,const FireRequest&,uint64_t,uint32_t,bool);
public:
 explicit Authority(Policy={});
 void begin(uint64_t epoch,std::shared_ptr<const stage::Collision> world,std::span<const Weapon> weapons,std::shared_ptr<const stage::Collision> targets={});
 void active(bool enabled);
 bool active()const{return active_;}
 // Recovered object state can replace the immutable scene between simulation
 // steps. A hit decision uses one consistent world revision throughout.
 bool world(std::shared_ptr<const stage::Collision>,std::shared_ptr<const stage::Collision> targets={});
 // Same-round object geometry transaction: retain in-flight projectiles, which
 // trace the new collision on their next step. Stage/epoch replacement uses world().
 bool object_world(std::shared_ptr<const stage::Collision>,std::shared_ptr<const stage::Collision> targets={});
 bool water(std::shared_ptr<const stage::Water>,float nativeRatio=.65f);
 bool oxygen_policy(water_gameplay::OxygenPolicy);
 // Authenticated lobby snapshots only, before this incarnation first spawns.
 bool install_loadout(const host_skills::Verified&);
 bool join(Identity,uint8_t team,const Pose&,uint32_t hp,uint32_t stamina,std::span<const uint16_t> inventory,uint64_t now);
 // Host-only transaction. Only a dead existing life may be replaced, and a
 // failed validated spawn restores the exact previous slot/inventory/revision.
 bool respawn(Identity,uint32_t nextLife,const std::function<bool()>& grant);
 // Available only inside the host's validated respawn callback. The initial
 // grant remains life 1 and uses the separate original initial-spawn group.
 uint32_t spawn_life(Identity id)const{return spawnLife_&&spawnLife_->first==id?spawnLife_->second:1;}
 bool leave(Identity);
 Reject pose(Identity,uint64_t epoch,uint32_t sequence,const Pose&,uint64_t now,uint32_t life=1);
 // Host-configured animation timing; no client packet can change these values.
 bool configure_sop(uint32_t startMs,uint32_t endMs);
 // Correlated to the latest successfully accepted pose sequence, once only.
 Reject special(Identity,uint64_t epoch,uint32_t poseSequence,bool pressed,bool held,uint64_t now,uint32_t life=1);
 bool configure_evade(EvadeProfile roll,EvadeProfile backstep);
 Reject evade(Identity,uint64_t epoch,uint32_t poseSequence,EvadeKind,uint32_t request,uint64_t now,uint32_t life=1);
 bool release_evade(Identity,uint64_t now);
 Reject cover(Identity,uint64_t epoch,uint32_t poseSequence,const cover::Intent&,uint64_t now,uint32_t life=1);
 bool release_cover(Identity);
 Reject assign_special(Identity,special_pc::Kind,bool showName,uint64_t now);
 Reject special_action(Identity,uint64_t epoch,uint32_t poseSequence,const special_pc::Intent&,uint64_t now,uint32_t life=1);
 void release_special_pc(Identity);
 void clear_jump_velocity(Identity);
 Decision advance_special_pc(uint64_t now);
 bool release_special(Identity,uint64_t now);
 // Native host interference adapter: clears this member on onset, requiring
 // a fresh gesture after recovery. Not original status136/160 or skill23.
 bool sop_jam(Identity,bool enabled,uint64_t now);
 std::optional<SopView> sop_view(Identity)const;
 std::optional<uint16_t> accuracy(Identity,uint64_t now)const; // Cone half-angle mrad; nullopt for unknown identity/backward clock.
 Reject equip(Identity,uint64_t epoch,uint16_t weapon,uint64_t now,uint32_t life=1);
 void aiming(Identity,bool); // Presentation state, admitted after pose/life/weapon validation.
 Decision reload(Identity,uint64_t epoch,uint64_t now,uint32_t life=1);
 Decision fire(Identity,const FireRequest&,uint64_t now,uint32_t subMsNs=0);
 Decision melee(Identity,const FireRequest&,uint64_t now);
 Decision advance_projectiles(uint64_t now);
 Decision explode(const burning::Blast&,uint64_t now);
 Decision advance_burning(uint64_t now);
 bool configure_health(const HealthRules&);
 Decision advance_falling(uint64_t now);
 Decision advance_items(uint64_t now);
 void item_box_catalog(const weapons::Catalog&);
 bool configure_mounted(const mounted::Registry&,uint8_t map);
 Reject mount(Identity,uint64_t epoch,uint32_t poseSequence,const mounted::Intent&,uint64_t now,uint32_t life=1);
 bool release_mounted(Identity,bool preserveSeat=false);
 void configure_ladders(std::vector<ladder::Anchor>);
 Decision ladder_action(Identity,uint64_t epoch,uint32_t poseSequence,const ladder::Intent&,uint64_t now,uint32_t life=1);
 void environment(uint64_t now); // HOST water clock before same-tick attacks
 void advance(uint64_t now);
 Snapshot snapshot()const;
 std::vector<projectile::DebugFlight> debug_flights()const{return projectiles_.debug_flights();}
 projectile::Scope debug_projectile_scope()const{return projectiles_.scope();}
 size_t debug_projectile_count()const{return projectiles_.size();}
 const auto& scores()const{return scores_;}
 bool configure_items(uint64_t generation,items::Capacity,float interactionRange=1500);
 items::SnapshotState item_state()const{return placedItems_.state();}
 std::optional<items::wire::Held> item_held(Identity,uint64_t token)const;
 items::Result item_action(Identity admitted,const items::wire::Command&,uint64_t now);
 void item_policies(const items::DropPolicies&); // replaces all native weapon overrides
 bool item_policy(uint32_t item,const items::DropPolicy&,items::Domain domain=items::Domain::weapon);
 std::optional<items::Contents> item_template(items::Domain,uint32_t)const;
 bool seed_items(std::span<const items::Seed>);
 void item_recovery(bool allowOtherPlayers){itemRecoverOthers_=allowOtherPlayers;}
};
// Snapshot state and transient events are separate. Late join establishes an
// event watermark without replaying earlier gunshots, hit sounds or deaths.
class Replica {
 std::optional<Snapshot> state_;uint64_t played_=0;
public:
 bool snapshot(const Snapshot&);
 std::vector<Event> events(std::span<const Event>,bool retainDeaths=false);
 void clear(){state_.reset();played_=0;}
 const std::optional<Snapshot>& state()const{return state_;}
};
}


