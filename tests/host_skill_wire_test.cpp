#include "host_skill_wire.h"
#include "combat_initial_profile.h"
#include "combat_service.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
using namespace mgo2mt;
namespace hs=mgo2mt::host_skills;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::vector<uint8_t> hex(std::string_view s){std::vector<uint8_t>b;check(s.size()%2==0,"hex fixture extent");auto digit=[](char c){return c<='9'?c-'0':c-'a'+10;};for(size_t i=0;i<s.size();i+=2)b.push_back(uint8_t(digit(s[i])*16+digit(s[i+1])));return b;}
void put(std::vector<uint8_t>&b,size_t at,uint64_t value,unsigned n){while(n){b[at+n-1]=uint8_t(value);value>>=8;--n;}}
std::vector<uint8_t> reply(const hs::Request&r,uint8_t level=1,uint32_t revision=1,uint8_t status=0){
 std::vector<uint8_t>b(status?72:76);put(b,0,0x47575348,4);b[4]=1;b[5]=status;put(b,8,r.nonce,8);put(b,16,r.scope.room,4);put(b,20,r.scope.character,4);
 if(!status){put(b,24,r.scope.host,4);put(b,28,1000+r.scope.character,4);put(b,32,revision,4);b[36]=4;b[37]=level;b[38]=1;std::copy(hs::catalog_digest.begin(),hs::catalog_digest.end(),b.begin()+40);put(b,72,3,2);b[74]=level;}return b;
}
hs::Verified verified(std::shared_ptr<const skills::Catalog> catalog,hs::Scope scope,uint8_t level,uint64_t seed=200){
 hs::Receiver r(catalog,seed);r.begin(scope.room,scope.host,scope.epoch);check(r.want(scope.slot,scope.instance,scope.character),"request trusted scope");auto request=r.take(100);check(bool(request),"request ready");check(r.receive(reply(*request,level),101),"authenticated reply accepted");auto values=r.drain();check(values.size()==1,"exactly one verified result");return values.front();
}
std::shared_ptr<const stage::Collision> floor(){std::vector<stage::Vec3>v{{-20000,0,-20000},{20000,0,-20000},{20000,0,20000},{-20000,0,20000}};std::vector<stage::CollisionTriangle>t{{{0,1,2}},{{0,2,3}}};return std::make_shared<const stage::Collision>(stage::Collision::make(v,t));}
combat::Pose pose(float z,bool reverse=false){combat::Pose p;p.feet={0,2,z};p.yaw=reverse?3.14159265358979323846f:0.f;return p;}
void codec(std::shared_ptr<const skills::Catalog> catalog){
 // Independent Python/Java contract fixture in outputs/skill_host_snapshot_20260913/wire_vectors.json.
 const auto requestHex=hex("47575348010100001122334455667788000000090000000a");
 const auto eightHex=hex("47575348010000001122334455667788000000090000000a0000000b0000007bffffffff0808080067e90aeca0a6b9dc8b08670dd32dac153fad26730cf2d77c9139127744ad9b0b0008010000070100000601000005010000040100000301000002010000010100");
 const auto errorHex=hex("47575348010400001122334455667788000000090000000a000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
 hs::Request request{{9,11,10,7,1,2},0x1122334455667788ull};check(hs::encode_request(request)==requestHex,"GWSH request matches independent wire literal");
 auto value=hs::decode_reply(eightHex);check(value.status==0&&value.nonce==request.nonce&&value.room==9&&value.character==10&&value.host==11&&value.membership==123&&value.revision==0xffffffffu&&value.capacity==8&&value.used==8,"8-entry fixed header");
 check(value.loadout.entries.size()==8,"8-entry count");for(unsigned i=0;i<8;++i)check(value.loadout.entries[i]==skills::Choice{uint16_t(8-i),1},"entry order retained");
 auto error=hs::decode_reply(errorHex);check(error.status==4&&error.nonce==request.nonce&&error.room==9&&error.character==10&&error.loadout.entries.empty(),"error echo fixture");
 auto reject=[](const std::vector<uint8_t>&b){try{hs::decode_reply(b);}catch(const std::invalid_argument&){return;}throw std::runtime_error("malformed reply accepted");};
 for(size_t at:{size_t(0),size_t(4),size_t(5),size_t(6),size_t(7),size_t(39),size_t(40),size_t(75)}){auto b=eightHex;b[at]=0xff;reject(b);}
 for(size_t n=0;n<72;++n){auto b=eightHex;b.resize(n);reject(b);}auto b=eightHex;b.push_back(0);reject(b);
 b=eightHex;b[38]=9;reject(b);b=eightHex;b[36]=3;reject(b);b=eightHex;put(b,28,0,4);reject(b);b=eightHex;put(b,32,0,4);reject(b);
 b=eightHex;b[76]=b[72];b[77]=b[73];reject(b);b=eightHex;b[74]=4;reject(b);b=errorHex;b[40]=1;reject(b);
 hs::Receiver r(catalog,request.nonce-1);r.begin(9,11,7);check(r.want(1,2,10),"fixture queued");auto pending=r.take(10);check(pending&&pending->nonce==request.nonce,"fixture nonce");check(r.receive(eightHex,11)&&r.drain().size()==1,"8 costs validated against original catalog");
 static_assert(!std::is_default_constructible_v<hs::Verified>);
 static_assert(!std::is_constructible_v<hs::Verified,hs::Scope,hs::Reply>);
}
void receiver(std::shared_ptr<const skills::Catalog> catalog){
 hs::Receiver r(catalog,10);r.begin(9,11,7);check(r.want(1,2,10)&&r.want(2,3,12),"two peers queued");check(!r.want(1,4,13)&&!r.want(1,3,10),"slot and incarnation collision rejected");
 auto a=r.take(100);check(a&&!r.take(101),"single flight");auto good=reply(*a,1);
 for(auto [at,n]:std::array<std::pair<size_t,unsigned>,4>{{{8,8},{16,4},{20,4},{24,4}}}){auto bad=good;put(bad,at,999,n);check(!r.receive(bad,102),"nonce room PC host mismatch rejected");}
 auto bad=good;bad[37]=0;check(!r.receive(bad,103),"claimed used cost must match catalog");bad=good;bad[40]^=1;check(!r.receive(bad,104),"wrong catalog rejected");
 check(r.receive(good,105),"correct reply accepted after invalid packets");check(!r.receive(good,106),"duplicate response not accepted twice");auto ready=r.drain();check(ready.size()==1&&ready[0].scope()==a->scope&&ready[0].level(3)==1&&ready[0].level(7)==0,"verified local scope and missing level0");
 auto next=r.take(107);check(next&&next->scope.character==12,"next request follows first");check(r.receive(reply(*next,1,1,6),108)&&r.drain().empty()&&r.availability()==hs::Availability::supported,"native not-saved error grants nothing but confirms capability");
 r.begin(9,11,8);check(r.want(1,2,10),"next round request");auto old=r.take(109);r.begin(9,11,9);check(!r.receive(reply(*old),110),"old epoch reply rejected");
 check(r.want(1,3,10),"reused identity queued");auto recycled=r.take(111);r.forget(1,2,10);check(r.receive(reply(*recycled),112),"stale leave does not erase new instance");r.forget(1,3,10);check(r.drain().empty(),"leave removes unconsumed verified snapshot");
 check(r.want(1,4,10),"new membership queued");auto forgotten=r.take(113);r.forget(1,4,10);check(!r.receive(reply(*forgotten),114),"leave invalidates outstanding response");check(r.want(1,5,10),"rejoin allowed");auto fresh=r.take(115);check(fresh->nonce!=forgotten->nonce&&!r.receive(reply(*forgotten),116)&&r.receive(reply(*fresh),117),"nonce prevents old incarnation adoption");r.drain();
 hs::Receiver timeout(catalog,30);timeout.begin(9,11,1);timeout.want(1,2,10);auto timed=timeout.take(1000);timeout.tick(3999);check(timeout.availability()==hs::Availability::unknown,"timeout not early");timeout.tick(4000);check(timeout.availability()==hs::Availability::unsupported&&!timeout.receive(reply(*timed),4000),"deadline3000 rejects late reply");timeout.begin(9,11,2);check(!timeout.want(1,2,10)&&!timeout.take(4001),"unsupported persists across rounds");
 hs::Receiver backwards(catalog,40);backwards.begin(9,11,1);backwards.want(1,2,10);backwards.take(100);backwards.tick(99);check(backwards.availability()==hs::Availability::unsupported,"clock reversal fails closed");
 hs::Receiver revisions(catalog,50);revisions.begin(9,11,1);revisions.want(1,2,10);auto v5=revisions.take(100);check(revisions.receive(reply(*v5,1,5),101),"initial revision5 accepted");revisions.drain();
 revisions.begin(9,11,2);revisions.want(1,2,10);auto secondRound=revisions.take(102);check(!revisions.receive(reply(*secondRound,1,4),103),"new nonce and epoch cannot roll revision backward");check(!revisions.receive(reply(*secondRound,2,5),104),"same revision cannot change ordered loadout");
 auto same=reply(*secondRound,1,5);same[36]=8;check(revisions.receive(same,105),"same revision same entries accepts independent capacity change");check(revisions.drain().front().profile().capacity==8,"new capacity retained after cost validation");
 hs::Receiver exhausted(catalog,std::numeric_limits<uint64_t>::max()-1);exhausted.begin(9,11,1);exhausted.want(1,2,10);auto last=exhausted.take(1);check(last&&last->nonce==std::numeric_limits<uint64_t>::max(),"last nonce can be used");exhausted.receive(reply(*last),2);exhausted.want(2,3,12);check(!exhausted.take(3)&&exhausted.availability()==hs::Availability::unsupported,"nonce exhaustion cannot wrap");
}
void authority(std::shared_ptr<const skills::Catalog> catalog){
 const combat::Identity a{1,10,101},b{2,11,202};const std::array<uint16_t,1> gear{25};const uint64_t epoch=7;
 auto first=verified(catalog,{9,11,a.character,epoch,a.slot,a.instance},1);auto third=verified(catalog,{9,11,b.character,epoch,b.slot,b.instance},3,300);auto changed=verified(catalog,{9,11,a.character,epoch,a.slot,a.instance},3,400);
 auto profiles=combat::initial_profiles(20,1,0);combat::Authority h;h.begin(epoch,floor(),profiles);check(h.install_loadout(first)&&h.install_loadout(third),"separate participants install verified saved loadouts");check(!h.install_loadout(changed),"second snapshot cannot replace pending first snapshot");
 check(h.join(a,1,pose(0),1000,100,gear,0)&&h.join(b,2,pose(3000,true),1000,100,gear,0),"two original AK grants");h.active(true);
 auto shot=[&](combat::Identity id,uint32_t sequence,uint64_t at,uint32_t life=1){return h.fire(id,{epoch,sequence,25,id==a?combat::Vec3{0,0,1}:combat::Vec3{0,0,-1},life},at);};
 check(bool(shot(a,1,0))&&bool(shot(b,1,0)),"each participant consumes one round");check(bool(h.reload(a,epoch,100))&&bool(h.reload(b,epoch,100)),"both start reload");auto state=h.snapshot();
 check(state.players[a.slot]->reloadUntil==3137&&state.players[b.slot]->reloadUntil==2436,"LV1/3 original motion completion deadlines differ");check(state.players[a.slot]->reloadLevel==1&&state.players[b.slot]->reloadLevel==3,"reload level latched per player");
 check(!h.install_loadout(changed)&&h.snapshot()==state,"late edit cannot change active reload");
 h.advance(1551);check(h.snapshot().players[b.slot]->ammo==29,"LV3 refill not early");h.advance(1552);check(h.snapshot().players[b.slot]->ammo==30&&h.snapshot().players[a.slot]->ammo==29,"LV3 refill1452ms only affects its player");
 h.advance(2001);check(h.snapshot().players[a.slot]->ammo==29,"LV1 refill not early");h.advance(2002);check(h.snapshot().players[a.slot]->ammo==30&&h.snapshot().players[a.slot]->reserve==89,"LV1 refill1902ms conserves reserve");
 h.advance(2436);check(!h.snapshot().players[b.slot]->reloadUntil&&h.snapshot().players[a.slot]->reloadUntil==3137,"LV3 ends first");h.advance(3137);check(!h.snapshot().players[a.slot]->reloadUntil&&!h.snapshot().players[a.slot]->reloadLevel,"finished reload clears latched level only");
 check(h.pose(a,epoch,1,pose(0),3300)==combat::Reject::none&&bool(shot(a,2,3300))&&bool(h.reload(a,epoch,3300)),"second LV1 reload");
 check(h.pose(b,epoch,1,pose(3000,true),3500)==combat::Reject::none&&bool(shot(b,2,3500))&&bool(shot(b,3,3610))&&bool(shot(b,4,3720)),"host-authoritative death during reload");
 auto dead=h.snapshot().players[a.slot];check(!dead->alive&&!dead->reloadUntil&&!dead->reloadLevel&&dead->masteryLevel==1&&dead->verifiedSkills,"death cancels motion but preserves round skills");check(!h.install_loadout(changed),"death is not a skill refresh opportunity");
 check(h.respawn(a,2,[&]{return h.join(a,1,pose(0),1000,100,gear,3800);}),"host grants next life");auto respawn=h.snapshot().players[a.slot];check(respawn->life==2&&respawn->masteryLevel==1&&respawn->verifiedSkills,"respawn retains frozen LV1");
 h.begin(epoch+1,floor(),profiles);check(!h.install_loadout(first),"old epoch verified profile rejected");check(h.join(a,1,pose(0),1000,100,gear,4000),"next round can spawn before snapshot");check(!h.snapshot().players[a.slot]->verifiedSkills&&!h.snapshot().players[a.slot]->masteryLevel,"epoch reset restores baseline");
 auto newer=verified(catalog,{9,11,a.character,epoch+1,a.slot,a.instance},3,500);check(!h.install_loadout(newer),"already spawned baseline remains frozen");
 combat::Service service(epoch);service.configure(floor(),profiles);check(!service.install_loadout(first),"service requires admitted peer");check(service.admit(a)&&service.install_loadout(first),"service forwards matching admitted identity");service.remove(a);check(!service.install_loadout(first),"service leave removes admission");auto reused=a;reused.instance++;check(service.admit(reused)&&!service.install_loadout(first),"old instance cannot install into reused peer");
}
}
int main(int argc,char**argv){try{check(argc==2,"usage: host_skill_wire_test skill_catalog.tsv");auto catalog=std::make_shared<skills::Catalog>();std::string error;check(catalog->load(argv[1],error),"reviewed original skill catalog loads");codec(catalog);receiver(catalog);authority(catalog);std::cout<<"GWSH golden wire, authenticated scope, timeout, two-player original AK timing and frozen round skills passed\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
