#include "combat_particle_effects.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <limits>
using namespace mgo2win::combat;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 particles::Pool pool({4,1600,900});Snapshot state;state.epoch=7;state.eventWatermark=10;Player p;p.identity={0,2,101};p.life=3;p.alive=true;state.players[0]=p;
 Event e;e.epoch=7;e.id=10;e.kind=EventKind::shot;e.source=p.identity;e.sourceLife=3;e.weapon=3;e.position={0,1500,0};e.normal={0,0,1};
 pool.synchronize(7,1,10,0);pool.dispatch({&e,1},state,0);check(pool.size()==0,"join history baseline");
 state.eventWatermark=14;e.id=12;pool.dispatch({&e,1},state,10);e.id=11;pool.dispatch({&e,1},state,10);pool.dispatch({&e,1},state,10);check(pool.size()==2,"delayed out-of-order chunk accepted once");
 auto first=pool.sample(state,10);check(first.size()==2&&first[0].kind==particles::Kind::casing&&first[0].rgba[0]>first[0].rgba[2],"native brass casing");auto later=pool.sample(state,300);check(later.size()==2&&later[0].from!=first[0].from&&later[0].rgba[3]<first[0].rgba[3],"ballistic motion and fade");
 e.id=13;e.weapon=50;pool.dispatch({&e,1},state,300);e.id=14;e.weapon=53;pool.dispatch({&e,1},state,300);check(pool.size()==2,"no RPG or grenade casings");
 e.id=15;e.weapon=50;e.kind=EventKind::projectileTrail;state.eventWatermark=15;pool.dispatch({&e,1},state,300);auto smoke=pool.sample(state,300);check(smoke.size()==3&&smoke.back().kind==particles::Kind::smoke&&smoke.back().rgba[0]==smoke.back().rgba[1],"RPG trail has grey smoke");
 check(pool.sample(state,910).size()==1,"casing exact lifetime");check(pool.sample(state,1900).empty(),"smoke exact lifetime");
 e.kind=EventKind::shot;e.weapon=2;e.id=16;state.eventWatermark=16;pool.dispatch({&e,1},state,2000);check(pool.size()==1,"MK2 accepted shot casing");state.players[0]->life=4;check(pool.sample(state,2001).empty(),"respawn discards old life visuals");e.id=17;state.eventWatermark=17;pool.dispatch({&e,1},state,2001);check(pool.size()==0,"old life event rejected");e.sourceLife=4;
 e.id=18;state.eventWatermark=18;e.position[0]=std::numeric_limits<float>::quiet_NaN();pool.dispatch({&e,1},state,2010);check(pool.size()==0,"malformed position rejected");e.position[0]=0;
 e.id=19;state.eventWatermark=19;e.source.instance=3;pool.dispatch({&e,1},state,2010);check(pool.size()==0,"full identity checked");e.source=p.identity;
 for(uint64_t id=20;id<35;++id){e.id=id;state.eventWatermark=id;pool.dispatch({&e,1},state,2020);}check(pool.size()==4,"bounded pool evicts oldest");check(pool.sample(state,2019).empty(),"clock rollback clears active particles");pool.dispatch({&e,1},state,2021);check(pool.size()==0,"rollback cannot replay old event");
 e.id=35;state.eventWatermark=35;e.weapon=3;pool.dispatch({&e,1},state,2030);check(pool.size()==1,"OPERATOR accepted event only");state.players[0]->alive=false;check(pool.sample(state,2031).empty(),"dead owner clears");state.players[0]->alive=true;
 pool.synchronize(7,2,35,2040);pool.dispatch({&e,1},state,2040);check(pool.size()==0,"scene resets historical baseline");pool.clear();pool.dispatch({&e,1},state,2050);check(pool.size()==0,"leave unavailable");
 particles::Pool ak;state.players[0]->alive=true;ak.synchronize(7,1,35,2100);e.id=36;e.weapon=25;e.sourceLife=4;state.eventWatermark=36;ak.dispatch({&e,1},state,2100);auto flash=ak.sample(state,2100);check(ak.size()==3&&std::count_if(flash.begin(),flash.end(),[](const auto&v){return v.kind==particles::Kind::flash;})==3,"AK accepted shot emits muzzle flash smoke and casing");ak.dispatch({&e,1},state,2101);check(ak.size()==3,"AK repeated event cannot emit again");check(ak.sample(state,2180).size()==2,"AK muzzle flash expires at 80ms");
 bool invalid=false;try{particles::Pool bad({4097,1,1});}catch(...){invalid=true;}check(invalid,"invalid policy refused");std::cout<<"native casing/smoke accepted event, full identity, late chunks, expiry, reset, capacity PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
