#include "combat_cycle.h"
#include <iostream>
#include <set>
using namespace mgo2mt;
namespace cw=combat::wire;
namespace {void check(bool v,const char*m){if(!v)throw std::runtime_error(m);}}
int main(int argc,char**argv){try{
 check(argc==4,"weapon catalog, drop catalog, actual stage directory");
 auto catalog=std::make_shared<weapons::Catalog>();std::string error;check(catalog->load(argv[1],error),"weapon catalog");
 combat::Cycle::Options options;options.stageRoot=argv[3];options.rotations={{20,1,0}};options.capacity=3;options.round.countdownMs=60000;options.round.roundDurationMs=100;options.endedDisplayMs=0;
 check(options.itemFacts.load(argv[2],error),"actual item facts");options.items.capacity.dropped=1;
 options.roundItems.useGcx=false;options.roundItems.enabled=true;options.roundItems.rules={{20,items::Domain::weapon,1,1,{}},{20,items::Domain::weapon,22,1,{}},{20,items::Domain::equipment,16,1,{}}};
 auto random=[](){return combat::spawn::Random{{1,2,3,4}};};combat::Cycle cycle(options,catalog,random);
 check(cycle.world()&&cycle.selector()&&cycle.repeat_enabled(),"actual Cycle loads world and reviewed spawn profile");
 auto initial=cycle.service().authority().item_state();check(initial.entities.size()==3,"Cycle auto-seeds knife PATRIOT STEALTH and expands capacity");
 std::set<std::pair<items::Domain,uint32_t>> ids;for(const auto&e:initial.entities){ids.emplace(e.contents.domain,e.contents.item);check(e.owner==items::Actor{}&&e.kind==items::PlacementKind::round,"neutral original-domain placement");}
 check(ids==std::set<std::pair<items::Domain,uint32_t>>{{items::Domain::weapon,1},{items::Domain::weapon,22},{items::Domain::equipment,16}},"equipment policy survives native weapon overrides");
 const combat::Identity player{1,257,100};check(cycle.admit(player,0)&&cycle.service().receive(player,cw::encode(cw::Accept{1}),0),"admit");
 cw::Command command;command.epoch=1;command.sequence=1;command.kind=cw::CommandKind::loaded;command.enabled=true;command.generation=1;command.sceneRevision=1;check(cycle.service().receive(player,cw::encode(command),0),"loaded");command={};command.epoch=1;command.sequence=2;command.kind=cw::CommandKind::ready;command.enabled=true;check(cycle.service().receive(player,cw::encode(command),0),"ready");cycle.poll(10);check(cycle.service().take_round_start(),"actual coordinator starts");
 // Use the actual safe-floor seed for a local HOST fixture. No packet grants
 // this spawn, item, or distance; the normal join still checks full clearance.
 const auto entity=initial.entities[1];combat::Pose pose;pose.feet={entity.position.x,entity.position.y,entity.position.z};
 bool joined=false;for(float dx:{0.f,300.f,-300.f,600.f,-600.f}){pose.feet[0]=entity.position.x+dx;if(cycle.service().authority().join(player,1,pose,1000,1000,std::array<uint16_t,1>{25},11)){joined=true;break;}}
 check(joined,"real floor permits nearby HOST pickup fixture");cycle.service().authority().active(true);
 auto held=cycle.service().authority().item_held(player,777);check(bool(held),"held identity");items::wire::Command pickup;pickup.header=held->header;pickup.header.sequence=1;pickup.action=items::wire::Action::pickup;pickup.heldSlot=1;pickup.heldRevision=held->slots[1].revision;pickup.entity=entity.key.id;pickup.entityRevision=entity.revision;
 check(bool(cycle.service().authority().item_action(player,pickup,11))&&cycle.service().authority().item_state().entities.size()==2,"initial nondrop item picked up through authoritative command");
 cycle.service().deliveries();cycle.poll(110);check(cycle.service().ended(),"timer expires");cycle.service().deliveries();check(cycle.object_deliveries().empty()&&cycle.advance(110),"next actual Cycle created after old FIFO drained");
 const auto restored=cycle.service().authority().item_state();check(cycle.epoch()==2&&restored.entities.size()==3&&restored.scope!=initial.scope,"next round restores all three under new identity");
 for(size_t n=0;n<3;++n)check(restored.entities[n].contents==initial.entities[n].contents&&restored.entities[n].position==initial.entities[n].position,"deterministic original-anchor placement restored");
 auto bad=options;bad.roundItems.rules.push_back({20,items::Domain::weapon,65535,1,{}});bool rejected=false;try{combat::Cycle invalid(bad,catalog,random);}catch(...){rejected=true;}check(rejected,"one unknown final rule rejects entire Cycle initialization");
 bad=options;bad.stageRoot/="missing";rejected=false;try{combat::Cycle invalid(bad,catalog,random);}catch(...){rejected=true;}check(rejected,"enabled placement cannot silently survive unavailable world");
 std::cout<<"actual Cycle / original floor anchors / weapon and equipment seeds / pickup / next-round restoration PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
