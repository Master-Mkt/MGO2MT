#include "combat_service.h"
#include "gekko_test_profiles.h"
#include "combat_health_rules.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace mgo2win;using namespace combat;
namespace {
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
Identity id(unsigned n){return {uint8_t(n),uint16_t(n+1),uint32_t(n+100)};}
Weapon gun(){return {25,999,0,100,300,10,10,50000,9001,9002,true};}
std::shared_ptr<const stage::Collision> terrain(float height=0){
 std::vector<Vec3> v{{-30000,0,-30000},{30000,0,-30000},{30000,0,30000},{-30000,0,30000}};std::vector<stage::CollisionTriangle> t{{{0,2,1}},{{0,3,2}}};
 if(height){v.insert(v.end(),{{-2000,height,-2000},{2000,height,-2000},{2000,height,2000},{-2000,height,2000}});t.push_back({{4,6,5}});t.push_back({{4,7,6}});}return std::make_shared<const stage::Collision>(stage::Collision::make(v,t));
}
void fall(float height,bool lethal){
 Authority h;h.begin(1,terrain(height),gekko_test_profiles(gun()));Pose p;p.feet={0,height+2,0};check(h.join(id(0),1,p,1000,1000,std::array<uint16_t,1>{25},0),"valid upper-platform spawn");h.active(true);
 uint64_t now=0;uint32_t seq=0;std::vector<Event> events;
 auto move=[&]{now+=200;h.advance_falling(now);check(h.pose(id(0),1,++seq,p,now)==Reject::none,"HOST accepts physically bounded fall pose");auto d=h.advance_falling(now);events.insert(events.end(),d.events.begin(),d.events.end());};
 for(int x=1000;x<=3000;x+=1000){p.feet[0]=float(x);move();}
 for(float y=height+2;y>2;){y=(std::max)(2.f,y-1000);p.feet[1]=y;move();}
 const auto q=*h.snapshot().players[0];check(q.hp==(lethal?0u:100u)&&q.alive==!lethal,"7m leaves100;10m kills at accepted landing");
 check(events.size()==(lethal?2u:1u),"one damage and optional death only");for(const auto& e:events)check(e.source==id(0)&&e.target==id(0)&&e.sourceLife==1&&e.targetLife==1&&e.weapon==fall_event_weapon,"suicide full identity event");
 check(h.scores()[0]->kills==0&&h.scores()[0]->deaths==(lethal?1u:0u),"fall never awards a kill");check(h.advance_falling(now+100).events.empty(),"no repeated fall damage");
 if(lethal){check(h.respawn(id(0),2,[&]{Pose floor;floor.feet={5000,2,0};return h.join(id(0),1,floor,1000,1000,std::array<uint16_t,1>{25},now+200);}),"validated life2 respawn");check(h.advance_falling(now+201).events.empty()&&h.snapshot().players[0]->hp==1000,"newlife has no old fall debt");}
 wire::Frame frame{h.snapshot(),wire::Status::active,events,h.sop_view(id(0)).value()};check(std::get<wire::Frame>(wire::decode(wire::encode(frame)))==frame,"self damage/death wire roundtrip");
}
void regeneration(){
 Authority h;h.begin(2,terrain(),gekko_test_profiles(gun()));Pose a,b;b.feet={0,2,5000};a.feet={0,2,0};check(h.join(id(0),1,a,1000,1000,std::array<uint16_t,1>{25},0)&&h.join(id(1),2,b,1000,1000,std::array<uint16_t,1>{25},0),"two players");h.active(true);
 check(h.assign_special(id(1),special_pc::Kind::gekko,true,0)==Reject::none,"gekko assignment");check(h.snapshot().players[1]->maxHp==1000,"gekko maximum1000");
 check(bool(h.fire(id(0),{2,1,25,{0,0,1}},0))&&h.snapshot().players[1]->hp==1,"999 damage remains1");h.environment(15000);check(h.snapshot().players[1]->hp==501,"halfperiod restores500");h.advance(30000);check(h.snapshot().players[1]->hp==1000,"30seconds full");
 check(h.assign_special(id(1),special_pc::Kind::human,true,30000)==Reject::none,"return to human");h.explode({{{2,0,1,100,1},50,0,1},1,b.feet,1000,100,false},30000);auto hp=h.snapshot().players[1]->hp;check(hp==900,"human damaged");h.advance(60000);check(h.snapshot().players[1]->hp==hp,"human not regenerated");
 check(h.assign_special(id(1),special_pc::Kind::gekko,true,60000)==Reject::none,"return gekko keeps ratio");check(h.snapshot().players[1]->hp==900,"no transform freeheal");h.explode({{{2,0,1,100,1},50,0,1},2,b.feet,1000,1000,false},60000);check(!h.snapshot().players[1]->alive,"lethal explosion");h.advance(90000);check(h.snapshot().players[1]->hp==0,"regeneration never revives");
}
void rules(){
 const std::string good="MGO2WIN_HEALTH 1\nfall_safe_height 3000\nfall_severe_height 7000\nfall_fatal_height 10000\nfall_severe_damage_permille 900\ngekko_full_recovery_ms 30000\n";
 std::istringstream in(good);auto p=HealthRules::read(in);check(p.valid()&&p.regeneration.fullRecoveryMs==30000,"valid explicit host settings");
 for(const auto& bad:{good+"gekko_full_recovery_ms 1\n",good+"unknown 1\n",std::string("MGO2WIN_HEALTH 1\n"),std::string(4097,'x')}){bool rejected=false;try{std::istringstream s(bad);HealthRules::read(s);}catch(...){rejected=true;}check(rejected,"invalid config rejected");}
 p.regeneration.fullRecoveryMs=30001;check(!p.valid(),"cannot exceed30second setting");
}
}
int main(){try{rules();fall(7000,false);fall(10000,true);regeneration();std::cout<<"HOST health rules PASS: validated fall/self-death and Gekko regeneration\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
