#include "combat_presentation.h"
#include "combat_death.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
std::vector<char> read(const char*p){std::ifstream f(p,std::ios::binary);check(bool(f),"fixture missing");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){try{
 using namespace combat;using namespace presentation;
 for(float height:{1700.f,1100.f,560.f})for(unsigned level=0;level<4;++level){Reload clock;Player p;p.pose.capsule.height=height;p.identity={1,2,3};p.life=4;p.alive=true;p.weapon=25;p.reloadUntil=999999;p.reloadLevel=uint8_t(level);std::vector<unsigned> sounds;
  for(unsigned ms=0;ms<4000;ms+=10){p.reloadElapsedMs=uint16_t(ms/25*25);auto next=clock.update(7,&p,10000+ms);sounds.insert(sounds.end(),next.begin(),next.end());}
  check(sounds==std::vector<unsigned>({17000,17001,height==1100?17003u:17004u,17005,17006,17007}),"Six original cues once, all stances and skill speeds");
  check(clock.seconds()>3.48&&clock.seconds()<3.49,"inclusive motion end");p.alive=false;check(clock.update(7,&p,14000).empty()&&!clock.active(),"death cancels reload");
 }
 Reload clock;Player p;p.identity={1,2,3};p.life=1;p.alive=true;p.weapon=25;p.reloadUntil=8000;p.reloadElapsedMs=2400;
 check(clock.update(7,&p,10).empty()&&clock.seconds()>2.39,"late join resumes pose without historical sounds");
 check(clock.update(7,&p,5).empty(),"clock regression has no repeated cues");p.life=2;p.reloadElapsedMs=0;check(clock.update(7,&p,11).empty()&&clock.seconds()==0,"new life resets reload");
 check(ak_magazine(114./300)==Magazine::mounted&&ak_magazine(115./300)==Magazine::left&&ak_magazine(310./300)==Magazine::hidden&&ak_magazine(430./300)==Magazine::left&&ak_magazine(650./300)==Magazine::mounted,"magazine boundaries");
 if(argc==3){CharacterCatalog catalog(read(argv[1]));PlayerMotionBank motions(read(argv[2]));auto world=stage::Collision::make({{-20000,0,-20000},{20000,0,-20000},{20000,0,20000},{-20000,0,20000}},{{{0,1,2}},{{0,2,3}}});
  for(unsigned gender=0;gender<2;++gender){player::Ragdoll rag;Death death;Snapshot s;s.epoch=9;Player body;body.identity={0,7,123};body.life=1;body.alive=true;auto pose=*motions.sample(PlayerMotion::Aim,0);death.scope(rag,9,3,body.identity,1);
   check(!death.update(rag,catalog,gender,pose,{0,2,0},1.f,body,s)&&!rag.active(),"live actor remains animated");body.alive=false;
   check(death.update(rag,catalog,gender,pose,{0,2,0},1.f,body,s)&&rag.active(),"HOST death starts physics");float before=rag.root_position()[1];
   for(unsigned frame=0;frame<360;++frame){check(!death.update(rag,catalog,gender,pose,{0,2,0},1.f,body,s),"repeated snapshot cannot restart corpse");rag.step(world,1.f/120);check(rag.active(),"corpse stays finite");}
   check(rag.root_position()[1]<before-200&&rag.maximum_joint_error()<200,"corpse falls and joints remain bounded");
   body.life=2;body.alive=true;check(death.scope(rag,9,3,body.identity,2)&&!rag.active(),"respawn removes old corpse");body.life=1;body.alive=false;check(!death.update(rag,catalog,gender,pose,{0,2,0},1.f,body,s)&&!rag.active(),"late old-life death rejected");
  }
 }
 std::cout<<"AK reload cues/clock and HOST death/respawn physics PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
