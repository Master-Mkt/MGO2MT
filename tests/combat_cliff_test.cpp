#include "combat_authority.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace mgo2mt;
using namespace mgo2mt::combat;
namespace {
unsigned checks=0;
void check(bool condition,const char*message){++checks;if(!condition)throw std::runtime_error(message);}
constexpr Identity self{0,1,100},other{1,2,200};
constexpr uint64_t floorPlayer=stage::attribute::floor|stage::attribute::player,playerOnly=stage::attribute::player,dontFall=stage::attribute::dont_fall;
Pose at(float z,float y=1002,float x=0){Pose p;p.feet={x,y,z};return p;}
std::shared_ptr<const stage::Collision> ledge(uint64_t attributes=floorPlayer,bool wall=false,bool overhead=false,uint64_t wallAttribute=playerOnly){
 std::vector<Vec3> vertices{{-10000,1000,-10000},{10000,1000,-10000},{10000,1000,0},{-10000,1000,0},
                          {-10000,-4000,-10000},{10000,-4000,-10000},{10000,-4000,10000},{-10000,-4000,10000}};
 std::vector<stage::CollisionTriangle> triangles{{{0,2,1},attributes},{{0,3,2},attributes},{{4,6,5},floorPlayer},{{4,7,6},floorPlayer}};
 if(wall){const auto i=unsigned(vertices.size());vertices.insert(vertices.end(),{{-3000,500,0},{3000,500,0},{3000,4000,0},{-3000,4000,0}});triangles.push_back({{i,i+1,i+2},wallAttribute});triangles.push_back({{i,i+2,i+3},wallAttribute});}
 if(overhead){const auto i=unsigned(vertices.size());vertices.insert(vertices.end(),{{-3000,2650,0},{3000,2650,0},{3000,2650,1000},{-3000,2650,1000}});triangles.push_back({{i,i+1,i+2},playerOnly});triangles.push_back({{i,i+2,i+3},playerOnly});}
 return std::make_shared<const stage::Collision>(stage::Collision::make(std::move(vertices),std::move(triangles)));
}
std::unique_ptr<Authority> host(std::shared_ptr<const stage::Collision> geometry,Pose start=at(-400)){
 auto h=std::make_unique<Authority>();Weapon w;w.id=23;w.damage=100;w.magazine=10;w.reserve=20;w.intervalMs=100;w.reloadMs=1000;w.range=50000;
 h->begin(1,std::move(geometry),std::array<Weapon,1>{w});check(h->join(self,1,start,10000,10000,std::array<uint16_t,1>{23},0),"HOST admits explicit Player floor fixture");
 check(h->configure_evade({800,4500},{600,2000}),"HOST evade timing");h->active(true);return h;
}
void walk(){
 auto world=ledge();const auto from=at(-400),to=at(400,982);Vec3 delta{};for(unsigned i=0;i<3;++i)delta[i]=to.feet[i]-from.feet[i];
 const auto chord=world->sweep(from.feet,delta,from.capsule);check(chord&&chord->fraction<.001f&&chord->normal[1]>.7f,"old diagonal chord falsely hits departure floor");
 auto h=host(world);check(h->pose(self,1,1,to,200)==Reject::none&&h->snapshot().players[0]->pose==to,"unflagged ledge permits horizontal departure then falling");
 auto descending=at(450,932);check(h->pose(self,1,2,descending,250)==Reject::none,"airborne continuation stays permitted");
 const auto accepted=h->snapshot();check(h->pose(self,1,2,descending,251)==Reject::sequence,"duplicate movement remains rejected");check(h->pose(self,1,3,descending,249)==Reject::clock,"backward clock remains rejected");check(h->pose(self,2,3,descending,300)==Reject::generation&&h->pose(self,1,3,descending,300,2)==Reject::generation,"epoch/life remain enforced");check(h->snapshot()==accepted,"rejected inputs do not mutate position");
 auto fast=host(world);check(fast->pose(self,1,1,to,50)==Reject::too_fast,"ledge fallback does not relax speed");
 auto rise=host(world);auto impossible=at(400,2002);check(rise->pose(self,1,1,impossible,50)==Reject::too_fast,"upward shortcut does not gain speed allowance");
 auto cliff=host(ledge(floorPlayer|stage::attribute::cliff));check(cliff->pose(self,1,1,to,200)==Reject::none,"Cliff alone does not imply DontFall");
 auto unknown=host(ledge(floorPlayer|(uint64_t(1)<<55)));check(unknown->pose(self,1,1,to,200)==Reject::none,"unknown unrelated bit does not become fall prevention");
}
void rolling(){
 for(auto kind:{EvadeKind::roll,EvadeKind::rollLeft,EvadeKind::rollRight}){
  auto h=host(ledge(),at(-1000));check(h->pose(self,1,1,at(-400),100)==Reject::none,"running approach to ledge");check(h->evade(self,1,1,kind,1,100)==Reject::none,"HOST admits roll at ledge");
  const auto to=at(400,982);check(h->pose(self,1,2,to,300)==Reject::none,"roll can leave ledge without DontFall");check(h->snapshot().players[0]->evadeKind==kind,"falling does not erase accepted roll");
 }
 auto protectedHost=host(ledge(floorPlayer|dontFall),at(-1000));check(protectedHost->pose(self,1,1,at(-400),100)==Reject::none,"protected floor approach");check(protectedHost->evade(self,1,1,EvadeKind::roll,1,100)==Reject::none,"protected floor roll starts");check(protectedHost->pose(self,1,2,at(400,982),300)==Reject::obstructed,"DontFall also blocks rolling departure");
}
void protected_edge(){
 auto h=host(ledge(floorPlayer|dontFall));check(h->pose(self,1,1,at(400,982),200)==Reject::obstructed,"DontFall prevents ground-to-air departure");check(h->snapshot().players[0]->pose==at(-400),"protected edge rejection keeps previous pose");
 check(h->pose(self,1,2,at(-600),200)==Reject::none,"DontFall still permits walking back on its surface");
 auto level=host(ledge(floorPlayer|dontFall));check(level->pose(self,1,1,at(400),200)==Reject::obstructed,"same-height hovering cannot bypass protected edge");
}
void obstacles(){
 auto wall=host(ledge(floorPlayer,true));check(wall->pose(self,1,1,at(400,982),200)==Reject::obstructed,"two-leg route never bypasses a real wall");
 auto barrier=host(ledge(floorPlayer,true,false,dontFall));check(barrier->pose(self,1,1,at(400,982),200)==Reject::obstructed,"vertical DontFall-only barrier remains solid to players");
 auto roof=host(ledge(floorPlayer,false,true));check(roof->pose(self,1,1,at(400,900),200)==Reject::obstructed,"corner headroom is checked before descending beneath ceiling");
 // The peer is above the final falling pose and beyond the diagonal chord.
 // It intersects only the horizontal leg, so endpoint-only/chord-only checks
 // would miss this occupied corner.
 auto peer=host(ledge(),at(-800));check(peer->join(other,2,at(400,2700),10000,10000,std::array<uint16_t,1>{23},0),"peer occupies horizontal corner");
 check(peer->pose(self,1,1,at(400,800),250)==Reject::obstructed,"fallback checks peer collision on both legs");
 auto changed=host(ledge());auto crouched=at(400,982);crouched.capsule.height=1100;check(changed->pose(self,1,1,crouched,200)==Reject::obstructed,"stance changes cannot use descending corner fallback");
 auto midair=host(ledge(),at(-400,1102));check(midair->pose(self,1,1,at(400,800),200)==Reject::obstructed,"airborne start cannot invent grounded horizontal route");
}
}
int main(){try{walk();rolling();protected_edge();obstacles();std::cout<<"PASS "<<checks<<" HOST cliff/roll checks: departure path, DontFall, real walls, peers, generation and speed\n";return 0;}catch(const std::exception&e){std::cerr<<"after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
