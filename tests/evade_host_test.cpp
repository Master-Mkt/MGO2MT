#include "combat_service.h"
#include "remote_avatar.h"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace mgo2mt;using namespace mgo2mt::combat;
namespace {
void check(bool value,const char*message){if(!value)throw std::runtime_error(message);}
Identity id(unsigned i){return {uint8_t(i),uint16_t(i+10),i+100};}
auto floor(){return std::make_shared<const stage::Collision>(stage::Collision::make({{-100000,0,-100000},{100000,0,-100000},{100000,0,100000},{-100000,0,100000}},{{{0,2,1}},{{0,3,2}}}));}
Weapon gun(){return {23,1000,0,100,300,20,40,20000,1,2,true};}
Pose at(float x=0,float z=0){Pose p;p.feet={x,2,z};return p;}
void prepare(Authority&h){auto w=gun();h.begin(7,floor(),std::array{w});check(h.configure_evade({400,3800},{300,1500}),"configure bounded synthetic profiles");check(h.configure_sop(100,50),"SOP profile");check(h.join(id(0),1,at(),1000,1000,std::array<uint16_t,1>{23},0),"spawn");h.active(true);}
void authority(){
 Authority h;prepare(h);check(!h.configure_evade({0,3800},{300,1500})&&!h.configure_evade({400,6001},{300,1500}),"invalid profiles fail closed");
 check(h.evade(id(0),7,1,EvadeKind::roll,1,100)==Reject::sequence,"request needs accepted pose");
 check(h.pose(id(0),7,1,at(0,380),100)==Reject::none,"running displacement accepted");check(h.evade(id(0),7,1,EvadeKind::roll,1,100)==Reject::none,"rolling admitted from recent host-validated movement");
 check(h.snapshot().players[0]->evadeSerial==1&&h.sop_view(id(0))->evadeRequest==1,"activation and recipient ACK");
 check(h.evade(id(0),7,1,EvadeKind::roll,1,101)==Reject::sequence,"duplicate cannot restart");
 check(h.reload(id(0),7,110).reject==Reject::unavailable&&h.fire(id(0),{7,1,23,{0,0,1}},110).reject==Reject::unavailable,"fire and reload blocked");
 check(h.special(id(0),7,1,true,true,110)==Reject::unavailable,"special excluded");
 auto crouch=at(0,380);crouch.capsule.height=1100;check(h.pose(id(0),7,2,crouch,120)==Reject::unavailable,"standing capsule retained");
 h.advance(250);check(h.snapshot().players[0]->evadeElapsedMs==150,"host elapsed monotonic");h.advance(249);check(h.snapshot().players[0]->evadeElapsedMs==150,"backward host clock cannot rewind action");
 check(h.pose(id(0),7,2,at(0,380),250)==Reject::none,"next accepted pose");check(h.evade(id(0),7,2,EvadeKind::backstep,2,250)==Reject::unavailable&&h.sop_view(id(0))->evadeRequest==2,"busy reject is acknowledged");
 h.advance(499);check(h.snapshot().players[0]->evadeKind==EvadeKind::roll,"duration lower boundary");h.advance(500);check(h.snapshot().players[0]->evadeKind==EvadeKind::none&&!h.snapshot().players[0]->evadeSerial,"duration clears optional payload");
 check(h.pose(id(0),7,3,at(0,380),510)==Reject::none,"post-action pose");check(h.evade(id(0),7,3,EvadeKind::backstep,2,510)==Reject::sequence,"denied edge cannot replay later");check(h.evade(id(0),7,3,EvadeKind::backstep,3,510)==Reject::none,"fresh backstep accepted");
 check(h.evade(id(0),8,3,EvadeKind::roll,4,510)==Reject::generation&&h.evade(id(0),7,3,EvadeKind::roll,4,510,2)==Reject::generation,"old scope rejected");
 h.active(false);check(h.snapshot().players[0]->evadeKind==EvadeKind::none,"round stop cancels action");
 Authority hit;prepare(hit);check(hit.join(id(1),2,at(0,-4000),1000,1000,std::array<uint16_t,1>{23},0),"attacker");check(hit.pose(id(0),7,1,at(),100)==Reject::none&&hit.evade(id(0),7,1,EvadeKind::backstep,1,100)==Reject::none,"target action");check(hit.pose(id(1),7,1,at(0,-4000),100)==Reject::none,"attacker pose");auto shot=hit.fire(id(1),{7,1,23,{0,0,1}},100);check(shot&&!hit.snapshot().players[0]->alive&&hit.snapshot().players[0]->evadeKind==EvadeKind::none,"evade grants no invulnerability and death cancels");
 check(hit.respawn(id(0),2,[&]{return hit.join(id(0),1,at(),1000,1000,std::array<uint16_t,1>{23},500);}),"respawn");check(hit.pose(id(0),7,1,at(),510,2)==Reject::none&&hit.evade(id(0),7,1,EvadeKind::backstep,1,510,2)==Reject::none,"new life can restart request serial namespace");
 Authority reload;prepare(reload);check(reload.pose(id(0),7,1,at(),100)==Reject::none&&reload.fire(id(0),{7,1,23,{0,0,1}},100)&&reload.reload(id(0),7,110),"reload active fixture");check(reload.pose(id(0),7,2,at(),120)==Reject::none&&reload.evade(id(0),7,2,EvadeKind::backstep,1,120)==Reject::reloading&&reload.sop_view(id(0))->evadeRequest==1,"reload rejection ACK");
 Authority stun;auto stunGun=gun();stunGun.damage=0;stunGun.staminaDamage=1000;stun.begin(7,floor(),std::array{stunGun});check(stun.configure_evade({400,3800},{300,1500}),"stun fixture profile");
 check(stun.join(id(0),1,at(),1000,1000,std::array<uint16_t,1>{23},0)&&stun.join(id(1),2,at(0,-4000),1000,1000,std::array<uint16_t,1>{23},0),"stun fixture spawn");stun.active(true);
 check(stun.pose(id(0),7,1,at(),100)==Reject::none&&stun.evade(id(0),7,1,EvadeKind::backstep,1,100)==Reject::none&&stun.pose(id(1),7,1,at(0,-4000),100)==Reject::none,"stun fixture evade");check(bool(stun.fire(id(1),{7,1,23,{0,0,1}},100))&&stun.snapshot().players[0]->stunned&&stun.snapshot().players[0]->evadeKind==EvadeKind::none,"stun clears evade immediately");
 for(auto side:{EvadeKind::rollLeft,EvadeKind::rollRight}){
  Authority lateral;prepare(lateral);auto p=at();p.yaw=side==EvadeKind::rollLeft?-1.5707963f:1.5707963f;
  check(lateral.pose(id(0),7,1,p,100)==Reject::none&&lateral.evade(id(0),7,1,side,1,100)==Reject::none,"standing left/right native roll admission uses shared roll policy");
  check(lateral.snapshot().players[0]->evadeKind==side&&lateral.sop_view(id(0))->evadeRequest==1,"direction kind and recipient ACK are authoritative");
  check(lateral.evade(id(0),7,1,side,1,101)==Reject::sequence,"lateral request replay cannot restart");
  auto turned=p;turned.yaw+=1.5707963f;check(lateral.pose(id(0),7,2,turned,120)==Reject::unavailable,"HOST rejects 90-degree redirection during left/right roll");
  auto looking=p;looking.pitch=.5f;check(lateral.pose(id(0),7,2,looking,120)==Reject::none,"rejected turn does not consume sequence and camera pitch remains independent");
  looking.yaw=p.yaw+.0008f;check(lateral.pose(id(0),7,3,looking,130)==Reject::none,"tiny yaw encoding tolerance accepted");
  looking.yaw=p.yaw+.0016f;check(lateral.pose(id(0),7,4,looking,140)==Reject::unavailable,"yaw tolerance is measured from start and cannot accumulate drift");
  looking.yaw=p.yaw;looking.feet[0]+=(side==EvadeKind::rollLeft?-76.f:76.f);check(lateral.pose(id(0),7,4,looking,150)==Reject::none,"fixed-heading lateral movement retains normal HOST speed/collision admission");
  check(lateral.fire(id(0),{7,1,23,{0,0,1}},160).reject==Reject::unavailable&&lateral.reload(id(0),7,160).reject==Reject::unavailable,"lateral firing and reload are suppressed");
  lateral.advance(499);check(lateral.snapshot().players[0]->evadeKind==side,"both lateral profiles last full rolling duration");
  lateral.advance(500);check(lateral.snapshot().players[0]->evadeKind==EvadeKind::none&&!lateral.snapshot().players[0]->evadeSerial,"lateral duration ends without array overrun or perpetual action");
  check(lateral.evade(id(0),7,1,side,2,500,2)==Reject::generation,"wrong-life lateral intent rejected");
 }
 Authority resting;prepare(resting);check(resting.pose(id(0),7,1,at(),100)==Reject::none&&resting.evade(id(0),7,1,EvadeKind::roll,1,100)==Reject::too_fast&&resting.sop_view(id(0))->evadeRequest==1,"first standing packet without running evidence is explicitly rejected and acknowledged");
}
void codec(){
 wire::Input a{7,1,at(),23};a.evadeKind=EvadeKind::backstep;a.evadeRequest=9;auto b=a;b.sequence=2;b.evadeKind=EvadeKind::none;b.evadeRequest=0;
 auto merged=wire::coalesce_input(a,b);check(merged.evadeRequest==9&&merged.sequence==2,"pending short edge survives newest pose");check(std::get<wire::Input>(wire::decode(wire::encode(merged)))==merged,"evade input round trip");b.suspended=true;check(!wire::coalesce_input(a,b).evadeRequest,"suspend discards short edge");b.suspended=false;b.life=2;check(!wire::coalesce_input(a,b).evadeRequest,"life does not inherit old request");
 for(unsigned kind=0;kind<8;++kind)for(unsigned serial:{0u,1u}){auto input=a;input.evadeKind=EvadeKind(kind);input.evadeRequest=serial;bool accepted=true;try{wire::encode(input);}catch(const wire::Invalid&){accepted=false;}check(accepted==(kind<=4&&((kind==0)==(serial==0))),"kind/counter validation");}
 auto conflict=a;conflict.fire=true;bool invalid=false;try{wire::encode(conflict);}catch(const wire::Invalid&){invalid=true;}check(invalid,"evade cannot share fire flags");
}
void service_remote(){
 Service svc(7,Policy{false,6000,15000,500,true});auto w=gun();svc.configure(floor(),std::array{w});check(svc.authority().configure_evade({3000,3800},{3000,1500}),"service profiles");
 host::Roster roster;roster.complete=true;
 for(unsigned i=0;i<24;++i){auto p=i==0?at():i==1?at(0,3000):at(float(i*2000),0);check(svc.authority().join(id(i),0,p,1000,1000,std::array<uint16_t,1>{23},0)&&svc.admit(id(i))&&svc.receive(id(i),wire::encode(wire::Accept{7}),0),"DM peers");host::Player r{uint8_t(i),id(i).instance,id(i).character,"Fixture",""};r.appearance=std::array<uint8_t,28>{};roster.slots[i]=r;}
 svc.authority().active(true);svc.deliveries();
 for(unsigned i=1;i<24;++i){wire::Input input{7,1,svc.authority().snapshot().players[i]->pose,23};input.evadeKind=EvadeKind::backstep;input.evadeRequest=1;check(svc.receive(id(i),wire::encode(input),100),"evade input");}
 svc.poll(100);svc.deliveries();auto state=svc.authority().snapshot();for(unsigned i=1;i<24;++i)check(state.players[i]->evadeKind==EvadeKind::backstep&&svc.authority().sop_view(id(i))->evadeRequest==1,"DM always receives per-request ACK");
 remote::Scene scene;check(scene.update(state,roster,id(0),100),"remote accepts real host action");auto avatars=scene.sample(150);check(avatars.size()==23&&avatars[0].evadeKind==EvadeKind::backstep&&avatars[0].evadeSerial==1&&avatars[0].evadeSeconds==.05,"remote action and elapsed transmitted");
 // A frame that spent less time in transit may carry a HOST age below the
 // previously extrapolated display. Do not cross the roll/recover boundary
 // backwards or restart the presentation lane for an unchanged activation.
 auto delayed=state;delayed.players[1]->evadeKind=EvadeKind::roll;delayed.players[1]->evadeElapsedMs=650;remote::Scene latency;
 check(latency.update(delayed,roster,id(0),1000),"delayed roll baseline");const auto advanced=latency.sample(1100).front().evadeSeconds;check(advanced>40./60.,"display reached recovery phase");
 ++delayed.revision;delayed.players[1]->evadeElapsedMs=660;check(latency.update(delayed,roster,id(0),1120),"newer low-age snapshot");auto preserved=latency.sample(1120).front().evadeSeconds;check(preserved>=advanced&&preserved>.769&&preserved<.771,"same activation cannot rewind across recovery boundary");
 ++delayed.revision;delayed.players[1]->evadeElapsedMs=700;check(latency.update(delayed,roster,id(0),1170)&&latency.sample(1170).front().evadeSeconds>=.819,"repeated variable delay remains monotonic");
 ++delayed.revision;delayed.players[1]->evadeSerial=2;delayed.players[1]->evadeElapsedMs=0;check(latency.update(delayed,roster,id(0),1180)&&latency.sample(1180).front().evadeSeconds==0,"new serial starts a new action clock");
 ++delayed.revision;delayed.players[1]->evadeKind=EvadeKind::none;delayed.players[1]->evadeSerial=0;check(latency.update(delayed,roster,id(0),1190)&&latency.sample(1190).front().evadeKind==EvadeKind::none,"host action end still clears immediately");
 Replica stable;check(stable.snapshot(state),"replica baseline");auto rewrite=state;rewrite.players[1]->evadeElapsedMs++;check(!stable.snapshot(rewrite),"same revision action rewrite rejected");rewrite=state;rewrite.players[1]->life=0;rewrite.revision++;check(!stable.snapshot(rewrite),"invalid incarnation rejected");
 wire::Input fire{7,1,at(),23};fire.firePressed=true;check(svc.receive(id(0),wire::encode(fire),150),"shooter input");svc.poll(150);auto deliveries=svc.deliveries();std::array<unsigned,24> counts{};std::array<uint64_t,24> last{};bool single=false;
 for(const auto&d:deliveries){check(d.payload.size()<=2000,"2000B cap");auto r=wire::decode(d.payload);if(auto*f=std::get_if<wire::Frame>(&r)){Replica replica;check(replica.snapshot(f->snapshot),"expanded snapshot validates");single|=f->events.size()==1;for(const auto&e:f->events){check(e.id==last[d.recipient.slot]+1,"chunked event order");last[d.recipient.slot]=e.id;++counts[d.recipient.slot];}}}
 for(auto n:counts)check(n==4,"24 recipients retain shot impact damage death");check(single,"crowded active-action snapshots dynamically fit one-event frames");
 svc.poll(600);check(svc.authority().snapshot().players[2]->evadeKind==EvadeKind::none,"stale input cancels active action");
}
}
int main(){try{codec();authority();service_remote();std::cout<<"Evade HOST admission, replay/ACK/life, coalescing, no invulnerability, remote playback and bounded GWCB9 chunks PASS\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
