#include "weapon_aim_presentation.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main(){try{
 for(const auto&weapon:original_weapon::firearms){reticle::Scope s{1,1,{0,1,1},1,weapon.id};reticle::Recoil r;combat::Event e;e.epoch=1;e.id=1;e.source=s.identity;e.sourceLife=1;e.weapon=weapon.id;r.update(s,true,0,{},0);r.update(s,true,1,std::span(&e,1),0);check(r.pitch()>0,"every firearm receives scoped native camera recoil");}
 reticle::Scope scope{7,4,{0,2,10},1,25};reticle::Recoil recoil;
 combat::Event shot;shot.kind=combat::EventKind::shot;shot.epoch=7;shot.id=2;shot.source=scope.identity;shot.sourceLife=1;shot.weapon=25;
 recoil.update(scope,true,1,{},0);recoil.update(scope,true,2,std::span(&shot,1),.016);
 check(recoil.pitch()>.004f,"accepted own shot produces camera kick");
 auto kicked=recoil.direction({0,0,1});check(kicked[1]>0&&std::abs(enemy_tag::dot(kicked,kicked)-1)<1e-5,"kick is upward unit camera rotation");
 float peak=recoil.pitch();recoil.update(scope,true,2,std::span(&shot,1),0);check(recoil.pitch()==peak,"same event never kicks twice");
 for(unsigned i=0;i<5;++i){auto bad=shot;bad.id=3+i;if(i==0)bad.source.instance++;if(i==1)bad.sourceLife++;if(i==2)bad.weapon=3;if(i==3)bad.epoch++;if(i==4)bad.kind=combat::EventKind::impact;recoil.update(scope,true,bad.id,std::span(&bad,1),0);}
 check(recoil.pitch()==peak,"foreign/stale-life/weapon/epoch/impact events never kick");
 recoil.update(scope,true,7,{},1);check(recoil.pitch()<.000001f,"kick smoothly decays while not firing");
 for(uint64_t id=8;id<108;++id){shot.id=id;recoil.update(scope,true,id,std::span(&shot,1),0);}
 check(recoil.pitch()<=.018f&&std::abs(recoil.yaw())<=.006f,"burst bounded without runaway camera");
 scope.life++;recoil.update(scope,true,107,std::span(&shot,1),0);check(recoil.pitch()==0,"new life discards queued past shots");
 recoil.update(scope,false,107,{},0);check(recoil.pitch()==0,"menu/death/ineligible resets kick");
 scope.life=1;recoil.update(scope,true,1,{},0);shot.id=2;recoil.update(scope,true,4,std::span(&shot,1),0);
 peak=recoil.pitch();shot.id=4;recoil.update(scope,true,4,std::span(&shot,1),0);check(recoil.pitch()>peak,"later chunk of same snapshot retains accepted shot");
 peak=recoil.pitch();shot.id=5;std::array duplicates{shot,shot};recoil.update(scope,true,4,duplicates,0);check(std::abs(recoil.pitch()-peak-.0045f)<1e-6,"post-snapshot arrival and within-chunk duplicate processed once");
 peak=recoil.pitch();recoil.update(scope,true,5,duplicates,0);check(recoil.pitch()==peak,"snapshot catches up without replaying drained shot");
 auto world=stage::Collision::make({{-5000,-5000,5000},{5000,-5000,5000},{5000,5000,5000},{-5000,5000,5000}},{{{0,1,2},stage::attribute::bullet},{{0,2,3},stage::attribute::bullet}});
 combat::Snapshot s;s.epoch=7;s.revision=1;auto aim=reticle::aim_point({0,0,0},{0,0,1},world,nullptr,s,scope.identity);
 check(aim&&std::abs((*aim)[2]-5000)<.1,"static contact establishes reticle target");
 combat::Player other;other.identity={1,3,20};other.alive=true;other.pose.feet={0,-850,2000};other.pose.capsule={260,1700,2};s.players[1]=other;
 aim=reticle::aim_point({0,0,0},{0,0,1},world,nullptr,s,scope.identity);check(aim&&std::abs((*aim)[2]-1740)<.1,"nearer body establishes presentation depth");
 auto angle=reticle::camera_angle(.03f,{0,0,0},{0,0,1000},{0,0,-1000},{0,0,1});
 check(angle&&std::abs(std::tan(*angle)-std::tan(.03f)*.5f)<1e-6,"third-person distance reduces finite target footprint");
 check(!reticle::camera_angle(.03f,{0,0,0},{0,0,1000},{0,0,2000},{0,0,1}),"behind-camera target hides reticle");
 auto p=enemy_tag::project({0,0,1000},{-280,0,-1400},{0,0,1},0,0,1280,720,1280.f/720);
 check(p&&p->x<640,"shoulder offset uses shooting ray rather than false centered crosshair");
 std::cout<<"aim presentation PASS: accepted shot scope/replay/decay/bounds and shoulder finite ray projection\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
