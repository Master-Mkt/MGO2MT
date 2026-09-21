#include "gameplay_config.h"
#include "combat_presentation.h"
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace mgo2mt;using namespace mgo2mt::combat;
namespace {
unsigned checks=0;
void check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
const Identity shooter{0,1,100},victim{1,1,101};
auto world(){return std::make_shared<const stage::Collision>(stage::Collision::make({{-200000,-200000,100000},{200000,-200000,100000},{200000,200000,100000},{-200000,200000,100000}},{{{0,1,2},stage::attribute::native_solid},{{0,2,3},stage::attribute::native_solid}}));}
Weapon profile(){Weapon w;w.id=25;w.damage=101;w.staminaDamage=55;w.intervalMs=10;w.reloadMs=400;w.reloadRefillMs=200;w.magazine=3;w.reserve=20;w.range=200000;w.tuning.weightKg=10;w.tuning.moveSpeedScale=1.2f;w.tuning.reloadSpeedScale=2;w.tuning.chamberCapacity=1;w.tuning.accuracy=weapon_accuracy::Policy{10000,40000,5000,10000,2,.5f,.25f,8};return w;}
void begin(Authority&h,Weapon w){h.begin(1,world(),std::array{w});check(h.join(shooter,1,{},10000,1000,std::array<uint16_t,1>{25},0),"shooter admitted");h.active(true);}
Decision fire(Authority&h,uint32_t sequence,uint64_t now){const auto p=h.snapshot().players[0]->pose;check(h.pose(shooter,1,sequence,p,now)==Reject::none,"accepted fresh HOST pose");return h.fire(shooter,{1,sequence,25,{0,0,1}},now);}
void chamber_reload(){
 const auto w=profile();Authority h;begin(h,w);check(weapon_move_scale(w)==1&&weapon_reload_scale(w)==2,"native weight and reload helper values");
 check(h.snapshot().players[0]->ammo==3&&weapon_chamber_rounds(w,3)==1&&weapon_magazine_rounds(w,3)==2,"spawn preserves three supplied cartridges; automatic chamber feed");
 check(bool(h.reload(shooter,1,0))&&h.snapshot().players[0]->reloadUntil==200,"tactical reload actual deadline scaled");
 const auto timing=weapon_reload_timing(w,*h.snapshot().players[0]);check(timing&&timing->endMs==200&&timing->refillMs==100,"shared presentation resolver matches HOST deadlines");presentation::Reload visual;auto visualPlayer=*h.snapshot().players[0];visual.update(1,&visualPlayer,0,3,3.5,{},timing->endMs);
 h.advance(99);check(h.snapshot().players[0]->ammo==3,"refill not early");h.advance(100);check(h.snapshot().players[0]->ammo==4&&h.snapshot().players[0]->reserve==19,"tactical refill preserves chamber; transfers exactly one");
 visualPlayer=*h.snapshot().players[0];visual.update(1,&visualPlayer,100,3,3.5,{},timing->endMs);check(std::abs(visual.seconds()-1.75)<.000001,"animation halfway at actual scaled HOST duration midpoint");
 check(h.fire(shooter,{1,1,25,{0,0,1}},100).reject==Reject::reloading,"refill cannot bypass animation deadline");
 h.advance(200);for(unsigned n=0;n<4;++n)check(bool(fire(h,n+2,200+n*10)),"four ready cartridges consumed");
 check(h.snapshot().players[0]->ammo==0&&fire(h,6,240).reject==Reject::no_ammo,"empty chamber blocks firing");
 check(bool(h.reload(shooter,1,241))&&h.snapshot().players[0]->reloadUntil==441,"empty reload deadline scaled");h.advance(341);
 check(h.snapshot().players[0]->ammo==3&&h.snapshot().players[0]->reserve==16,"empty refill uses only magazine capacity, chamber feeds from it");
 h.advance(441);check(bool(h.reload(shooter,1,442)),"subsequent tactical topup starts");h.advance(542);check(h.snapshot().players[0]->ammo==4&&h.snapshot().players[0]->reserve==15,"topup conserves reserve");
 h.advance(642);check(h.pose(shooter,1,7,{},642)==Reject::none,"fresh pose before topped-up drop");auto held=h.item_held(shooter,123);check(held&&held->slots[0].contents.magazine==4,"inventory holds ready magazine plus chamber");items::wire::Command command;command.header={{1,1},123,{shooter.slot,shooter.instance,shooter.character,1},1};command.action=items::wire::Action::drop;command.heldSlot=0;command.heldRevision=held->slots[0].revision;auto dropped=h.item_action(shooter,command,642);check(dropped&&dropped.entity&&dropped.entity->contents.magazine==4,"dropped chamber round retained");held=h.item_held(shooter,123);command.header.sequence=2;command.action=items::wire::Action::pickup;command.heldRevision=held->slots[0].revision;command.entity=dropped.entity->key.id;command.entityRevision=dropped.entity->revision;check(bool(h.item_action(shooter,command,643))&&h.snapshot().players[0]->ammo==4&&h.snapshot().players[0]->reserve==15,"pickup permits and preserves topped-up chamber without duplication");
 auto off=w;off.tuning={};Authority legacy;begin(legacy,off);check(bool(fire(legacy,1,0))&&bool(legacy.reload(shooter,1,1))&&legacy.snapshot().players[0]->reloadUntil==401,"omitted tuning retains original timing");legacy.advance(201);check(legacy.snapshot().players[0]->ammo==3,"zero chamber legacy refill unchanged");
}
void cone_and_damage(){
 auto w=profile();Authority h;begin(h,w);check(h.accuracy(shooter,0)==10,"configured baseline cone");Pose moving;moving.feet[0]=100;check(h.pose(shooter,1,1,moving,100)==Reject::none&&h.accuracy(shooter,100)==20,"HOST movement expands reticle cone");
 moving.capsule.height=1100;check(h.pose(shooter,1,2,moving,101)==Reject::none&&h.sop_view(shooter)->spreadMilliRadians==10,"crouch and movement multiply immediately in replicated footer");h.advance(252);check(h.accuracy(shooter,252)==5,"idle crouch smaller");
 moving.capsule.height=560;check(h.pose(shooter,1,3,moving,253)==Reject::none&&h.accuracy(shooter,253)==3,"prone smallest; advertised cone rounds outward");
 Authority spread,repeat;begin(spread,w);begin(repeat,w);auto a=fire(spread,1,0),b=fire(repeat,1,0);check(a&&b&&a.events==b.events,"pellet rays deterministic for HOST scope");
 unsigned impacts=0,shots=0;std::set<Vec3> points;for(const auto&e:a.events){if(e.kind==EventKind::shot)++shots;if(e.kind==EventKind::impact){++impacts;points.insert(e.position);const double radius=std::hypot(double(e.position[0]),double(e.position[1]-1550));check(radius<=std::tan(.01)*100000+.1,"every pellet stays inside pre-shot reticle cone");}}
 check(impacts==8&&points.size()==8&&shots==1&&spread.snapshot().players[0]->ammo==2,"eight independent impacts, one audio/flash event and one cartridge");check(spread.accuracy(shooter,0)==15,"one trigger advances bloom once");
 Authority hit;begin(hit,w);Pose target;target.feet={0,0,1000};check(hit.join(victim,2,target,10000,1000,std::array<uint16_t,1>{25},0),"target admitted");auto d=fire(hit,1,0);unsigned hp=0,stamina=0,damageEvents=0;for(const auto&e:d.events)if(e.kind==EventKind::damage){hp+=e.hpDamage;stamina+=e.staminaDamage;++damageEvents;}check(d&&damageEvents==8&&hp==101&&stamina==55&&hit.snapshot().players[1]->hp==9899,"all pellets share exact configured HP/ST budget including remainder");
 w.damage=10000;Authority kill;begin(kill,w);check(kill.join(victim,2,target,500,1000,std::array<uint16_t,1>{25},0),"fragile target admitted");auto death=fire(kill,1,0);unsigned deaths=0;for(const auto&e:death.events)deaths+=e.kind==EventKind::death;check(death&&deaths==1&&kill.scores()[0]&&kill.scores()[0]->kills==1,"multiple pellets cannot duplicate death or kill credit");
 w=profile();w.tuning.weightKg=50;w.tuning.moveSpeedScale=1;Authority heavy;begin(heavy,w);Pose fast;fast.feet[0]=400;check(heavy.pose(shooter,1,1,fast,100)==Reject::too_fast,"HOST enforces weight-adjusted movement ceiling");fast.feet[0]=299;check(heavy.pose(shooter,1,2,fast,100)==Reject::none,"legitimate weighted movement accepted");
}
std::string edited(std::string text,uint16_t id,const std::string& tuning){const auto list=text.find("\"weapons\":");auto at=text.find("\"id\": "+std::to_string(id)+",",list);check(at!=text.npos,"fixture weapon ID");at=text.find("\"parameters\": {",at);check(at!=text.npos,"fixture parameters");at+=15;text.insert(at,"\"tuning\":"+tuning+",");return text;}
void parser(const std::filesystem::path&path){std::ifstream file(path,std::ios::binary);std::string text{std::istreambuf_iterator<char>(file),{}};gameplay::Config c;std::string error;check(c.load_text(text,error),error.c_str());check(c.find(25)&&!c.find(25)->weapon.tuning.accuracy,"legacy JSON default unchanged");
 const auto okay=edited(text,25,R"({"weightKg":10,"moveSpeedScale":1.2,"reloadSpeedScale":2,"chamberCapacity":1,"accuracy":{"base":10000,"maximum":40000,"perShot":5000,"recovery":10000,"movingMultiplier":2,"crouchMultiplier":0.5,"proneMultiplier":0.25,"pellets":8}})");check(c.load_text(okay,error),error.c_str());check(c.find(25)->weapon.tuning.accuracy->pellets==8&&c.find(25)->weapon.tuning.chamberCapacity==1,"full tuning roundtrip");
 for(const char*invalid:{R"({"weightKg":-1})",R"({"weightKg":101})",R"({"moveSpeedScale":0})",R"({"reloadSpeedScale":5.1})",R"({"chamberCapacity":9})",R"({"chamberCapacity":0.5})",R"({"accuracy":{"maximum":2}})",R"({"accuracy":{"pellets":33}})",R"({"accuracy":{"movingMultiplier":0.9}})",R"({"accuracy":{"crouchMultiplier":1.1}})",R"({"accuracy":{"proneMultiplier":0}})",R"({"accuracy":{"recovery":0}})",R"({"acuracy":{}})"}){check(!c.load_text(edited(text,25,invalid),error),"invalid tuning rejected");check(c.find(25)->weapon.tuning.accuracy->pellets==8,"failed tuning load atomic");}
 check(!c.load_text(edited(text,52,R"({"accuracy":{}})"),error),"grenade cannot secretly use ray pellet mode");check(!c.load_text(edited(text,52,R"({"chamberCapacity":1})"),error),"grenade has no chamber");
 check(c.load_text(edited(text,25,R"({"accuracy":{},"weightKg":0})"),error)&&c.find(25)->weapon.tuning.accuracy->base==3000,"partial accuracy fields use documented defaults");
 auto withPath=text;const auto at=withPath.find("\"resources\": {")+14;withPath.insert(at,"\"weaponEffectsManifest\":\"fx/weapon_effects.json\",");check(c.load_text(withPath,error)&&c.resources().weaponEffectsManifest=="fx/weapon_effects.json","optional editor manifest safe path");withPath.replace(withPath.find("fx/weapon_effects.json"),22,"../escape.json");check(!c.load_text(withPath,error),"unsafe editor manifest path rejected");
}
}
int main(int argc,char**argv){try{chamber_reload();cone_and_damage();if(argc==2)parser(argv[1]);else check(false,"gameplay.json fixture required");std::cout<<"Weapon tuning PASS: "<<checks<<" checks, trusted stance/movement cone, independent pellets and total budget, chamber conservation, reload timing, weight, strict/atomic JSON\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
