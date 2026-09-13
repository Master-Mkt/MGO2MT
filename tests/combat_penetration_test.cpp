#include "combat_ballistics.h"
#include "combat_initial_profile.h"
#include "combat_service.h"
#include "combat_audio.h"
#include "bullet_decals_overlay.h"
#include <iostream>
using namespace mgo2win;
using namespace mgo2win::combat;
namespace {
void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
std::shared_ptr<const stage::Collision> scene(int resistance,int layers=1,bool known=true,bool back=false,uint64_t attribute=4,uint32_t materialHash=1){
 std::vector<Vec3> vertices{{-200000,0,-200000},{200000,0,-200000},{200000,0,200000},{-200000,0,200000}};
 std::vector<stage::CollisionTriangle> triangles{{{0,1,2}},{{0,2,3}}};
 for(int i=0;i<layers;++i){unsigned n=unsigned(vertices.size());float z=1500.f+i*30;vertices.insert(vertices.end(),{{-1000,0,z},{1000,0,z},{1000,3104,z},{-1000,3104,z}});
  triangles.push_back({back?std::array<unsigned,3>{n,n+1,n+2}:std::array<unsigned,3>{n,n+2,n+1},attribute,0,0,0});
  triangles.push_back({back?std::array<unsigned,3>{n,n+2,n+3}:std::array<unsigned,3>{n,n+3,n+2},attribute,0,0,0});
 }
 return std::make_shared<const stage::Collision>(stage::Collision::make(vertices,triangles,{{materialHash,.5f,.5f,true,resistance,known}}));
}
constexpr Identity a{1,10,101},b{2,11,202};
Pose pose(float z){Pose p;p.feet={0,2,z};return p;}
void join(Authority& h){check(h.join(a,1,pose(0),1000,1000,std::array<uint16_t,1>{25},0)&&h.join(b,2,pose(5000),1000,1000,std::array<uint16_t,1>{25},0),"actors admitted outside layers");h.active(true);}
uint32_t damage(std::shared_ptr<const stage::Collision> world){Authority h;h.begin(1,world,initial_profiles(20,1,0));join(h);auto result=h.fire(a,{1,1,25,{0,0,1}},0);check(bool(result)&&h.snapshot().players[a.slot]->ammo==29,"accepted shot consumes once");return 1000-h.snapshot().players[b.slot]->hp;}
}
int main(int argc,char** argv){try{
 check(damage(scene(100,0))==275,"unobstructed near AK base HP");
 check(damage(scene(100))==247,"one prior front surface costs100force, base HP truncates to247");
 check(damage(scene(100,2))==220,"two surfaces resistance200 leave budget50 and HP220");
 {auto source=scene(150,2);auto vertices=source->vertices;for(size_t i=8;i<vertices.size();++i)vertices[i][2]=1500.005f;auto thin=std::make_shared<const stage::Collision>(stage::Collision::make(vertices,source->triangles,source->materials));check(damage(thin)==0,"distinct closely spaced layers cannot merge into a penetrable single face");}
 check(damage(scene(100,3))==0&&damage(scene(250))==0,"cumulative resistance and equality stop");
 check(damage(scene(0,1,false))==0,"missing material fails closed at1000");
 check(damage(scene(1000,1,true,true))==275,"backface free");
 check(damage(scene(1000,1,true,false,0x8000))==275,"low32 free surface flag");
 check(damage(scene(0,12))==0,"exhausted force cannot heal or use zero-force full-damage sentinel");
 check(damage(scene(0,70))==0,"native surface work bound cannot shoot past truncation");
 auto wall=scene(100);auto shared=trace_ak102({0,1552,0},{0,0,1},5000,*wall);check(shared.impacts.size()==1&&shared.priorForceCost==100,"shared diagonal counts one plane");
 // Use actual recovered stage material hashes and bank/WAV mapping at the
 // real authority-to-audio boundary, rather than dispatching a synthetic cue.
 {Authority host;host.begin(7,scene(100,1,true,false,4,0x15bccc),initial_profiles(20,1,0));join(host);auto hit=host.fire(a,{7,1,25,{0,0,1}},0);check(hit.events.size()==4&&hit.events[1].cue==8000&&hit.events[2].cue==8168,"host selects raw GEOM material cue8000 while body retains8168");if(argc>1){Effects audio;check(audio.load(argv[1]),"actual decoded PCM material bank loads");std::vector<Sound> played;audio.dispatch(hit.events,{0,1552,0},[&](const auto& s){played.push_back(s);});check(played.size()==3&&played[0].cue==10002&&played[1].cue==8000&&played[2].cue==8168,"actual shot/material/body files dispatched from host events");audio.dispatch(hit.events,{0,1552,0},[&](const auto& s){played.push_back(s);});check(played.size()==3,"material sound replay suppressed");check(!audio.play_cue(0,{},{},[](const auto&){}),"authored silence stays silent");}}
 // Actual Service output spans several original four-event frames and replicas
 // consume the complete ordered event stream exactly once.
 Service service(9);service.configure(scene(100,2),initial_profiles(20,1,0));check(service.admit(a)&&service.receive(a,wire::encode(wire::Accept{9}),0),"offered/accepted service");join(service.authority());service.deliveries();Replica replica;check(replica.snapshot(service.authority().snapshot()),"pre-shot event baseline");
 wire::Input input;input.epoch=9;input.sequence=1;input.pose=pose(0);input.weapon=25;input.firePressed=true;check(service.receive(a,wire::encode(input),0),"authenticated fire input");service.poll(0);
 size_t events=0,frames=0;for(const auto& delivery:service.deliveries())if(auto decoded=wire::decode(delivery.payload);std::holds_alternative<wire::Frame>(decoded)){const auto& f=std::get<wire::Frame>(decoded);check(f.events.size()<=4&&delivery.payload.size()<=2000,"unchanged wire limits");check(replica.snapshot(f.snapshot),"same final snapshot accepted across chunks");events+=replica.events(f.events).size();check(replica.events(f.events).empty(),"retransmission is silent");if(!f.events.empty())++frames;}
 check(events==5&&frames==2&&replica.state()->players[b.slot]->hp==780,"five shot/impact/damage events in two bounded frames");
 // Maximum supported identity count, many-layer simultaneous shots. This
 // exercises Service's global delivery bound, separate from peer wire windows.
 {auto source=scene(0,64);auto v=source->vertices;for(size_t i=4;i<v.size();++i)v[i][0]*=10;auto wide=std::make_shared<const stage::Collision>(stage::Collision::make(v,source->triangles,source->materials));Service burst(10);burst.configure(wide,initial_profiles(20,1,0));burst.authority().active(true);
  for(uint8_t slot=0;slot<24;++slot){Identity who{slot,uint16_t(100+slot),uint32_t(1000+slot)};check(burst.admit(who)&&burst.receive(who,wire::encode(wire::Accept{10}),0),"burst accepted peer");auto p=pose(0);p.feet[0]=(int(slot)-12)*600.f;check(burst.authority().join(who,1,p,1000,1000,std::array<uint16_t,1>{25},0),"24 separated shooters");wire::Input shot;shot.epoch=10;shot.sequence=1;shot.pose=p;shot.weapon=25;shot.firePressed=true;check(burst.receive(who,wire::encode(shot),0),"24 simultaneous native shots");}
  burst.deliveries();burst.poll(0);auto sent=burst.deliveries();check(sent.size()>9000&&sent.size()<16384,"global delivery bound supports24x24 penetration frames");size_t count=0;for(const auto& d:sent){check(d.payload.size()<=2000,"full24player frames keep2000byte ceiling");if(d.recipient.slot==0){auto frame=std::get<wire::Frame>(wire::decode(d.payload));count+=frame.events.size();}}check(count==24*65,"all65events per shot retained for one recipient");}
 // A mark requires a matching static surface and is occluded by later geometry.
 Event impact;impact.epoch=9;impact.id=1;impact.kind=EventKind::impact;impact.position={0,1552,1500};impact.normal={0,0,-1};
 auto mark=decals::static_impact(impact,{9,1},*wall);check(bool(mark),"static impact revalidated against GEOM");decals::Pool pool({1024,120000,10000,32,1});pool.synchronize({9,1},0,0);pool.synchronize({9,1},1,1);check(pool.emit(*mark,1),"new event creates mark");
 std::vector<uint32_t> image(1280*720);auto marks=pool.sample(2);decals::paint(image,marks,{0,1552,0},{0,0,1},*wall);check(std::count_if(image.begin(),image.end(),[](auto p){return p!=0;})>5,"native mark projects into scene");
 for(size_t n=0;n<image.size();++n)if(image[n])check(n%1280>=620&&n%1280<1236&&n/1280>=120&&n/1280<512,"mark confined to scene viewport");
 auto blocker=stage::Collision::make({{-10000,0,750},{10000,0,750},{10000,5000,750},{-10000,5000,750}},{{{0,1,2}},{{0,2,3}}});std::fill(image.begin(),image.end(),0);decals::paint(image,marks,{0,1552,0},{0,0,1},*wall,&blocker);check(std::none_of(image.begin(),image.end(),[](auto p){return p!=0;}),"closer geometry occludes marks");
 impact.target=b;check(!decals::static_impact(impact,{9,1},*wall),"body has no wall decal");impact.target={};check(!decals::static_impact(impact,{9,1},*scene(100,1,true,false,0x40048000)),"water has no bullet mark");
 std::cout<<"AK GEOM budget, force/HP, shared faces, 64-layer bound, service chunks and visible static decals PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
