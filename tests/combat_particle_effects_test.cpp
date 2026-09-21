#include "combat_particle_effects.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <limits>
using namespace mgo2mt::combat;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 Snapshot s;s.epoch=7;s.eventWatermark=10;Player p;p.identity={0,2,101};p.life=3;p.alive=true;s.players[0]=p;
 Event e;e.epoch=7;e.id=10;e.kind=EventKind::shot;e.source=p.identity;e.sourceLife=3;e.weapon=3;e.position={0,1500,0};e.normal={0,0,1};
 particles::Pool pool;pool.synchronize(7,1,10,0);pool.dispatch({&e,1},s,0);check(pool.size()==0,"join historical baseline");
 e.id=12;s.eventWatermark=12;pool.dispatch({&e,1},s,10);e.id=11;pool.dispatch({&e,1},s,10);pool.dispatch({&e,1},s,10);check(pool.size()==6,"late chunk emitted once");check(pool.sprites(s,10).size()==4,"OP flash and smoke original sprites");
 check(pool.sprites(s,90).size()==2,"flash exact 80ms expiry");check(pool.sample(s,910).size()==2,"casing exact 900ms expiry");check(pool.sprites(s,1610).empty(),"smoke exact expiry");
 for(uint16_t weapon:std::array<uint16_t,23>{2,3,4,7,8,15,18,20,23,24,25,26,30,31,35,37,38,39,41,42,43,44,50}){
  pool.clear();pool.synchronize(7,1,e.id,2000);e.id++;s.eventWatermark=e.id;e.weapon=weapon;pool.dispatch({&e,1},s,2000);auto sprites=pool.sprites(s,2000);check(sprites.size()==2&&sprites[0].texture==0x090aec&&sprites[1].texture==0xca92b7,"every firearm receives original texture keys");auto lines=pool.sample(s,2000);check(std::count_if(lines.begin(),lines.end(),[](const auto&v){return v.kind==particles::Kind::casing;})==(weapon==50?0:1),"firearm casing eligibility");
 }
 pool.clear();pool.synchronize(7,1,e.id,3000);e.id++;s.eventWatermark=e.id;e.weapon=52;pool.dispatch({&e,1},s,3000);check(pool.size()==0,"throw cannot emit firearm flash");
 e.id++;s.eventWatermark=e.id;e.kind=EventKind::explosion;s.players[0]->alive=false;pool.dispatch({&e,1},s,3000);check(pool.sprites(s,3000).size()==2,"accepted explosion survives owner death");pool.dispatch({&e,1},s,3000);check(pool.size()==1,"explosion replay refused");check(pool.sprites(s,3600).size()==1,"explosion flame ends, original smoke remains");check(pool.sprites(s,5400).empty(),"explosion exact expiry");
 for(uint16_t weapon=56;weapon<=59;++weapon){e.id++;s.eventWatermark=e.id;e.kind=EventKind::smoke;e.weapon=weapon;pool.dispatch({&e,1},s,6000);}auto cloud=pool.sprites(s,7000);check(cloud.size()==16,"four original smoke billboards per grenade");check(cloud[4].rgba[0]>cloud[4].rgba[1]&&cloud[8].rgba[1]>cloud[8].rgba[0],"native red green tint on original pixels");check(pool.sprites(s,18000).empty(),"12s smoke expiry");
 s.players[0]->alive=true;e.kind=EventKind::shot;e.weapon=25;e.id++;s.eventWatermark=e.id;pool.dispatch({&e,1},s,19000);s.players[0]->life=4;check(pool.sprites(s,19001).empty(),"firearm old life cleared");e.id++;s.eventWatermark=e.id;pool.dispatch({&e,1},s,19001);check(pool.size()==0,"old life event refused");e.sourceLife=4;e.position[0]=std::numeric_limits<float>::quiet_NaN();e.id++;s.eventWatermark=e.id;pool.dispatch({&e,1},s,19001);check(pool.size()==0,"nonfinite position refused");e.position[0]=0;e.id++;s.eventWatermark=e.id;pool.dispatch({&e,1},s,19002);check(pool.sprites(s,19001).empty(),"clock rollback clears active");pool.dispatch({&e,1},s,19003);check(pool.size()==0,"rollback cannot replay");
 particles::Pool bounded({4,1600,900});bounded.synchronize(7,1,e.id,20000);for(unsigned i=0;i<20;++i){e.id++;s.eventWatermark=e.id;bounded.dispatch({&e,1},s,20000);}check(bounded.size()==4,"bounded eviction");bounded.synchronize(7,2,e.id,20001);check(bounded.sprites(s,20001).empty(),"scene clears all effects");
 // Caller supplies a posed CNP frame without changing the authoritative event
 // or the independently placed muzzle flash. Verify direction and late replay.
 pool.clear();pool.synchronize(7,3,e.id,21000);e.id++;s.eventWatermark=e.id;unsigned calls=0;
 auto emission=[&](const Event&)->std::optional<particles::CasingEmission>{++calls;return particles::CasingEmission{{200,1700,300},{-2,0,0}};};
 pool.dispatch({&e,1},s,21000,emission);auto emitted=pool.sample(s,21000);auto case0=std::find_if(emitted.begin(),emitted.end(),[](const auto& x){return x.kind==particles::Kind::casing;});
 check(case0!=emitted.end()&&(case0->from[0]+case0->to[0])*.5f==200&&(case0->from[1]+case0->to[1])*.5f==1700,"CNP ejection position independent of muzzle");
 check(pool.sprites(s,21000)[0].position==e.position,"CNP casing leaves muzzle flash unchanged");
 auto moved=pool.sample(s,21100);auto case1=std::find_if(moved.begin(),moved.end(),[](const auto& x){return x.kind==particles::Kind::casing;});check((case1->from[0]+case1->to[0])*.5f<90&&(case1->from[2]+case1->to[2])*.5f==300,"CNP direction drives casing velocity");
 pool.dispatch({&e,1},s,21100,emission);check(calls==1,"Replay never invokes emitter twice");
 std::cout<<"Original texture sprites all firearms, smoke/explosion, source life, replay, clock and limits PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
