#pragma once
#include "combat_authority.h"
#include "weapon_effect_config.h"
#include <set>
#include <functional>
namespace mgo2mt::combat::particles {
enum class Kind {casing,smoke,flash,explosion,smokeCloud,humanBlood,gekkoBlood};
struct Segment {Vec3 from{},to{};std::array<float,4> rgba{};float widthPixels=1;Kind kind=Kind::casing;};
struct Sprite {Vec3 position{};float radius=1,rotation=0;std::array<float,4> rgba{1,1,1,1};uint32_t texture=0;std::array<float,4> uv{0,0,1,1};bool additive=false;std::array<float,2> stretch{1,1};};
// Original effect image references. Presentation timing/atlas selection is a
// native adapter, not an implementation of the original CPEF particle VM.
bool has_muzzle(uint16_t weapon) noexcept;
bool has_casing(uint16_t weapon) noexcept;
// Native visual policy, not recovered original particle physics/hand transforms.
struct Policy {size_t capacity=256;uint64_t smokeMs=1600,casingMs=900,bloodMs=650,gekkoBloodMs=900;};
struct CasingEmission {Vec3 origin{},direction{};};
using CasingResolver=std::function<std::optional<CasingEmission>(const Event&)>;
class Pool {
 struct Entry {Kind kind;Identity owner;uint32_t life;Vec3 origin,velocity;uint64_t born,id;uint16_t weapon=0;Vec3 direction{};uint64_t contactAge=0;bool landed=false;Vec3 landedPosition{};};
 Policy policy_;std::vector<Entry> entries_;std::set<uint64_t> seen_;
 std::map<uint16_t,std::pair<uint32_t,uint32_t>> textures_;
 std::map<uint16_t,uint16_t> sources_;
 std::shared_ptr<const weapon_effect::Config> config_;
 uint64_t epoch_=0,scene_=0,floor_=0,now_=0;
 void expire(const Snapshot&,uint64_t);
 const std::vector<weapon_effect::Emitter>* configured(const Entry&)const;
 uint64_t lifetime(const Entry&)const;
 uint16_t source(uint16_t weapon)const{auto i=sources_.find(weapon);return i==sources_.end()?weapon:i->second;}
public:
 explicit Pool(Policy={});
 void configure(std::shared_ptr<const weapon_effect::Config> config){config_=std::move(config);entries_.clear();}
 void effect_source(uint16_t weapon,uint16_t legacy){sources_[weapon]=legacy;}
 void textures(uint16_t weapon,uint32_t flash,uint32_t smoke){if(weapon&&flash&&smoke)textures_[weapon]={flash,smoke};}
 void clear();
 // New scope seeds a historical-event baseline. On later snapshots, delayed
 // event chunks up to the current snapshot watermark are accepted once.
 void synchronize(uint64_t epoch,uint64_t scene,uint64_t watermark,uint64_t now);
 void dispatch(std::span<const Event>,const Snapshot&,uint64_t now,const CasingResolver& casing={});
 // Blood consumes HP damage with a current target/life, body contact position
 // and unit emission direction. It survives that life's death, not respawn.
 // Zero-normal environmental damage and stamina-only hits do not emit blood.
 std::vector<Segment> sample(const Snapshot&,uint64_t now);
 std::vector<Sprite> sprites(const Snapshot&,uint64_t now);
 // First swept contact of each original CNP casing trajectory, at most once.
 // These are local presentation contacts for sound, never damage events.
 std::vector<Event> casing_contacts(const Snapshot&,uint64_t now,const stage::Collision&);
 size_t size()const noexcept{return entries_.size();}
};
}
