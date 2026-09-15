#include "combat_initial_profile.h"
#include "combat_service.h"
#include "bullet_decals_overlay.h"
#include "material_effects_overlay.h"
#include <fstream>
#include <iostream>
using namespace mgo2win;
using namespace mgo2win::combat;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::shared_ptr<const stage::Collision> scene(uint32_t material,uint64_t attributes=4){
 std::vector<Vec3> vertices{{-200000,0,-200000},{200000,0,-200000},{200000,0,200000},{-200000,0,200000},
  {-1000,0,1500},{1000,0,1500},{1000,3104,1500},{-1000,3104,1500}};
 std::vector<stage::CollisionTriangle> triangles{{{0,1,2}},{{0,2,3}},{{4,6,5},attributes,0,0,0},{{4,7,6},attributes,0,0,0}};
 return std::make_shared<const stage::Collision>(stage::Collision::make(vertices,triangles,{{material,.5f,.5f,true,100,true}}));
}
constexpr Identity shooter{1,10,101},victim{2,11,202};
Pose pose(float z){Pose p;p.feet={0,2,z};return p;}
void save(const char* path,std::span<const uint32_t> pixels){
 std::ofstream out(path,std::ios::binary);out<<"P6\n1280 720\n255\n";
 for(auto p:pixels){char rgb[]{char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
 check(bool(out),"write effect render artifact");
}
}
int main(int argc,char**argv){try{
 std::vector<uint32_t> image(1280*720);
 for(uint8_t map:{1,4,20,21})for(uint32_t material:{0x48c4b8u,0x189cd4u,0xabcdefu}){
  auto world=scene(material);Service service(9);service.configure(world,initial_profiles(map,1,0));
  check(service.admit(shooter)&&service.receive(shooter,wire::encode(wire::Accept{9}),0),"authenticated peer");
  auto& host=service.authority();check(host.join(shooter,1,pose(0),1000,1000,std::array<uint16_t,1>{25},0)&&host.join(victim,2,pose(5000),1000,1000,std::array<uint16_t,1>{25},0),"room actors");host.active(true);service.deliveries();
  Replica replica;check(replica.snapshot(host.snapshot()),"pre-shot baseline");
  material_effects::Pool pool;decals::Scope scope{9,1};pool.synchronize(scope,replica.state()->eventWatermark,0);
  wire::Input input;input.epoch=9;input.sequence=1;input.pose=pose(0);input.weapon=25;input.firePressed=true;
  check(service.receive(shooter,wire::encode(input),1),"live shot request");service.poll(1);
  unsigned solids=0,bodies=0;std::optional<decals::Impact> mark;
  for(const auto& delivery:service.deliveries()){
   auto decoded=wire::decode(delivery.payload);if(!std::holds_alternative<wire::Frame>(decoded))continue;
   const auto& frame=std::get<wire::Frame>(decoded);check(replica.snapshot(frame.snapshot),"wire snapshot");
   pool.synchronize(scope,replica.state()->eventWatermark,1);
   for(const auto& event:replica.events(frame.events)){
    auto checked=decals::static_impact(event,scope,*world);
    if(checked){++solids;mark=checked;pool.emit(*checked,material_effects::verified_kind(map,checked->material),1);}
    else if(event.kind==EventKind::impact&&event.target.character)++bodies;
   }
   check(replica.events(frame.events).empty(),"network retransmission is not a second effect");
  }
  check(solids==1&&bodies==1&&mark.has_value(),"only GEOM surface, not actor impact");
  auto kind=material_effects::verified_kind(map,material);const bool enabled=kind!=material_effects::Kind::unknown;
  check(pool.size()==(enabled?8:0),"verified original material selects native emitter");
  check(!pool.emit(*mark,kind,2),"same authoritative event cannot respawn particles");
  auto lines=pool.lines(81);std::fill(image.begin(),image.end(),0);
  material_effects::paint(image,lines,{0,1552,0},{0,0,1},*world);
  size_t painted=std::count_if(image.begin(),image.end(),[](auto p){return p!=0;});
  check(enabled?painted>10:painted==0,"real event projects visible selected effect");
  for(size_t n=0;n<image.size();++n)if(image[n])check(n%1280>=620&&n%1280<1236&&n/1280>=120&&n/1280<512,"effect stays in gameplay viewport");
  if(argc>1&&map==20&&material==0x48c4b8)save(argv[1],image);
  auto blocker=stage::Collision::make({{-10000,0,750},{10000,0,750},{10000,5000,750},{-10000,5000,750}},{{{0,1,2}},{{0,2,3}}});
  std::fill(image.begin(),image.end(),0);material_effects::paint(image,lines,{0,1552,0},{0,0,1},*world,&blocker);
  check(std::none_of(image.begin(),image.end(),[](auto p){return p!=0;}),"foreground object blocks all sparks and fragments");
  auto waterEvent=Event{};waterEvent.epoch=9;waterEvent.id=1;waterEvent.kind=EventKind::impact;waterEvent.position=mark->position;waterEvent.normal=mark->normal;
  check(!decals::static_impact(waterEvent,scope,*scene(material,0x40048000)),"water is not a solid material emitter");
  check(pool.lines(551).empty(),"native particles expire without replay");
  pool.synchronize({10,2},replica.state()->eventWatermark,552);check(!pool.emit(*mark,kind,552),"room and scene replacement discard stale impacts");
 }
 // A narrow foreground post can hide the middle while both endpoints remain
 // visible. Endpoint-only visibility would paint over the post.
 {
  auto world=scene(0x48c4b8);
  auto post=stage::Collision::make({{-10,1500,750},{10,1500,750},{10,1600,750},{-10,1600,750}},{{{0,1,2}},{{0,2,3}}});
  std::array<material_effects::Line,1> trail{{{{-60,1552,1490},{60,1552,1490},{1,.78f,.3f,1},1,material_effects::Kind::metal}}};
  check(enemy_tag::visible({0,1552,0},trail[0].from,*world,&post)&&enemy_tag::visible({0,1552,0},trail[0].to,*world,&post),"both endpoints pass the narrow post");
  std::fill(image.begin(),image.end(),0);material_effects::paint(image,trail,{0,1552,0},{0,0,1},*world,&post);
  check(image[316*1280+928]==0,"trail center is hidden by the post");
  check(std::count_if(image.begin(),image.end(),[](auto p){return p!=0;})>10,"visible sides of trail remain");
 }
 // Different endpoint depths: the screen center lies at world parameter .2,
 // not .5. Linear world interpolation would test a ray beside the post while
 // painting its center. Exercise both endpoint orders independently.
 {
  auto world=stage::Collision::make({{-10000,0,-10000},{10000,0,-10000},{10000,0,10000},{-10000,0,10000}},{{{0,1,2}},{{0,2,3}}});
  auto post=stage::Collision::make({{-15,1500,500},{15,1500,500},{15,1600,500},{-15,1600,500}},{{{0,1,2}},{{0,2,3}}});
  const Vec3 eye{0,1552,0};
  std::array<material_effects::Line,1> trail{{{{-200,1552,1000},{800,1552,4000},{1,.78f,.3f,1},1,material_effects::Kind::metal}}};
  check(enemy_tag::visible(eye,trail[0].from,world,&post)&&enemy_tag::visible(eye,trail[0].to,world,&post),"unequal-depth endpoints pass post");
  check(!enemy_tag::visible(eye,{0,1552,1600},world,&post)&&enemy_tag::visible(eye,{300,1552,2500},world,&post),"perspective and linear midpoint rays differ");
  for(unsigned order=0;order<2;++order){
   std::fill(image.begin(),image.end(),0);material_effects::paint(image,trail,eye,{0,0,1},world,&post);
   check(image[316*1280+928]==0,"perspective-correct center is occluded at unequal depths");
   check(image[316*1280+888]!=0&&image[316*1280+968]!=0,"unequal-depth visible left and right remain");
   std::swap(trail[0].from,trail[0].to);
  }
 }
 std::cout<<"4stage host shot -> wire -> replica -> checked GEOM -> classified particle -> occluded pixels PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
