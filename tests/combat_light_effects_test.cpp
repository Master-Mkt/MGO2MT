#include "combat_light_effects.h"
#include <iostream>
#include <stdexcept>
#include <sstream>
using namespace mgo2win::combat;
static void check(bool ok,const char*s){if(!ok)throw std::runtime_error(s);}
int main(){try{
 LightEffects light;Snapshot state;state.epoch=7;state.eventWatermark=10;Player p;p.identity={0,2,101};p.life=3;state.players[0]=p;
 Event shot;shot.epoch=7;shot.id=10;shot.kind=EventKind::shot;shot.source=p.identity;shot.sourceLife=3;shot.weapon=25;shot.position={0,1500,0};
 light.synchronize(7,1,10,100);light.dispatch({&shot,1},state,100);check(light.sample(100).empty(),"initial history must not flash");
 shot.id=11;state.eventWatermark=11;light.synchronize(7,1,11,110);light.dispatch({&shot,1},state,110);auto a=light.sample(110);check(a.size()==1&&a[0].intensity==2&&a[0].position==shot.position,"accepted new shot lights once");
 light.dispatch({&shot,1},state,110);check(light.sample(110).size()==1,"duplicate suppressed");check(light.sample(150)[0].intensity==1,"flash fades by local arrival age");check(light.sample(190).empty(),"exact TTL removes flash");
 shot.id=12;state.eventWatermark=12;shot.kind=EventKind::impact;light.dispatch({&shot,1},state,200);check(light.sample(200).empty(),"impact is not an explosion");
 shot.kind=EventKind::shot;shot.id=13;state.eventWatermark=13;shot.sourceLife=2;light.dispatch({&shot,1},state,200);check(light.sample(200).empty(),"old life cannot flash");shot.sourceLife=3;
 shot.id=14;state.eventWatermark=14;shot.source.instance=99;light.dispatch({&shot,1},state,200);check(light.sample(200).empty(),"slot reuse cannot flash");shot.source=p.identity;
 shot.id=15;state.eventWatermark=15;shot.position[0]=NAN;light.dispatch({&shot,1},state,200);check(light.sample(200).empty(),"invalid origin");shot.position[0]=0;
 shot.id=16;state.eventWatermark=16;shot.weapon=0;light.dispatch({&shot,1},state,200);check(light.sample(200).empty(),"unreviewed weapon has no generic flash");shot.weapon=25;
 for(unsigned i=17;i<40;++i){shot.id=i;state.eventWatermark=i;light.dispatch({&shot,1},state,220);}
 check(light.sample(220).size()==mgo2win::maximum_dynamic_lights,"bounded pool");
 light.synchronize(7,2,39,230);check(light.sample(230).empty(),"scene change clears pulses");
 check(light.explosion(7,1,{0,0,0},240),"explicit local explosion effect");check(!light.explosion(7,1,{0,0,0},240),"explosion duplicate");check(!light.explosion(6,2,{0,0,0},240),"explosion wrong epoch");
 check(light.sample(240).size()==1&&light.sample(240)[0].radius==6500,"explosion radius");check(light.sample(415)[0].intensity==1.5f,"explosion fade");check(light.sample(590).empty(),"explosion end");
 light.explosion(7,2,{0,0,0},600);check(light.sample(599).empty(),"clock reversal clears stale flash");
 light.synchronize(8,1,0,650);shot.epoch=7;shot.id=40;state.eventWatermark=40;light.dispatch({&shot,1},state,650);check(light.sample(650).empty(),"old round rejects events");light.clear();check(!light.explosion(8,1,{0,0,0},660),"closed room rejects local effects");
 // An actual Authority decision, rather than an input button, produces a flash.
 std::istringstream emptyText("MGO2WIN.STAGE_COLLISION 1 0 0\n");auto world=std::make_shared<const mgo2win::stage::Collision>(mgo2win::stage::Collision::read(emptyText));
 Authority authority;Weapon ak;ak.id=25;ak.damage=275;ak.intervalMs=100;ak.range=10000;ak.magazine=1;ak.reloadMs=100;ak.automatic=true;
 authority.begin(9,world,{&ak,1});Identity id{0,1,123};Pose pose;uint16_t inventory=25;
 check(authority.join(id,1,pose,1000,1000,{&inventory,1},1000),"authority fixture join");authority.active(true);
 light.synchronize(9,1,0,1000);auto decision=authority.fire(id,{9,1,25,{0,0,1},1},1000);check(bool(decision)&&decision.events.size()==1,"accepted shot emits one event");
 auto accepted=authority.snapshot();light.dispatch(decision.events,accepted,1000);check(light.sample(1000).size()==1,"accepted authoritative fire lights");light.sample(1080);
 auto emptyShot=authority.fire(id,{9,2,25,{0,0,1},1},1200);check(emptyShot.reject==Reject::no_ammo&&emptyShot.events.empty(),"empty magazine rejected");light.dispatch(emptyShot.events,authority.snapshot(),1200);check(light.sample(1200).empty(),"rejected empty shot never lights");
 std::cout<<"combat light event identity, history, lifetime, pool, explicit explosion, authority fire/reject and context passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
