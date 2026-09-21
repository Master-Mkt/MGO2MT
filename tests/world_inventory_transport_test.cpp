#include "host_session.h"
#include "dedicated_peer.h"
#include "world_inventory_session.h"
#include "world_inventory_host.h"
#include "combat_initial_profile.h"
#include <iostream>
#include <deque>
using namespace mgo2mt;
namespace {
void check(bool v,const char*why){if(!v)throw std::runtime_error(why);}
const host::Hello serverHello{100,0x12345678,2,1,{{{192,0,2,1},5740}}};
const host::Hello clientHello{200,0xabcdef01,2,2,{{{192,0,2,2},5730}}};
const combat::Identity id{1,0x101,200};
std::vector<uint8_t> profile(){std::vector<uint8_t> p(45);p[0]=2;p.insert(p.end(),{'P',0});return p;}
struct Pair {
 host::Machine client{clientHello,100,profile(),0};host::DedicatedPeer server{serverHello,clientHello,0};
 items::ClientSession session;items::HostSession inventory;combat::Authority authority;
 items::ClientContext context{{7,1},{1,0x101,200,1},{0,2,0,0},true};
 uint64_t token=777;unsigned commands=0,probes=0,loss=0;size_t largestDatagram=0;
 Pair(){auto floor=std::make_shared<const stage::Collision>(stage::Collision::make({{-20000,0,-20000},{20000,0,-20000},{20000,0,20000},{-20000,0,20000}},{{{0,1,2}},{{0,2,3}}}));authority.begin(7,floor,combat::initial_profiles(20,1,0));check(authority.configure_items(1,{4096,4096}),"large explicit item capacity");combat::Pose p;p.feet={0,2,0};check(authority.join(id,1,p,1000,1000,std::array<uint16_t,1>{25},0),"native actor");authority.active(true);}
 void tick(uint64_t now,bool dropOne=false){
  for(auto& body:session.pump(context,now,token,client.radio_writable(),client.inventory_messages()))check(client.inventory_send(body,now),"client native optional writable");
  for(const auto& p:client.poll(now))server.receive(p,now);
  for(const auto& event:server.events()){
   if(items::wire::recognized(event)){auto decoded=items::wire::decode(event);check(decoded.has_value(),"wire decoded after encrypted transport");probes+=std::holds_alternative<items::wire::Probe>(*decoded);commands+=std::holds_alternative<items::wire::Command>(*decoded);inventory.receive(authority,id,event,now);}
   else if(event[0]==2){server.queue(host::roster_record({0,0x100,100,"HOST",{}},serverHello),now);server.queue(host::roster_record({id.slot,id.instance,id.character,"PC",{}},clientHello),now);server.queue({7,0,0,0,0,0,3},now);}
   else if(event[0]==10){server.queue(host::room_snapshot(std::array<host::Rotation,1>{{{20,1,0}}},1),now);server.queue(combat::wire::encode(combat::wire::Offer{7,id}),now);}
  }
  inventory.poll(authority,now);if(auto b=inventory.front(id);b&&server.optional_queue(*b,now))inventory.pop(id);
  for(const auto& p:server.poll(now)){largestDatagram=std::max(largestDatagram,p.size());if(dropOne){dropOne=false;++loss;continue;}client.receive(p,now);client.receive(p,now);}
 }
};
}
int main(){try{
 Pair p;for(uint64_t now=0;now<700;now+=10)p.tick(now,now==150);
 check(p.client.result().stage==host::Stage::joined&&p.session.state().status==items::ClientStatus::ready&&p.session.state().held&&p.session.state().world,"real encrypted admission and optional inventory handshake");
 check(p.probes==1&&p.commands==0,"outer retransmission cannot duplicate application probe");
 auto held=*p.session.state().held;check(held.slots[0].contents.magazine==30,"actual Authority holdings");
 // One bounded full page exceeds the previous 1024-byte optional limit.
 const auto pageRevision=p.session.state().world->revision+1;
 items::SnapshotState snapshot{p.context.scope,pageRevision,{4096,4096},{}};
 for(uint64_t i=1;i<=529;++i)snapshot.entities.push_back({{snapshot.scope,i},1,items::PlacementKind::dropped,p.context.actor,{25,1,30,90,0,items::Resource::ammunition},{float(i),2,3,0}});
 auto pages=items::wire::pages(snapshot,held.header);check(pages&&pages->size()==41,"capacity extension requires more pages than inbox and pending limits");
 auto full=*items::wire::encode(pages->front());check(full.size()==1124,"actual full 13-row page1124");
 std::deque<std::vector<uint8_t>> outgoing;for(const auto& page:*pages)outgoing.push_back(*items::wire::encode(page));
 unsigned queued=0;while(!outgoing.empty()&&p.server.optional_queue(outgoing.front(),700)){outgoing.pop_front();++queued;}
 check(queued==24&&!outgoing.empty()&&!p.server.closed(),"24optional slots apply backpressure without closing");
 check(p.server.queue({0},700)!=0,"room traffic reserve survives page pressure");
 uint64_t now=700;for(;now<3000&&(!outgoing.empty()||!p.session.state().world||p.session.state().world->revision!=pageRevision);now+=10){
  if(!outgoing.empty()&&p.server.optional_queue(outgoing.front(),now))outgoing.pop_front();
  p.tick(now,now==700);
 }
 p.tick(now);check(outgoing.empty()&&p.session.state().world&&p.session.state().world->revision==pageRevision&&p.session.state().world->entities.size()==529,"all pages atomically delivered through reliable encrypted queue with packet loss");
 check(p.largestDatagram>1124&&p.largestDatagram<=host::max_datagram,"encrypted1124page fits original2048 datagram limit");
 check(p.loss>=1,"deterministic first-page loss exercised reliable retransmission");
 check(p.client.result().stage==host::Stage::joined&&!p.server.closed(),"paging leaves room alive");
 // Same revision and stale identity/token packets are syntactically valid but
 // cannot rewrite the current replica after passing through the real transport.
 auto conflict=held;conflict.slots[0].contents.magazine=29;check(p.server.optional_queue(*items::wire::encode(conflict),now)!=0,"inject contradictory held");
 auto selectedConflict=held;selectedConflict.selectedSlot=255;check(p.server.optional_queue(*items::wire::encode(selectedConflict),now)!=0,"inject same revision selection rewrite");
 for(unsigned variant=0;variant<5;++variant){auto bad=held;bad.slots[0].revision++;bad.slots[0].contents.magazine=28;
  if(variant==0)bad.header.token++;if(variant==1)bad.header.actor.life++;if(variant==2)bad.header.actor.instance++;if(variant==3)bad.header.actor.character++;if(variant==4)bad.header.scope.epoch++;
  check(p.server.optional_queue(*items::wire::encode(bad),now)!=0,"inject identity-bound old holdings");
 }
 for(unsigned i=0;i<20;++i){now+=10;p.tick(now);}check(p.session.state().held->slots[0].contents.magazine==30&&p.session.state().held->selectedSlot==held.selectedSlot,"same revision/identity/token cannot replace held state or selected slot");
 auto malformed=full;malformed.push_back(0);check(!items::wire::decode(malformed),"1173byte page rejected by codec");
 std::vector<uint8_t> tooLarge(2001,0xec);check(!p.server.optional_queue(tooLarge,now)&&!p.server.closed(),"2001byte optional payload rejected without close");
 // A real request accepted by the authority mutates once; replay, wrong token,
 // wrong life and wrong admitted actor cannot mint another ground entity.
 items::wire::Command drop;drop.header=held.header;drop.header.sequence=1;drop.action=items::wire::Action::drop;drop.heldSlot=0;drop.heldRevision=held.slots[0].revision;
 check(p.authority.pose(id,7,1,{{0,2,0},0,0},now,1)==combat::Reject::none,"fresh host pose for mutation");
 const auto body=*items::wire::encode(drop);check(p.inventory.receive(p.authority,id,body,now),"host real drop command");auto count=p.authority.item_state().entities.size();check(count==1,"one actual ground entity");
 check(p.inventory.receive(p.authority,id,body,now)&&p.authority.item_state().entities.size()==count,"replayed sequence cannot mutate twice");
 auto wrong=drop;wrong.header.sequence=2;wrong.header.token++;check(!p.inventory.receive(p.authority,id,*items::wire::encode(wrong),now),"host token mismatch");wrong=drop;wrong.header.actor.life++;check(!p.inventory.receive(p.authority,id,*items::wire::encode(wrong),now),"host stale life mismatch");
 check(!p.inventory.receive(p.authority,{id.slot,uint16_t(id.instance+1),id.character},body,now),"admitted identity mismatch");
 p.session.disconnect();check(!p.session.state().held&&!p.session.state().world,"disconnect clears all replicas");
 // Populate the actual Authority, not just synthetic wire pages, so its real
 // per-peer snapshot queue cannot silently swallow a valid concurrent command.
 Pair loaded;for(uint64_t at=0;at<700;at+=10)loaded.tick(at);
 for(uint32_t i=0;i<529;++i){
  combat::Identity seed{2,uint16_t(500+i),10000+i};combat::Pose pose;pose.feet={5000,2,0};
  check(loaded.authority.join(seed,1,pose,1000,1000,std::array<uint16_t,1>{25},2000),"seed actor admitted");
  auto inventory=loaded.authority.item_held(seed,999);check(inventory.has_value(),"seed actual holding");
  items::wire::Command command;command.header=inventory->header;command.header.sequence=1;command.action=items::wire::Action::drop;command.heldSlot=0;command.heldRevision=inventory->slots[0].revision;
  check(bool(loaded.authority.item_action(seed,command,2000)),"seed real world entity");check(loaded.authority.leave(seed),"seed actor exits without deleting world item");
 }
 check(loaded.authority.item_state().entities.size()==529,"actual expanded world queue");loaded.inventory.poll(loaded.authority,2000);
 check(loaded.inventory.front(id)&&loaded.inventory.front(id)->size()==1124,"actual paged snapshot pending");
 check(loaded.authority.pose(id,7,1,{{0,2,0},0,0},2000,1)==combat::Reject::none,"fresh concurrent command pose");
 check(loaded.session.submit(items::wire::Action::drop,0),"UI command while 34 pages remain pending");
 for(uint64_t at=2000;at<2100;at+=10)loaded.tick(at);
 check(loaded.commands==1&&loaded.session.state().delivery==items::Delivery::confirmed&&loaded.authority.item_state().entities.size()==530,"reply priority confirms drop before snapshot backlog drains");
 combat::wire::Frame unarmed;unarmed.status=combat::wire::Status::active;unarmed.snapshot=loaded.authority.snapshot();
 check(unarmed.snapshot.players[id.slot]&&!unarmed.snapshot.players[id.slot]->weapon&&!unarmed.snapshot.players[id.slot]->ammo&&!unarmed.snapshot.players[id.slot]->reserve,"real drop creates unarmed authoritative frame");
 check(loaded.server.queue(combat::wire::encode(unarmed),2100)!=0,"unarmed GWCB frame uses actual encrypted transport");
 for(uint64_t at=2100;at<4000;at+=10)loaded.tick(at);
 check(loaded.session.state().world&&loaded.session.state().world->entities.size()==530&&loaded.session.state().held->slots[0].contents.item==0,"latest full world converges after priority ACK");
 const auto replica=loaded.client.result().combat_state;check(replica&&replica->players[id.slot]&&replica->players[id.slot]->identity==id&&replica->players[id.slot]->weapon==0&&replica->players[id.slot]->ammo==0&&replica->players[id.slot]->reserve==0,"Machine replica accepts valid unarmed GWCB self after drop");
 std::cout<<"world_inventory_transport_test PASS: encrypted1124pages,41page backpressure/loss/duplicates, held binding, authoritative replay and2000 ceiling; largest="<<p.largestDatagram<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

