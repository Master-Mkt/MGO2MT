#include "combat_object_damage.h"
#include "combat_ballistics.h"
#include "combat_initial_profile.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2win;
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
constexpr combat::Identity shooter{0,1,100};
struct Lamp {uint32_t binding=0,component=0;size_t index=0;};
void projectile_world_revision(){
 auto empty=std::make_shared<const stage::Collision>(stage::Collision::make({},{}));
 auto wall=[](float z,uint32_t object){auto mesh=stage::Collision::make({{-10000,-10000,z},{10000,-10000,z},{10000,10000,z},{-10000,10000,z}},{{{0,1,2},0,0,~0u,object},{{0,2,3},0,0,~0u,object}});return std::make_shared<const stage::Collision>(std::move(mesh));};
 auto run=[&](bool objectChange){
  auto a=std::make_unique<combat::Authority>();combat::Weapon w;w.id=50;w.damage=1125;w.intervalMs=100;w.reloadMs=1000;w.magazine=1;w.reserve=3;w.range=150000;w.nativeProjectile=true;
  a->begin(1,wall(10000,88),std::array{w});check(a->join(shooter,1,{},10000,1000,std::array<uint16_t,1>{50},0),"RPG world revision shooter");a->active(true);
  check(bool(a->fire(shooter,{1,1,50,{0,0,1}},0)),"RPG flight admitted before object revision");a->advance_projectiles(20);
  check(objectChange?a->object_world(wall(1500,77)):a->world(wall(1500,77)),"new object collision published");
  auto result=a->advance_projectiles(100);const bool newImpact=std::any_of(result.events.begin(),result.events.end(),[](const auto&e){return e.kind==combat::EventKind::impact&&e.object==77&&std::abs(e.position[2]-1500)<1;});
  check(objectChange?newImpact:result.events.empty(),"object revision preserves flight against new wall; stage replacement still cancels flight");
 };
 run(true);run(false);
}
struct Fixture {
 stage::ObjectRegistry registry;stage::SceneAuthority objects;combat::World world;stage::SceneReceiver receiver;
 combat::Authority authority;combat::ObjectDamage damage;host::LoadRequest request{1,7,0,0,{7,1,0},host::MatchTransition::initial};
 std::vector<Lamp> lamps;uint64_t epoch=10;
 Fixture(const std::filesystem::path& root,uint16_t hits):registry(stage::load_combat_object_registry(root/"n007a.objects.cfg")),objects(registry),world(combat::World::load(root,7)),receiver(registry,0),damage(combat::ObjectDamage::load(root,7,{hits})){
  auto bindings=stage::read_object_bindings(root,false,7);
  for(size_t i=0;i<bindings.size();++i){const auto& b=bindings[i];
   if(!std::any_of(b.lights.begin(),b.lights.end(),[](const auto& r){return r.mask==1&&r.value==1&&!r.light.enabled;}))continue;
   for(const auto& p:b.parts)if(p.collision&&p.mask==1&&p.value==0){lamps.push_back({b.bindingId,p.componentId,i});break;}
  }
  check(lamps.size()==15&&damage.component_count()>=lamps.size(),"actual BB has fifteen explicitly bound lamps, not guessed prop_num indices");reset(10);
 }
 void reset(uint64_t nextEpoch){epoch=nextEpoch;objects.begin(request);receiver.begin(request);check(receiver.receive(request,*objects.snapshot(0))&&receiver.snapshot()&&world.apply(*receiver.snapshot()),"complete initial or next-round BB scene");
  combat::Weapon w;w.id=25;w.damage=50;w.intervalMs=100;w.reloadMs=1000;w.magazine=30;w.reserve=90;w.range=10000;
  authority.begin(epoch,std::make_shared<const stage::Collision>(stage::Collision::make({},{})),std::array{w});check(authority.join(shooter,1,{},1000,1000,std::array<uint16_t,1>{25},0),"admitted shooter");
  check(authority.world(world.collision(),world.targets()),"actual BB collision installed");authority.active(true);
 }
 combat::Event event(uint64_t id,size_t lamp=0)const{combat::Event e;e.epoch=epoch;e.id=id;e.kind=combat::EventKind::impact;e.source=shooter;e.sourceLife=1;e.weapon=25;e.object=lamps[lamp].component;return e;}
 combat::ObjectDamage::Result apply(combat::Event e){return damage.apply({&e,1},objects,world,authority,0);}
 uint8_t state(size_t lamp=0)const{return world.snapshot()->objects[lamps[lamp].index].current;}
 };
