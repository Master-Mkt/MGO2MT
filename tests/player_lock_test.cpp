#include "player_lock.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
using namespace mgo2win;
using player_lock::Lock;
using player_lock::Policy;
static_assert(!std::is_default_constructible_v<Lock>);
static_assert(!std::is_default_constructible_v<Policy>);
static void check(bool value,const char*label){if(!value)throw std::runtime_error(label);}
static stage::Collision wall(float z){
 std::ostringstream out;out<<"MGO2WIN.STAGE_COLLISION 1 3 1\n-5000 -5000 "<<z<<"\n5000 -5000 "<<z<<"\n0 5000 "<<z<<"\n0 1 2 0 1\n";
 std::istringstream in(out.str());return stage::Collision::read(in);
}
struct Fixture {
 combat::Snapshot s;host::Roster r;player_lock::Input i;
 stage::Collision empty=stage::Collision::make({},{});
 Lock lock{Policy{20000,.9f,.5f}}; // Synthetic test policy, never an original constant.
 Fixture(){s.epoch=7;s.revision=1;r.complete=true;r.revision=1;
  add(0,{0,0,0},1);add(1,{0,0,5000},2);
  i.enabled=i.active=true;i.self=s.players[0]->identity;i.expectedEpoch=7;i.sceneToken=11;i.rule=1;i.eye={0,850,0};i.look={0,0,1};
 }
 void add(unsigned slot,combat::Vec3 feet,uint8_t team){combat::Player p;p.identity={uint8_t(slot),uint16_t(slot+1),100+slot};p.life=1;p.alive=true;p.team=team;p.pose.feet=feet;s.players[slot]=p;
  host::Player row;row.slot=uint8_t(slot);row.instance=uint16_t(slot+1);row.character=100+slot;row.name="PLAYER";r.slots[slot]=row;
 }
 auto acquire(){return lock.acquire(s,r,i,empty);}
 auto update(){return lock.update(s,r,i,empty);}
};
template<class Mutation>static void releases(Mutation mutate,const char*message){Fixture f;check(bool(f.acquire()),"release setup");mutate(f);check(!f.update(),message);check(!f.lock.current(),"released state cleared");}
int main(){try{
 Fixture f;check(!f.update(),"update never acquires implicitly");auto t=f.acquire();check(t&&t->identity==f.s.players[1]->identity&&t->life==1,"acquire identity and life");
 check(t->aimPoint==combat::Vec3{0,850,5000}&&t->distance==5000,"explicit aim height and range units");
 check(bool(f.update()),"same snapshot update stable");f.s.players[1]->pose.feet[0]=500;++f.s.revision;t=f.update();check(t&&t->aimPoint[0]==500,"held target follows authoritative pose");
 f.add(2,{0,0,10000},2);++f.s.revision;check(f.update()->identity.slot==1,"better candidate cannot steal held target");
 check(f.acquire()->identity.slot==2,"explicit acquire prioritizes nearest angle over range");
 f.lock.clear();check(!f.lock.current()&&!f.update(),"explicit release clears context");
 {
  Fixture q;q.s.players[1]->pose.feet={500,0,5000};q.add(2,{-500,0,5000},2);
  check(q.acquire()->identity.slot==1,"equal angle and distance deterministic slot tie");
  q.s.players[1]->pose.feet={0,0,5000};q.s.players[2]->pose.feet={0,0,10000};check(q.acquire()->identity.slot==1,"nearest collinear body wins");
 }
 releases([](auto&q){q.i.enabled=false;},"OFF releases");
 releases([](auto&q){q.i.active=false;},"inactive input releases");
 releases([](auto&q){q.s.players[0]->alive=false;},"self death releases");
 releases([](auto&q){q.s.players[0]->stunned=true;},"self stun releases");
 releases([](auto&q){++q.s.players[0]->life;},"self new life releases");
 releases([](auto&q){++q.i.self.instance;},"self identity mismatch releases");
 releases([](auto&q){++q.s.players[0]->identity.instance;q.i.self=q.s.players[0]->identity;++q.r.slots[0]->instance;},"matched new self identity still releases");
 releases([](auto&q){++q.s.epoch;},"offer epoch mismatch releases");
 releases([](auto&q){++q.s.epoch;++q.i.expectedEpoch;},"new matched epoch releases previous lock");
 releases([](auto&q){++q.i.sceneToken;},"scene change releases");
 releases([](auto&q){q.i.sceneToken=0;},"unknown scene releases");
 releases([](auto&q){q.i.rule=0;},"rule transition releases");
 releases([](auto&q){q.i.rule=7;},"unknown rule releases");
 releases([](auto&q){q.s.players[1]->alive=false;q.add(2,{0,0,10000},2);},"target death does not switch to another enemy");
 releases([](auto&q){q.s.players[1]->stunned=true;},"target stun releases");
 releases([](auto&q){++q.s.players[1]->life;},"target respawn releases");
 releases([](auto&q){q.s.players[1].reset();},"target departure releases");
 releases([](auto&q){++q.s.players[1]->identity.instance;++q.r.slots[1]->instance;},"matched slot reuse still releases");
 releases([](auto&q){++q.s.players[1]->identity.character;++q.r.slots[1]->character;},"matched different character releases");
 releases([](auto&q){q.r.complete=false;},"incomplete roster releases");
 releases([](auto&q){q.r.slots[0].reset();},"missing observer roster releases");
 releases([](auto&q){++q.r.slots[0]->character;},"observer roster mismatch releases");
 releases([](auto&q){++q.r.slots[1]->instance;},"target roster mismatch releases");
 releases([](auto&q){q.r.slots[1]->slot=2;},"target roster slot mismatch releases");
 releases([](auto&q){q.s.players[1]->team=1;},"target becomes friendly releases");
 releases([](auto&q){q.s.players[1]->team=0;},"unknown target team releases");
 releases([](auto&q){q.s.players[0]->team=0;},"unknown observer team releases");
 releases([](auto&q){q.s.players[1]->pose.feet[2]=30000;},"out of range releases");
 releases([](auto&q){q.i.look={1,0,0};},"out of cone releases");
 releases([](auto&q){q.i.look={0,0,0};},"zero look releases");
 releases([](auto&q){q.i.look={NAN,0,1};},"nonfinite look releases");
 releases([](auto&q){q.i.eye={INFINITY,0,0};},"nonfinite eye releases");
 releases([](auto&q){q.s.players[1]->pose.feet[0]=NAN;},"nonfinite target releases");
 releases([](auto&q){q.s.players[1]->pose.capsule.height=1;},"invalid target capsule releases");
 releases([](auto&q){q.s.players[1]->identity.slot=23;},"misindexed snapshot target releases");
 {
  Fixture q;q.s.revision=10;check(bool(q.acquire()),"revision setup");q.s.revision=9;check(!q.update(),"snapshot rollback releases");
  q.s.revision=0;check(!q.acquire(),"unversioned snapshot never acquires");
 }
 {
  Fixture q;q.i.rule=0;q.s.players[0]->team=q.s.players[1]->team=0;check(bool(q.acquire()),"DM treats other player as enemy without teams");
  q.s.players[1]->identity.character=q.i.self.character;q.r.slots[1]->character=q.i.self.character;check(!q.acquire(),"duplicate self character cannot become target");
 }
 {
  Fixture q;auto front=wall(2500),behind=wall(12000);
  check(!q.lock.acquire(q.s,q.r,q.i,front),"stage wall prevents acquisition");
  check(!q.lock.acquire(q.s,q.r,q.i,q.empty,&front),"hit-only object prevents acquisition");
  check(bool(q.lock.acquire(q.s,q.r,q.i,behind)),"wall beyond target does not block");
  check(!q.lock.update(q.s,q.r,q.i,q.empty,&front),"new hit-only obstruction releases held target");
  check(bool(q.acquire()),"stage obstruction setup");check(!q.lock.update(q.s,q.r,q.i,front),"new stage obstruction releases");
 }
 {
  Fixture q;q.add(2,{0,0,2500},1);check(!q.acquire(),"living friend blocks target");
  q.r.slots[2].reset();check(!q.acquire(),"unidentified living body still blocks target");
  q.s.players[2]->stunned=true;check(!q.acquire(),"stunned living body still blocks target");
  q.s.players[2]->alive=false;check(bool(q.acquire()),"dead body does not block target");
 }
 {
  Fixture q;Lock exact{Policy{5000,1,.5f}};check(bool(exact.acquire(q.s,q.r,q.i,q.empty)),"range and cone include exact boundary");
  Lock shortRange{Policy{4999,1,.5f}};check(!shortRange.acquire(q.s,q.r,q.i,q.empty),"range immediately outside excluded");
  q.i.look={0,0,10};check(bool(exact.acquire(q.s,q.r,q.i,q.empty)),"look direction normalized");
  for(auto p:{Policy{0,.9f,.5f},Policy{-1,.9f,.5f},Policy{INFINITY,.9f,.5f},Policy{10000,NAN,.5f},
      Policy{10000,-.1f,.5f},Policy{10000,1.1f,.5f},Policy{10000,.9f,-.1f},Policy{10000,.9f,1.1f}}){
   Lock invalid{p};check(!invalid.valid_policy()&&!invalid.acquire(q.s,q.r,q.i,q.empty),"invalid policy fails closed");
  }
 }
 {
  Fixture q;q.lock=Lock{Policy{*original_lock::ak102_parameters(25,0,0.f,0),.5f}};
  q.s.players[1]->pose.feet={450,0,100};check(bool(q.acquire()),"original half-width admits a close off-axis enemy");
  q.s.players[1]->pose.feet={700,0,1500};check(bool(q.update()),"original retention widens angle for held identity");
  check(!q.acquire(),"same target outside original acquisition angle cannot be freshly captured");
  q.s.players[1]->pose.feet={0,0,5000};check(bool(q.acquire()),"original range fixture");
  q.s.players[1]->pose.feet[2]=7700;check(bool(q.update()),"original retention extends range by 1000");
  check(!q.acquire(),"extended retention range is not acquisition range");
  q.s.players[1]->pose.feet={0,0,5000};check(bool(q.acquire()),"original lifecycle fixture");++q.s.players[1]->life;check(!q.update(),"original policy preserves life generation rejection");
 }
 {
  Fixture q;q.lock=Lock{Policy{*original_lock::ak102_parameters(25,0,0.f,0),.5f}};
  q.s.players[1]->pose.feet={500,0,5000};q.add(2,{0,0,6000},2);
  check(q.acquire()->identity.slot==1,"original mode0 selects shorter range over more central angle");
  auto front=wall(2500);check(!q.lock.acquire(q.s,q.r,q.i,front),"original geometry still requires world visibility");
  q.s.players[2].reset();q.i.look={1,0,0};q.s.players[1]->pose.feet={5000,0,-550};check(bool(q.acquire()),"original geometry uses native camera basis after yaw rotation");
  q.i.look={0,.70710678f,.70710678f};q.s.players[1]->pose.feet={0,5000,5000};check(bool(q.acquire()),"native aim frame pitch adapter preserves target alignment");
  q.i.enabled=false;check(!q.update(),"room or individual OFF releases original policy lock");
 }
 std::cout<<"Player lock identity / lifecycle / explicit capture / cone and original AK geometry / two-world and body occlusion passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
