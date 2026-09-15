#include "combat_initial_profile.h"
#include "combat_wire.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
using namespace mgo2win;using namespace mgo2win::combat;
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
constexpr Identity self{0,10,100},enemy{1,11,101};
auto empty(){return std::make_shared<const stage::Collision>(stage::Collision::make({},{}));}
auto walls(bool penetrableFirst=false){
 std::vector<stage::Vec3> v;std::vector<stage::CollisionTriangle> t;std::vector<stage::CollisionMaterial> m;
 for(unsigned i=0;i<(penetrableFirst?2u:1u);++i){const float z=penetrableFirst?(i?3000.f:1000.f):2000.f;const auto base=unsigned(v.size());v.insert(v.end(),{{-100000,-100000,z},{100000,-100000,z},{100000,100000,z},{-100000,100000,z}});m.push_back({i+1,.5f,.5f,true,i==0&&penetrableFirst?100:1000,true});t.push_back({{base,base+2,base+1},0,0,i,i+20});t.push_back({{base,base+3,base+2},0,0,i,i+20});}
 return std::make_shared<const stage::Collision>(stage::Collision::make(std::move(v),std::move(t),std::move(m)));
}
const Event& shot(const Decision& d){auto i=std::find_if(d.events.begin(),d.events.end(),[](const auto&e){return e.kind==EventKind::shot;});check(i!=d.events.end(),"accepted shot event exists");return *i;}
void host_distance(){
 auto fire=[&](std::shared_ptr<const stage::Collision> world,bool ak,bool target,uint16_t weapon=25){
  auto h=std::make_unique<Authority>();auto profiles=initial_profiles(20,1,0);if(!ak&&weapon==25){Weapon w;w.id=25;w.damage=50;w.intervalMs=100;w.reloadMs=1000;w.magazine=2;w.reserve=3;w.range=10000;profiles={w};}
  for(auto& profile:profiles)if(profile.id==25)profile.magazine=5;h->begin(7,world,profiles);check(h->join(self,1,{},10000,1000,std::array<uint16_t,1>{weapon},0),"source joined");if(target){Pose p;p.feet={0,0,3000};check(h->join(enemy,2,p,10000,1000,std::array<uint16_t,1>{weapon},0),"target joined");}h->active(true);
  auto result=h->fire(self,{7,1,weapon,{0,0,1}},0);check(bool(result),"HOST accepts configured shot");check(valid_shot_distance(shot(result)),"HOST emits valid resolved distance");return result;
 };
 auto miss=fire(empty(),false,false);check(shot(miss).shotDistance==10000&&shot(miss).normal==Vec3{0,0,1},"miss ends at configured range");
 auto hit=fire(walls(),false,false);check(std::abs(shot(hit).shotDistance-2000)<.01f,"ordinary wall truncates shot distance");
 auto body=fire(empty(),false,true);const float expected=3000-std::sqrt(260.f*260.f-110.f*110.f);check(std::abs(shot(body).shotDistance-expected)<.01f,"nearest target capsule surface truncates distance");
 auto stopped=fire(walls(),true,false);const auto& s=shot(stopped);check(std::abs((s.position[2]+s.normal[2]*s.shotDistance)-2000)<.1f,"AK tracer ends at HOST post-spread blocked surface");
 auto through=fire(walls(true),true,false);const auto& p=shot(through);check(std::abs((p.position[2]+p.normal[2]*p.shotDistance)-3000)<.1f,"AK tracer reaches final surface after penetrating earlier wall");check(std::any_of(through.events.begin(),through.events.end(),[](const auto&e){return e.kind==EventKind::impact&&e.object==20;}),"penetrated first surface remains a separate impact");
 auto akMiss=fire(empty(),true,false);check(shot(akMiss).shotDistance==200000,"AK miss reaches configured maximum despite spread");
 for(uint16_t weapon:{uint16_t(50),uint16_t(53)})check(shot(fire(empty(),false,false,weapon)).shotDistance==0,"RPG/WP projectile launch does not create instantaneous tracer");
 auto mk2=fire(walls(),false,false,2);check(shot(mk2).shotDistance==0,"MK2 trace ends at its actual nonpenetrating hit");
 for(const auto* d:{&miss,&hit,&body,&stopped,&through,&akMiss,&mk2})for(const auto&e:d->events)if(e.kind!=EventKind::shot)check(e.shotDistance==0,"non-shot events retain zero distance");
 auto denied=std::make_unique<Authority>();auto profiles=initial_profiles(20,1,0);denied->begin(8,empty(),profiles);check(denied->join(self,1,{},1000,1000,std::array<uint16_t,1>{3},0),"held-only fixture");denied->active(true);check(denied->fire(self,{8,1,3,{0,0,1}},0).events.empty(),"held-only/rejected fire never creates tracer event");
}
void wire_distance(){
 wire::Frame f;f.snapshot.epoch=7;f.snapshot.revision=1;f.snapshot.eventWatermark=1;f.status=wire::Status::active;Event e;e.epoch=7;e.id=1;e.source=self;e.weapon=25;e.normal={0,0,1};f.events={e};
 auto rejects=[](const auto& value){try{wire::encode(value);return false;}catch(const wire::Invalid&){return true;}};
 for(float distance:{0.f,.01f,200000.f,1000000.f}){f.events[0].shotDistance=distance;auto bytes=wire::encode(f);check(bytes[5]==19&&std::get<wire::Frame>(wire::decode(bytes))==f,"finite distance including inclusive maximum round trips through GWCB19");}
 f.events[0].shotDistance=1234.5f;auto bytes=wire::encode(f);auto old=bytes;old[5]=15;check(!wire::recognized(old),"GWCB15 mixed peer is unrecognized");bool refused=false;try{wire::decode(old);}catch(const wire::Invalid&){refused=true;}check(refused,"old version cannot decode new shot layout");
 const auto distanceAt=bytes.size()-5;auto bits=std::bit_cast<uint32_t>(1234.5f);for(unsigned i=0;i<4;++i)check(bytes[distanceAt+i]==uint8_t(bits>>(8*i)),"shot-only extension is little-endian float32 after life fields");
 auto shortRecord=bytes;shortRecord.erase(shortRecord.begin()+distanceAt,shortRecord.begin()+distanceAt+4);refused=false;try{wire::decode(shortRecord);}catch(const wire::Invalid&){refused=true;}check(refused,"missing shot extension is rejected even with current version");
 for(float bad:{-1.f,1000001.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){f.events[0].shotDistance=bad;check(rejects(f),"invalid distance cannot encode");auto raw=bytes;auto rawBits=std::bit_cast<uint32_t>(bad);for(unsigned i=0;i<4;++i)raw[distanceAt+i]=uint8_t(rawBits>>(8*i));refused=false;try{wire::decode(raw);}catch(const wire::Invalid&){refused=true;}check(refused,"invalid raw distance cannot decode");}
 f.events[0]=e;f.events[0].shotDistance=10;f.events[0].normal={0,0,2};check(rejects(f),"positive distance requires unit HOST direction");f.events[0]=e;
 const auto shotBytes=wire::encode(f).size();f.events[0].kind=EventKind::impact;const auto impactBytes=wire::encode(f).size();check(shotBytes==impactBytes+4,"only shot events grow by four bytes");f.events[0].shotDistance=1;check(rejects(f),"non-shot distance cannot be smuggled through omitted field");
 // Direct Replica input has the same checked contract as decoded packets.
 Replica replica;Snapshot snap;snap.epoch=7;snap.revision=1;Player player;player.identity=self;player.hp=player.maxHp=player.stamina=player.maxStamina=1000;player.alive=true;snap.players[self.slot]=player;check(replica.snapshot(snap),"replica scope baseline");auto invalid=e;invalid.shotDistance=1;invalid.normal={};check(replica.events({&invalid,1}).empty(),"Replica rejects invalid distance direction without advancing cursor");check(replica.events({&e,1}).size()==1,"valid same sequence still plays once");
 // The existing packet budget continues to be measured, never guessed.
 f.events.clear();f.snapshot.eventWatermark=4;for(unsigned i=0;i<24;++i){auto p=player;p.identity={uint8_t(i),uint16_t(i+1),100+i};p.weapon=25;p.ammo=30;p.reserve=90;f.snapshot.players[i]=p;}
 size_t fit=0;for(unsigned n=1;n<=4;++n){auto s=e;s.id=n;s.shotDistance=10000;f.events.push_back(s);if(rejects(f))break;fit=n;check(wire::encode(f).size()<=2000,"24-player frame with shots remains under datagram bound");}check(fit>=1,"ordinary full roster still fits at least one shot event");
 if(fit==4){auto extra=e;extra.id=5;f.snapshot.eventWatermark=5;f.events.push_back(extra);check(rejects(f),"native frame event count remains bounded at four");}
 // Rich action snapshots use the same adaptive byte budget as Service.
 f.events.clear();f.snapshot.eventWatermark=4;for(auto& p:f.snapshot.players){p->evadeKind=EvadeKind::rollRight;p->evadeSerial=1;p->evadeElapsedMs=1;}f.sop.recipient=f.snapshot.players[0]->identity;f.sop.life=1;
 size_t richFit=0,largest=0;for(unsigned n=1;n<=4;++n){auto event=e;event.id=n;event.shotDistance=10000;f.events.push_back(event);if(rejects(f))break;richFit=n;const auto packet=wire::encode(f);largest=packet.size();check(packet.size()<=2000&&std::get<wire::Frame>(wire::decode(packet))==f,"full side-roll roster and recipient footer preserve strict event framing");}check(richFit>=1,"full action roster plus recipient footer fits a shot");
 std::cout<<"GWCB19 shot extension bytes=4 full24_fit="<<fit<<" action24_fit="<<richFit<<" action24_bytes="<<largest<<'\n';
}
}
int main(){try{host_distance();wire_distance();std::cout<<"HOST miss/wall/body/AK final ray, no projectile tracer, strict shot distance wire/Replica PASS\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
