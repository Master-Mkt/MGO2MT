#include "dedicated_peer.h"
#include "host_session.h"
#include "host_briefing.h"
#include "host_rules.h"
#include <iostream>
using namespace mgo2win::host;
static void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(){try{
 Hello host{100,0x12345678,2,1,{{{192,0,2,1},5740}}},client{200,0xabcdef01,2,2,{{{192,0,2,2},5730}}};
 std::vector<uint8_t>profile(45);profile[0]=2;profile.insert(profile.end(),{'P','l','a','y','e','r',0});
 auto names=profile_names(profile);check(names.name=="Player"&&names.clan.empty(),"variable profile name");
 Machine joined(client,host.character,profile,0);DedicatedPeer server(host,client,0);bool admitted=false,synchronized=false;unsigned profiles=0;
 for(uint64_t now=0;now<2000;now+=10){
  for(auto&b:joined.poll(now))server.receive(b,now);
  for(auto&e:server.events()){
   if(e[0]==2){++profiles;admitted=true;server.queue(roster_record({0,0x100,100,"Host",{}},host),now);server.queue(roster_record({1,0x101,200,names.name,names.clan},client),now);server.queue({7,0,0,0,0,0,3},now);}
   if(e[0]==10){check(admitted,"sync cannot precede accepted profile");synchronized=true;server.queue(room_snapshot(std::array<Rotation,1>{{{20,1,2}}},1),now);}
  }
  auto sends=server.poll(now);for(auto it=sends.rbegin();it!=sends.rend();++it){joined.receive(*it,now);joined.receive(*it,now);} // reversed delivery and packet duplicates
 }
 auto result=joined.result();check(!server.closed()&&result.stage==Stage::joined&&profiles==1&&synchronized,"native client and dedicated host admission with reordered handshake");
 check(result.roster.count()==2&&result.roster.complete&&result.match.request&&result.match.request->rotation==Rotation{20,1,2},"roster identity and DP map request");
 check(!result.match.phase,"room admission does not invent gameplay phase");
 auto leave=joined.leave_packet();check(leave.has_value(),"native client exit");server.receive(*leave,2010);auto events=server.events();check(events.size()==1&&events[0]==std::vector<uint8_t>{1},"host observes explicit exit");
 server.poll(12010);check(server.closed(),"silent peer expires");
 for(auto bad:{std::vector<uint8_t>{2},std::vector<uint8_t>(373,0)}){bool caught=false;try{profile_names(bad);}catch(const Invalid&){caught=true;}check(caught,"malformed profile bounded");}
 auto changed=profile;changed[39]=128;bool caught=false;try{profile_names(changed);}catch(const Invalid&){caught=true;}check(caught,"invalid skill cannot shift profile offsets");
 auto old=result.roster;update_roster(old,roster_remove(0x101));check(old.count()==1&&old.slots[0]->character==100,"full instance removal");
 DedicatedPeer invalid(host,client,0);Keys keys{host.seed^client.seed,host.seed^client.seed^initial_mac};Message good;good.channel=1;good.payload=profile;Message bad=good;bad.serial=1;bad.payload={2};invalid.receive(encode({0,{good,bad}},keys),1);check(invalid.closed()&&invalid.events().empty(),"failed packet cannot leak an earlier admission event");
 for(auto payload:{std::vector<uint8_t>{10},std::vector<uint8_t>{1,0},std::vector<uint8_t>{0,0}}){DedicatedPeer malformed(host,client,0);good.payload=payload;malformed.receive(encode({0,{good}},keys),1);check(malformed.closed()&&malformed.events().empty(),"premature sync or oversized control rejected");}
 DedicatedPeer stalled(host,client,0);good.payload={0};for(unsigned i=0;i<8;++i){good.serial=uint8_t(i);stalled.receive(encode({uint16_t(i),{good,{0,true,true,false,0,{}}}},keys),i*1000);stalled.events();stalled.poll(i*1000);}stalled.poll(8000);check(stalled.closed(),"keepalives cannot reserve a slot without profile forever");
 // A stalled recipient can retain 32 active wire records plus the bounded
 // unsent FIFO. Keep another recipient sending/ACKing through serial wrap
 // while filling that capacity; overflowing one must not affect the other.
 DedicatedPeer slow(host,client,0),healthy(host,client,0);
 constexpr size_t slowCapacity=32+MandatoryBacklog::capacity;
 auto healthyRoundTrip=[&](size_t i){
  auto ticket=healthy.queue({0},i+1);check(ticket&&!healthy.closed(),"healthy recipient accepts its own record");
  check(!healthy.poll(i+1).empty(),"healthy recipient sends before receipt");
  healthy.receive(encode({uint16_t(i),{{1,true,true,false,uint8_t(i),{}}}},keys),i+1);
  check(healthy.delivery_complete(ticket)&&healthy.mandatory_backlog()==0,"healthy recipient ACKs independently through serial wrap");
 };
 for(size_t i=0;i<slowCapacity;++i){check(slow.queue({0},1)&&!slow.closed(),"slow recipient retains active and unsent capacity");healthyRoundTrip(i);}
 check(slow.mandatory_backlog()==MandatoryBacklog::capacity,"slow recipient reaches exact unsent capacity");
 check(!slow.queue({0},1)&&slow.closed()&&slow.close_reason()==PeerCloseReason::mandatory_overflow,"slow recipient overflows only its own peer queue");
 healthyRoundTrip(slowCapacity);check(!healthy.closed(),"healthy recipient remains usable after another recipient overflows");
 DedicatedPeer delivery(host,client,0);auto firstTicket=delivery.queue({0},0),snapshotTicket=delivery.queue({0},0);
 check(!delivery.delivery_complete(0)&&!delivery.delivery_complete(snapshotTicket),"queuing a snapshot does not establish receipt");
 delivery.receive(encode({0,{{1,true,true,false,1,{}}}},keys),1);
 delivery.poll(1);
 check(!delivery.delivery_complete(snapshotTicket),"ACK for a never-sent record does not complete it");
 delivery.receive(encode({1,{{1,true,true,false,1,{}}}},keys),2);
 check(!delivery.delivery_complete(firstTicket)&&!delivery.delivery_complete(snapshotTicket),"later ACK cannot claim preceding reliable messages were delivered");
 delivery.receive(encode({2,{{1,true,true,false,0,{}}}},keys),3);
 check(delivery.delivery_complete(firstTicket)&&delivery.delivery_complete(snapshotTicket),"contiguous ACKs establish ordered application delivery");
 auto unsentTicket=delivery.queue({0},3);check(unsentTicket>snapshotTicket&&!delivery.delivery_complete(unsentTicket),"later queued record stays pending");
 delivery.close();check(!delivery.delivery_complete(snapshotTicket),"lost session invalidates previously delivered preparation");
 // Actual reliable transport must confirm metadata before a native timer can
 // publish the verified rule-preparation phase. This is not a scene-ready ACK.
 Machine waitingClient(client,host.character,profile,0);DedicatedPeer waitingHost(host,client,0);
 RoundRules briefing({200,1,false});ParticipantToken participant{1,0x101,client.character};
 uint64_t metadataTicket=0;bool committed=false,blockedAtDeadline=false;
 for(uint64_t now=0;now<3000;now+=10){
  // Withhold client ACKs after initial sync until well past the deadline.
  if(!metadataTicket||now>=1500)for(auto&b:waitingClient.poll(now))waitingHost.receive(b,now);
  for(auto&e:waitingHost.events()){
   if(e[0]==2){check(briefing.join(participant,ParticipantRole::player,now),"briefing participant admitted once");waitingHost.queue(roster_record({0,0x100,100,"Host",{}},host),now);waitingHost.queue(roster_record({1,0x101,200,"Player",{}},client),now);waitingHost.queue({7,0,0,0,0,0,3},now);}
   else if(e[0]==10){waitingHost.queue(room_snapshot(std::array<Rotation,1>{{{20,1,0}}},1),now);metadataTicket=waitingHost.queue(phase_update(0),now);}
  }
  if(waitingHost.delivery_complete(metadataTicket))briefing.set_prepared(participant,briefing.generation(),true);
  if(briefing.advance(now)!=StartReason::none){check(!committed,"preparation is published once");committed=true;waitingHost.queue(phase_update(2),now);}
  for(auto&b:waitingHost.poll(now))waitingClient.receive(b,now);
  if(now==1400){blockedAtDeadline=true;check(metadataTicket&&!committed&&waitingClient.result().match.phase==0,"expired timer cannot bypass missing reliable receipt");}
 }
 check(blockedAtDeadline&&committed&&!waitingHost.closed()&&waitingClient.result().stage==Stage::joined&&waitingClient.result().match.phase==2,"delayed metadata ACK advances preparation without leaving admission or publishing combat");
 // Run both sides beyond the 8-bit application serial wrap, dropping packets
 // in each direction. ACK loss must lead to retransmission, not re-admission.
 Machine lossyClient(client,host.character,profile,0);DedicatedPeer lossyHost(host,client,0);unsigned clientPackets=0,hostPackets=0,accepted=0,keepalives=0;
 for(uint64_t now=0;now<=620000;now+=100){
  for(auto&b:lossyClient.poll(now))if(++clientPackets%7!=1)lossyHost.receive(b,now);
  for(auto&e:lossyHost.events()){if(e[0]==2){++accepted;lossyHost.queue(roster_record({0,0x100,100,"Host",{}},host),now);lossyHost.queue(roster_record({1,0x101,200,"Player",{}},client),now);lossyHost.queue({7,0,0,0,0,0,3},now);}else if(e[0]==10)lossyHost.queue(room_snapshot(std::array<Rotation,1>{{{20,1,0}}},1),now);else if(e[0]==0)++keepalives;}
  for(auto&b:lossyHost.poll(now))if(++hostPackets%5!=1)lossyClient.receive(b,now);
 }
 check(!lossyHost.closed()&&lossyClient.result().stage==Stage::joined&&accepted==1&&keepalives>256,"loss, retries and application serial wrap preserve admitted session");
 std::cout<<"dedicated/client handshake, reliable ordering, roster, map/DP, leave and bounds passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
