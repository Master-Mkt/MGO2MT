#include "native_radio_context.h"
#include "dedicated_peer.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
namespace {
void check(bool v,const char* s){if(!v)throw std::runtime_error(s);}
const host::Hello hostHello{100,0x12345678,2,1,{{{192,0,2,1},5740}}};
const std::array<host::Hello,2> hellos{{{200,0xabcdef01,2,2,{{{192,0,2,2},5730}}},{201,0xabcdef02,2,2,{{{192,0,2,3},5730}}}}};
const std::array<radio::Identity,2> ids{{{1,0x101,200},{2,0x102,201}}};
std::vector<uint8_t> profile(){std::vector<uint8_t> p(45);p[0]=2;p.insert(p.end(),{'P',0});return p;}
struct Pair {
 size_t index;host::Machine client;host::DedicatedPeer host;bool native=true;radio::Client radio;unsigned requests=0,leaves=0;
 Pair(size_t i,bool n=true):index(i),client(hellos[i],100,profile(),0),host(hostHello,hellos[i],0),native(n){}
};
void admission(Pair& p,const std::vector<uint8_t>& event,uint64_t now){
 if(event[0]==2){p.host.queue(host::roster_record({0,0x100,100,"HOST",{}},hostHello),now);for(size_t i=0;i<2;++i)p.host.queue(host::roster_record({ids[i].slot,ids[i].instance,ids[i].character,"PC",{}},hellos[i]),now);p.host.queue({7,0,0,0,0,0,3},now);}
 else if(event[0]==10){p.host.queue(host::room_snapshot(std::array<host::Rotation,1>{{{20,1,0}}},1),now);if(p.native)p.host.queue(combat::wire::encode(combat::wire::Offer{7,ids[p.index]}),now);}
 else if(event[0]==1)++p.leaves;
}
void tick(Pair& p,uint64_t now){
 for(const auto& b:p.client.poll(now))p.host.receive(b,now);
 for(const auto& e:p.host.events())admission(p,e,now);
 for(const auto& b:p.host.poll(now)){p.client.receive(b,now);p.client.receive(b,now);}
}
void active(Pair& p,uint64_t now){
 combat::wire::Preparation prep;prep.epoch=7;prep.revision=1;prep.self=ids[p.index];prep.generation=1;prep.phase=combat::wire::RoundPhase::active;prep.runtimeReady=true;prep.supported={25};prep.requiredCategories=1;
 combat::wire::Frame frame;frame.snapshot.epoch=7;frame.snapshot.revision=1;frame.status=combat::wire::Status::active;
 for(auto id:ids){prep.players[id.slot]=combat::wire::RoundPlayer{id,1,true,true,true,1};combat::Player player;player.identity=id;player.team=1;player.hp=player.maxHp=player.stamina=player.maxStamina=100;player.weapon=25;player.alive=true;frame.snapshot.players[id.slot]=player;}
 p.host.queue(combat::wire::encode(prep),now);p.host.queue(combat::wire::encode(frame),now);
}
}
int main(){try{
 Pair a(0),b(1);for(uint64_t now=0;now<500;now+=10){tick(a,now);tick(b,now);}
 check(a.client.result().stage==host::Stage::joined&&b.client.result().stage==host::Stage::joined,"two admitted clients");
 auto before=radio::client_context(a.client.result());check(before.epoch==7&&before.members.size()==3&&!before.members[1].eligible&&before.members[1].life==0,"unspawned probe context");
 active(a,500);active(b,500);for(uint64_t now=500;now<700;now+=10){tick(a,now);tick(b,now);}
 auto c=radio::client_context(a.client.result());check(c.members.size()==3&&c.members[1].eligible&&c.members[2].eligible,"authoritative context active");
 auto result=a.client.result();result.preparation->players[1]->life++;check(!radio::client_context(result).members[1].eligible,"entitlement mismatch");result=a.client.result();result.preparation->players[1]->team=2;check(!radio::client_context(result).members[1].eligible,"team mismatch");result=a.client.result();result.match.request->rotation.flags=2;check(!radio::client_context(result).members[1].eligible,"unsupported rule flags");result=a.client.result();result.preparation->generation++;check(!radio::client_context(result).members[1].eligible,"stale generation");result=a.client.result();result.combat_state->players[1]->alive=false;check(!radio::client_context(result).members[1].eligible,"dead native boundary");
 radio::Service service({1000});service.reset(7);
 for(auto* p:{&a,&b}){p->radio.bind(7,ids[p->index]);auto probe=p->radio.probe(123+p->index,700);check(probe&&p->client.radio_send(*probe,700),"probe optional queue");}
 auto radioTick=[&](uint64_t now){
  for(auto* p:{&a,&b}){
   for(const auto& packet:p->client.poll(now))p->host.receive(packet,now);
   for(const auto& event:p->host.events()){
    if(radio::recognized(event)){++p->requests;for(const auto& d:service.receive(ids[p->index],event,c,now,radio::same_team)){auto* target=d.recipient==ids[0]?&a:&b;check(target->host.optional_queue(d.body,now)!=0,"host optional delivery");}}
    else admission(*p,event,now);
   }
  }
  for(auto* p:{&a,&b}){for(const auto& packet:p->host.poll(now)){p->client.receive(packet,now);p->client.receive(packet,now);}for(const auto& body:p->client.radio_messages())p->radio.receive(body,radio::client_context(p->client.result()),now,radio::same_team);}
 };
 for(uint64_t now=700;now<1000;now+=10)radioTick(now);
 check(a.radio.status()==radio::Status::ready&&b.radio.status()==radio::Status::ready,"two negotiated peers");
 auto submission=a.radio.submit(9,c,1000);check(submission.body&&a.client.radio_send(*submission.body,1000),"request send");for(uint64_t now=1000;now<1400;now+=10)radioTick(now);
 auto ae=a.radio.drain(),be=b.radio.drain();check(ae.size()==1&&be.size()==1&&ae[0].sequence==submission.sequence&&be[0].sender==ids[0],"reliable selfecho and remote exactly once");check(a.leaves==0&&b.leaves==0&&a.requests==2,"no leave opcode collision");
 auto malformed=*submission.body;malformed[5]=99;check(!a.client.radio_send(malformed,1400),"malformed outbound rejected");auto notice=radio::encode({radio::Kind::notification,7,123,ids[0],1,10,0,2});check(!a.client.radio_send(*notice,1400),"wrong outbound direction");check(a.host.optional_queue(malformed,1400)!=0,"inject malformed optional host payload");for(uint64_t now=1400;now<1500;now+=10)radioTick(now);check(a.client.result().stage==host::Stage::joined,"malformed optional incoming does not close");
 // Machine raw radio queue is bounded independently of UI consumption.
 for(uint32_t seq=10;seq<75;++seq){auto body=radio::encode({radio::Kind::notification,7,123,ids[0],1,seq,0,2});check(a.host.optional_queue(*body,1500)!=0,"queue one notice");tick(a,1500);tick(a,1500);}
 check(a.client.radio_messages().size()==64&&a.client.radio_messages().empty()&&a.client.result().stage==host::Stage::joined,"machine bounded64 drain");
 // Congestion rejects optional traffic without stealing the remaining slots.
 for(unsigned n=0;n<24;++n)check(a.host.optional_queue(*notice,1600)!=0,"24 host optional capacity");check(!a.host.optional_queue(*notice,1600)&&!a.host.closed(),"25th optional host rejects without close");check(a.host.queue({0},1600)!=0&&!a.host.closed(),"room reserve still available");
 for(unsigned n=0;n<8;++n)check(b.client.radio_send(*radio::encode({radio::Kind::probe,7,124,ids[1]}),1600),"8 client optional capacity");check(!b.client.radio_send(*radio::encode({radio::Kind::probe,7,124,ids[1]}),1600)&&b.client.result().stage==host::Stage::joined,"9th client optional nonfatal");
 Pair legacy(0,false);for(uint64_t now=0;now<500;now+=10)tick(legacy,now);check(legacy.client.result().stage==host::Stage::joined&&!legacy.client.radio_send(*submission.body,500),"legacy host no native offer stays joined");
 // A pre-radio GWCB host acknowledges unknown ED and ignores its application.
 Pair oldNative(0);for(uint64_t now=0;now<500;now+=10)tick(oldNative,now);radio::Client unsupported;unsupported.bind(7,ids[0]);auto probe=unsupported.probe(888,500);check(oldNative.client.radio_send(*probe,500),"older GWCB native probe");for(uint64_t now=500;now<3600;now+=10)tick(oldNative,now);unsupported.tick(3500);check(unsupported.status()==radio::Status::unavailable&&oldNative.client.result().stage==host::Stage::joined&&!oldNative.host.closed(),"old GWCB host graceful radio timeout");
 // Global generation drops raw notices before another epoch can consume them.
 oldNative.host.optional_queue(*notice,3600);tick(oldNative,3600);oldNative.host.queue(host::next_round_snapshot(2,1),3600);tick(oldNative,3600);check(oldNative.client.radio_messages().empty(),"generation clears radio inbox");
 auto leave=oldNative.client.leave_packet();check(leave.has_value(),"explicit leave still available");oldNative.host.receive(*leave,3610);auto exit=oldNative.host.events();check(exit.size()==1&&exit[0]==std::vector<uint8_t>{1},"actual leave remains type1");oldNative.client.cancel();check(oldNative.client.radio_messages().empty(),"cancel clear");
 std::cout<<"native_radio_transport_test PASS: two real machines, optional queues, original leave, old host, context and bounded inbox\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
