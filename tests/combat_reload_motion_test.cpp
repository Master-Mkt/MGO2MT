#include "combat_authority.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt;
using namespace mgo2mt::combat;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::shared_ptr<const stage::Collision> floor(){
 const std::vector<Vec3> vertices{{-20000,0,-20000},{20000,0,-20000},{20000,0,20000},{-20000,0,20000}};
 const std::vector<stage::CollisionTriangle> triangles{{{0,1,2}},{{0,2,3}}};
 return std::make_shared<const stage::Collision>(stage::Collision::make(vertices,triangles));
}
// Only reload timing is original here. This is not a playable weapon profile.
Weapon fixture(float rate=1){Weapon w{23,1,0,100,0,30,90,10000,0,0,true};w.reloadMotion=original::ak102_reload;w.reloadMotion->rate=rate;return w;}
}
int main(){try{
 const std::array<uint32_t,4> fillFrames{130,114,100,87},endFrames{209,182,161,140};
 const std::array<uint32_t,4> fillMs{2169,1902,1669,1452},endMs{3487,3037,2687,2336};
 auto world=floor();constexpr Identity id{1,1,101};Pose pose;pose.feet={0,2,0};
 for(size_t level=0;level<4;++level){
  auto w=fixture(original::rifle_reload_rates[level]);auto timing=original::reload_timing(*w.reloadMotion);
  check(timing&&timing->refillFrame==fillFrames[level]&&timing->endFrame==endFrames[level],"original float32 event and inclusive end frames");
  check(timing->refillMs==fillMs[level]&&timing->endMs==endMs[level],"rational nominal clock rounds deadlines only once");
  Authority host;host.begin(1,world,std::span(&w,1));check(host.join(id,1,pose,1000,1000,std::array<uint16_t,1>{23},0),"join fixture");host.active(true);
  check(bool(host.fire(id,{1,1,23,{0,0,1}},0)),"spend one round");
  auto started=host.reload(id,1,100);check(bool(started)&&started.events.size()==1,"one reload event");
  const auto fill=100+fillMs[level],end=100+endMs[level];
  host.advance(fill-1);check(host.snapshot().players[1]->ammo==29,"no early magazine fill");
  host.advance(fill);auto state=host.snapshot();check(state.players[1]->ammo==30&&state.players[1]->reserve==89&&state.players[1]->reloadUntil==end,"refill conserves ammo while motion remains active");
  host.advance(fill);check(host.snapshot()==state,"repeated poll does not grant ammo twice");
  check(host.reload(id,1,fill).reject==Reject::reloading,"refill is not completion");
  host.advance(end-1);check(host.snapshot().players[1]->reloadUntil==end,"end boundary not early");
  host.advance(end);check(!host.snapshot().players[1]->reloadUntil,"original end boundary clears host state");
  check(host.pose(id,1,1,pose,end)==Reject::none&&bool(host.fire(id,{1,2,23,{0,0,1}},end)),"fire after completed motion");
  check(bool(host.reload(id,1,end)),"second reload");host.advance(end+endMs[level]+5000);
  state=host.snapshot();check(state.players[1]->ammo==30&&state.players[1]->reserve==88&&!state.players[1]->reloadUntil,"delayed tick crosses both phases exactly once");
  host.advance(end+endMs[level]+5001);check(host.snapshot()==state,"no repeated delayed refill");
 }
 auto reject=[&](Weapon w){bool threw=false;try{Authority h;h.begin(1,world,std::span(&w,1));}catch(const std::invalid_argument&){threw=true;}check(threw,"invalid or ambiguous timing accepted");};
 auto bad=fixture();bad.reloadMs=3500;reject(bad);bad=fixture();bad.reloadRefillMs=650;reject(bad);
 for(float rate:{0.f,-1.f,17.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){bad=fixture(rate);reject(bad);}
 bad=fixture();bad.reloadMotion->baseTick=0;reject(bad);bad=fixture();bad.reloadMotion->intervals=1;reject(bad);
 bad=fixture();bad.reloadMotion->refillTick=1046;reject(bad);bad=fixture(.0001f);reject(bad);
 // A host near timestamp exhaustion must reject without partial state change.
 auto w=fixture();Authority h;h.begin(1,world,std::span(&w,1));check(h.join(id,1,pose,1000,1000,std::array<uint16_t,1>{23},0),"overflow fixture");h.active(true);check(bool(h.fire(id,{1,1,23,{0,0,1}},0)),"overflow spends ammo");
 auto before=h.snapshot();check(h.reload(id,1,std::numeric_limits<uint64_t>::max()-1).reject==Reject::clock&&h.snapshot()==before,"deadline overflow is atomic");
 std::cout<<"Original reload motion: four rates, distinct fill/end, finite ammo, missed ticks and invalid profiles passed\n";
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
