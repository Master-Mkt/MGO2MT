#include "combat_service.h"
#include "combat_wire_budget.h"
#include "host_session.h"
#include "dedicated_peer.h"
#include "world_inventory_host.h"
#include <iostream>
#include <set>
#include <limits>
using namespace mgo2win;
namespace {
void check(bool ok,const char*s){if(!ok)throw std::runtime_error(s);}
std::shared_ptr<const stage::Collision> floor(){return std::make_shared<const stage::Collision>(stage::Collision::make({{-20000,0,-20000},{20000,0,-20000},{20000,0,20000},{-20000,0,20000}},{{{0,1,2}},{{0,2,3}}}));}
std::vector<uint8_t> profile(){std::vector<uint8_t>b(45);b[0]=2;b.insert(b.end(),{'P','l','a','y','e','r',0});return b;}
}
int main(){try{
 // Synthetic fixture values; this test is not a shipped weapon/stat profile.
 combat::Weapon gun{23,35,0,100,300,3,4,10000,9001,9002,true};combat::Service service(1);service.configure(floor(),std::array{gun});
 host::Hello owner{100,0x12345678,2,1,{{{192,0,2,1},5732}}};
 std::array<host::Hello,2>hello{{{200,0xabcdef01,2,2,{{{192,0,2,2},5730}}},{300,0x11223344,2,2,{{{192,0,2,3},5730}}}}};
 std::array<host::Machine,2>client{{{hello[0],100,profile(),0},{hello[1],100,profile(),0}}};
 std::array<host::DedicatedPeer,2>server{{{owner,hello[0],0},{owner,hello[1],0}}};
 std::array<host::Player,2>roster{{{1,0x101,200,"Player",{}},{2,0x102,300,"Player",{}}}};
 std::array<combat::Identity,2>ids{{{1,0x101,200},{2,0x102,300}}};
 std::array<bool,2>admitted{},synced{};std::array<std::set<uint64_t>,2>seen;std::array<unsigned,2>shots{},sounds{};
 bool spawned=false,dropped=false;unsigned clientPackets=0,serverPackets=0;
 for(uint64_t now=0;now<3500;now+=10){
  if(now==600){for(unsigned i=0;i<2;++i){combat::Pose p;p.feet={0,2,float(i*3000)};check(service.authority().join(ids[i],uint8_t(i+1),p,100,100,std::array<uint16_t,1>{23},now),"only host grants real simulation identities, poses and inventory");}service.authority().active(true);spawned=true;}
  if(now==1000||now==1300||now==1600){combat::wire::Input in{1,uint32_t(now),{{0,2,0}},23,true,false};check(client[0].combat_input(in,now),"native client submits trigger only after host handshake and spawn");}
  for(unsigned i=0;i<2;++i){
   for(auto&packet:client[i].poll(now))if(++clientPackets%17!=0)server[i].receive(packet,now);
   for(auto&e:server[i].events()){
    if(e[0]==2&&!admitted[i]){admitted[i]=true;server[i].queue(host::roster_record({0,0x100,100,"Host",{}},owner),now);for(unsigned j=0;j<2;++j)if(admitted[j])server[i].queue(host::roster_record(roster[j],hello[j]),now);server[i].queue({7,0,0,0,0,0,3},now);for(unsigned j=0;j<2;++j)if(i!=j&&admitted[j])server[j].queue(host::roster_record(roster[i],hello[i]),now);}
    else if(e[0]==10&&!synced[i]){synced[i]=true;server[i].queue(host::room_snapshot(std::array<host::Rotation,1>{{{20,1,0}}},1),now);check(service.admit(ids[i]),"combat offered only to admitted session");}
    else if(combat::wire::recognized(e))service.receive(ids[i],e,now);
   }
  }
  service.poll(now);for(auto&d:service.deliveries()){unsigned i=d.recipient.character==200?0:1;server[i].queue(std::move(d.payload),now);}
  for(unsigned i=0;i<2;++i){auto packets=server[i].poll(now);for(auto it=packets.rbegin();it!=packets.rend();++it){
   if(i==1&&now>=1000&&!dropped){dropped=true;continue;}if(++serverPackets%19==0)continue;client[i].receive(*it,now);client[i].receive(*it,now);
  }for(auto&e:client[i].combat_events()){check(seen[i].insert(e.id).second,"reliable retry cannot replay an effect");if(e.kind==combat::EventKind::shot)++shots[i];if(e.cue)++sounds[i];}}
 }
 check(spawned&&dropped&&shots[0]==3&&shots[1]==3&&sounds[0]==6&&sounds[1]==6,"two encrypted sessions receive exactly the same three shots and six sound cues despite loss");
 for(auto&c:client){auto s=c.result();check(s.stage==host::Stage::joined&&s.combat_state&&s.combat_state->players[2]->hp==0&&s.combat_state->players[1]->ammo==0,"both participant states converge to host death and ammunition");}
 check(service.authority().snapshot().players[2]->hp==0,"host owns lethal result");
 auto before=service.authority().snapshot();combat::wire::Input fake{1,5000,{{0,2,0}},999,true,false};check(!service.receive({1,0x999,200},combat::wire::encode(fake),4000),"forged incarnation rejected before authority");check(service.authority().snapshot()==before,"forged identity has no mutation");
 auto initial=before;combat::Replica late;check(late.snapshot(initial),"late join gets current HP snapshot");combat::Event old;old.epoch=1;old.id=initial.eventWatermark;old.source=ids[0];check(late.events(std::array{old}).empty(),"late join never plays historical shot sound");
 // Wire round-trip, exact extents, finite values and full-roster size boundary.
 combat::wire::Frame full;full.snapshot=before;full.status=combat::wire::Status::active;
 auto p=*before.players[1];for(unsigned i=0;i<24;++i){p.identity={uint8_t(i),uint16_t(i+1),1000+i};full.snapshot.players[i]=p;}
 full.snapshot.eventWatermark=4;for(unsigned i=0;i<4;++i){combat::Event e;e.epoch=full.snapshot.epoch;e.id=i+1;e.kind=combat::EventKind::shot;e.source=full.snapshot.players[i]->identity;e.weapon=25;full.events.push_back(e);}
 // Derive the current roster capacity from exact encoded base and event
 // extents; every retained event and the encrypted envelope must round-trip.
 const auto budget=combat_test::budget(full);full.events.resize(budget.capacity);
 auto largest=combat::wire::encode(full);host::Packet envelope;envelope.messages.push_back({1,true,false,false,0,largest});auto datagram=host::encode(envelope);
 check(largest.size()==budget.fullBytes&&datagram.size()<=host::max_datagram&&host::decode(datagram).messages[0].payload==largest,"full current roster and maximum complete event chunk fit encrypted datagram");
 auto smaller=full;smaller.snapshot.players[23].reset();const auto smallerBudget=combat_test::budget(smaller);check(smallerBudget.capacity>=budget.capacity&&smallerBudget.baseBytes<budget.baseBytes,"fewer players cannot reduce event capacity");
 auto obsolete=largest;obsolete[5]=5;check(!combat::wire::recognized(obsolete),"obsolete v5 cannot be interpreted as the current native version");
 std::cout<<"full_roster_events="<<budget.capacity<<" record_bytes="<<largest.size()<<" datagram_bytes="<<datagram.size()<<'\n';
 for(combat::wire::Record r:std::array<combat::wire::Record,4>{combat::wire::Offer{1,ids[0]},combat::wire::Accept{1},fake,full}){auto bytes=combat::wire::encode(r);check(combat::wire::decode(bytes)==r,"all record kinds round-trip");for(size_t n=0;n<bytes.size();++n){bool bad=false;try{combat::wire::decode(std::span(bytes).first(n));}catch(const combat::wire::Invalid&){bad=true;}check(bad,"all truncated records rejected");}bytes.push_back(0);bool bad=false;try{combat::wire::decode(bytes);}catch(const combat::wire::Invalid&){bad=true;}check(bad,"trailing bytes rejected");}
 fake.pose.feet[0]=std::numeric_limits<float>::infinity();bool bad=false;try{combat::wire::encode(fake);}catch(const combat::wire::Invalid&){bad=true;}check(bad,"nonfinite request rejected");
 combat::Service waiting(1);check(waiting.admit(ids[0]),"world may be waiting during room admission");waiting.deliveries();check(waiting.receive(ids[0],combat::wire::encode(combat::wire::Accept{1}),0),"capability handshake does not require a fake spawn");auto d=waiting.deliveries();check(std::get<combat::wire::Frame>(combat::wire::decode(d[0].payload)).status==combat::wire::Status::awaiting_world,"missing world explicitly remains unavailable");
 // A reliable gap releases several inputs in one host tick. Preserve the
 // button action and newest pose, without a catch-up burst of extra shots.
 combat::Service burst(1);burst.configure(floor(),std::array{gun});for(unsigned i=0;i<2;++i){check(burst.admit(ids[i]),"burst identity");combat::Pose burstPose;burstPose.feet={0,2,float(i*3000)};check(burst.authority().join(ids[i],uint8_t(i+1),burstPose,100,100,std::array<uint16_t,1>{23},0),"burst host grant");check(burst.receive(ids[i],combat::wire::encode(combat::wire::Accept{1}),0),"burst handshake");}burst.authority().active(true);burst.deliveries();
 combat::wire::Input move{1,1,{{0,2,100}},23,false,false},trigger{1,2,{{0,2,200}},23,true,false,true},latest{1,3,{{0,2,300}},23,false,false};
 check(burst.receive(ids[0],combat::wire::encode(move),100)&&burst.receive(ids[0],combat::wire::encode(trigger),100)&&burst.receive(ids[0],combat::wire::encode(latest),100),"same-tick move/fire/move accepted without receive-interval loss");
 check(!burst.receive(ids[0],combat::wire::encode(trigger),100),"burst replay rejected");burst.poll(100);auto state=burst.authority().snapshot();check(state.players[1]->pose.feet[2]==300&&state.players[1]->ammo==2&&state.players[2]->hp==65,"host applies latest allowed pose and exactly one queued trigger");
 burst.poll(101);check(burst.authority().snapshot().players[1]->ammo==2,"consumed trigger does not repeat next tick");
 trigger.sequence=4;latest.sequence=5;latest.reload=true;check(burst.receive(ids[0],combat::wire::encode(trigger),300)&&burst.receive(ids[0],combat::wire::encode(latest),300),"fire and reload coalesce");burst.poll(300);check(burst.authority().snapshot().players[1]->reloadUntil==600&&burst.authority().snapshot().players[2]->hp==65,"reload wins over same-tick fire");burst.deliveries();
 auto secondGun=gun;secondGun.id=7;combat::Service changing(1);changing.configure(floor(),std::array{gun,secondGun});check(changing.admit(ids[0]),"changing identity");check(changing.authority().join(ids[0],1,combat::Pose{{0,2,0}},100,100,std::array<uint16_t,2>{23,7},0),"both weapons explicitly granted");changing.authority().active(true);check(changing.receive(ids[0],combat::wire::encode(combat::wire::Accept{1}),0),"changing handshake");trigger.sequence=1;trigger.pose.feet={0,2,0};latest.sequence=2;latest.pose.feet={0,2,0};latest.weapon=7;latest.reload=false;
 check(changing.receive(ids[0],combat::wire::encode(trigger),100)&&changing.receive(ids[0],combat::wire::encode(latest),100),"new observed weapon pose follows earlier trigger");changing.poll(100);check(changing.authority().snapshot().players[1]->weapon==23&&changing.authority().snapshot().players[1]->ammo==3&&changing.authority().snapshot().eventWatermark==0,"observed Input weapon neither equips nor transfers old trigger");
 // Selection now goes through the authenticated, revision-checked inventory
 // command, not the last observed weapon ID in an ordinary movement packet.
 items::HostSession equipment;auto holdings=changing.authority().item_held(ids[0],777);check(holdings.has_value(),"real owned-slot revisions");
 check(equipment.receive(changing.authority(),ids[0],*items::wire::encode(items::wire::Probe{holdings->header}),101),"inventory capability handshake");
 trigger.sequence=3;latest.sequence=4;check(changing.receive(ids[0],combat::wire::encode(trigger),110),"old weapon press queued before equip");
 items::wire::Command select;select.header=holdings->header;select.header.sequence=1;select.action=items::wire::Action::equip;select.heldSlot=1;select.heldRevision=holdings->slots[1].revision;
 check(equipment.receive(changing.authority(),ids[0],*items::wire::encode(select),110)&&changing.authority().snapshot().players[1]->weapon==7,"HOST explicitly accepts owned second weapon");
 check(changing.receive(ids[0],combat::wire::encode(latest),110),"new weapon released input follows equip");changing.poll(110);check(changing.authority().snapshot().players[1]->weapon==7&&changing.authority().snapshot().players[1]->ammo==3&&changing.authority().snapshot().eventWatermark==0,"approved weapon change cancels earlier weapon's pending trigger");
 changing.poll(111);check(changing.authority().snapshot().eventWatermark==0,"cancelled edge cannot replay on next tick");
 latest.sequence=5;latest.fire=latest.firePressed=true;check(changing.receive(ids[0],combat::wire::encode(latest),120),"fresh second-weapon press");changing.poll(120);check(changing.authority().snapshot().players[1]->ammo==2&&changing.authority().snapshot().eventWatermark==1,"new weapon remains usable through a fresh explicit press");
 // Fill the participant's eight reliable slots before allowing any ACKs.
 // A reload followed by movement must survive in the one coalesced pending slot.
 for(unsigned i=0;i<10;++i){combat::wire::Input in{1,6000+i,{{0,2,float(i*10)}},23,false,i==8};check(client[0].combat_input(in,3500),"congested participant retains latest pose and reload without dropping input");}
 for(uint64_t now=3500;now<4700;now+=10){
  for(unsigned i=0;i<2;++i){for(auto&packet:client[i].poll(now))server[i].receive(packet,now);for(auto&e:server[i].events())if(combat::wire::recognized(e))service.receive(ids[i],e,now);}
  service.poll(now);for(auto&delivery:service.deliveries())server[delivery.recipient.character==200?0:1].queue(std::move(delivery.payload),now);
  for(unsigned i=0;i<2;++i){for(auto&packet:server[i].poll(now))client[i].receive(packet,now);client[i].combat_events();}
 }
 auto resumed=service.authority().snapshot();check(resumed.players[1]->ammo==3&&resumed.players[1]->reserve==1&&resumed.players[1]->pose.feet[2]==90,"ACK release sends exactly one retained reload and newest pose without a new button press");
 check(client[0].result().combat_state==service.authority().snapshot(),"congested client converges after retained reload");
 // A short trigger press behind eight outstanding reliable records must not
 // arrive at the host as a permanently held automatic trigger.
 for(unsigned i=0;i<10;++i){combat::wire::Input in{1,7000+i,{{0,2,90}},23,i==8,false,i==8};check(client[0].combat_input(in,4700),"congested sender accepts short press and release");}
 for(uint64_t now=4700;now<5600;now+=10){
  for(unsigned i=0;i<2;++i){for(auto&packet:client[i].poll(now))server[i].receive(packet,now);for(auto&e:server[i].events())if(combat::wire::recognized(e))service.receive(ids[i],e,now);}
  service.poll(now);for(auto&delivery:service.deliveries())server[delivery.recipient.character==200?0:1].queue(std::move(delivery.payload),now);
  for(unsigned i=0;i<2;++i){for(auto&packet:server[i].poll(now))client[i].receive(packet,now);client[i].combat_events();}
 }
 check(service.authority().snapshot().players[1]->ammo==2&&service.authority().snapshot().players[1]->reserve==1,"sender congestion retains exactly one tap with released trigger");
 check(client[0].result().combat_state==service.authority().snapshot(),"released trigger and one consumed bullet converge after congestion");
 std::cout<<"combat: two real encrypted protocol machines, packet loss/reorder/duplicate, coalesced move/fire bursts, sender congestion/reload retention, host HP/ammo, six identical SE cues, invalid identities and bounded wire passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
