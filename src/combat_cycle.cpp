#include "combat_cycle.h"
#include "combat_initial_profile.h"
#include "stage_profiles.h"
#include "gcx_round_items.h"
#include "weapon_restrictions.h"
#include "gameplay_fingerprint.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
namespace mgo2mt::combat {
namespace {
bool restricted_round_item(const restrictions::Bits& bits,items::Domain domain,uint32_t item){
 if(!restrictions::enabled(bits))return false;
 if(domain==items::Domain::weapon){
  for(const auto& entry:restrictions::catalog())
   if(entry.weapon_id&&*entry.weapon_id==item)return restrictions::effective_locked(bits,entry);
 }else if(domain==items::Domain::equipment){
  // Reviewed runtime equipment joins. Numeric equipment IDs are not mask bits.
  const auto* entry=restrictions::find(item==10?"drum":item==22?"envg":"");
  if(entry)return restrictions::effective_locked(bits,*entry);
 }
 return false; // No invented restriction for equipment without a catalog row.
}
}
std::optional<host::LoadRequest> next_cycle_request(const host::LoadRequest&r){
 if(!r.sequence||!r.generation||r.sequence==std::numeric_limits<uint64_t>::max()||r.index||!stage::runtime_stage_supported(r.rotation.map)||r.rotation.rule>1||r.rotation.flags)return {};
 return host::LoadRequest{r.sequence+1,uint8_t(r.generation==255?1:r.generation+1),0,uint8_t(r.round+1),r.rotation,host::MatchTransition::next_round};
}
Cycle::Cycle(Options options,std::shared_ptr<const weapons::Catalog> catalog,Random random):options_(std::move(options)),random_(std::move(random)),catalog_(std::move(catalog)){
 if(options_.rotations.empty()||options_.rotations.size()>15||!options_.capacity||options_.capacity>17||options_.endedDisplayMs>60000||!options_.roundItems.valid()||!options_.health.valid()||!options_.lightDamage.valid())throw std::invalid_argument("Native cycle options");
 // Freeze one validated configuration for the room. Invalid present JSON must
 // never silently restore old damage or loadout values.
 const auto data=options_.stageRoot.parent_path();std::string configError;auto fingerprint=gameplay::fingerprint(data);if(!fingerprint)throw std::runtime_error("Gameplay configuration fingerprint failed");configuration_=*fingerprint;
 if(std::filesystem::exists(data/"gameplay.json")){gameplay::Config checked;if(!checked.load(data/"gameplay.json",configError))throw std::runtime_error(configError);gameplay_=std::move(checked);}
 if(std::filesystem::exists(data/"mounted_weapons.json")&&!mounted_.load(data/"mounted_weapons.json",configError))throw std::runtime_error(configError);
 if(gameplay::fingerprint(data)!=std::optional(configuration_))throw std::runtime_error("Gameplay configuration changed while loading");
 const auto rotation=options_.rotations.front();
 repeat_=options_.rotations.size()==1&&stage::runtime_stage_supported(rotation.map)&&rotation.rule<=1&&!rotation.flags&&!options_.round.dpEnabled&&options_.round.roundDurationMs;
 request_={1,1,0,0,options_.rotations.front(),host::MatchTransition::initial};
 try{registry_=stage::load_combat_object_registry(options_.stageRoot/(std::string(stage::name(rotation.map))+".objects.cfg"));}catch(...){/* unavailable content stays unavailable */}
 content_=build(epoch_,request_);
}
Cycle::Content Cycle::build(uint64_t epoch,const host::LoadRequest&request)const{
 Content next;auto combatPolicy=options_.combat;combatPolicy.freeForAll=request.rotation.rule==0;next.service=std::make_unique<Service>(epoch,combatPolicy);next.service->configure_environment(options_.environment);
 next.service->configuration(configuration_);
 auto profiles=initial_profiles(request.rotation.map,request.rotation.rule,request.rotation.flags);
 if(gameplay_&&!profiles.empty())profiles=gameplay_->profiles(request.rotation.map);
 // A prohibited knife must not enter the authority's grant or pickup registry.
 if(restricted_round_item(options_.round.restrictions,items::Domain::weapon,1))
  std::erase_if(profiles,[](const Weapon& weapon){return weapon.id==1;});
 // The complete candidate remains private until build returns. Determine
 // selected CBOX ordinals before the first E0 snapshot, so both HOST collision
 // and every joining receiver see current=initial=3 for a replaced original.
 items::GcxRoundPlan gcxPlan;
 const bool explicitGcx=options_.roundItems.useGcx&&std::any_of(options_.roundItems.replacements.begin(),options_.roundItems.replacements.end(),[&](const auto&r){return r.map==request.rotation.map;});
 const bool gcxRoute=stage::runtime_stage_supported(request.rotation.map)&&request.rotation.rule<=1&&!request.rotation.flags;
 if(explicitGcx&&!gcxRoute)throw std::runtime_error("Configured GCX replacement is outside reviewed DM/TDM scope");
 if(options_.roundItems.useGcx&&gcxRoute){
  auto layout=items::load_gcx_item_layout(options_.stageRoot,request.rotation.map);stage::CboxLayout cboxes;
  if(layout&&layout->verified){
   const auto path=stage::asset_path(options_.stageRoot,request.rotation.map,".cbox.cfg");
   if(!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)>16384)throw std::runtime_error("GCX replacement CBOX layout unavailable");
   std::ifstream in(path);cboxes=stage::CboxLayout::read(in);
  }
  gcxPlan=items::plan_gcx_round_items(options_.roundItems,layout?&*layout:nullptr,cboxes,request.rotation.map,request.rotation.rule,request.generation);
  std::erase_if(gcxPlan.items,[&](const items::GcxPlacement& item){
   if(!restricted_round_item(options_.round.restrictions,item.domain,item.item))return false;
   const bool replacement=std::any_of(options_.roundItems.replacements.begin(),options_.roundItems.replacements.end(),[&](const auto&r){return r.map==request.rotation.map&&r.source==item.source&&(!r.sourceOffset||r.sourceOffset==item.sourceOffset);});
   if(replacement)throw std::runtime_error("GCX replacement target is prohibited by HOST item restrictions");
   return true;
  });
  // Missing research does not mean the stage has zero original items. Keep
  // that stage's existing behavior, but never silently ignore an explicit edit.
  if(!gcxPlan.verified&&explicitGcx)throw std::runtime_error("Configured GCX replacement has no verified stage layout");
 }
 const bool requireGcx=options_.roundItems.useGcx&&gcxPlan.verified;
 size_t planned=gcxPlan.items.size();
 if(options_.roundItems.enabled)for(const auto&r:options_.roundItems.rules)if(r.map==request.rotation.map)planned+=r.count;
 if(options_.roundItems.enabled)for(const auto&r:options_.roundItems.rules)if(r.map==request.rotation.map&&restricted_round_item(options_.round.restrictions,r.domain,r.item))throw std::runtime_error("Manual round item is prohibited by HOST item restrictions");
 if(planned>4096)throw std::runtime_error("Combined GCX/manual round item capacity exceeds 4096");
 if(registry_){next.objects=std::make_shared<stage::SceneAuthority>(*registry_);next.objects->begin(request);}
 if(requireGcx){
  if(!next.objects||!next.objects->request())throw std::runtime_error("GCX round item scene unavailable");
  for(auto binding:gcxPlan.removeCboxBindings){
   const auto&entries=next.objects->registry().entries;
   auto found=std::find_if(entries.begin(),entries.end(),[&](const auto&e){return e.bindingId==binding;});
   if(found==entries.end()||found->width!=2||found->update!=host::ObjectStates::Update::maximum||!next.objects->update(binding,3))throw std::runtime_error("GCX replacement CBOX binding rejected");
  }
 }
 if(next.objects&&next.objects->request())try{
  auto world=std::make_shared<World>(World::load(options_.stageRoot,request.rotation.map));stage::SceneReceiver scene(*registry_,0);scene.begin(request);
  if(auto image=next.objects->snapshot(0))scene.receive(request,*image);
  if(scene.snapshot()&&world->apply(*scene.snapshot())){next.service->configure(world->collision(),profiles,world->targets());next.world=std::move(world);}
 }catch(...){if(requireGcx)throw;/* malformed or absent world never grants combat */}
 if(requireGcx&&!next.world)throw std::runtime_error("GCX round item world unavailable");
 if(next.world&&!next.service->authority().configure_mounted(mounted_,request.rotation.map))throw std::runtime_error("Mounted weapon scene configuration rejected");
 if(next.world&&!next.service->authority().configure_health(options_.health))throw std::runtime_error("Combat health settings");
 if(next.world&&next.objects){
  auto damage=std::make_shared<ObjectDamage>(ObjectDamage::load(options_.stageRoot,request.rotation.map,options_.lightDamage));next.objectRecords=std::make_shared<std::vector<std::vector<uint8_t>>>();
  next.service->event_handler([damage,objects=next.objects,world=next.world,records=next.objectRecords,service=next.service.get()](std::span<const Event> events,uint64_t now){
   auto result=damage->apply(events,*objects,*world,service->authority(),now);for(auto& record:result.records)records->push_back(std::move(record));return result.combat;
  });
 }
 if(options_.capacity>1&&next.world&&stage::runtime_stage_supported(request.rotation.map)&&request.rotation.rule<=1&&!request.rotation.flags)try{
  std::ifstream in(options_.stageRoot/(std::string(stage::name(request.rotation.map))+(request.rotation.rule==0?".dm-spawns.cfg":".tdm-spawns-v2.cfg")));auto profile=spawn::StageProfile::read(in);
  if(profile.stage()!=stage::name(request.rotation.map))throw std::runtime_error("Stage spawn identity");
  if(random_)next.selector=std::make_shared<spawn::StageSelector>(std::move(profile),spawn::Context{epoch,uint8_t(options_.capacity-1),false,request.rotation.map,request.rotation.rule},random_());
 }catch(...){/* missing original spawn or RNG keeps deployment unavailable */}
 RoundCoordinator::Spawn grant;
 if(next.world&&next.selector)grant=[world=next.world,selector=next.selector](Authority&a,Identity id,uint8_t team,std::span<const uint16_t> inventory,uint64_t now){
  auto raw=selector->context().rule==0?std::optional<uint8_t>(0):spawn::raw_team(team);if(!raw)return false;auto proposal=selector->propose(a.spawn_life(id)>1?spawn::Kind::respawn:spawn::Kind::initial,*raw);if(!proposal)return false;
  std::vector<Pose> occupied;for(const auto&p:a.snapshot().players)if(p&&p->alive)occupied.push_back(p->pose);
  auto movement=stage::movement_collision(world->collision());
  auto placement=spawn::place(*proposal,*movement,occupied);if(!placement||!a.join(id,team,placement.pose,1000,1000,inventory,now))return false;
  if(!selector->commit(*proposal)){a.leave(id);return false;}return true;
 };
 auto policy=options_.round;policy.generation=request.generation;policy.endOnTimeout=repeat_;policy.freeForAll=request.rotation.rule==0;
 next.service->configure_round(policy,catalog_,std::move(grant));if(next.world){auto& a=next.service->authority();a.item_box_catalog(*catalog_);try{std::ifstream waterIn(options_.stageRoot/(std::string(stage::name(request.rotation.map))+".gww"));if(waterIn)a.water(std::make_shared<const stage::Water>(stage::Water::read(waterIn)));}catch(...){throw std::runtime_error("Invalid stage water");}if(!a.configure_items(request.generation,options_.items.capacity))throw std::runtime_error("Item capacity");a.item_recovery(options_.items.recoverOthers);
  const auto ladderPath=options_.stageRoot/(std::string(stage::name(request.rotation.map))+"_ladders.cfg");
  if(std::filesystem::exists(ladderPath))a.configure_ladders(ladder::load(ladderPath));
  a.item_policies(options_.itemFacts);
  for(const auto&[key,entry]:options_.itemFacts.entries())if(entry.domain==items::Domain::weapon)a.item_policy(entry.id,options_.items.policy(entry.id,&entry.policy));
  for(const auto&[id,entry]:options_.items.weapons)if(!options_.itemFacts.find(id))a.item_policy(id,options_.items.policy(id));
  if(options_.roundItems.enabled||requireGcx){
   std::vector<stage::Vec3> anchors;
   if(next.selector)for(uint8_t team=0;team<3;++team)for(const auto& e:next.selector->profile().group(spawn::Variant::normal,spawn::Kind::initial,team))anchors.push_back(e.position);
   auto movement=stage::movement_collision(next.world->collision());
   const auto lookup=[&a](items::Domain domain,uint32_t id){return a.item_template(domain,id);};
   auto seeds=requireGcx?items::resolve_gcx_round_items(gcxPlan,*movement,lookup):std::vector<items::Seed>{};
   auto manual=items::resolve_round_items(options_.roundItems,request.rotation.map,*movement,anchors,lookup);
   // Each resolver checks its own batch. Cross-source overlap must also
   // reject before the single seed commit; never erase the source CBOX and
   // publish two items sharing that placement.
   for(const auto& placed:manual)if(std::any_of(seeds.begin(),seeds.end(),[&](const auto& original){
    return std::hypot(placed.position.x-original.position.x,placed.position.z-original.position.z)<150&&std::abs(placed.position.y-original.position.y)<200;
   }))throw std::runtime_error("Combined GCX/manual round item placements overlap");
   seeds.insert(seeds.end(),manual.begin(),manual.end());
   if(seeds.size()>4096)throw std::runtime_error("Combined GCX/manual round item capacity exceeds 4096");
   auto capacity=options_.items.capacity;capacity.dropped=(std::max)(capacity.dropped,static_cast<uint32_t>(seeds.size()));
   if(!a.configure_items(request.generation,capacity)||!a.seed_items(seeds))throw std::runtime_error("Round item initial placement rejected");
  }
 }else if(options_.roundItems.enabled||requireGcx||explicitGcx)throw std::runtime_error("Round item stage unavailable");return next;
}
bool Cycle::admit(Identity id,uint64_t now){if(!service().admit(id,now))return false;peers_.emplace(id.character,id);return true;}
void Cycle::remove(Identity id){service().remove(id);auto it=peers_.find(id.character);if(it!=peers_.end()&&it->second==id)peers_.erase(it);}
void Cycle::poll(uint64_t now,uint32_t subMsNs){service().poll(now,subMsNs);if(service().ended()&&!endedAt_)endedAt_=now;}
bool Cycle::advance(uint64_t now){
 if(!repeat_||!endedAt_||now<*endedAt_||now-*endedAt_<options_.endedDisplayMs||service().pending_deliveries()||(content_.objectRecords&&!content_.objectRecords->empty())||epoch_==std::numeric_limits<uint64_t>::max())return false;
 auto request=next_cycle_request(request_);if(!request)return false;
 auto next=build(epoch_+1,*request);
 for(const auto&[character,id]:peers_){auto old=service().preparation(id,now);if(!old||!old->players[id.slot]||!next.service->admit(id,now,old->players[id.slot]->team))throw std::runtime_error("Native cycle retained identity");}
 content_=std::move(next);request_=*request;++epoch_;endedAt_.reset();return true;
}
}
