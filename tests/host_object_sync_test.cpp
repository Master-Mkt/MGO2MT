#include "host_session.h"
#include "dedicated_peer.h"
#include <algorithm>
#include <iostream>
using namespace mgo2win;
static void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);}
struct Pair {
 host::Hello clientHello,serverHello;host::Machine client;host::DedicatedPeer server;
 uint8_t slot;bool dropped=false;bool dropFirst=false;
 Pair(uint32_t id,uint8_t s,const stage::ObjectRegistry&r,const std::vector<uint8_t>&profile,uint64_t now):
  clientHello{id,0x1000+id,2,2,{{{192,0,2,1},5730}}},serverHello{10,0x12345678,2,1,{{{192,0,2,2},5732}}},
  client(clientHello,10,profile,now,r),server(serverHello,clientHello,now),slot(s){}
 void pump(stage::SceneAuthority&authority,uint64_t now){
  for(auto&b:client.poll(now))server.receive(b,now);
  for(auto&e:server.events()){
   if(e[0]==2){server.queue(host::roster_record({0,0x100,10,"HOST",{}},serverHello),now);server.queue(host::roster_record({slot,uint16_t(0x100+slot),clientHello.character,"TEST",{}},clientHello),now);server.queue({7,0,0,0,0,0,3},now);}
   if(e[0]==10){server.objects(authority.registry(),*authority.request(),slot);server.queue(host::room_snapshot(std::array{authority.request()->rotation},authority.request()->generation),now);}
  }
  for(auto s:server.snapshot_requests())server.queue_object(*authority.snapshot(s),now);
  auto packets=server.poll(now);std::reverse(packets.begin(),packets.end());
  const host::Keys keys{clientHello.seed^serverHello.seed,clientHello.seed^serverHello.seed^host::initial_mac};
  for(auto&b:packets){
   bool object=false;try{for(const auto&m:host::decode(b,keys).messages)if((m.channel&0x7ff)==734&&!m.ack)object=true;}catch(const host::Invalid&){}
   if(dropFirst&&!dropped&&object){dropped=true;continue;}
   client.receive(b,now);client.receive(b,now); // packet duplicate cannot republish
  }
 }
};
int main(){try{
 using Update=host::ObjectStates::Update;
 stage::ObjectRegistry registry{20,{{91,1,Update::bits},{92,8,Update::bits},{93,2,Update::maximum}},true};
 stage::SceneAuthority authority(registry);host::LoadRequest request{1,1,0,0,{20,1,0},host::MatchTransition::initial};authority.begin(request);
 std::vector<uint8_t>info(579),personal(245),skills(4);info[3]=1;info[4]='P';info[5]='C';auto profile=host::profile_payload(1,info,personal,skills);
 Pair first(1,1,registry,profile,0);first.dropFirst=true;
 for(uint64_t now=0;now<1200;now+=25)first.pump(authority,now);
 check(first.dropped&&first.client.result().scene_status==stage::SceneSyncStatus::ready,"lost E0 retransmits through encrypted reliable channel");
 check(first.client.result().stage==host::Stage::joined&&first.client.result().scene->objects.size()==3,"scene receipt separate from admission");
 bool rejected=false;try{first.server.queue_object({0xe0,1,0},1200);}catch(const host::Invalid&){rejected=true;}
 check(rejected&&!first.server.closed(),"duplicate host E0 must still have exact full extent");
 for(auto[id,value]:{std::pair{91u,uint8_t(1)},std::pair{92u,uint8_t(128)},std::pair{93u,uint8_t(2)}})first.server.queue_object(*authority.update(id,value),1200);
 for(uint64_t now=1200;now<1800;now+=25)first.pump(authority,now);
 check(first.client.result().scene->objects[1].current==128&&first.client.result().scene->objects[1].initial==0,"live deltas retain silent baseline");
 Pair late(2,2,registry,profile,1800);
 for(uint64_t now=1800;now<2500;now+=25){first.pump(authority,now);late.pump(authority,now);}
 check(late.client.result().scene_status==stage::SceneSyncStatus::ready,"second receiver restores full state");
 for(size_t i=0;i<3;++i){const auto a=first.client.result().scene->objects[i],b=late.client.result().scene->objects[i];check(a.current==b.current&&b.current==b.initial,"two peers converge, late restore stays silent");}
 check(!authority.update(93,1),"stale max damage cannot resurrect");
 auto update=*authority.update(92,4);first.server.queue_object(update,2500);late.server.queue_object(update,2500);
 for(uint64_t now=2500;now<3000;now+=25){first.pump(authority,now);late.pump(authority,now);}
 check(first.client.result().scene->objects[1].current==132&&late.client.result().scene->objects[1].current==132,"one host update is shared across receivers");
 const auto old=*first.client.result().scene;
 request.sequence=2;request.generation=2;authority.begin(request);
 first.server.objects(registry,request,1);first.server.queue(host::room_snapshot(std::array{request.rotation},2),3000);
 for(uint64_t now=3000;now<3500;now+=25)first.pump(authority,now);
 check(first.client.result().scene&&first.client.result().scene->request.generation==2&&first.client.result().scene->objects[0].current==0,"new generation receives new baseline");
 check(first.client.result().scene->revision>old.revision,"published scene revision remains monotonic");
 host::Keys keys{first.clientHello.seed^first.serverHello.seed,first.clientHello.seed^first.serverHello.seed^host::initial_mac};
 host::Message fast;fast.channel=735;fast.reliable=false;fast.payload={1,8};
 first.client.receive(host::encode({1000,{fast}},keys),3500);
 check(first.client.result().scene->objects[1].current==8,"original 735 unreliable state path accepted for current generation");
 const auto fastRevision=first.client.result().scene->revision;
 fast.channel|=0x800;fast.payload={1,16};first.client.receive(host::encode({1001,{fast}},keys),3501);
 check(first.client.result().scene->revision==fastRevision&&first.client.result().scene->objects[1].current==8,"old generation parity cannot alter scene");
 auto unverified=registry;unverified.complete=false;stage::SceneAuthority blocked(unverified);blocked.begin(request);check(!blocked.snapshot(1),"candidate host registry cannot fabricate E0");
 std::cout<<"encrypted object snapshot/delta, loss, reordering, two receivers and round reset passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
