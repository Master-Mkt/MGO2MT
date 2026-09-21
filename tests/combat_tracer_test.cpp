#include "combat_tracer.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt::combat;
void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
int main(){try{
 tracers::Pool pool;Snapshot s;s.epoch=1;s.eventWatermark=10;Player p;p.identity={0,2,100};p.life=3;p.alive=true;s.players[0]=p;auto remote=p;remote.identity={1,4,101};s.players[1]=remote;
 Event e;e.id=10;e.epoch=1;e.source=p.identity;e.sourceLife=3;e.weapon=25;e.position={0,0,0};e.normal={0,0,1};e.shotDistance=10000;
 pool.synchronize(1,2,p.identity,3,10,0);pool.dispatch({&e,1},s,0);check(pool.size()==0,"join baseline silent");
 e.id=12;s.eventWatermark=12;pool.dispatch({&e,1},s,10);e.id=11;e.source=remote.identity;pool.dispatch({&e,1},s,10);pool.dispatch({&e,1},s,10);check(pool.size()==2,"self and remote delayed chunks once");
 auto first=pool.sample(s,10);check(first.size()==2&&first[0].from[2]==0&&first[0].to[2]==1200,"accepted direction first frame streak");auto half=pool.sample(s,60);check(half[0].from[2]==4400&&half[0].to[2]==5600&&half[0].opacity==.5f,"travels within authoritative endpoint");check(pool.sample(s,110).empty(),"exact TTL");
 e.source=p.identity;for(auto weapon:{50,53}){e.weapon=uint16_t(weapon);e.id=++s.eventWatermark;pool.dispatch({&e,1},s,120);}check(pool.size()==0,"RPG and WP excluded");
 e.weapon=25;for(float d:{0.f,-1.f,1000001.f,std::numeric_limits<float>::quiet_NaN()}){e.shotDistance=d;e.id=++s.eventWatermark;pool.dispatch({&e,1},s,120);}check(pool.size()==0,"legacy zero and malformed distance excluded");
 e.shotDistance=100;e.source.instance++;e.id=++s.eventWatermark;pool.dispatch({&e,1},s,120);check(pool.size()==0,"full identity");e.source=p.identity;e.sourceLife++;e.id=++s.eventWatermark;pool.dispatch({&e,1},s,120);check(pool.size()==0,"old life");e.sourceLife=3;
 e.normal={0,0,2};e.id=++s.eventWatermark;pool.dispatch({&e,1},s,120);check(pool.size()==0,"nonunit direction");e.normal={0,0,1};e.id=++s.eventWatermark;pool.dispatch({&e,1},s,120);check(pool.sample(s,120)[0].to[2]==100,"short ray exact endpoint");
 s.players[1]->life++;e.source=remote.identity;e.id=++s.eventWatermark;pool.dispatch({&e,1},s,120);check(pool.size()==1,"remote respawn rejects previous life");
 s.players[0]->life++;check(pool.sample(s,121).empty(),"self life clears all remote trails");p.life=4;s.players[0]=p;pool.synchronize(1,2,p.identity,4,s.eventWatermark,130);e.source=p.identity;e.sourceLife=4;
 for(int i=0;i<300;++i){e.id=++s.eventWatermark;pool.dispatch({&e,1},s,130);}check(pool.size()==tracers::Pool::capacity,"bounded capacity");check(pool.sample(s,129).empty(),"backward clock clears");pool.dispatch({&e,1},s,131);check(pool.size()==0,"clock rollback cannot replay");
 e.id=++s.eventWatermark;pool.dispatch({&e,1},s,132);check(pool.size()==1,"post rollback new event");pool.synchronize(1,3,p.identity,4,s.eventWatermark,133);check(pool.size()==0,"scene change clears");pool.dispatch({&e,1},s,133);check(pool.size()==0,"scene baseline rejects history");pool.synchronize(0,0,{},0,0,134);check(pool.size()==0,"pause leave clear");
 std::cout<<"HOST tracer distance/direction, owner/scene/life/replay/TTL/clock/capacity PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
