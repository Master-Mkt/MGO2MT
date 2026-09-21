#include "combat_initial_profile.h"
#include "combat_service.h"
#include "combat_wire_budget.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;using namespace mgo2mt::combat;
namespace {
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
Identity identity(unsigned slot){return {uint8_t(slot),uint16_t(slot+100),uint32_t(slot+1000)};}
Pose pose(unsigned slot){Pose p;p.feet[1]=2;if(slot<10){p.feet[0]=(int(slot)-5)*700.f;p.feet[2]=5000;}else if(slot<14){p.feet[0]=(int(slot)-10)*700.f;p.feet[2]=7000;}else p.feet[0]=(int(slot)-19)*700.f;return p;}
std::shared_ptr<const stage::Collision> scene(){
 std::vector<Vec3> vertices{{-200000,0,-200000},{200000,0,-200000},{200000,0,200000},{-200000,0,200000}};std::vector<stage::CollisionTriangle> triangles{{{0,1,2},stage::attribute::native_solid},{{0,2,3},stage::attribute::native_solid}};
 for(unsigned i=0;i<64;++i){const auto n=uint32_t(vertices.size());const float z=1500.f+i*30;vertices.insert(vertices.end(),{{-20000,0,z},{20000,0,z},{20000,4000,z},{-20000,4000,z}});triangles.push_back({{n,n+2,n+1},stage::attribute::native_solid,0,0,0});triangles.push_back({{n,n+3,n+2},stage::attribute::native_solid,0,0,0});}
 return std::make_shared<const stage::Collision>(stage::Collision::make(vertices,triangles,{{1,.5f,.5f,true,0,true}}));
}
size_t run(bool evade){
 Service service(10);auto profiles=initial_profiles(20,1,0);profiles[0].nativeAkHitRegions=false; // Capsule fixture; preserve real AK penetration and accuracy.
 service.configure(scene(),profiles);service.authority().active(true);
 for(unsigned i=0;i<24;++i){auto id=identity(i);check(service.authority().join(id,i<14?2:1,pose(i),1000,1000,std::array<uint16_t,1>{25},0),"24 separated mixed actors join");check(service.admit(id)&&service.receive(id,wire::encode(wire::Accept{10}),0),"24 native peers accepted");}
 for(unsigned i=0;i<24;++i){if(i<14&&!evade)continue;wire::Input input;input.epoch=10;input.sequence=1;input.weapon=25;input.pose=pose(i);if(i<14){input.evadeKind=EvadeKind::backstep;input.evadeRequest=1;}else input.firePressed=true;check(service.receive(identity(i),wire::encode(input),0),"same tick mixed evade/fire request");}
 service.deliveries();service.poll(0);auto deliveries=service.deliveries();std::array<unsigned,24> frames{},events{};std::array<uint64_t,24> lastEvent{},revision{};std::array<SopView,24> footer{};std::array<std::vector<wire::Frame>,24> eventChunks;std::array<std::vector<Event>,24> received;size_t maximumBytes=0;
 for(auto& d:deliveries){maximumBytes=std::max(maximumBytes,d.payload.size());check(d.payload.size()<=2000,"all encoded recipient frames <=2000 bytes");auto decoded=wire::decode(d.payload);auto*f=std::get_if<wire::Frame>(&decoded);check(f!=nullptr,"only expected combat frame");const auto slot=d.recipient.slot;++frames[slot];check(f->sop.recipient==d.recipient&&f->sop.life==1,"recipient identity/life footer retained");check(f->snapshot.revision>=revision[slot],"snapshot revisions never reverse");if(f->snapshot.revision==revision[slot])check(f->sop==footer[slot],"same-revision chunk footer exact equality");revision[slot]=f->snapshot.revision;footer[slot]=f->sop;
  if(!f->events.empty())eventChunks[slot].push_back(*f);
  received[slot].insert(received[slot].end(),f->events.begin(),f->events.end());
  for(const auto&e:f->events){check(e.id==lastEvent[slot]+1,"all event IDs consecutive per recipient");lastEvent[slot]=e.id;++events[slot];check(e.source.slot>=14&&e.source.slot<24,"only admitted shooters produce combat events");}
 }
 for(unsigned i=0;i<24;++i){check(events[i]==10*67,"every recipient gets 10 times all67 shot/surface/target/damage events");check(frames[i]<1024,"single tick per-peer mandatory FIFO remains within1024");}
 auto final=service.authority().snapshot();for(unsigned i=0;i<24;++i){check(final.players[i]->alive,"zero remaining penetration force does not manufacture death");if(i<14)check((final.players[i]->evadeKind==EvadeKind::backstep)==evade,"14 evade records remain present through burst");else check(final.players[i]->ammo==29,"each accepted shooter spends one round");}
 const auto measured=combat_test::check_batches(eventChunks[0],67,10);
 // The oracle must reject an underfilled split even when no event is lost.
 auto underfilled=eventChunks[0];check(underfilled.front().events.size()>1,"fixture has a multi-event first chunk");
 auto tail=underfilled.front();tail.events.erase(tail.events.begin());underfilled.front().events.resize(1);underfilled.insert(underfilled.begin()+1,tail);
 bool rejected=false;try{combat_test::check_batches(underfilled,67,10);}catch(const std::runtime_error&){rejected=true;}
 check(rejected,"budget oracle rejects non-greedy splits despite complete ordered events");
 const size_t expectedPerPeer=measured.chunks+(evade?14:0)+1;
 check(deliveries.size()==24*expectedPerPeer,"all actual-kind shot batches plus control and final broadcasts");
 for(unsigned i=0;i<24;++i){const auto peer=combat_test::check_batches(eventChunks[i],67,10);
  check(peer.chunks==measured.chunks&&peer.maximumBytes==measured.maximumBytes&&received[i]==received[0],"all peers receive identical complete event streams and measured chunks");
  check(frames[i]==expectedPerPeer,"every recipient gets expected greedy byte-bounded chunk count");}
 check(maximumBytes==measured.maximumBytes,"exact largest encoded frame matches actual event kinds");
 std::cout<<(evade?"mixed14evade10shooters":"plain10shooters")<<" deliveries="<<deliveries.size()<<" per_peer="<<frames[0]<<" max_bytes="<<maximumBytes<<" conservative_capacity="<<measured.conservativeCapacity<<" events_per_peer="<<events[0]<<'\n';return deliveries.size();
}
}
int main(){try{run(false);run(true);std::cout<<"Mixed burst PASS: actual Service64surfaces/24recipients, actual encoded chunks, no gaps, footer consistency, <=2000bytes, <1024per peer\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
