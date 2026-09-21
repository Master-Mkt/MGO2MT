#include "combat_service.h"
#include "remote_avatar.h"
#include "cover_hit_geometry.h"
#include "combat_initial_profile.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;using namespace mgo2mt::combat;
namespace {
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
Identity id(unsigned n){return {uint8_t(n),uint16_t(n+1),100+n};}
std::shared_ptr<stage::Collision> scene(){return std::make_shared<stage::Collision>(stage::Collision::make({{-100000,0,-100000},{-100000,0,100000},{100000,0,100000},{100000,0,-100000},{-2000,0,0},{0,0,0},{0,2500,0},{-2000,2500,0}},{{{0,1,2},stage::attribute::native_solid},{{0,2,3},stage::attribute::native_solid},{{4,5,6},stage::attribute::native_solid},{{4,6,7},stage::attribute::native_solid}}));}
Weapon gun(){Weapon w;w.id=23;w.damage=100;w.intervalMs=100;w.reloadMs=500;w.magazine=30;w.reserve=60;w.range=20000;w.automatic=true;return w;}
Pose pose(float x=-100,float z=-300,float yaw=0){return {{x,0,z},yaw,0,{260,1700,2}};}
void authority(){
 Authority h;h.begin(8,scene(),std::array{gun()});check(h.join(id(0),1,pose(),1000,1000,std::array<uint16_t,1>{23},0),"join");h.active(true);
 check(h.pose(id(0),8,1,pose(),100)==Reject::none,"pose");cover::Intent attach{1,cover::Action::attach};
 check(h.cover(id(0),8,1,attach,100)==Reject::none&&h.snapshot().players[0]->cover.attached&&h.sop_view(id(0))->coverRequest==1,"attach accepted and ACK");
 check(h.fire(id(0),{8,1,23,{0,0,1}},100).reject==Reject::obstructed&&h.snapshot().players[0]->ammo==30,"covered shot rejected without consuming ammo");
 check(h.pose(id(0),8,2,pose(0,-200),150)==Reject::too_fast,"normal escape rejected");
 check(h.pose(id(0),8,2,pose(),150)==Reject::none,"stationary pose");attach.lean=-1;check(h.cover(id(0),8,2,attach,150)==Reject::none&&h.snapshot().players[0]->cover.lean==-1,"repeated edge updates held lean without reattach");
 auto shot=h.fire(id(0),{8,2,23,{0,0,1}},150);check(bool(shot)&&shot.events.front().position[0]>300,"same HOST ray begins at admitted popout");
 check(h.join(id(1),2,pose(320,4000,3.14159265f),1000,1000,std::array<uint16_t,1>{23},150),"opponent");check(h.pose(id(1),8,1,pose(320,4000,3.14159265f),160)==Reject::none,"opponent pose");
 auto incoming=h.fire(id(1),{8,1,23,{0,0,-1}},160);check(bool(incoming)&&h.snapshot().players[0]->hp==900,"exposed upper body can be hit around finite wall end");
 check(h.pose(id(0),8,3,pose(),200)==Reject::none&&h.cover(id(0),8,3,{2,cover::Action::detach},200)==Reject::none&&!h.snapshot().players[0]->cover.attached,"explicit detach");
 check(h.pose(id(0),8,4,pose(),250)==Reject::none&&h.cover(id(0),8,4,{1,cover::Action::attach},250)==Reject::sequence,"old attach cannot return after detach");
 check(h.cover(id(0),9,4,{3,cover::Action::attach},250)==Reject::generation&&h.cover(id(0),8,4,{3,cover::Action::attach},250,2)==Reject::generation,"epoch/life mismatch");
 check(h.pose(id(0),8,5,pose(),300)==Reject::none&&h.cover(id(0),8,5,{3,cover::Action::attach},300)==Reject::none,"fresh attach");h.active(false);check(!h.snapshot().players[0]->cover.attached&&h.sop_view(id(0))->coverRequest==3,"inactive clears action but preserves edge ACK");
}
void service(){
 Service s(8,Policy{false,6000,15000,500,true});s.configure(scene(),std::array{gun()});host::Roster roster;roster.complete=true;
 for(unsigned n=0;n<24;++n){auto p=n?pose(float(n*2000),-2000):pose();check(s.authority().join(id(n),0,p,1000,1000,std::array<uint16_t,1>{23},0)&&s.admit(id(n))&&s.receive(id(n),wire::encode(wire::Accept{8}),0),"24 native peers");host::Player r{uint8_t(n),id(n).instance,id(n).character,"Fixture",""};r.appearance=std::array<uint8_t,28>{};roster.slots[n]=r;}
 s.authority().active(true);s.deliveries();wire::Input input{8,1,pose(),23};input.cover={1,cover::Action::attach,-1,false};check(s.receive(id(0),wire::encode(input),100),"wire attach");s.poll(100);s.deliveries();
 input.sequence=2;input.cover={0,cover::Action::none,-1,false};input.firePressed=true;check(s.receive(id(0),wire::encode(input),150),"wire popout fire");s.poll(150);auto deliveries=s.deliveries();std::array<unsigned,24> shots{};
 for(auto& d:deliveries){check(d.payload.size()<=2000,"packet cap");auto r=wire::decode(d.payload);if(auto*f=std::get_if<wire::Frame>(&r)){check(f->snapshot.players[0]->cover.lean==-1,"remote cover survives codec");for(auto&e:f->events)if(e.kind==EventKind::shot)++shots[d.recipient.slot];}}
 for(auto count:shots)check(count==1,"all peers receive shot exactly once");remote::Scene remote;check(remote.update(s.authority().snapshot(),roster,id(1),150),"remote scope");check(remote.sample(150).front().cover.attached&&remote.sample(150).front().cover.lean==-1,"remote avatar authorized cover exposure");
 auto turned=s.authority().snapshot();++turned.revision;turned.players[0]->pose.yaw=1;turned.players[0]->pose.feet[0]-=10;check(remote.update(turned,roster,id(1),250),"remote camera differs from wall body");auto avatar=remote.sample(250).front();check(std::abs(avatar.yaw)<.0001f&&avatar.coverMove==1,"wall body orientation and screen-right slide independent of camera yaw");
 auto full=turned;for(auto&p:full.players)if(p)p->cover={true,1,3.14159265f};full.eventWatermark=1;Event event;event.epoch=8;event.id=1;event.source=id(0);event.weapon=23;
 wire::Frame crowded{full,wire::Status::active,{event},s.authority().sop_view(id(0)).value()};check(wire::encode(crowded).size()<=2000,"24 simultaneous cover lean states plus one event and ACK fit");
 s.poll(650);check(!s.authority().snapshot().players[0]->cover.attached,"stale input cancels action");
 wire::Input old{8,3,pose(),23},next=old;old.cover={9,cover::Action::detach};next.sequence=4;next.cover.firstPerson=true;next.cover.lean=-1;
 auto merged=wire::coalesce_input(old,next);check(merged.cover.request==9&&merged.cover.action==cover::Action::detach&&merged.cover.lean==-1,"edge retained held side latest");check(std::get<wire::Input>(wire::decode(wire::encode(merged)))==merged,"intent roundtrip");next.suspended=true;next.cover={};check(wire::coalesce_input(old,next).cover==cover::Intent{},"suspend drops pending edge");
}
void native_exposure(){
 auto damage=[](int resistance){auto base=scene();auto vertices=base->vertices;auto triangles=base->triangles;
  const auto n=unsigned(vertices.size());vertices.insert(vertices.end(),{{200,0,1500},{450,0,1500},{450,2500,1500},{200,2500,1500}});triangles.push_back({{n,n+1,n+2},stage::attribute::native_solid,0,0});triangles.push_back({{n,n+2,n+3},stage::attribute::native_solid,0,0});
  auto world=std::make_shared<stage::Collision>(stage::Collision::make(vertices,triangles,{{0x15bccc,.5f,.5f,true,resistance,true}}));
  Authority h;h.begin(8,world,initial_profiles(20,1,0));check(h.join(id(0),1,pose(),1000,1000,std::array<uint16_t,1>{25},0)&&h.join(id(1),2,pose(320,4000,3.14159265f),1000,1000,std::array<uint16_t,1>{25},0),"native AK exposure actors");h.active(true);
  check(h.pose(id(0),8,1,pose(),100)==Reject::none&&h.cover(id(0),8,1,{1,cover::Action::attach,-1,false},100)==Reject::none,"native target lean");
  check(h.pose(id(1),8,1,pose(320,4000,3.14159265f),100)==Reject::none,"native shooter pose");auto shot=h.fire(id(1),{8,1,25,{0,0,-1}},100);check(bool(shot),"native spread/penetration shot accepted");return 1000-h.snapshot().players[0]->hp;
 };
 check(damage(100)>0&&damage(250)==0,"exposed upper BOX shares original material budget and native AK spread ray");
 auto bones=host_hit::pose(0,host_hit::Stance::standing);auto center=bones[4].origin;const auto& box=original_hit_regions::boxes[0];
 for(unsigned axis=0;axis<3;++axis)for(unsigned j=0;j<3;++j)center[j]+=bones[4].axes[axis][j]*box.offset[axis];center[0]+=420;
 auto from=center;from[2]-=1000;auto exposed=cover::upper_hit(from,{0,0,1},2000,{},0,host_hit::Stance::standing,{420,0,0});
 check(exposed&&exposed->bone==4&&!host_hit::query(from,{0,0,1},2000,{},0,0,host_hit::Stance::standing),"head BOX exposed beyond normal fixed body");
 Authority wet;wet.begin(8,scene(),std::array{gun()});check(wet.join(id(0),1,pose(),1000,1000,std::array<uint16_t,1>{23},0),"water actor");wet.active(true);wet.water(std::make_shared<stage::Water>(stage::Water::make({{{0,500,0},{10000,500,10000}}})));
 check(wet.pose(id(0),8,1,pose(),100)==Reject::none&&wet.cover(id(0),8,1,{1,cover::Action::attach},100)==Reject::unavailable&&wet.sop_view(id(0))->coverRequest==1,"wet admission rejected and acknowledged");
}
void tuned_cover(){
 auto weapon=gun();weapon.tuning.weightKg=100;weapon.tuning.moveSpeedScale=.1f;
 Authority h;h.begin(8,scene(),std::array{weapon});check(h.join(id(0),1,pose(-1000),1000,1000,std::array<uint16_t,1>{23},0),"heavy cover actor");h.active(true);
 check(h.pose(id(0),8,1,pose(-1000),100)==Reject::none&&h.cover(id(0),8,1,{1,cover::Action::attach},100)==Reject::none,"heavy weapon cover attaches");
 check(h.pose(id(0),8,2,pose(-1040),200)==Reject::none,"cover slide uses dedicated speed even with heavy movement tuning");
}
}
int main(){try{authority();service();native_exposure();tuned_cover();std::cout<<"Cover HOST collision/ACK/replay/scope/exposed hitbox/native AK penetration/spread/water/weighted slide and 24-peer wire/service/remote PASS\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
