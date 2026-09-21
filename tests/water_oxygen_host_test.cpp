#include "combat_initial_profile.h"
#include "combat_wire.h"
#include "combat_service.h"
#include "water_oxygen.h"
#include <iostream>
using namespace mgo2mt;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(){try{
 using namespace water_gameplay;
 OxygenPolicy policy;Oxygen slow,fast;slow.reset(policy);fast.reset(policy);
 uint32_t damageSlow=0,damageFast=0;
 for(int t=0;t<70000;t+=1000)damageSlow+=slow.advance(policy,1000,true,1000);
 for(int t=0;t<70000;t+=10)damageFast+=fast.advance(policy,10,true,1000);
 check(slow.amount(policy)==0&&damageSlow==500&&damageSlow==damageFast,"host clock independent drain and continuous damage");
 for(int t=0;t<15000;t+=100)check(slow.advance(policy,100,false,1000)==0,"surfacing stops HP damage");
 check(slow.amount(policy)==10000,"surface refill");
 auto unchanged=slow.credit;check(slow.advance(policy,1001,true,1000)==0&&slow.credit==unchanged,"unbounded elapsed rejected");
 auto world=std::make_shared<const stage::Collision>(stage::Collision::make({{-100000,0,-100000},{100000,0,-100000},{100000,0,100000},{-100000,0,100000}},{{{0,1,2}},{{0,2,3}}}));
 auto water=std::make_shared<const stage::Water>(stage::Water::make({{{0,0,0},{100000,1200,100000}}}));
 auto weapons=combat::initial_profiles(21,0,0);combat::Identity id{0,1,100};combat::Pose stand;stand.feet={0,2,0};
 combat::Authority host;host.begin(1,world,weapons);check(host.water(water),"install water");
 check(host.oxygen_policy({1000,1000,500}),"bounded host native oxygen tuning before active");
 check(host.join(id,0,stand,1000,1000,std::array<uint16_t,1>{25},0),"join standing");
 host.active(true);host.advance(0);host.advance(1000);
 check(host.snapshot().players[0]->oxygen==10000&&!host.snapshot().players[0]->faceSubmerged,"wet legs, dry face do not drain");
 auto crouch=stand;crouch.capsule.height=1100;
 check(host.pose(id,1,1,crouch,1000)==combat::Reject::none,"crouch submerges proxy face");
 host.advance(1500);check(host.snapshot().players[0]->oxygen==5000,"half oxygen snapshot");
 auto snapshot=host.snapshot();auto encoded=combat::wire::encode(combat::wire::Frame{snapshot,combat::wire::Status::active,{},{}});
 check(std::get<combat::wire::Frame>(combat::wire::decode(encoded)).snapshot==snapshot,"oxygen and submerged flag round trip");
 auto invalid=encoded;invalid[5]=9;check(!combat::wire::recognized(invalid),"old wire rejected");
 invalid=encoded;invalid[invalid.size()-4]|=0x40;bool rejected=false;try{combat::wire::decode(invalid);}catch(const combat::wire::Invalid&){rejected=true;}check(rejected,"reserved oxygen flag rejected");
 snapshot.players[0]->oxygen=10001;combat::Replica replica;check(!replica.snapshot(snapshot),"direct invalid oxygen snapshot rejected");
 host.advance(1400);check(host.snapshot().players[0]->oxygen==5000,"backward host time ignored");
 host.advance(2000);check(host.snapshot().players[0]->hp==1000&&host.snapshot().players[0]->oxygen==0,"no damage before oxygen reaches zero");
 host.advance(2500);check(host.snapshot().players[0]->hp==750,"suffocation continuous HP damage");
 auto before=host.snapshot();host.advance(2500);check(host.snapshot()==before,"duplicate clock no double damage");
 check(host.pose(id,1,2,stand,2500)==combat::Reject::none,"stand back into air");
 host.advance(3000);check(host.snapshot().players[0]->hp==750&&host.snapshot().players[0]->oxygen==5000,"surfacing stops damage and recovers");
 check(host.pose(id,1,3,crouch,3000)==combat::Reject::none,"submerge again");
 host.advance(4000);host.advance(5000);
 check(!host.snapshot().players[0]->alive&&host.snapshot().players[0]->hp==0,"drowning kills");
 check(host.scores()[0]->deaths==1&&host.scores()[0]->kills==0&&host.snapshot().eventWatermark==0,"one environmental death, no kill or fake weapon event");
 host.advance(6000);check(host.scores()[0]->deaths==1,"dead body no repeated score");
 check(host.respawn(id,2,[&]{return host.join(id,0,stand,1000,1000,std::array<uint16_t,1>{25},6000);}),"validated respawn");
 check(host.snapshot().players[0]->oxygen==10000&&host.snapshot().players[0]->life==2,"new life full oxygen");
 host.active(false);host.advance(6100);host.advance(6500);check(host.snapshot().players[0]->hp==1000,"inactive round no drain");
 // No new input at the lethal tick: environment must resolve before automatic fire.
 combat::Policy cp;cp.stalePoseMs=5000;combat::Service service(7,cp);service.configure(world,weapons);
 auto& authority=service.authority();authority.water(water);authority.oxygen_policy({1000,1000,1000});
 check(authority.join(id,0,crouch,1000,1000,std::array<uint16_t,1>{25},0),"automatic drowning fixture");
 authority.active(true);check(service.admit(id,0)&&service.receive(id,combat::wire::encode(combat::wire::Accept{7}),0),"automatic peer");
 combat::wire::Input held{7,1,crouch,25,true,false,true};check(service.receive(id,combat::wire::encode(held),0),"automatic hold");
 service.poll(0);service.deliveries();service.poll(1000);service.deliveries();
 auto shotsBefore=authority.snapshot().eventWatermark;service.poll(2000);
 check(!authority.snapshot().players[0]->alive&&authority.snapshot().eventWatermark==shotsBefore,"lethal water tick cannot fire without new input");
 // Maximum 24-player snapshot plus one event must fit the same native datagram.
 auto full=host.snapshot();for(unsigned i=0;i<24;++i){auto p=*full.players[0];p.identity={uint8_t(i),uint16_t(i+1),100+i};p.evadeKind=combat::EvadeKind::roll;p.evadeSerial=1;p.evadeElapsedMs=1;p.pose.feet[0]=float(i)*3000;full.players[i]=p;}
 full.eventWatermark=1;combat::Event event;event.epoch=1;event.id=1;event.source=full.players[0]->identity;event.sourceLife=2;event.weapon=25;
 auto frame=combat::wire::encode(combat::wire::Frame{full,combat::wire::Status::active,{event},{}});
 check(frame.size()<=2000,"24 evading players + oxygen + event datagram bound");
 std::cout<<"Oxygen fixed-point clock, HOST death/respawn, snapshot and wire bounds PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

