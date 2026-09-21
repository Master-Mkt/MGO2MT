#include "stage_navigation.h"
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace mgo2mt::stage;
namespace {
unsigned checks=0;
void check(bool b,const char*message){++checks;if(!b)throw std::runtime_error(message);}
Collision plane(uint64_t flags){return Collision::make({{-1000,0,0},{1000,0,0},{1000,3000,0},{-1000,3000,0}},{{{0,1,2},flags},{{0,2,3},flags}});}
Collision ledge(uint64_t extra=0){return Collision::make({{-10000,1000,-10000},{10000,1000,-10000},{10000,1000,0},{-10000,1000,0},
 {-10000,-5000,-10000},{10000,-5000,-10000},{10000,-5000,10000},{-10000,-5000,10000}},
 {{{0,2,1},attribute::floor|attribute::player|extra},{{0,3,2},attribute::floor|attribute::player|extra},{{4,6,5},attribute::floor|attribute::player},{{4,7,6},attribute::floor|attribute::player}});}
void masks(){
 for(unsigned i=0;i<64;++i){const auto bit=uint64_t(1)<<i;auto w=plane(bit);
  for(unsigned j=0;j<64;++j){const CollisionQuery q{uint64_t(1)<<j};const bool expected=i==j;
   check(bool(w.ray({0,1000,-1000},{0,0,1},2000,q))==expected,"ray selects exact 64-bit purpose");
   check(!w.ray_all({0,1000,-1000},{0,0,1},2000,q).empty()==expected,"sorted hits use same mask");
   check(bool(w.sweep({0,2,-1000},{0,0,2000},{},q))==expected,"capsule sweep uses purpose");
   check(w.clear({0,2,0},{},q)!=expected,"clear uses purpose");
   check(!w.contacts({0,350,0},{0,1350,0},350,1,32,q).empty()==expected,"contacts use purpose");
  }
 }
 auto none=plane(0);check(bool(none.ray({0,1000,-1000},{0,0,1},2000)),"raw inspection retains None triangles");
 check(!none.ray({0,1000,-1000},{0,0,1},2000,query::player),"None does not imply player collision");
 check(movement_collision(std::make_shared<Collision>(none))->triangles.empty(),"movement view excludes None");
 for(auto flags:{attribute::cliff,attribute::camera,attribute::water,attribute::reserved_32})check(movement_collision(std::make_shared<Collision>(plane(flags)))->triangles.empty(),"unrelated triggers never become player walls");
 check(movement_collision(std::make_shared<Collision>(plane(attribute::dont_fall)))->triangles.size()==2,"explicit vertical fall barrier retained");
 check(movement_collision(std::make_shared<Collision>(plane(0x20a004030ULL)))->triangles.empty(),"original QQ Player plus Cliff band is not an invisible wall");
 check(movement_collision(std::make_shared<Collision>(plane(0x20a004030ULL|attribute::dont_fall)))->triangles.size()==2,"adding Don't Fall retains same ledge band");
 check(movement_collision(std::make_shared<Collision>(plane(0x20804adb0ULL)))->triangles.empty(),"original player Type Through band permits walking");
 check(movement_collision(std::make_shared<Collision>(plane(attribute::player|attribute::recoil)))->triangles.empty(),"normal player excludes original Recoil query type");
 const CollisionQuery combined{attribute::floor,attribute::player|attribute::enemy,attribute::through};
 check(combined.matches(attribute::floor|attribute::enemy)&&!combined.matches(attribute::floor)&&!combined.matches(attribute::floor|attribute::player|attribute::through),"required any excluded have independent meanings");
 std::istringstream input("MGO2MT.STAGE_COLLISION 1 3 1\n0 0 0\n1 0 0\n0 1 0\n0 1 2 18446744073709551615 65535\n");auto loaded=Collision::read(input);
 check(loaded.triangles[0].attribute==~uint64_t(0)&&loaded.triangles[0].polygonAttribute==65535,"parser preserves full attribute and separate polygon field");
 check(attribute::names.size()==34&&attribute::dont_fall==0x20000&&attribute::water==0x40000000&&attribute::reserved_33==0x200000000ULL,"user table bit identities");
}
void travel(){
 {
  // Floor-only snapping must not pass a nearer Player wall's rounded rim.
  auto w=Collision::make({{-5000,0,-5000},{5000,0,-5000},{5000,0,5000},{-5000,0,5000},
   {0,0,-5000},{0,200,-5000},{0,200,5000},{0,0,5000}},
   {{{0,1,2},attribute::floor|attribute::player},{{0,2,3},attribute::floor|attribute::player},
    {{4,5,6},attribute::player},{{4,6,7},attribute::player}});
  Navigation n;check(n.authoritative(w,{300,38,0},0,0,{}),"clear body next to wall rim");
  for(unsigned i=0;i<60;++i){n.advance(w,{},1.f/120);check(w.clear(n.feet(),n.capsule()),"ground snapping never passes a nearer physical wall lip");}
 }
 for(auto extra:{uint64_t(0),attribute::cliff,attribute::reserved_33})for(float speed:{3500.f,7000.f}){
  auto w=ledge(extra);Navigation n;check(n.place(w,{0,1200,-1000}),"place on tagged floor");
  for(unsigned i=0;i<40;++i)n.advance(w,{1,0,0,0,speed},1.f/60);
  check(n.feet()[2]>1200&&n.feet()[1]<700&&!n.grounded(),"walking and rolling speed leave unprotected cliff and fall");
 }
 auto protectedWorld=ledge(attribute::dont_fall);Navigation n;check(n.place(protectedWorld,{0,1200,-1000}),"place on DontFall floor");
 for(unsigned i=0;i<120;++i)n.advance(protectedWorld,{1,0,0,0,7000},1.f/60);
 check(n.feet()[2]<0&&n.feet()[2]>-5&&n.feet()[1]>999&&n.grounded(),"explicit Don't Fall retains player at edge");
 const auto before=n.feet();n.advance(protectedWorld,{-1,0,0,0,3500},.1f);check(n.feet()[2]<before[2]-300,"protected edge can move back");
 check(protectedWorld.fall_prevention_fraction({0,2000,-1000},{0,0,2000},{})==1,"airborne travel is not a grounded edge guard");
 check(protectedWorld.fall_prevention_fraction({0,1030,-1000},{0,0,2000},{})==1,"nearby airborne body does not inherit floor guard");
 check(ledge().fall_prevention_fraction({0,1002,-400},{0,0,800},{})==1,"unflagged floor never gains fall prevention");
}
}
int main(){try{masks();travel();std::cout<<"PASS "<<checks<<" attribute and cliff navigation checks\n";return 0;}catch(const std::exception&e){std::cerr<<"after "<<checks<<": "<<e.what()<<'\n';return 1;}}
