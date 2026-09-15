#pragma once
#include "combat_service.h"
#include "combat_world.h"
#include "combat_spawn_profile.h"
#include "item_settings.h"
#include "combat_object_damage.h"
#include "round_items.h"
#include <utility>
namespace mgo2win::combat {
// Windows-only bounded continuation. No score, tickets, team swap, DB writes,
// rotation skipping or original round-count interpretation is performed here.
std::optional<host::LoadRequest> next_cycle_request(const host::LoadRequest&);
class Cycle {
public:
 struct Options {
  std::filesystem::path stageRoot;std::vector<host::Rotation> rotations;
  uint8_t capacity=17;Policy combat;RoundCoordinator::Policy round;
  uint32_t endedDisplayMs=3000;items::Settings items;items::DropPolicies itemFacts;items::RoundItems roundItems;HealthRules health;ObjectDamage::Policy lightDamage;
 };
 using Random=std::function<spawn::Random()>;
private:
 Options options_;Random random_;std::shared_ptr<const weapons::Catalog> catalog_;
 std::optional<stage::ObjectRegistry> registry_;
 struct Content {std::shared_ptr<stage::SceneAuthority> objects;std::shared_ptr<World> world;std::shared_ptr<spawn::StageSelector> selector;std::unique_ptr<Service> service;std::shared_ptr<std::vector<std::vector<uint8_t>>> objectRecords;};
 Content content_;host::LoadRequest request_;uint64_t epoch_=1;bool repeat_=false;
 std::map<uint32_t,Identity> peers_;std::optional<uint64_t> endedAt_;
 Content build(uint64_t,const host::LoadRequest&)const;
public:
 Cycle(Options,std::shared_ptr<const weapons::Catalog>,Random);
 Service& service(){return *content_.service;}
 const host::LoadRequest& request()const{return request_;}
 const stage::SceneAuthority* objects()const{return content_.objects?&*content_.objects:nullptr;}
 const World* world()const{return content_.world.get();}
 std::vector<std::vector<uint8_t>> object_deliveries(){return content_.objectRecords?std::exchange(*content_.objectRecords,{}):std::vector<std::vector<uint8_t>>{};}
 const spawn::StageSelector* selector()const{return content_.selector.get();}
 bool repeat_enabled()const{return repeat_;}
 uint64_t epoch()const{return epoch_;}
 bool admit(Identity,uint64_t);void remove(Identity);
 void poll(uint64_t,uint32_t subMsNs=0);
 // Caller drains old deliveries first, then queues new generation/global state
 // before draining the new offers. UDP peer identities and roster are retained.
 bool advance(uint64_t);
};
}
