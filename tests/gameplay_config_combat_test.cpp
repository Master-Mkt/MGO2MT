#include "gameplay_config.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;using namespace mgo2mt::combat;
namespace {
void check(bool value,const std::string& why){if(!value)throw std::runtime_error(why);}
std::string read(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
std::string changed(std::string text,uint16_t weapon,const std::string&field,const std::string&value){auto start=text.find("\"weapons\":");start=text.find("\"id\": "+std::to_string(weapon)+",",start);check(start!=text.npos,"fixture weapon");auto at=text.find("\""+field+"\": ",start);check(at!=text.npos,"fixture field");at+=field.size()+4;auto end=text.find_first_of(",\r\n",at);text.replace(at,end-at,value);return text;}
auto floor(){return std::make_shared<const stage::Collision>(stage::Collision::make({{-200000,0,-200000},{200000,0,-200000},{200000,0,200000},{-200000,0,200000}},{{{0,2,1},stage::attribute::native_solid},{{0,3,2},stage::attribute::native_solid}}));}
const Identity source{0,1,100},target{1,1,101};
auto begin(const std::string&text,uint16_t weapon,float distance=1200,float pitch=0){gameplay::Config config;std::string error;check(config.load_text(text,error),error);auto h=std::make_unique<Authority>();h->begin(1,floor(),config.profiles(20));Pose from;from.feet={0,2,0};from.pitch=pitch;Pose to;to.feet={0,2,distance};check(h->join(source,1,from,10000,1000,std::array{weapon},0)&&h->join(target,2,to,10000,1000,std::array<uint16_t,1>{25},0),"fixture joins");h->active(true);return h;}
void projectile_damage(const std::string&text){
 auto h=begin(changed(text,50,"damage","222"),50,10000);check(bool(h->fire(source,{1,1,50,{0,0,1}},0)),"configured RPG fire");h->advance_projectiles(400);check(h->snapshot().players[1]->hp==9778,"RPG configured damage reaches actual blast");
 for(uint16_t id:{52,54}){auto edit=changed(text,id,id==52?"damage":"staminaDamage","123");auto g=begin(edit,id,1200,-1);check(bool(g->fire(source,{1,1,id,{0,-std::sin(1.f),std::cos(1.f)}},0)),"configured grenade throw");for(uint64_t now=100;now<=3500;now+=100)g->advance_projectiles(now);auto p=*g->snapshot().players[1];check((id==52?p.hp==9877:p.stamina==877),"grenade configured HP/ST reaches actual fuse blast");}
 auto shortFlight=begin(changed(text,50,"range","1000"),50,10000);check(bool(shortFlight->fire(source,{1,1,50,{0,0,1}},0)),"short range rocket launched");shortFlight->advance_projectiles(400);check(shortFlight->snapshot().players[1]->hp==10000&&shortFlight->debug_projectile_count()==0,"configured rocket range limits physical flight");
}
void trap_damage(const std::string&text){for(uint16_t id:{64,65,66,67}){const bool hp=id==64||id==66;auto h=begin(changed(text,id,hp?"damage":"staminaDamage","123"),id);check(bool(h->fire(source,{1,1,id,{0,0,1}},0)),"configured charge deployed");if(id==66||id==67){check(h->pose(source,1,1,{{0,2,0}},1001)==Reject::none,"detonator fresh pose");check(bool(h->reload(source,1,1001)),"configured charge detonated");}else h->advance_projectiles(1001);auto p=*h->snapshot().players[1];check(hp?p.hp==9877:p.stamina==877,"trap configured HP/ST applied");}}
void gekko_damage(const std::string&text){for(uint16_t id:{130,131}){auto edited=changed(changed(text,id,"damage","123"),id,"staminaDamage","321");auto h=begin(edited,25,1800);check(h->assign_special(source,special_pc::Kind::gekko,true,0)==Reject::none&&h->equip(source,1,id,0)==Reject::none,"configured Gekko setup");check(bool(h->fire(source,{1,1,id,{0,0,1}},0)),"configured Gekko attack");h->advance_special_pc(900);auto p=*h->snapshot().players[1];check(p.hp==9877&&p.stamina==679,"Gekko configured HP/ST applied");}
 auto h=begin(changed(text,130,"range","500"),25,1800);check(h->assign_special(source,special_pc::Kind::gekko,true,0)==Reject::none&&h->equip(source,1,130,0)==Reject::none,"short Gekko setup");check(bool(h->fire(source,{1,1,130,{0,0,1}},0)),"short Gekko attack");h->advance_special_pc(900);check(h->snapshot().players[1]->hp==10000,"Gekko configured reach honored");
}
void reload_damage(const std::string&text){
 auto edited=changed(text,3,"intervals","200");edited=changed(edited,3,"refillTick","800");auto h=begin(edited,3,5000);check(bool(h->fire(source,{1,1,3,{0,0,1}},0)),"pistol shot before custom reload");check(bool(h->reload(source,1,1)),"custom reload begins");const auto timing=original::reload_timing({5,200,800,1});check(timing&&h->snapshot().players[0]->reloadUntil==1+timing->endMs,"configured reload duration survives original posture lookup");h->advance(timing->refillMs);check(h->snapshot().players[0]->ammo==6,"custom refill not early");h->advance(timing->refillMs+1);check(h->snapshot().players[0]->ammo==7,"custom refill event honored");
}
void mounted_m2(const std::string& text){
 gameplay::Config config;std::string error;
 check(config.load_text(text,error),error);
 const auto* definition=config.find(104);
 check(definition&&definition->weapon.mountedOnly&&definition->weapon.damage==550&&definition->weapon.intervalMs==100&&definition->weapon.magazine==1,"original M2 seed profile");
 Authority host;host.begin(1,floor(),config.profiles(20));
 mounted::Registry registry;mounted::Type type;
 type.id="m2";type.name="M2 BROWNING";type.weapon=104;type.model="mounted/m2_browning.gwm";
 type.pivot={0,1129,0};type.muzzle={0,1200,1000};type.operatorOffset={0,0,-875};type.infiniteAmmo=true;
 registry.types.push_back(type);registry.placements.push_back({20,1,"m2",{0,2,0},0});
 check(host.configure_mounted(registry,20),"configured ID104 mounted scene");
 Pose from;from.feet={0,2,-875};Pose to;to.feet={0,2,10000};
 const std::array<uint16_t,1> carried{25};
 check(host.join(source,1,from,10000,1000,carried,0)&&host.join(target,2,to,10000,1000,carried,0),"M2 fixture players");
 host.active(true);
 check(host.pose(source,1,1,from,0)==Reject::none&&host.mount(source,1,1,{mounted::Action::mount,1,1},0)==Reject::none,"ID104 mount accepted");
 const auto first=host.fire(source,{1,1,104,{0,0,1}},0);
 check(bool(first)&&host.snapshot().players[target.slot]->hp==9450,"configured original M2 torso hit applies 550 HP");
 check(host.snapshot().players[source.slot]->ammo==1,"M2 infinite proxy magazine retained");
 check(!first.events.empty()&&first.events.front().weapon==104&&first.events.front().position==Vec3{0,1202,1000},"M2 event retains original identity and HOST muzzle");
 check(host.fire(source,{1,2,104,{0,0,1}},99).reject==Reject::interval,"M2 99ms shot rejected");
 check(bool(host.fire(source,{1,3,104,{0,0,1}},100)),"M2 100ms shot accepted");
 check(host.snapshot().players[target.slot]->hp==8900&&host.snapshot().players[source.slot]->ammo==1,"second M2 hit and infinite magazine");
}
}
int main(int argc,char**argv){try{check(argc==2,"gameplay.json required");auto text=read(argv[1]);projectile_damage(text);trap_damage(text);gekko_damage(text);reload_damage(text);mounted_m2(text);std::cout<<"JSON actual HOST: RPG/grenade/placed HP and ST, projectile range, Gekko HP/ST/range, reload deadlines, original M2 damage/cadence/infinite ammo PASS\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
