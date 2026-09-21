#include "combat_service.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
using namespace mgo2mt::combat;
namespace {
unsigned checks=0;
void check(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
constexpr Identity operatorId{0,1,100},targetId{1,2,200};
Pose standing(float z,float x=0){Pose p;p.feet={x,2,z};return p;}
std::shared_ptr<const stage::Collision> floor_world(bool wall=false){
 std::vector<Vec3> v{{-100000,0,-100000},{100000,0,-100000},{100000,0,100000},{-100000,0,100000}};
 std::vector<stage::CollisionTriangle> t{{{0,2,1},stage::attribute::native_solid},{{0,3,2},stage::attribute::native_solid}};
 if(wall){v.insert(v.end(),{{-10000,0,1000},{10000,0,1000},{10000,10000,1000},{-10000,10000,1000}});t.push_back({{4,5,6},stage::attribute::native_solid});t.push_back({{4,6,7},stage::attribute::native_solid});}
 return std::make_shared<const stage::Collision>(stage::Collision::make(std::move(v),std::move(t)));
}
std::vector<Weapon> profiles(){
 Weapon carried;carried.id=25;carried.damage=100;carried.intervalMs=100;carried.reloadMs=1000;carried.magazine=10;carried.reserve=20;carried.range=50000;
 auto mortar=carried;mortar.id=103;mortar.damage=1500;mortar.magazine=2;mortar.reserve=0;mortar.nativeProjectile=true;mortar.mountedOnly=true;mortar.intervalMs=2000;
 return {carried,mortar};
}
mounted::Registry registry(mounted::Kind kind,bool infinite=true){
 mounted::Registry r;mounted::Type t;t.id="launcher";t.name="Launcher";t.model="data/launcher.gwm";t.kind=kind;t.weapon=kind==mounted::Kind::catapult?0:103;
 t.operatorOffset={0,kind==mounted::Kind::catapult?1000.f:0.f,-600};t.pivot={0,76.5f,0};t.muzzle={0,1071,4.2f};
 t.bindPitch=1.57079632679f;t.launchDirection={0,1,0};t.initialPitch=.7f;t.pitchMin=.3f;t.pitchMax=1.2f;t.yawMin=-.5f;t.yawMax=.5f;
 t.launchSpeed=5000;t.gravity=9800;t.maxFlightMs=6000;t.cooldownMs=2000;t.blastRadius=3000;t.infiniteAmmo=infinite;
 r.types.push_back(t);r.placements.push_back({20,1,t.id,{0,2,0},0});return r;
}
void start(Authority& h,mounted::Kind kind,bool wall=false,bool target=false,bool infinite=true){
 h.begin(1,floor_world(wall),profiles());check(h.configure_mounted(registry(kind,infinite),20),"launcher registry");const uint16_t gear[]{25};
 check(h.join(operatorId,1,standing(-600),10000,10000,gear,0),"operator join");
 if(target)check(h.join(targetId,2,standing(2500),10000,10000,gear,0),"target join");h.active(true);
}
void mount(Authority& h,uint64_t now=0,uint32_t sequence=1,uint32_t request=1){
 auto pose=h.snapshot().players[0]->pose;check(h.pose(operatorId,1,sequence,pose,now)==Reject::none,"operator heartbeat");
 check(h.mount(operatorId,1,sequence,{mounted::Action::mount,1,request},now)==Reject::none,"board launcher");
}
Vec3 aim(const Player&p){return {std::sin(p.pose.yaw)*std::cos(p.pose.pitch),std::sin(p.pose.pitch),std::cos(p.pose.yaw)*std::cos(p.pose.pitch)};}
Decision fire(Authority&h,uint64_t now=0,uint32_t sequence=1){auto p=*h.snapshot().players[0];return h.fire(operatorId,{1,sequence,p.weapon,aim(p),p.life},now);}
void mortar(){
 Authority h;start(h,mounted::Kind::mortar,false,true,false);mount(h);auto p=*h.snapshot().players[0];check(p.weapon==103&&p.pose.pitch==.7f,"mortar original ID and initial pitch");
 const auto inventory=h.item_held(operatorId,1)->slots;auto accepted=fire(h);check(bool(accepted)&&accepted.events.size()==2&&accepted.events[0].kind==EventKind::shot&&accepted.events[1].kind==EventKind::projectile,"mortar launches HOST projectile");
 check(accepted.events[0].object==1&&accepted.events[1].object==0,"mortar shot identifies launcher without changing projectile object semantics");
 check(h.debug_projectile_count()==1&&h.snapshot().players[0]->ammo==1,"finite mortar ammunition");
 const auto origin=accepted.events[0].position;h.advance_projectiles(100);auto flight=h.debug_flights();check(flight.size()==1&&flight[0].position[1]>origin[1]&&flight[0].position[2]>origin[2],"mortar ballistic ascent follows recovered up-axis");
 bool exploded=false,damaged=false;for(uint64_t now=200;now<=2000;now+=100){auto result=h.advance_projectiles(now);for(const auto&e:result.events){exploded|=e.kind==EventKind::explosion&&e.weapon==103;damaged|=e.kind==EventKind::damage&&e.target==targetId&&e.weapon==103;}}
 check(exploded&&damaged&&h.snapshot().players[1]->hp==8500,"mortar collision blast uses configured HP1500");check(h.debug_projectile_count()==0,"mortar removed on impact");
 check(h.pose(operatorId,1,2,h.snapshot().players[0]->pose,1999)==Reject::none,"mortar cooldown heartbeat");check(fire(h,1999,2).reject==Reject::interval,"mortar rejects before cooldown");
 check(h.pose(operatorId,1,3,h.snapshot().players[0]->pose,2000)==Reject::none&&bool(fire(h,2000,3)),"mortar accepts exact cooldown");
 check(!h.snapshot().players[0]->ammo,"mortar second round consumed");h.release_mounted(operatorId);check(h.item_held(operatorId,1)->slots==inventory,"mortar preserves carried inventory");
 h.leave(operatorId);h.advance_projectiles(2100);check(h.debug_projectile_count()==0,"disconnect cancels mortar flight");
 Authority immutable;start(immutable,mounted::Kind::mortar,false,false);mount(immutable);auto detachedShot=fire(immutable);check(bool(detachedShot),"infinite mortar shot");check(immutable.snapshot().players[0]->ammo==2,"infinite mortar retains ammo");immutable.release_mounted(operatorId);wire::Frame detached{immutable.snapshot(),wire::Status::active,detachedShot.events};check(!detached.snapshot.players[0]->mountedId&&std::get<wire::Frame>(wire::decode(wire::encode(detached)))==detached&&detached.events[0].object==1,"mortar launcher identity survives dismount and wire roundtrip");immutable.active(false);immutable.advance_projectiles(100);check(!immutable.debug_projectile_count(),"round stop cancels mortar projectile");
}
void catapult(){
 Authority exit;start(exit,mounted::Kind::catapult);mount(exit);check(exit.release_mounted(operatorId)&&exit.snapshot().players[0]->pose.feet==standing(-600).feet,"elevated seat dismount returns safe ground");
 Authority h;start(h,mounted::Kind::catapult);check(h.equip(operatorId,1,0,0)==Reject::none,"empty hands before catapult");const auto inventory=h.item_held(operatorId,1)->slots;mount(h);
 auto p=*h.snapshot().players[0];check(p.mountedId==1&&!p.weapon&&p.pose.feet[1]==1002&&valid_mounted(p),"elevated catapult seat with no weapon");auto launched=fire(h);
 check(bool(launched)&&launched.events.size()==1&&launched.events[0].kind==EventKind::catapultLaunch&&launched.events[0].weapon==0,"catapult dedicated event");
 p=*h.snapshot().players[0];check(p.flightId==1&&!p.mountedId&&!p.weapon&&valid_flight(p),"catapult flight identity");
 check(h.equip(operatorId,1,25,0)==Reject::unavailable&&h.reload(operatorId,1,0).reject==Reject::unavailable&&fire(h,0,2).reject==Reject::unavailable,"flight gates fire equip reload");
 check(h.mount(operatorId,1,1,{mounted::Action::dismount,0,2},0)==Reject::unavailable,"cannot dismount mid-flight");
 const auto before=p.pose;auto fake=standing(90000,90000);fake.yaw=2;fake.pitch=-1;check(h.pose(operatorId,1,2,fake,100)==Reject::none&&h.snapshot().players[0]->pose==before,"flight ignores client teleport and camera input");
 check(h.pose(operatorId,1,2,fake,100)==Reject::sequence&&h.pose(operatorId,1,3,fake,100,2)==Reject::generation,"flight replay/life rejected");
 h.advance(100);p=*h.snapshot().players[0];check(p.flightId&&p.pose.feet[1]>before.feet[1]&&p.pose.feet[2]>before.feet[2],"HOST advances player trajectory");
 for(uint64_t now=200;now<=1800;now+=100)h.advance(now);p=*h.snapshot().players[0];check(!p.flightId&&!p.flightElapsedMs&&p.pose.feet[1]>=1.9f&&p.pose.feet[1]<3,"HOST landing returns ground control");check(h.item_held(operatorId,1)->slots==inventory,"catapult never consumes carried inventory");
 Authority wall;start(wall,mounted::Kind::catapult,true);mount(wall);check(bool(fire(wall)),"wall catapult launch");for(uint64_t now=100;now<=1800;now+=100)wall.advance(now);p=*wall.snapshot().players[0];check(!p.flightId&&p.pose.feet[2]<740&&p.pose.feet[1]<3,"catapult capsule stops at wall then falls to floor");
 Authority peer;start(peer,mounted::Kind::catapult);const uint16_t peerGear[]{25};check(peer.join(targetId,2,standing(1000),10000,10000,peerGear,0),"peer on flight route");mount(peer);check(bool(fire(peer)),"peer route launch");for(uint64_t now=100;now<=1800;now+=100)peer.advance(now);p=*peer.snapshot().players[0];check(p.pose.feet[2]<480||std::abs(p.pose.feet[0])>520,"fast launch cannot cross a peer body");
 Authority timeout;start(timeout,mounted::Kind::catapult);mount(timeout);check(bool(fire(timeout)),"gap flight");timeout.advance(2001);p=*timeout.snapshot().players[0];check(!p.flightId&&p.pose.feet==standing(-600).feet,"large clock gap recovers safe boarding ground");
 Authority stop;start(stop,mounted::Kind::catapult);mount(stop);check(bool(fire(stop)),"round flight");stop.advance(100);stop.active(false);check(!stop.snapshot().players[0]->flightId&&stop.snapshot().players[0]->pose.feet==standing(-600).feet,"round stop releases managed flight");
 Authority stun;start(stun,mounted::Kind::catapult,false,true);mount(stun);check(bool(fire(stun)),"stun flight");burning::Blast blast{{{1,targetId.slot,targetId.instance,targetId.character,1},52,0,2},1,{0,1700,-600},3000,0,false,10000};stun.explode(blast,0);check(stun.snapshot().players[0]->stunned&&stun.snapshot().players[0]->flightId&&stun.snapshot().players[0]->blastFlight,"grenade stun replaces catapult flight with HOST blast motion");
}
void wire_budget(){
 Authority h;start(h,mounted::Kind::catapult);mount(h);auto launch=fire(h);h.advance(250);auto s=h.snapshot();for(unsigned i=0;i<24;++i){auto p=*s.players[0];p.identity={uint8_t(i),uint16_t(i+1),i+1};p.flightId=uint16_t(i+1);s.players[i]=p;}
 Event shot;shot.epoch=1;shot.id=s.eventWatermark+1;shot.kind=EventKind::shot;shot.source=s.players[0]->identity;shot.weapon=25;s.eventWatermark=shot.id;
 auto sop=h.sop_view(operatorId).value();sop.recipient=s.players[0]->identity;wire::Frame frame{s,wire::Status::active,{shot},sop};auto bytes=wire::encode(frame);
 std::cout<<"24 flights + full SOP + shot bytes="<<bytes.size()<<'\n';check(bytes.size()<=2000,"24 simultaneous flights remain within datagram budget");check(std::get<wire::Frame>(wire::decode(bytes))==frame,"flight snapshot wire roundtrip");Replica replica;check(replica.snapshot(s),"flight snapshot replica");auto bad=s;bad.players[0]->mountedId=1;check(!replica.snapshot(bad),"flight and mount cannot coexist");bad=s;bad.players[0]->flightElapsedMs=1;check(!replica.snapshot(bad),"noncanonical flight elapsed rejected");
 frame.events=launch.events;frame.events[0].source=s.players[0]->identity;frame.snapshot.eventWatermark=frame.events[0].id;bytes=wire::encode(frame);check(std::get<wire::Frame>(wire::decode(bytes))==frame,"weapon-zero catapult event roundtrip");frame.events[0].hpDamage=1;bool rejected=false;try{wire::encode(frame);}catch(const wire::Invalid&){rejected=true;}check(rejected,"launch event cannot carry arbitrary damage");
}
void service(){
 Service s(1);s.configure(floor_world(),profiles());check(s.authority().configure_mounted(registry(mounted::Kind::catapult),20),"service catapult");const uint16_t gear[]{25};check(s.authority().join(operatorId,1,standing(-600),10000,10000,gear,0),"service operator");s.authority().active(true);check(s.authority().equip(operatorId,1,0,0)==Reject::none,"service empty hands");s.admit(operatorId);check(s.receive(operatorId,wire::encode(wire::Accept{1}),0),"service accept");wire::Input input;input.epoch=1;input.sequence=1;input.pose=standing(-600);input.mounted={mounted::Action::mount,1,1};check(s.receive(operatorId,wire::encode(input),0),"service boarding input");s.poll(0);check(s.authority().snapshot().players[0]->mountedId==1,"service boarding");input.sequence=2;input.mounted={};input.pose=s.authority().snapshot().players[0]->pose;input.fire=true;input.firePressed=true;check(s.receive(operatorId,wire::encode(input),1),"service launch input without weapon");s.poll(1);check(s.authority().snapshot().players[0]->flightId==1,"service empty hands launches");s.deliveries();s.poll(51);bool snapshot=false;for(const auto&delivery:s.deliveries())if(auto record=wire::decode(delivery.payload);std::holds_alternative<wire::Frame>(record))snapshot|=std::get<wire::Frame>(record).snapshot.players[0]->flightId==1;check(snapshot,"flight sends first corrected position within 50ms");
}
}
int main(){try{mortar();catapult();wire_budget();service();std::cout<<"PASS "<<checks<<" mortar/catapult HOST checks\n";return 0;}catch(const std::exception&e){std::cerr<<"after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
