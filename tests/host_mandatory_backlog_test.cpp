#include "host_mandatory_backlog.h"
#include "host_session.h"
#include "dedicated_peer.h"
#include "combat_wire.h"
#include <iostream>
using namespace mgo2win;
namespace {
void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
const host::Hello sh{100,0x12345678,2,1,{{{192,0,2,1},5740}}},ch{200,0xabcdef01,2,2,{{{192,0,2,2},5730}}};
const combat::Identity id{1,0x101,200};
std::vector<uint8_t> profile(){std::vector<uint8_t>b(45);b[0]=2;b.insert(b.end(),{'P',0});return b;}
combat::Snapshot snapshot(uint64_t epoch,uint64_t rev,uint64_t watermark){combat::Snapshot s{epoch,rev,watermark,{}};combat::Player p;p.identity=id;p.pose.feet={0,2,0};p.hp=p.maxHp=p.stamina=p.maxStamina=1000;p.weapon=25;p.ammo=30;p.reserve=90;p.alive=true;s.players[id.slot]=p;return s;}
struct Pair {
 host::Machine client{ch,100,profile(),0};host::DedicatedPeer server{sh,ch,0};
 uint16_t sequence=0;bool dropped=false;unsigned events=0;uint64_t lastEvent=0;
 void tick(uint64_t now,bool dropHead=false){
  for(const auto& b:client.poll(now))server.receive(b,now);
  for(const auto& b:server.events()){
   if(b[0]==2){server.queue(host::roster_record({0,0x100,100,"HOST",{}},sh),now);server.queue(host::roster_record({id.slot,id.instance,id.character,"PC",{}},ch),now);server.queue({7,0,0,0,0,0,3},now);}
   else if(b[0]==10){server.queue(host::room_snapshot(std::array<host::Rotation,1>{{{20,1,0}}},1),now);server.queue(combat::wire::encode(combat::wire::Offer{7,id}),now);}
   else if(combat::wire::recognized(b)){auto record=combat::wire::decode(b);if(auto accept=std::get_if<combat::wire::Accept>(&record))server.queue(combat::wire::encode(combat::wire::Frame{snapshot(accept->epoch,1,0),combat::wire::Status::active,{}}),now);}
  }
  for(const auto& b:server.poll(now)){
   // After admission all packets use negotiated keys. The first missing
   // application serial exercises ACK-gap protection; HELLO is not dropped.
   if(dropHead&&!dropped){dropped=true;continue;}
   client.receive(b,now);for(const auto&e:client.combat_events()){check(e.epoch==7&&e.id==lastEvent+1,"ordered old-epoch events no gap or replay");lastEvent=e.id;++events;}
   client.receive(b,now);check(client.combat_events().empty(),"duplicate datagram cannot replay events");
  }
  check(!server.closed(),"bounded mandatory burst keeps HOST connected");check(client.result().stage!=host::Stage::protocol_error,"serial window and epoch order keep client connected");
 }
};
}
int main(){try{
 host::MandatoryBacklog q;std::vector<uint8_t> large(2000,42);
 for(uint64_t i=1;i<=q.capacity;++i)check(q.push(i,large),"exact configured record capacity fits bounded FIFO");
 check(q.bytes()==q.maxBytes&&!q.push(1025,{1}),"memory and count bounded without dropping earlier record");
 for(uint64_t i=1;i<=q.capacity;++i){auto v=q.pop();check(v.ticket==i&&v.payload==large,"FIFO ticket and body exact");}check(q.empty()&&!q.bytes(),"FIFO accounting returns zero");
 check(!q.push(1,{})&&!q.push(0,{1})&&!q.push(1,std::vector<uint8_t>(2001)),"invalid payload rejected");
 Pair p;for(uint64_t now=0;now<700;now+=10)p.tick(now);
 check(p.client.result().stage==host::Stage::joined&&p.client.result().combat_state&&p.client.result().combat_state->epoch==7,"real encrypted admission and baseline");
 const std::array<uint8_t,1> keepalive{0};for(unsigned i=0;i<24;++i)check(p.server.optional_queue(keepalive,700),"optional pressure fills24serials");
 constexpr unsigned frames=409,eventCount=frames*4;const auto endState=snapshot(7,2,eventCount);uint64_t lastTicket=0;
 for(unsigned i=0;i<frames;++i){std::vector<combat::Event> events;for(unsigned j=0;j<4;++j){combat::Event e;e.epoch=7;e.id=i*4+j+1;e.kind=combat::EventKind::impact;e.source=id;e.weapon=25;e.position={0,2,100};e.normal={0,0,-1};events.push_back(e);}lastTicket=p.server.queue(combat::wire::encode(combat::wire::Frame{endState,combat::wire::Status::active,events}),700);check(lastTicket!=0,"all409mandatoryframes retained");}
 check(p.server.mandatory_backlog()>0&&!p.server.delivery_complete(lastTicket)&&!p.server.optional_queue(keepalive,700),"unsent ticket incomplete and optional cannot overtake backlog");
 // New room epoch must follow every old-epoch event, even across serial wrap.
 auto metadata=p.server.queue(host::next_round_snapshot(2,1),700);auto offer=p.server.queue(combat::wire::encode(combat::wire::Offer{8,id}),700);check(metadata&&offer,"new epoch metadata enters same FIFO");
 p.tick(700,true);for(uint64_t now=710;now<1040;now+=10)p.tick(now);check(p.dropped&&!p.server.delivery_complete(lastTicket),"losthead retains backlog until retransmission");
 for(uint64_t now=1040;now<7000&&(!p.server.delivery_complete(offer)||!p.client.result().combat_state||p.client.result().combat_state->epoch!=8);now+=10)p.tick(now);
 check(p.events==eventCount&&p.lastEvent==eventCount,"1636old events delivered exactly once before newepoch");
 check(p.server.delivery_complete(lastTicket)&&p.server.delivery_complete(metadata)&&p.server.delivery_complete(offer)&&!p.server.mandatory_backlog(),"ACK receipts follow enqueue order across wrap");
 check(p.client.result().combat_state&&p.client.result().combat_state->epoch==8,"new epoch applies after old FIFO");
 // Overload is an explicit bounded disconnect, not silent gameplay event loss.
 host::DedicatedPeer full(sh,ch,0);for(unsigned i=0;i<32+host::MandatoryBacklog::capacity;++i)check(full.queue({0},0),"finite active plus backlog capacity");
 check(!full.queue({0},0)&&full.closed()&&full.close_reason()==host::PeerCloseReason::mandatory_overflow,"overload has explicit reason");
 std::cout<<"mandatory backlog PASS:409frames/1636events,24optionalpressure,losthead,serialwrap,old-newepoch,tickets,capacity\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
