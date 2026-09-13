#include "combat_cycle.h"
#include "combat_initial_profile.h"
#include "stage_profiles.h"
#include <fstream>
#include <limits>
namespace mgo2win::combat {
std::optional<host::LoadRequest> next_cycle_request(const host::LoadRequest&r){
 if(!r.sequence||!r.generation||r.sequence==std::numeric_limits<uint64_t>::max()||r.index||!stage::runtime_stage_supported(r.rotation.map)||r.rotation.rule>1||r.rotation.flags)return {};
 return host::LoadRequest{r.sequence+1,uint8_t(r.generation==255?1:r.generation+1),0,uint8_t(r.round+1),r.rotation,host::MatchTransition::next_round};
}
Cycle::Cycle(Options options,std::shared_ptr<const weapons::Catalog> catalog,Random random):options_(std::move(options)),random_(std::move(random)),catalog_(std::move(catalog)){
 if(options_.rotations.empty()||options_.rotations.size()>15||!options_.capacity||options_.capacity>17||options_.endedDisplayMs>60000)throw std::invalid_argument("Native cycle options");
 const auto rotation=options_.rotations.front();
 repeat_=options_.rotations.size()==1&&stage::runtime_stage_supported(rotation.map)&&rotation.rule<=1&&!rotation.flags&&!options_.round.dpEnabled&&options_.round.roundDurationMs;
 request_={1,1,0,0,options_.rotations.front(),host::MatchTransition::initial};
 try{registry_=stage::load_combat_object_registry(options_.stageRoot/(std::string(stage::name(rotation.map))+".objects.cfg"));}catch(...){/* unavailable content stays unavailable */}
 content_=build(epoch_,request_);
}
Cycle::Content Cycle::build(uint64_t epoch,const host::LoadRequest&request)const{
 Content next;auto combatPolicy=options_.combat;combatPolicy.freeForAll=request.rotation.rule==0;next.service=std::make_unique<Service>(epoch,combatPolicy);
 auto profiles=initial_profiles(request.rotation.map,request.rotation.rule,request.rotation.flags);
 if(registry_){next.objects.emplace(*registry_);next.objects->begin(request);}
 if(next.objects&&next.objects->request())try{
  auto world=std::make_shared<World>(World::load(options_.stageRoot,request.rotation.map));stage::SceneReceiver scene(*registry_,0);scene.begin(request);
  if(auto image=next.objects->snapshot(0))scene.receive(request,*image);
  if(scene.snapshot()&&world->apply(*scene.snapshot())){next.service->configure(world->collision(),profiles,world->targets());next.world=std::move(world);}
 }catch(...){/* malformed or absent world never grants combat */}
 if(options_.capacity>1&&next.world&&stage::runtime_stage_supported(request.rotation.map)&&request.rotation.rule<=1&&!request.rotation.flags)try{
  std::ifstream in(options_.stageRoot/(std::string(stage::name(request.rotation.map))+(request.rotation.rule==0?".dm-spawns.cfg":".tdm-spawns-v2.cfg")));auto profile=spawn::StageProfile::read(in);
  if(profile.stage()!=stage::name(request.rotation.map))throw std::runtime_error("Stage spawn identity");
  if(random_)next.selector=std::make_shared<spawn::StageSelector>(std::move(profile),spawn::Context{epoch,uint8_t(options_.capacity-1),false,request.rotation.map,request.rotation.rule},random_());
 }catch(...){/* missing original spawn or RNG keeps deployment unavailable */}
 RoundCoordinator::Spawn grant;
 if(next.world&&next.selector)grant=[world=next.world,movement=stage::movement_collision(next.world->collision()),selector=next.selector](Authority&a,Identity id,uint8_t team,std::span<const uint16_t> inventory,uint64_t now){
  auto raw=selector->context().rule==0?std::optional<uint8_t>(0):spawn::raw_team(team);if(!raw)return false;auto proposal=selector->propose(a.spawn_life(id)>1?spawn::Kind::respawn:spawn::Kind::initial,*raw);if(!proposal)return false;
  std::vector<Pose> occupied;for(const auto&p:a.snapshot().players)if(p&&p->alive)occupied.push_back(p->pose);
  auto placement=spawn::place(*proposal,*movement,occupied);if(!placement||!a.join(id,team,placement.pose,1000,1000,inventory,now))return false;
  if(!selector->commit(*proposal)){a.leave(id);return false;}return true;
 };
 auto policy=options_.round;policy.generation=request.generation;policy.endOnTimeout=repeat_;policy.freeForAll=request.rotation.rule==0;
 next.service->configure_round(policy,catalog_,std::move(grant));if(next.world){auto& a=next.service->authority();try{std::ifstream waterIn(options_.stageRoot/(std::string(stage::name(request.rotation.map))+".gww"));if(waterIn)a.water(std::make_shared<const stage::Water>(stage::Water::read(waterIn)));}catch(...){throw std::runtime_error("Invalid stage water");}if(!a.configure_items(request.generation,options_.items.capacity))throw std::runtime_error("Item capacity");a.item_recovery(options_.items.recoverOthers);
  for(const auto&[key,entry]:options_.itemFacts.entries())if(entry.domain==items::Domain::weapon)a.item_policy(entry.id,options_.items.policy(entry.id,&entry.policy));
  for(const auto&[id,entry]:options_.items.weapons)if(!options_.itemFacts.find(id))a.item_policy(id,options_.items.policy(id));
 }return next;
}
bool Cycle::admit(Identity id,uint64_t now){if(!service().admit(id,now))return false;peers_.emplace(id.character,id);return true;}
void Cycle::remove(Identity id){service().remove(id);auto it=peers_.find(id.character);if(it!=peers_.end()&&it->second==id)peers_.erase(it);}
void Cycle::poll(uint64_t now,uint32_t subMsNs){service().poll(now,subMsNs);if(service().ended()&&!endedAt_)endedAt_=now;}
bool Cycle::advance(uint64_t now){
 if(!repeat_||!endedAt_||now<*endedAt_||now-*endedAt_<options_.endedDisplayMs||service().pending_deliveries()||epoch_==std::numeric_limits<uint64_t>::max())return false;
 auto request=next_cycle_request(request_);if(!request)return false;
 auto next=build(epoch_+1,*request);
 for(const auto&[character,id]:peers_){auto old=service().preparation(id,now);if(!old||!old->players[id.slot]||!next.service->admit(id,now,old->players[id.slot]->team))throw std::runtime_error("Native cycle retained identity");}
 content_=std::move(next);request_=*request;++epoch_;endedAt_.reset();return true;
}
}
