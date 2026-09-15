#include "combat_object_damage.h"
#include "combat_wire.h"
#include <iostream>
using namespace mgo2win;
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
int main(int argc,char**argv){try{
 check(argc==2,"actual stage directory required");const std::filesystem::path root=argv[1];
 auto registry=stage::load_combat_object_registry(root/"n022a.objects.cfg");stage::SceneAuthority objects(registry);host::LoadRequest request{1,7,0,0,{20,1,0},host::MatchTransition::initial};objects.begin(request);
 auto world=combat::World::load(root);stage::SceneReceiver receiver(registry,0);receiver.begin(request);receiver.receive(request,*objects.snapshot(0));check(receiver.snapshot()&&world.apply(*receiver.snapshot()),"complete initial actual scene");
 auto damage=combat::ObjectDamage::load(root);check(damage.component_count()==14,"two reviewed drum solid and six hit components each");
 check(combat::ObjectDamage::load(root,1).component_count()==0,"no invented AA destructible mapping");
 combat::Authority authority;combat::Weapon w;w.id=25;w.damage=50;w.intervalMs=100;w.reloadMs=1000;w.magazine=30;w.reserve=90;w.range=10000;std::array<combat::Weapon,1> weapons{w};
 authority.begin(10,std::make_shared<const stage::Collision>(stage::Collision::make({},{})),weapons);
 const combat::Identity shooter{0,1,100};std::array<uint16_t,1> inventory{25};check(authority.join(shooter,1,{},1000,1000,inventory,0),"admitted shooter fixture");
 check(authority.world(world.collision(),world.targets()),"actual world installed");authority.active(true);
 const auto solid=world.collision()->triangles.size(),hits=world.targets()->triangles.size();
 combat::Event event;event.epoch=10;event.id=1;event.kind=combat::EventKind::impact;event.source=shooter;event.sourceLife=1;event.weapon=25;event.object=32;
 auto result=damage.apply({&event,1},objects,world,authority,0);check(result.destroyed==1&&result.records.size()==1&&result.records[0]==std::vector<uint8_t>{10},"first drum produces exact original one-bit index delta");
 check(world.collision()->triangles.size()+46==solid&&world.targets()->triangles.size()+72==hits,"actual solid and hit geometry switch together");
 check(receiver.receive(request,result.records[0])&&receiver.snapshot()->objects[10].current==1&&receiver.snapshot()->objects[11].current==0,"client original scene receiver applies same drum delta");
 check(damage.apply({&event,1},objects,world,authority,0).records.empty(),"duplicate impact cannot replay destruction");
 ++event.id;event.object=39;result=damage.apply({&event,1},objects,world,authority,0);check(result.destroyed==1&&result.records[0]==std::vector<uint8_t>{11},"second distinct drum delta");
 check(world.collision()->triangles.size()+92==solid&&world.targets()->triangles.size()+144==hits,"only both drums removed");
 ++event.id;event.object=1;check(damage.apply({&event,1},objects,world,authority,0).records.empty(),"car component not misclassified as drum");
 ++event.id;event.object=32;--event.epoch;check(damage.apply({&event,1},objects,world,authority,0).records.empty(),"old epoch never destroys current scene");
 // New round recreates original states and accepts a fresh current hit. Reset
 // remains explicit; old events retain their old combat epoch.
 ++request.sequence;++request.generation;objects.begin(request);receiver.begin(request);receiver.receive(request,*objects.snapshot(0));check(world.apply(*receiver.snapshot()),"new generation restores actual geometry");
 check(world.collision()->triangles.size()==solid&&world.targets()->triangles.size()==hits,"complete next-generation restoration");
 std::cout<<"actual n022a drum component -> one-bit delta -> HOST world and client receiver PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