void actual_fire(const std::filesystem::path& root){
 auto owned=std::make_unique<Fixture>(root,uint16_t(1));auto& f=*owned;
 const auto component=f.lamps[0].component;auto low=stage::Vec3{1e9f,1e9f,1e9f},high=stage::Vec3{-1e9f,-1e9f,-1e9f};bool geometry=false;
 for(const auto& t:f.world.targets()->triangles)if(t.object==component)for(auto index:t.vertices){geometry=true;const auto& v=f.world.targets()->vertices[index];for(unsigned a=0;a<3;++a){low[a]=(std::min)(low[a],v[a]);high[a]=(std::max)(high[a],v[a]);}}
 check(geometry,"BB target geometry contains a bound lamp component");stage::Vec3 center{};for(unsigned a=0;a<3;++a)center[a]=(low[a]+high[a])*.5f;
 std::optional<combat::Pose> chosen;stage::Vec3 direction{};unsigned targetHits=0,obstructed=0,occupied=0;
 for(float below:{300.f,600.f,900.f,1200.f})for(float horizontal:{700.f,1200.f,1800.f,2500.f})for(unsigned turn=0;turn<8&&!chosen;++turn){
  const float angle=float(turn)*.78539816339f;auto origin=center;origin[0]+=std::sin(angle)*horizontal;origin[1]-=below;origin[2]+=std::cos(angle)*horizontal;
  stage::Vec3 ray{};float length=0;for(unsigned a=0;a<3;++a){ray[a]=center[a]-origin[a];length+=ray[a]*ray[a];}length=std::sqrt(length);for(auto& v:ray)v/=length;
  auto hit=f.world.targets()->ray(origin,ray,length+100);if(!hit||f.world.targets()->triangles[hit->triangle].object!=component)continue;++targetHits;
  const auto path=combat::trace_ak102(origin,ray,length+100,*f.world.collision(),f.world.targets().get());if(!std::any_of(path.impacts.begin(),path.impacts.end(),[&](const auto& impact){return impact.object==component;})){++obstructed;continue;}
  combat::Pose pose;pose.feet=origin;pose.feet[1]-=pose.capsule.height-150;pose.yaw=std::atan2(ray[0],ray[2]);pose.pitch=std::asin(ray[1]);
  if(std::abs(pose.pitch)>1.4f||!f.world.collision()->clear(pose.feet,pose.capsule)){++occupied;continue;}chosen=pose;direction=ray;
 }
 if(!chosen)std::cerr<<"lamp="<<component<<" targetRays="<<targetHits<<" occluded="<<obstructed<<" occupied="<<occupied<<" center="<<center[0]<<","<<center[1]<<","<<center[2]<<"\n";
 check(bool(chosen),"find a collision-clear actual BB shooter pose with unobstructed lamp sightline");
 const auto profiles=combat::initial_profiles(7,1,0);check(!profiles.empty(),"actual runtime BB weapon profiles available");++f.epoch;f.authority.begin(f.epoch,f.world.collision(),profiles,f.world.targets());
 check(f.authority.join(shooter,1,*chosen,1000,1000,std::array<uint16_t,1>{25},0),"actual BB admits shooter at verified collision-clear pose");f.authority.active(true);
 check(f.authority.pose(shooter,f.epoch,1,*chosen,10)==combat::Reject::none,"HOST validates the aiming pose before fire");
 const auto fired=f.authority.fire(shooter,{f.epoch,1,25,direction},10);check(bool(fired)&&f.authority.snapshot().players[0]->ammo==29,"real Authority fire accepts direction and consumes one round");
 check(std::any_of(fired.events.begin(),fired.events.end(),[&](const auto&e){return e.kind==combat::EventKind::impact&&e.object==component&&e.source==shooter&&e.sourceLife==1;}),"real raycast creates authenticated impact for the exact lamp component");
 const auto destroyed=f.damage.apply(fired.events,f.objects,f.world,f.authority,10);check(destroyed.destroyed==1&&destroyed.records.size()==1&&f.state()==1,"fire-generated impact commits the lamp state");
 check(f.receiver.receive(f.request,destroyed.records[0])&&f.receiver.snapshot()->objects[f.lamps[0].index].current==1,"fire-to-impact-to-delta reaches existing SceneReceiver");
 std::cout<<"actual BB Authority::pose/fire -> bound lamp impact -> ObjectDamage -> SceneReceiver PASS\n";
}
}
int main(int argc,char**argv){try{
 projectile_world_revision();
 if(argc==2&&std::string(argv[1])=="--projectiles-only"){std::cout<<"same-round geometry preserves projectile; stage change cancels PASS\n";return 0;}
 check(argc==2,"actual n007a stage root required");const std::filesystem::path root=argv[1];

 for(uint16_t invalid:{uint16_t(0),uint16_t(10001),uint16_t(65535)}){bool rejected=false;try{combat::ObjectDamage::load(root,7,{invalid});}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid native hit threshold rejected before asset loading");}
 auto fOwned=std::make_unique<Fixture>(root,uint16_t(3));auto& f=*fOwned;const auto initial=*f.world.snapshot();const auto collision=f.world.collision()->triangles.size(),targets=f.world.targets()->triangles.size();
 for(unsigned bad=0;bad<8;++bad){auto e=f.event(99);switch(bad){case 0:e.id=0;break;case 1:--e.epoch;break;case 2:e.kind=combat::EventKind::shot;break;case 3:e.object=~0u;break;case 4:++e.source.instance;break;case 5:++e.source.character;break;case 6:++e.sourceLife;break;case 7:e.weapon=0;break;}check(f.apply(e).records.empty()&&f.state()==0,"unknown/stale/unadmitted event cannot damage light");}
 f.authority.active(false);check(f.apply(f.event(101)).records.empty(),"inactive round ignores impacts");f.authority.active(true);
 check(f.apply(f.event(101)).records.empty()&&f.apply(f.event(101)).records.empty()&&f.state()==0,"first impact counts once despite retransmission");
 check(f.apply(f.event(100)).records.empty()&&f.state()==0,"distinct out-of-order HOST event counts within bounded replay window");
 auto result=f.apply(f.event(102));check(result.destroyed==1&&result.records.size()==1&&result.records.front()==std::vector<uint8_t>{uint8_t(f.lamps[0].index)},"threshold produces exact one-bit lamp delta");
 check(result.combat.events.empty()&&f.authority.snapshot().players[0]->hp==1000&&!f.authority.snapshot().players[0]->burning,"lamp destruction never invokes drum explosion or burning");
 check(f.state()==1&&f.state(1)==0&&f.world.collision()->triangles.size()<=collision&&f.world.targets()->triangles.size()<=targets,"state and collision revision commit together without affecting another lamp");
 check(f.receiver.receive(f.request,result.records.front())&&f.receiver.snapshot()->objects[f.lamps[0].index].current==1,"existing receiver applies lamp delta");
 check(f.apply(f.event(102)).records.empty()&&f.apply(f.event(103)).records.empty(),"destroyed lamp cannot repeat state or sound effects");
 stage::SceneReceiver late(f.registry,1);late.begin(f.request);check(late.receive(f.request,*f.objects.snapshot(1))&&late.snapshot()->objects[f.lamps[0].index].current==1,"late join full snapshot contains broken lamp without replaying damage");
 auto corrupt=*f.world.snapshot();++corrupt.revision;corrupt.objects[f.lamps[1].index].current=2;check(!f.world.apply(corrupt)&&f.state(1)==0,"unknown lamp state fails closed");
 ++f.request.sequence;++f.request.generation;f.reset(11);check(f.state()==0&&f.state(1)==0&&f.world.collision()->triangles.size()==collision&&f.world.targets()->triangles.size()==targets,"next round restores all intact geometry and lamp states");
 auto old=f.event(104);old.epoch=10;check(f.apply(old).records.empty(),"prior epoch impact does not affect next round");
 check(f.apply(f.event(1)).records.empty()&&f.apply(f.event(2)).records.empty()&&f.apply(f.event(3)).destroyed==1,"new epoch resets threshold and event serial scope");
 auto oneOwned=std::make_unique<Fixture>(root,uint16_t(1));auto& one=*oneOwned;check(one.apply(one.event(1,1)).destroyed==1&&one.state()==0&&one.state(1)==1,"default one-hit native policy targets only the hit lamp");
 auto atomicOwned=std::make_unique<Fixture>(root,uint16_t(1));auto& atomic=*atomicOwned;auto saturated=*atomic.world.snapshot();saturated.revision=UINT64_MAX;check(atomic.world.apply(saturated),"revision boundary fixture");auto before=atomic.objects.snapshot(0);check(atomic.apply(atomic.event(1)).records.empty()&&atomic.objects.snapshot(0)==before&&atomic.state()==0,"revision exhaustion leaves state and published world intact");
 auto windowOwned=std::make_unique<Fixture>(root,uint16_t(10000));auto& window=*windowOwned;std::vector<combat::Event> many;many.reserve(9999);for(uint64_t i=1;i<10000;++i)many.push_back(window.event(i));check(window.damage.apply(many,window.objects,window.world,window.authority,0).records.empty()&&window.state()==0,"bounded replay history retains threshold progress");
 check(window.apply(window.event(1)).records.empty()&&window.state()==0,"retired old serial cannot replay after bounded history eviction");check(window.apply(window.event(10000)).destroyed==1,"exact configured upper threshold commits once");
 actual_fire(root);
 std::cout<<"BB actual lamp bindings / native threshold / replay / scene transaction / late join / round reset PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
