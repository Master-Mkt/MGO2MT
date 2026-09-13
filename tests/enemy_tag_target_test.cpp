#include "enemy_tag_target.h"
#include <sstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
static void check(bool ok,const char*msg){if(!ok)throw std::runtime_error(msg);}
int main(){try{
 std::istringstream e("MGO2WIN.STAGE_COLLISION 1 0 0\n");auto empty=stage::Collision::read(e);
 std::istringstream w("MGO2WIN.STAGE_COLLISION 1 3 1\n-3000 -3000 2000\n3000 -3000 2000\n0 4000 2000\n0 1 2 0 1\n");auto wall=stage::Collision::read(w);
 combat::Snapshot s;s.epoch=7;s.revision=1;host::Roster r;r.complete=true;
 for(unsigned i=0;i<3;++i){combat::Player p;p.identity={uint8_t(i),uint16_t(i+1),100+i};p.alive=true;p.team=i?2:1;p.pose.feet={0,0,float(i*5000)};s.players[i]=p;host::Player row;row.slot=i;row.instance=i+1;row.character=100+i;row.name=i==1?"ENEMY":"OTHER";r.slots[i]=row;}
 auto self=s.players[0]->identity;auto run=[&](const stage::Collision&world,bool enabled=true,unsigned rule=1){return enemy_tag::select(s,r,self,rule,{0,1550,0},{0,0,1},world,enabled);};
 auto t=run(empty);check(t&&t->identity==s.players[1]->identity&&t->distance<5000,"nearest enemy along ray");
 check(!run(wall),"wall blocks target");check(!run(empty,false),"option OFF hides");check(!run(empty,true,7),"unsupported rule cannot establish enemy");
 check(!enemy_tag::select(s,r,self,1,{0,1550,0},{0,0,1},empty,true,&wall),"hit-only object blocks actual firing ray");
 check(!enemy_tag::visible({0,1550,0},{0,1550,5000},wall),"interpolated display pose blocked by camera wall");
 check(!enemy_tag::visible({0,1550,0},{0,1550,5000},empty,&wall),"display pose blocked by hit-only object");
 check(enemy_tag::visible({0,1550,0},{0,1550,1000},wall),"wall behind display position is harmless");
 s.players[1]->team=1;check(!run(empty),"friend blocks farther enemy");check(bool(run(empty,true,0)),"DM does not use team membership");s.players[1]->team=0;check(!run(empty),"unknown team hidden");s.players[1]->team=2;
 r.slots[1]->instance=99;check(!run(empty),"slot reuse cannot borrow previous name");r.slots[1]->instance=2;
 r.complete=false;check(!run(empty),"incomplete roster hidden");r.complete=true;
 s.players[0]->alive=false;check(!run(empty),"dead observer hidden");s.players[0]->alive=true;s.players[0]->stunned=true;check(!run(empty),"stunned observer hidden");s.players[0]->stunned=false;
 s.players[1]->alive=false;t=run(empty);check(t&&t->identity==s.players[2]->identity,"dead player no target or obstruction");s.players[1]->alive=true;
 s.players[1]->pose.feet[0]=3000;t=run(empty);check(t&&t->identity==s.players[2]->identity,"off-axis enemy not selected");s.players[1]->pose.feet[0]=0;
 check(!enemy_tag::select(s,r,self,1,{0,1550,0},{0,0,0},empty,true),"zero ray");
 check(!enemy_tag::select(s,r,self,1,{0,1550,0},{NAN,0,1},empty,true),"nonfinite ray");
 combat::Pose p;p.capsule={260,520,2};p.feet={0,0,5000};auto sphere=enemy_tag::capsule({0,260,0},{0,0,1},p);check(sphere&&std::abs(*sphere-4740)<1,"collapsed capsule sphere");p.capsule.height=1700;auto parallel=enemy_tag::capsule({0,3000,5000},{0,-1,0},p);check(parallel&&std::abs(*parallel-1300)<1,"vertical capsule parallel ray");
 auto center=enemy_tag::project({0,0,1000},{0,0,0},{0,0,1},620,120,616,392);check(center&&center->x==928&&center->y==316,"actual stage viewport center");
 check(!enemy_tag::project({0,0,-1000},{0,0,0},{0,0,1},620,120,616,392),"behind camera");check(!enemy_tag::project({99999,0,1000},{0,0,0},{0,0,1},620,120,616,392),"outside view");
 auto right=enemy_tag::project({100,100,1000},{0,0,0},{0,0,1},620,120,616,392);check(right&&right->x>928&&right->y<316,"projection axes");
 std::cout<<"Enemy name target / occlusion / identity / projection passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
