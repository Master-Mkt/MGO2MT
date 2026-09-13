#include "player_lock.h"
#include "stage_navigation.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace mgo2win;
static void check(bool ok,const char*s){if(!ok)throw std::runtime_error(s);}
int main(){try{
 std::istringstream input("MGO2WIN.STAGE_COLLISION 1 0 0\n");auto world=std::make_shared<const stage::Collision>(stage::Collision::read(input));
 combat::Authority authority;combat::Weapon weapon;weapon.id=25;weapon.damage=275;weapon.intervalMs=100;weapon.reloadMs=100;weapon.range=10000;weapon.magazine=1;
 authority.begin(1,world,{&weapon,1});uint16_t inventory=25;combat::Identity self{0,1,100},enemy{1,1,101};combat::Pose own,target;target.feet={700,0,6000};
 check(authority.join(self,1,own,1000,1000,{&inventory,1},1000)&&authority.join(enemy,2,target,275,1000,{&inventory,1},1000),"host actors");authority.active(true);
 host::Roster roster;roster.complete=true;for(auto id:{self,enemy}){host::Player row;row.slot=id.slot;row.instance=id.instance;row.character=id.character;row.name="Player";roster.slots[id.slot]=row;}
 player_lock::Lock lock({*original_lock::ak102_parameters(25,0,0.f,0),.65f});stage::Navigation navigation;check(navigation.authoritative(*world,own.feet,0,0,own.capsule),"native view");
 for(unsigned frame=0;frame<60;++frame){auto state=authority.snapshot();player_lock::Input controls{true,true,self,1,1,1,navigation.eye(),navigation.direction()};
  auto selected=lock.current()?lock.update(state,roster,controls,*world):lock.acquire(state,roster,controls,*world);
  check(selected&&selected->identity==enemy,"explicit lock identity");check(navigation.track_view(selected->aimPoint,1.f/60),"camera follows");
  combat::Pose sent{navigation.feet(),navigation.yaw(),navigation.pitch(),navigation.capsule()};
  check(authority.pose(self,1,frame+1,sent,1000+frame*17)==combat::Reject::none,"host accepts same view pose sent by client");
 }
 auto decision=authority.fire(self,{1,1,25,navigation.direction(),1},2004);check(bool(decision),"fire direction matches host pose");
 check(std::any_of(decision.events.begin(),decision.events.end(),[&](const auto&e){return e.kind==combat::EventKind::death&&e.target==enemy;}),"host alone resolves the locked shot hit");
 auto state=authority.snapshot();player_lock::Input controls{true,true,self,1,1,1,navigation.eye(),navigation.direction()};check(!lock.update(state,roster,controls,*world),"death releases lock");
 std::cout<<"Lock acquisition, bounded camera tracking, accepted host pose/fire and death release passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
