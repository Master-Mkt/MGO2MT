#include "combat_ballistics.h"
#include "bullet_decals_overlay.h"
#include "footstep_presentation.h"
#include "foot_ik.h"
#include "weapon_projectiles.h"
#include "mortar_shell_renderer.h"
#include "weapon_aim_presentation.h"
#include "player_lock.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace mgo2mt;
namespace {
unsigned checks=0;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);++checks;}
void near(float a,float b,const char* why){check(std::abs(a-b)<.01f,why);}
struct Scene {
 std::vector<stage::Vec3> vertices;
 std::vector<stage::CollisionTriangle> triangles;
 void wall(float z,uint64_t flags,unsigned material=0){const auto n=unsigned(vertices.size());vertices.insert(vertices.end(),{{-5000,-5000,z},{5000,-5000,z},{5000,5000,z},{-5000,5000,z}});
  triangles.push_back({{n,n+2,n+1},flags,0,material});triangles.push_back({{n,n+3,n+2},flags,0,material});}
 void floor(float y,uint64_t flags,unsigned material=0){const auto n=unsigned(vertices.size());vertices.insert(vertices.end(),{{-5000,y,-5000},{5000,y,-5000},{5000,y,5000},{-5000,y,5000}});
  triangles.push_back({{n,n+2,n+1},flags,0,material});triangles.push_back({{n,n+3,n+2},flags,0,material});}
 stage::Collision build()const{return stage::Collision::make(vertices,triangles,{{0x15bccc,.5f,.5f,true,1000,true},{0xdeadbe,.5f,.5f,true,1000,true}});}
};
projectile::Profile profile(uint16_t id){return {id,10000,0,10000,3000,0,true,0,0,1000};}
projectile::Step launch(uint16_t id,const stage::Collision& world,unsigned& calls){
 projectile::Pool pool;pool.reset({5,7});check(pool.spawn({{5,7},{0,1,10,2},1,id,{0,0,0},{0,0,1}},profile(id),100)==projectile::Submit::accepted,"Original category fixture shot accepted");
 return pool.advance_typed(200,[&](stage::Vec3 origin,stage::Vec3 direction,float maximum,projectile::Owner owner,uint16_t weapon)->std::optional<projectile::Contact>{
  ++calls;check(owner.life==2&&weapon==id,"Trace receives exact accepted flight weapon and incarnation");const auto q=projectile::collision_query(weapon);check(bool(q),"Accepted projectile has explicit collision category");
  if(auto hit=world.ray(origin,direction,maximum,*q))return projectile::Contact{hit->distance,hit->normal,{},0};return {};
 });
}
}
int main(){try{
 namespace a=stage::attribute;
 Scene source;source.wall(100,a::none);source.wall(150,a::reserved_32|a::reserved_33);source.wall(200,a::player);source.wall(300,a::bullet);source.wall(400,a::missile);source.wall(500,a::bomb);source.wall(600,a::camera);source.wall(700,a::bullet_mark);
 const auto world=source.build();const auto originalTriangles=world.triangles;
 near(world.ray({},{0,0,1},1000)->distance,100,"Raw geometry still contains None surface");
 auto bullet=combat::trace_ak102({},{0,0,1},1000,world);check(bullet.blocked&&bullet.impacts.size()==1,"Actual bullet consumer excludes unrelated collision categories");near(bullet.distance,300,"Bullet stops at Bullet surface");
 Scene object;object.wall(250,a::bullet);auto target=object.build();near(combat::trace_ak102({},{0,0,1},1000,world,&target).distance,250,"Bullet applies category to object collision too");
 Scene through;through.wall(50,a::bullet|a::through);through.wall(300,a::bullet);auto penetrable=through.build();bullet=combat::trace_ak102({},{0,0,1},1000,penetrable);check(bullet.blocked&&bullet.impacts.size()==1&&bullet.priorForceCost==0,"Original Through surface remains free inside Bullet query");near(bullet.distance,300,"Through does not become a global collision exclusion");
 for(uint16_t id:{uint16_t(2),uint16_t(50),uint16_t(129),uint16_t(52),uint16_t(53),uint16_t(54),uint16_t(55),uint16_t(56),uint16_t(57),uint16_t(58),uint16_t(59),uint16_t(63),uint16_t(103)}){
  unsigned calls=0;const auto result=launch(id,world,calls);check(calls>0&&result.impacts.size()==1,"Typed original projectile produces one admitted surface impact");near(result.impacts[0].position[2],id==2?300.f:(id==50||id==129)?400.f:500.f,"Projectile category reaches correct separate surface");check(result.impacts[0].weapon==id,"Impact retains its original weapon ID");}
 for(uint16_t id:{uint16_t(0),uint16_t(1),uint16_t(25),uint16_t(104),uint16_t(65535)})check(!projectile::collision_query(id),"Unknown projectile ID has no guessed collision category");
 // Two different flights of the same player retain their own masks; no lookup
 // of an owner's currently selected equipment occurs during future updates.
 projectile::Pool mixed;mixed.reset({5,7});const projectile::Owner owner{0,1,10,2};
 check(mixed.spawn({{5,7},owner,1,50,{},{0,0,1}},profile(50),0)==projectile::Submit::accepted&&mixed.spawn({{5,7},owner,2,52,{},{0,0,1}},profile(52),0)==projectile::Submit::accepted,"Mixed projectile owner is admitted");
 auto mixedResult=mixed.advance_typed(100,[&](auto origin,auto direction,float distance,auto,uint16_t weapon)->std::optional<projectile::Contact>{if(auto h=world.ray(origin,direction,distance,*projectile::collision_query(weapon)))return projectile::Contact{h->distance,h->normal,{},0};return {};});
 check(mixedResult.impacts.size()==2&&mixedResult.impacts[0].weapon==50&&mixedResult.impacts[1].weapon==52,"Both in-flight types retain classification after weapon switching");near(mixedResult.impacts[0].position[2],400,"Missile flight hits missile geometry");near(mixedResult.impacts[1].position[2],500,"Grenade flight hits bomb geometry");
 // Foot material must come from the same Floor + Player support used by the
 // original control cache, even when a closer Sound/IK/Bullet overlay exists.
 Scene floorSource;floorSource.floor(60,a::sound,1);floorSource.floor(50,a::bullet,1);floorSource.floor(40,a::ik,1);floorSource.floor(0,a::floor|a::player);auto floor=floorSource.build();
 auto material=combat::footsteps::dry_floor(floor,{0,2,0});check(material&&material->id==0x15bccc,"Footsteps skip closer non-player floor layers and select original floor material");
 check(!(floor.triangles.back().attribute&a::sound),"A valid material footstep does not require the unrelated Sound bit");
 check(foot_ik::grounded(&floor,{0,2,0},{}),"Foot IK body support uses Floor + Player below decorative IK planes");
 Scene ungrounded;ungrounded.floor(0,a::ik|a::sound|a::camera);const auto onlyIk=ungrounded.build();check(!foot_ik::grounded(&onlyIk,{0,2,0},{}),"IK/Sound/Camera-only plane cannot plant actor body");check(!combat::footsteps::dry_floor(onlyIk,{0,2,0}),"Unrelated categories cannot manufacture floor footsteps");
 Scene zero;zero.floor(0,a::none);const auto none=zero.build();check(!foot_ik::grounded(&none,{0,2,0},{})&&!combat::footsteps::dry_floor(none,{0,2,0}),"None is not silently promoted to player floor");
 auto sound=floor.ray({0,100,0},{0,-1,0},200,stage::query::sound);check(sound&&sound->position[1]==60,"Sound remains independently queryable without muting original footsteps");
 // A static decal needs BulletMark, not merely Player or Bullet. The event is
 // checked against its original surface position with the short recheck ray.
 combat::Event impact;impact.kind=combat::EventKind::impact;impact.epoch=5;impact.id=1;impact.position={0,0,300};impact.normal={0,0,-1};
 check(!combat::decals::static_impact(impact,{5,7},world),"Bullet-blocking surface without BulletMark receives no decal");impact.position[2]=700;auto mark=combat::decals::static_impact(impact,{5,7},world);check(mark&&mark->position==stage::Vec3{0,0,700},"BulletMark surface receives validated native decal");
 Scene water;water.wall(700,a::bullet_mark|a::water);impact.position[2]=700;check(!combat::decals::static_impact(impact,{5,7},water.build()),"Water remains excluded from dry bullet decals");
 near(world.ray({},{0,0,1},1000,stage::query::camera)->distance,600,"Camera mask skips invisible player and weapon barriers");
 combat::Snapshot aimSnapshot;combat::Player aiming;aiming.identity={0,1,10};aimSnapshot.players[0]=aiming;
 for(uint16_t weapon:{uint16_t(25),uint16_t(50),uint16_t(129),uint16_t(103)}){aimSnapshot.players[0]->weapon=weapon;const auto point=reticle::aim_point({},{0,0,1},world,nullptr,aimSnapshot,aiming.identity);check(bool(point),"Aim ray projects an admitted weapon contact");near((*point)[2],weapon==25?300.f:weapon==103?500.f:400.f,"Reticle uses the equipped weapon collision category");}
 Scene cameraOnly;cameraOnly.wall(500,a::camera);const auto cameraGeometry=cameraOnly.build();Scene stopEye;stopEye.wall(500,a::stop_eye);const auto eyeGeometry=stopEye.build();
 check(enemy_tag::visible({0,850,0},{0,850,3000},cameraGeometry),"Camera-only plane does not block eye visibility");check(!enemy_tag::visible({0,850,0},{0,850,3000},eyeGeometry),"StopEye plane blocks eye visibility");
 combat::Snapshot lockSnapshot;lockSnapshot.epoch=5;lockSnapshot.revision=1;host::Roster roster;roster.complete=true;
 for(unsigned i=0;i<2;++i){combat::Player p;p.identity={uint8_t(i),uint16_t(i+1),10+i};p.life=1;p.alive=true;p.team=uint8_t(i+1);p.pose.feet={0,0,float(i*3000)};lockSnapshot.players[i]=p;host::Player row;row.slot=uint8_t(i);row.instance=uint16_t(i+1);row.character=10+i;row.name="TEST";roster.slots[i]=row;}
 player_lock::Input lockInput;lockInput.self=lockSnapshot.players[0]->identity;lockInput.expectedEpoch=5;lockInput.sceneToken=7;lockInput.rule=1;lockInput.eye={0,850,0};lockInput.look={0,0,1};lockInput.active=lockInput.enabled=true;
 player_lock::Lock lock{player_lock::Policy{10000,.9f,.5f}};
 check(bool(lock.acquire(lockSnapshot,roster,lockInput,cameraGeometry)),"Camera-only plane does not prevent actual AUTOAIM capture");check(!lock.acquire(lockSnapshot,roster,lockInput,eyeGeometry),"StopEye plane prevents actual AUTOAIM capture");
 check(bool(enemy_tag::select(lockSnapshot,roster,lockInput.self,1,lockInput.eye,lockInput.look,cameraGeometry,true)),"Camera-only plane does not block aimed enemy-name selection");
 Scene hitProxy;hitProxy.wall(500,a::bullet);const auto hitGeometry=hitProxy.build();check(!enemy_tag::visible(lockInput.eye,{0,850,3000},cameraGeometry,&hitGeometry),"Native GM_HIT proxy uses its explicit Bullet category for visual obstruction");
 // Cosmetic mortar point flight uses the same Bomb category as HOST ID103.
 mortar_shells::Simulation shells(300000);mounted::Registry registry;mounted::Type type;type.id="mortar";type.weapon=103;type.kind=mounted::Kind::mortar;type.launchSpeed=10000;type.gravity=100;registry.types.push_back(type);registry.placements.push_back({20,7,"mortar",{},0});
 combat::Snapshot snapshot;snapshot.epoch=5;snapshot.eventWatermark=1;combat::Player player;player.identity={0,1,10};player.life=2;player.mountedId=7;snapshot.players[0]=player;
 combat::Event shot;shot.epoch=5;shot.id=1;shot.source=player.identity;shot.sourceLife=2;shot.weapon=103;shot.object=7;shot.normal={0,0,1};auto geometry=std::make_shared<const stage::Collision>(world);
 shells.update(std::span(&shot,1),&snapshot,registry,20,geometry,0,7);shells.update({},&snapshot,registry,20,geometry,40,7);check(shells.size()==1,"Visible mortar shell passes Bullet/Missile-only planes");shells.update({},&snapshot,registry,20,geometry,60,7);check(shells.size()==0,"Visible mortar shell disappears at Bomb plane");
 check(world.triangles.size()==originalTriangles.size(),"Consumer filtering preserves source triangle count");for(size_t i=0;i<world.triangles.size();++i)check(world.triangles[i].attribute==originalTriangles[i].attribute&&world.triangles[i].vertices==originalTriangles[i].vertices,"Original 64-bit attributes and topology stay immutable");
 std::cout<<"PASS "<<checks<<" purpose-specific bullet/projectile/footstep/IK/decal collision checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
