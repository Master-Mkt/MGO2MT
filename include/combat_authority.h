#pragma once
#include "stage_collision.h"
#include "stage_water.h"
#include "original_reload_timing.h"
#include "original_fire_timing.h"
#include "host_skill_wire.h"
#include "world_inventory_wire.h"
#include "original_sop.h"
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>
#include <functional>
namespace mgo2win::combat {
using Vec3=stage::Vec3;
struct Identity {uint8_t slot=255;uint16_t instance=0;uint32_t character=0;bool operator==(const Identity&)const=default;};
enum class SpecialPhase:uint8_t {none,start,hold,end};
struct SopView {Identity recipient;uint32_t life=0,visibleMask=0,activation=0;Vec3 origin{};bool jammed=false;uint32_t inputSequence=0;bool inputSequenced=false;bool operator==(const SopView&)const=default;};
struct Pose {Vec3 feet{};float yaw=0,pitch=0;stage::Capsule capsule{260,1700,2};bool operator==(const Pose&p)const{return feet==p.feet&&yaw==p.yaw&&pitch==p.pitch&&capsule.radius==p.capsule.radius&&capsule.height==p.capsule.height&&capsule.skin==p.capsule.skin;}};
// Values must come from a reviewed weapon profile supplied by the host. There
// is no generic "unknown weapon = rifle" or client-supplied damage fallback.
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
};
struct Player {
 Identity identity;uint8_t team=0;Pose pose;uint32_t hp=0,maxHp=0,stamina=0,maxStamina=0;
 uint16_t weapon=0,ammo=0,reserve=0;uint64_t reloadUntil=0;bool alive=false,stunned=false;uint32_t life=1;
 // Frozen at first successful spawn; reloadLevel is latched for one motion.
 uint8_t reloadLevel=0,masteryLevel=0,surveyorLevel=0;bool verifiedSkills=false;
 SpecialPhase specialPhase=SpecialPhase::none;
 bool operator==(const Player&)const=default;
};
inline bool valid_skills(const Player&p){return p.reloadLevel<=3&&p.masteryLevel<=3&&p.surveyorLevel<=3&&(p.verifiedSkills||(!p.reloadLevel&&!p.masteryLevel&&!p.surveyorLevel))&&(p.reloadUntil||!p.reloadLevel)&&(!p.reloadLevel||(p.weapon==25&&p.reloadLevel==p.masteryLevel));}

