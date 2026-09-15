#include "combat_sop_view.h"
#include "combat_service.h"
#include "combat_wire_budget.h"
#include "dedicated_peer.h"
#include "host_session.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2win;
namespace {
namespace wire=combat::wire;
void check(bool b,const char*why){if(!b)throw std::runtime_error(why);}
template<class F>void invalid(F fn,const char*why){bool caught=false;try{fn();}catch(const wire::Invalid&){caught=true;}check(caught,why);}
combat::Identity identity(unsigned slot){return {uint8_t(slot),uint16_t(0x100+slot),uint32_t(100+slot*100)};}
combat::Player player(unsigned slot,uint8_t team=1){combat::Player p;p.identity=identity(slot);p.team=team;p.pose.feet={float(slot*2000),2,0};p.hp=p.maxHp=p.stamina=p.maxStamina=1000;p.weapon=25;p.ammo=30;p.reserve=90;p.alive=true;p.life=1;return p;}
wire::Frame frame(unsigned count=4,unsigned recipient=1){wire::Frame f;f.snapshot={7,10,0,{}};f.status=wire::Status::active;for(unsigned i=0;i<count;++i)f.snapshot.players[i]=player(i,i==3?2:1);f.sop.recipient=identity(recipient);f.sop.life=1;f.sop.visibleMask=1u<<(recipient==1?2:1);f.sop.activation=3;f.sop.origin={10,20,30};f.sop.inputSequence=9;f.sop.inputSequenced=true;return f;}
std::vector<combat::Event> events(unsigned n){std::vector<combat::Event> out;for(unsigned i=0;i<n;++i){combat::Event e;e.epoch=7;e.id=i+1;e.kind=combat::EventKind::impact;e.source=identity(1);e.weapon=25;e.position={0,2,float(i+1)};e.normal={0,0,-1};out.push_back(e);}return out;}
void codec(){
 auto f=frame();auto bytes=wire::encode(f);check(std::get<wire::Frame>(wire::decode(bytes))==f,"recipient-specific SOP footer round-trip");
 // GWCB13 appends cover and special-PC ACKs after the GWCB11 cone. The three
 // canonical boolean positions remain relative to the start of this footer.
 constexpr size_t footerSize=1+7+4+3+4+12+1+4+1+4+2+4+4;check(bytes.size()>footerSize,"present footer extent");size_t footer=bytes.size()-footerSize;
 for(size_t n=0;n<bytes.size();++n)invalid([&]{wire::decode(std::span(bytes).first(n));},"truncated SOP frame rejected");
 for(unsigned kind=0;kind<12;++kind){auto bad=f;switch(kind){case 0:bad.sop.recipient.instance++;break;case 1:bad.sop.life++;break;case 2:bad.sop.visibleMask|=1u<<1;break;case 3:bad.sop.visibleMask=1u<<3;break;case 4:bad.sop.visibleMask=1u<<4;break;case 5:bad.sop.visibleMask|=1u<<24;break;case 6:bad.sop.jammed=true;break;case 7:bad.snapshot.players[2]->alive=false;bad.snapshot.players[2]->hp=0;break;case 8:bad.sop.activation=0;break;case 9:bad.sop.inputSequenced=false;break;case 10:bad.sop.origin[0]=std::numeric_limits<float>::quiet_NaN();break;case 11:bad.status=wire::Status::ended;break;}invalid([&]{wire::encode(bad);},"invalid identity/life/team/mask/jam/status footer rejected");}
 check(bytes[footer]==1&&bytes[footer+31]==0&&bytes[footer+36]==1,"fixture offsets identify present/jammed/inputSequenced booleans");
 for(size_t offset:{footer,footer+31,footer+36})for(unsigned value:{2u,255u}){auto bad=bytes;bad[offset]=uint8_t(value);invalid([&]{wire::decode(bad);},"noncanonical SOP boolean rejected");}
 auto absent=f;absent.sop={};check(std::get<wire::Frame>(wire::decode(wire::encode(absent))).sop==combat::SopView{},"absent footer remains empty");absent.sop.activation=1;invalid([&]{wire::encode(absent);},"absent recipient cannot smuggle activation");
 auto jammed=f;jammed.sop.visibleMask=0;jammed.sop.jammed=true;check(std::get<wire::Frame>(wire::decode(wire::encode(jammed)))==jammed,"jammed recipient with no disclosed peers is valid");
 auto zero=f;zero.sop.activation=0;zero.sop.origin={};invalid([&]{wire::encode(zero);},"linked visibility needs a nonzero activation serial");
 for(uint16_t angle:{uint16_t(0),uint16_t(3),uint16_t(30)}){auto cone=f;cone.sop.spreadMilliRadians=angle;check(std::get<wire::Frame>(wire::decode(wire::encode(cone)))==cone,"recipient angle round-trip at zero/base/max");}
 for(unsigned kind=0;kind<3;++kind){auto bad=f;bad.sop.spreadMilliRadians=7;if(kind==0)bad.sop.spreadMilliRadians=31;if(kind==1)bad.snapshot.players[1]->weapon=3;if(kind==2){bad.snapshot.players[1]->alive=false;bad.snapshot.players[1]->hp=0;bad.sop.visibleMask=0;}invalid([&]{wire::encode(bad);},"excessive/unsupported/dead recipient cone rejected");}
 auto old=bytes;old[5]=10;check(!wire::recognized(old),"old GWCB10 cannot mix with GWCB11");invalid([&]{wire::decode(old);},"old wire version rejected");
 auto malformed=bytes;malformed[malformed.size()-10]=31;invalid([&]{wire::decode(malformed);},"malformed cone byte rejected");
 // Current compressed states retain exact nonzero reload deadlines. Use
 // the worst human reload roster to prove the real 2000-byte boundary.
 auto big=frame(24);for(auto&p:big.snapshot.players)p->reloadUntil=UINT64_MAX;
 big.events=events(1);big.snapshot.eventWatermark=4;const auto capacity=combat_test::budget(big);
 check(capacity.capacity<4&&capacity.baseBytes+(capacity.capacity+1)*capacity.eventBytes>2000,"worst human roster next event exceeds byte ceiling");
 big.events=events(unsigned(capacity.capacity));auto bounded=wire::encode(big);
 check(bounded.size()==capacity.fullBytes&&std::get<wire::Frame>(wire::decode(bounded))==big,"bounded full snapshot and complete SOP roundtrip");
 auto noSop=big;noSop.sop={};auto absentBudget=combat_test::budget(noSop);check(absentBudget.baseBytes<capacity.baseBytes&&absentBudget.capacity>=capacity.capacity,"omitting footer only increases available event budget");
 host::Message message;message.channel=1;message.serial=1;message.payload=bounded;
 auto packet=host::encode({1,{message}},host::Keys{1,2});
 auto decoded=host::decode(packet,host::Keys{1,2},1);
 check(packet.size()<=host::max_datagram&&decoded.messages.size()==1&&decoded.messages[0].payload==bounded,"2000-byte SOP record round-trips inside encrypted datagram limit");
 std::cout<<"full_roster_sop_events="<<capacity.capacity<<" record_bytes="<<bounded.size()<<" datagram_bytes="<<packet.size()<<'\n';
 wire::Input press;press.epoch=7;press.life=1;press.sequence=10;press.weapon=25;press.specialPressed=press.specialHeld=true;
 auto release=press;release.sequence=11;release.specialPressed=release.specialHeld=false;auto merged=wire::coalesce_input(press,release);check(merged.specialPressed&&!merged.specialHeld&&merged.sequence==11,"short Y press survives coalescing and newest release wins");check(std::get<wire::Input>(wire::decode(wire::encode(merged)))==merged,"coalesced Y input encodes");
 auto staleHold=press;staleHold.specialPressed=false;check(!wire::coalesce_input(staleHold,release).specialHeld,"held level cannot leak past newest release");
 auto fire=release;fire.fire=fire.firePressed=true;auto specialWins=wire::coalesce_input(press,fire);check(specialWins.specialPressed&&!specialWins.specialHeld&&!specialWins.fire&&!specialWins.firePressed&&!specialWins.reload,"retained special edge cannot turn into fire");
 for(unsigned change=0;change<3;++change){auto fresh=release;if(change==0)fresh.epoch++;if(change==1)fresh.life++;if(change==2)fresh.suspended=true;check(!wire::coalesce_input(press,fresh).specialPressed,"new life/epoch/suspension discards pending special press");}
 for(unsigned flags=1;flags<=3;++flags){auto bad=press;if(flags==1)bad.fire=true;if(flags==2)bad.reload=true;if(flags==3)bad.suspended=true;invalid([&]{wire::encode(bad);},"special excludes fire/reload/suspended actions");}
}
struct Pair {
 host::Hello serverHello{100,0x12345678,2,1,{{{192,0,2,1},5740}}},clientHello;
 std::vector<uint8_t> profile;std::unique_ptr<host::Machine> client;std::unique_ptr<host::DedicatedPeer> server;wire::Frame baseline;uint64_t now=0;
 explicit Pair(unsigned slot):clientHello{identity(slot).character,0xabcdef01,2,2,{{{192,0,2,2},5730}}},profile(45),baseline(frame(4,slot)){
  profile[0]=2;profile.insert(profile.end(),{'P','l','a','y','e','r',0});client=std::make_unique<host::Machine>(clientHello,100,profile,0);server=std::make_unique<host::DedicatedPeer>(serverHello,clientHello,0);pump(700);check(client->result().stage==host::Stage::joined&&client->result().combat_sop==baseline.sop,"encrypted Machine receives its own footer");
 }
 void pump(uint64_t until){for(;now<until;now+=10){for(auto&b:client->poll(now))server->receive(b,now);for(auto&e:server->events()){
   if(e[0]==2){server->queue(host::roster_record({0,0x100,100,"HOST",{}},serverHello),now);for(unsigned slot=1;slot<4;++slot){auto id=identity(slot);auto hello=clientHello;hello.character=id.character;server->queue(host::roster_record({id.slot,id.instance,id.character,"Player",{}},hello),now);}server->queue({7,0,0,0,0,0,3},now);}
   else if(e[0]==10){server->queue(host::room_snapshot(std::array<host::Rotation,1>{{{20,1,0}}},1),now);server->queue(wire::encode(wire::Offer{7,baseline.sop.recipient}),now);}
   else if(wire::recognized(e)&&std::holds_alternative<wire::Accept>(wire::decode(e)))server->queue(wire::encode(baseline),now);
  }for(auto&b:server->poll(now))client->receive(b,now);}}
 void send(const wire::Frame& f){check(server->queue(wire::encode(f),now)!=0,"fixture queues SOP frame");pump(now+30);}
};
void machine(){
 Pair first(1),second(2);check(first.client->result().combat_sop.recipient!=second.client->result().combat_sop.recipient,"distinct recipient footer identities");
 first.send(first.baseline);check(first.client->result().stage==host::Stage::joined,"same revision exact footer is idempotent");
 auto newer=first.baseline;newer.snapshot.revision++;newer.sop.visibleMask=0;newer.sop.jammed=true;first.send(newer);check(first.client->result().combat_sop==newer.sop,"new revision safely clears jammed links");
 auto rewrite=newer;rewrite.sop.inputSequence++;first.send(rewrite);check(first.client->result().stage==host::Stage::protocol_error,"same revision footer rewrite rejected");
 auto wrong=second.baseline;wrong.snapshot.revision++;wrong.sop=frame(4,1).sop;second.send(wrong);check(second.client->result().stage==host::Stage::protocol_error,"valid other-recipient footer rejected by Machine scope");
}
std::shared_ptr<const stage::Collision> floor(){return std::make_shared<const stage::Collision>(stage::Collision::make({{-100000,0,-100000},{100000,0,-100000},{100000,0,100000},{-100000,0,100000}},{{{0,2,1}},{{0,3,2}}}));}
void service(){
 combat::Weapon ak{25,1000,0,100,300,30,90,10000,9001,9002,true};ak.nativeAkAccuracy=true;combat::Service svc(7);svc.configure(floor(),std::array{ak});check(svc.authority().configure_sop(100,100),"configure host SOP animation policy");
 for(unsigned slot=0;slot<24;++slot){auto id=identity(slot);combat::Pose p;p.feet=slot==0?combat::Vec3{0,2,0}:slot==1?combat::Vec3{0,2,3000}:combat::Vec3{float(slot*2000),2,0};check(svc.authority().join(id,slot==1?2:1,p,1000,1000,std::array<uint16_t,1>{25},0),"24 actual authority spawns");check(svc.admit(id)&&svc.receive(id,wire::encode(wire::Accept{7}),0),"24 accepted service recipients");}
 svc.authority().active(true);svc.deliveries();wire::Input fire;fire.epoch=7;fire.sequence=1;fire.pose=svc.authority().snapshot().players[0]->pose;fire.weapon=25;fire.firePressed=true;check(svc.receive(identity(0),wire::encode(fire),100),"accepted host fire creates multi-event frame");svc.poll(100);auto deliveries=svc.deliveries();std::array<unsigned,24> counts{},eventFrames{};std::array<size_t,24> capacities{};std::array<uint64_t,24> latest{};
 for(auto&d:deliveries){check(d.payload.size()<=2000,"every service delivery stays within datagram record bound");auto record=wire::decode(d.payload);if(auto f=std::get_if<wire::Frame>(&record)){check(f->sop.recipient==d.recipient&&f->sop.life==f->snapshot.players[d.recipient.slot]->life,"service footer belongs to actual recipient incarnation");if(!f->events.empty()){const auto budget=combat_test::budget(*f);auto& capacity=capacities[d.recipient.slot];if(!capacity)capacity=budget.capacity;check(capacity==budget.capacity,"same recipient roster retains exact encoded capacity");const size_t remaining=4-counts[d.recipient.slot];check(f->events.size()==(std::min)(capacity,remaining),"Service greedily fills the exact byte-bounded event chunk");++eventFrames[d.recipient.slot];}for(auto&e:f->events){check(e.id==latest[d.recipient.slot]+1,"service chunk preserves event order");latest[d.recipient.slot]=e.id;++counts[d.recipient.slot];}}}
 for(unsigned slot=0;slot<24;++slot){check(counts[slot]==4,"24 recipients receive all four shot/impact/damage/death events without truncation");check(capacities[slot]&&eventFrames[slot]==(4+capacities[slot]-1)/capacities[slot],"exact per-recipient event chunk count follows current encoding capacity");}
 check(svc.authority().sop_view(identity(0))->spreadMilliRadians==7&&svc.authority().sop_view(identity(1))->spreadMilliRadians==0&&svc.authority().sop_view(identity(2))->spreadMilliRadians==3,"only shooter blooms; dead target clears; other recipient base");
 svc.poll(500);bool recovered=false;for(auto&d:svc.deliveries()){auto record=wire::decode(d.payload);if(auto f=std::get_if<wire::Frame>(&record);f&&d.recipient==identity(0)){check(f->events.empty()&&f->sop.spreadMilliRadians==3,"idle recovery arrives as HOST state without fake shot");recovered=true;}}
 check(recovered,"service publishes recovered recipient cone");
}
void gesture(){
 combat::Weapon ak{25,35,0,100,300,30,90,10000,9001,9002,true};combat::Service svc(7);svc.configure(floor(),std::array{ak});check(svc.authority().configure_sop(100,50),"short gesture policy");
 for(unsigned slot=0;slot<3;++slot){auto p=player(slot,slot==2?2:1);p.pose.feet={0,2,float(slot*1200)};check(svc.authority().join(p.identity,p.team,p.pose,1000,1000,std::array<uint16_t,1>{25},0)&&svc.admit(p.identity)&&svc.receive(p.identity,wire::encode(wire::Accept{7}),0),"gesture peers admitted");}
 svc.authority().active(true);svc.deliveries();wire::Input press;press.epoch=7;press.sequence=1;press.weapon=25;press.pose=svc.authority().snapshot().players[0]->pose;press.specialPressed=press.specialHeld=true;check(svc.receive(identity(0),wire::encode(press),100),"gesture press accepted");auto release=press;release.sequence=2;release.specialPressed=release.specialHeld=false;check(svc.receive(identity(0),wire::encode(release),101),"gesture release accepted before host tick");svc.poll(101);check(svc.authority().snapshot().players[0]->specialPhase==combat::SpecialPhase::start,"coalesced short press starts once");svc.deliveries();svc.poll(201);auto a=svc.authority().sop_view(identity(0)),b=svc.authority().sop_view(identity(1)),enemy=svc.authority().sop_view(identity(2));check(a&&b&&enemy&&a->visibleMask==(1u<<1)&&b->visibleMask==1&&!enemy->visibleMask,"HOST gesture links only eligible teammate with recipient-specific masks");check(a->inputSequenced&&a->inputSequence==2&&a->activation==1,"footer confirms latest accepted pose sequence and activation");auto activation=a->activation;check(svc.authority().snapshot().players[0]->specialPhase==combat::SpecialPhase::end,"released level exits hold after single attempt");
 auto spoof=release;spoof.sequence=3;spoof.life=2;check(!svc.receive(identity(0),wire::encode(spoof),202),"old or ungranted life cannot submit gesture");check(!svc.receive({0,0x777,100},wire::encode(release),202),"forged full identity rejected");check(!svc.receive(identity(0),wire::encode(release),202),"same input sequence cannot reapply gesture");svc.poll(301);check(svc.authority().sop_view(identity(0))->activation==activation,"no repeated activation from held/coalesced history");
 check(svc.authority().sop_jam(identity(0),true,302),"native host jammer applied");check(!svc.authority().sop_view(identity(0))->visibleMask&&!svc.authority().sop_view(identity(1))->visibleMask,"jam dissolution removes both visible directions");
 auto prone=release;prone.sequence=3;prone.pose=svc.authority().snapshot().players[1]->pose;prone.pose.capsule={260,560,2};prone.specialPressed=prone.specialHeld=true;check(svc.receive(identity(1),wire::encode(prone),310),"prone input admitted for authority validation");svc.poll(310);for(auto&d:svc.deliveries())wire::decode(d.payload);check(svc.authority().snapshot().players[1]->pose.capsule.height==560&&svc.authority().snapshot().players[1]->specialPhase==combat::SpecialPhase::none,"prone gesture cannot create an unencodable special phase");
}
}
int main(){try{codec();machine();service();gesture();std::cout<<"SOP network PASS: strict footer codec/scope, Y coalescing, Machine recipient/revision checks, 24-player service event chunking and authoritative gesture lifecycle\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
