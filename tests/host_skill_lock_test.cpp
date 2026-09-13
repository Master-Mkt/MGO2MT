#include "host_skill_wire.h"
#include "combat_initial_profile.h"
#include "combat_service.h"
#include "player_lock.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
namespace hs=mgo2win::host_skills;
namespace {
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
void put(std::vector<uint8_t>& b,size_t at,uint64_t v,unsigned n){while(n){b[at+n-1]=uint8_t(v);v>>=8;--n;}}
// Saved empty set is the LV0 fixture; no zero-level skill entry is fabricated.
std::vector<uint8_t> reply(const hs::Request& r,unsigned level){
 std::vector<uint8_t>b(level?76:72);put(b,0,0x47575348,4);b[4]=1;
 put(b,8,r.nonce,8);put(b,16,r.scope.room,4);put(b,20,r.scope.character,4);
 put(b,24,r.scope.host,4);put(b,28,1000+r.scope.character,4);put(b,32,1,4);
 b[36]=4;b[37]=uint8_t(level);b[38]=level?1:0;
 std::copy(hs::catalog_digest.begin(),hs::catalog_digest.end(),b.begin()+40);
 if(level){put(b,72,7,2);b[74]=uint8_t(level);}return b;
}
hs::Verified verified(std::shared_ptr<const skills::Catalog> c,combat::Identity id,uint64_t epoch,unsigned level){
 hs::Receiver r(c,100);r.begin(9,99,epoch);check(r.want(id.slot,id.instance,id.character),"trusted participant queued");
 auto q=r.take(10);check(bool(q),"GWSH request generated");check(r.receive(reply(*q,level),11),"GWSH reply verified");
 auto result=r.drain();check(result.size()==1&&result[0].level(7)==level,"SURVEYOR extracted only from Receiver Verified");return result.front();
}
std::shared_ptr<const stage::Collision> floor(){
 std::vector<stage::Vec3>v{{-30000,0,-30000},{30000,0,-30000},{30000,0,30000},{-30000,0,30000}};
 return std::make_shared<const stage::Collision>(stage::Collision::make(v,{{{0,1,2}},{{0,2,3}}}));
}
stage::Collision wall(){return stage::Collision::make({{-20000,-5000,3000},{20000,-5000,3000},{0,20000,3000}},{{{0,1,2}}});}
combat::Pose pose(float z){combat::Pose p;p.feet={0,2,z};return p;}
struct Fixture {
 static constexpr uint64_t epoch=7;
 combat::Identity a{0,10,101},b{1,11,202};
 std::shared_ptr<const stage::Collision> world=floor();
 combat::Service service{epoch};
 host::Roster roster;player_lock::Input input;combat::Snapshot snapshot;
 uint64_t now=0;uint32_t sequence=0;
 Fixture(std::shared_ptr<const skills::Catalog> catalog,unsigned level){
  auto profiles=combat::initial_profiles(20,1,0);service.configure(world,profiles);
  check(service.admit(a)&&service.admit(b),"two service admissions");
  check(service.install_loadout(verified(catalog,a,epoch,level)),"service installs observer verified profile");
  // Target has an independently verified empty set; it must not borrow observer LV.
  check(service.install_loadout(verified(catalog,b,epoch,0)),"target empty set installed independently");
  const std::array<uint16_t,1> gear{25};
  check(service.authority().join(a,1,pose(0),1000,100,gear,0)&&service.authority().join(b,2,pose(7000),1000,100,gear,0),"host grants two AK bodies");
  service.authority().active(true);service.deliveries();
  check(service.receive(a,combat::wire::encode(combat::wire::Accept{epoch}),0),"GWCB v6 accepted");consume();
  roster.complete=true;roster.revision=1;
  for(auto id:{a,b}){host::Player row;row.slot=id.slot;row.instance=id.instance;row.character=id.character;row.name="FIXTURE";roster.slots[id.slot]=row;}
  input.enabled=input.active=true;input.self=a;input.expectedEpoch=epoch;input.sceneToken=50;input.rule=1;
  input.eye={0,2+1700*.65f,0};input.look={0,0,1};
  check(snapshot.players[a.slot]->surveyorLevel==level&&snapshot.players[a.slot]->verifiedSkills,"v6 keeps observer frozen Surveyor level");
  check(snapshot.players[b.slot]->surveyorLevel==0&&snapshot.players[b.slot]->verifiedSkills,"v6 keeps different PC level0");
 }
 void consume(){bool found=false;for(auto&d:service.deliveries())if(d.recipient==a){auto r=combat::wire::decode(d.payload);if(auto f=std::get_if<combat::wire::Frame>(&r)){snapshot=f->snapshot;found=true;}}
  check(found,"actual Service delivery decodes to GWCB Frame");check(snapshot==service.authority().snapshot(),"GWCB v6 preserves complete authoritative snapshot");}
 void move(float z){
  // Advance in legal steps through the real authority's default speed limit.
  float current=snapshot.players[b.slot]->pose.feet[2];
  do{current+=std::clamp(z-current,-1000.f,1000.f);now+=250;
   check(service.authority().pose(b,epoch,++sequence,pose(current),now)==combat::Reject::none,"host validates target movement");
   service.poll(now);consume();
  }while(current!=z);
 }
 player_lock::Lock make_lock(combat::Identity observer)const{
  const auto&p=snapshot.players[observer.slot];check(p&&p->identity==observer,"policy observer identity matches decoded player");
  // Same explicit native adapter as runtime: original flags0/scalar0, native torso .65.
  auto limits=original_lock::ak102_parameters(25,0,0,p->surveyorLevel);check(bool(limits),"decoded level resolves current original AK geometry");
  return player_lock::Lock{player_lock::Policy{*limits,.65f}};
 }
};
void levels(std::shared_ptr<const skills::Catalog> catalog){
 const std::array<float,4> ranges{7200,8000,9600,11200};
 for(unsigned level=0;level<4;++level){Fixture f(catalog,level);auto lock=f.make_lock(f.a);const float range=ranges[level];
  for(float distance:{7200.f,8000.f,9600.f,11200.f}){f.move(distance);check(bool(lock.acquire(f.snapshot,f.roster,f.input,*f.world))==(distance<=range),"LV0/1/2/3 actual acquire distance matrix");}
  f.move(range);check(bool(lock.acquire(f.snapshot,f.roster,f.input,*f.world)),"exact acquisition distance included");
  f.move(range+1);check(bool(lock.update(f.snapshot,f.roster,f.input,*f.world)),"existing lock retained beyond acquisition distance");
  auto fresh=f.make_lock(f.a);check(!fresh.acquire(f.snapshot,f.roster,f.input,*f.world),"new lock cannot use retention allowance");
  f.move(range+1000);check(bool(lock.update(f.snapshot,f.roster,f.input,*f.world)),"exact retained range included");
  f.move(range+1001);check(!lock.update(f.snapshot,f.roster,f.input,*f.world)&&!lock.current(),"beyond retention clears target");
  f.move(range);auto obstruction=wall();check(!lock.acquire(f.snapshot,f.roster,f.input,obstruction),"world occlusion overrides every verified level");
  check(bool(lock.acquire(f.snapshot,f.roster,f.input,*f.world)),"clear line reacquires");
  check(!lock.update(f.snapshot,f.roster,f.input,*f.world,&obstruction),"new hit-only occlusion releases every level");
  check(!f.service.install_loadout(verified(catalog,f.a,Fixture::epoch,3)),"spawn freezes verified profile against late edit");
 }
}
void isolation(std::shared_ptr<const skills::Catalog> catalog){
 Fixture f(catalog,3);f.move(9000);auto longLock=f.make_lock(f.a);check(bool(longLock.acquire(f.snapshot,f.roster,f.input,*f.world)),"LV3 observer locks at9000");
 auto other=f.make_lock(f.b);auto input=f.input;input.self=f.b;input.eye={0,2+1700*.65f,9000};input.look={0,0,-1};
 check(!other.acquire(f.snapshot,f.roster,input,*f.world),"other PC cannot borrow LV3 range");
 f.service.remove(f.b);f.consume();check(!longLock.update(f.snapshot,f.roster,f.input,*f.world),"authoritative leave releases lock");
 auto replacement=f.b;++replacement.instance;++replacement.character;
 check(f.service.admit(replacement)&&!f.service.install_loadout(verified(catalog,f.b,Fixture::epoch,3)),"old PC and instance cannot install into replacement");
 const std::array<uint16_t,1> gear{25};check(f.service.authority().join(replacement,2,pose(7000),1000,100,gear,f.now),"replacement baseline spawn");
 f.now+=250;f.service.poll(f.now);f.consume();check(!f.snapshot.players[replacement.slot]->verifiedSkills&&!f.snapshot.players[replacement.slot]->surveyorLevel,"new identity has no inherited skills");
 check(!longLock.acquire(f.snapshot,f.roster,f.input,*f.world),"stale roster cannot identify replacement");
 f.roster.slots[replacement.slot]->instance=replacement.instance;f.roster.slots[replacement.slot]->character=replacement.character;
 check(bool(longLock.acquire(f.snapshot,f.roster,f.input,*f.world)),"matching replacement roster may acquire explicitly");
 ++f.input.sceneToken;check(!longLock.update(f.snapshot,f.roster,f.input,*f.world),"scene reset clears previous acquisition");
 check(bool(longLock.acquire(f.snapshot,f.roster,f.input,*f.world)),"epoch reset setup");
 combat::Service next(Fixture::epoch+1);next.configure(f.world,combat::initial_profiles(20,1,0));check(next.admit(f.a),"new epoch admitted");
 check(!next.install_loadout(verified(catalog,f.a,Fixture::epoch,3)),"old epoch verified snapshot rejected");
 check(next.authority().join(f.a,1,pose(0),1000,100,gear,0),"new epoch baseline spawn");next.deliveries();
 check(next.receive(f.a,combat::wire::encode(combat::wire::Accept{Fixture::epoch+1}),0),"new epoch v6 accepted");
 auto deliveries=next.deliveries();check(deliveries.size()==1,"new epoch initial frame");auto decoded=combat::wire::decode(deliveries.front().payload);auto fresh=std::get<combat::wire::Frame>(decoded).snapshot;
 check(!fresh.players[f.a.slot]->verifiedSkills&&fresh.players[f.a.slot]->surveyorLevel==0,"new epoch returns to unverified LV0");
 ++f.input.expectedEpoch;check(!longLock.update(fresh,f.roster,f.input,*f.world),"new decoded epoch releases old lock");
 auto baseline=original_lock::ak102_parameters(25,0,0,fresh.players[f.a.slot]->surveyorLevel);check(baseline&&baseline->acquire.range==7200,"reset player resolves original absent-skill factor .9");
 hs::Receiver pending(catalog,300);pending.begin(9,99,7);pending.want(f.a.slot,f.a.instance,f.a.character);auto old=pending.take(1);pending.begin(9,99,8);
 check(old&&!pending.receive(reply(*old,3),2)&&pending.drain().empty(),"Receiver reset never issues Verified for stale in-flight reply");
}
}
int main(int argc,char**argv){try{check(argc==2,"usage: host_skill_lock_test skill_catalog.tsv");auto catalog=std::make_shared<skills::Catalog>();std::string error;check(catalog->load(argv[1],error),"original skill catalog loads");levels(catalog);isolation(catalog);std::cout<<"Verified Surveyor -> Service/Authority -> GWCBv6 -> original AK Lock: four levels, retention, occlusion and identity/epoch reset passed\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