enum class Reject {none,unavailable,identity,generation,sequence,invalid_pose,obstructed,too_fast,not_active,dead,weapon,interval,reloading,no_ammo,invalid_direction,clock};
enum class EventKind:uint8_t {shot,impact,damage,death,reload};
struct Event {
 uint64_t epoch=0,id=0;EventKind kind=EventKind::shot;Identity source,target;
 uint16_t weapon=0;uint32_t cue=0,hp=0,stamina=0,hpDamage=0,staminaDamage=0,object=0;
 Vec3 position{},normal{};
 uint32_t sourceLife=1,targetLife=0;
 bool operator==(const Event&)const=default;
};
struct Snapshot {uint64_t epoch=0,revision=0,eventWatermark=0;std::array<std::optional<Player>,24> players{};bool operator==(const Snapshot&)const=default;};
struct FireRequest {uint64_t epoch=0;uint32_t sequence=0;uint16_t weapon=0;Vec3 direction{};uint32_t life=1;};
struct Decision {Reject reject=Reject::none;std::vector<Event> events;explicit operator bool()const{return reject==Reject::none;}};
struct Score {Identity id;uint32_t kills=0,deaths=0;bool operator==(const Score&)const=default;};
struct Policy {bool friendlyFire=false;float maxHorizontalSpeed=6000,maxVerticalSpeed=15000;uint32_t stalePoseMs=500;bool freeForAll=false;};
class Authority {
 struct Slot {Player state;std::array<items::HeldSlot,3> inventory;uint64_t poseAt=0,fireAt=0,reloadRefillAt=0,itemSequence=0;uint32_t fireSubMsNs=0,fireSequence=0,poseSequence=0;bool fired=false,fireSequenced=false,poseSequenced=false;
  uint64_t specialAt=0;uint32_t specialSequence=0;bool specialSequenced=false,specialHeld=false,sopJammed=false;SopView sopView;
 };
 original_sop::Groups sopGroups_;bool sopEnabled_=false;uint32_t specialStartMs_=0,specialEndMs_=0;
 void clear_sop_slot(uint8_t);void refresh_sop_views();void advance_sop(uint64_t);
 items::WorldInventory placedItems_{{64,64}};uint64_t itemGeneration_=1;float itemRange_=1500;
 std::map<uint32_t,items::DropPolicy> itemPolicies_;bool itemRecoverOthers_=true;
 static items::HeldSlot* holding(Slot&,uint16_t);
 std::map<uint16_t,uint64_t> fireIntervalsNs_;
 std::array<std::optional<Score>,24> scores_{};
 std::array<std::optional<Slot>,24> slots_{};std::map<uint16_t,Weapon> weapons_;
 std::optional<std::pair<Identity,uint32_t>> spawnLife_;
 struct Skills {Identity id;std::optional<host_skills::Verified> verified;bool frozen=false;};
 std::array<std::optional<Skills>,24> skills_{};
 std::shared_ptr<const stage::Collision> world_,targets_,movement_;std::shared_ptr<const stage::Water> water_;float waterRatio_=.65f;float water_scale(const Pose&)const;Policy policy_;uint64_t epoch_=0,revision_=0,event_=0;bool active_=false;
 Slot* slot(Identity);const Slot* slot(Identity)const;void emit(Decision&,Event);void finish_reload(Slot&,uint64_t);
public:
 explicit Authority(Policy={});
 void begin(uint64_t epoch,std::shared_ptr<const stage::Collision> world,std::span<const Weapon> weapons,std::shared_ptr<const stage::Collision> targets={});
 void active(bool enabled);
 bool active()const{return active_;}
 // Recovered object state can replace the immutable scene between simulation
 // steps. A hit decision uses one consistent world revision throughout.
 bool world(std::shared_ptr<const stage::Collision>,std::shared_ptr<const stage::Collision> targets={});
 bool water(std::shared_ptr<const stage::Water>,float nativeRatio=.65f);
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
 bool release_special(Identity,uint64_t now);
 // Native host interference adapter: clears this member on onset, requiring
 // a fresh gesture after recovery. Not original status136/160 or skill23.
 bool sop_jam(Identity,bool enabled,uint64_t now);
 std::optional<SopView> sop_view(Identity)const;
 Reject equip(Identity,uint64_t epoch,uint16_t weapon,uint64_t now,uint32_t life=1);
 Decision reload(Identity,uint64_t epoch,uint64_t now,uint32_t life=1);
 Decision fire(Identity,const FireRequest&,uint64_t now,uint32_t subMsNs=0);
 void advance(uint64_t now);
 Snapshot snapshot()const;
 const auto& scores()const{return scores_;}
 bool configure_items(uint64_t generation,items::Capacity,float interactionRange=1500);
 items::SnapshotState item_state()const{return placedItems_.state();}
 std::optional<items::wire::Held> item_held(Identity,uint64_t token)const;
 items::Result item_action(Identity admitted,const items::wire::Command&,uint64_t now);
 void item_policies(const items::DropPolicies&); // replaces all native weapon overrides
 bool item_policy(uint32_t weapon,const items::DropPolicy&);
 void item_recovery(bool allowOtherPlayers){itemRecoverOthers_=allowOtherPlayers;}
};
// Snapshot state and transient events are separate. Late join establishes an
// event watermark without replaying earlier gunshots, hit sounds or deaths.
class Replica {
 std::optional<Snapshot> state_;uint64_t played_=0;
public:
 bool snapshot(const Snapshot&);
 std::vector<Event> events(std::span<const Event>);
 void clear(){state_.reset();played_=0;}
 const std::optional<Snapshot>& state()const{return state_;}
};
}
