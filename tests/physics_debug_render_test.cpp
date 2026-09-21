#include "physics_debug_render.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt;
using namespace physics_debug;
namespace {void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}size_t lit(const std::vector<uint32_t>&p){return std::count_if(p.begin(),p.end(),[](auto v){return v!=0;});}}
int main(){try{
 Frame shape;shape.standing({100,200,300},{260,1700,2});check(shape.lines().size()==76,"Capsule ring/hemisphere topology");float minY=1e9f,maxY=-1e9f;for(auto l:shape.lines())for(auto p:{l.a,l.b}){minY=(std::min)(minY,p[1]);maxY=(std::max)(maxY,p[1]);}check(std::abs(minY-200)<.01f&&std::abs(maxY-1900)<.01f,"Exact solver capsule feet and height");
 Frame ball;ball.capsule({0,0,0},{0,0,0},30);check(ball.lines().size()==36,"Degenerate capsule uses sphere");Frame box;box.box({1,2,3},{4,5,6});check(box.lines().size()==12,"Finite box has 12 edges");
 physics::RigidBody body;body.position={10,20,30};body.radius=17;body.halfLength=40;body.rotation={0,0,.70710678f,.70710678f};auto before=body;Frame rb;rb.rigid(body);check(body.position==before.position&&body.rotation==before.rotation,"Inspection does not move rigid body");check(rb.lines().size()==76,"Actual rigid segment used");
 PreparedCharacter actor;actor.bonePositions[1]={0,0,0};actor.bonePositions[2]={100,0,0};std::array<CatalogBone,2>bones{{{1,-1,{},{}},{2,0,{},{}}}};Frame sk;sk.skeleton(actor,bones,{10,20,30},1.57079632679f);check(sk.lines().size()==1&&std::abs(sk.lines()[0].a[2]+70)<.01f&&sk.lines()[0].b==Vec3{10,20,30},"Actual bone parent and actor transform");
 Frame bounded({3,1,20,100});for(unsigned i=0;i<100;++i)bounded.line({-10,0,50},{10,0,50},Kind::body);check(bounded.lines().size()==3&&bounded.stats().omitted==97,"Line cap");std::vector<uint32_t> p(128*72);View v;v.width=128;v.height=72;auto ps=paint(p,128,72,bounded,v);check(ps.pixels==20&&lit(p)<=20,"Hard pixel budget");
 Frame clip;clip.line({-10000,0,100},{10000,0,100},Kind::body);std::fill(p.begin(),p.end(),0);auto clipped=paint(p,128,72,clip,v);check(clipped.pixels==128&&lit(p)==128,"Offscreen endpoints clip to full viewport");Frame near;near.line({0,-5,-10},{0,5,100},Kind::rigid);std::fill(p.begin(),p.end(),0);check(paint(p,128,72,near,v).pixels>0,"Near plane crossing survives");
 Frame location;location.line({10,0,100},{10,0,100},Kind::bone);std::fill(p.begin(),p.end(),0);paint(p,128,72,location,v);size_t x=0;while(!p[x])++x;check(x%128<64,"Source horizontal handedness");v.verticalFov=.4f;std::fill(p.begin(),p.end(),0);paint(p,128,72,location,v);size_t zoomX=0;while(!p[zoomX])++zoomX;check(zoomX%128<x%128,"Lens FOV expands projected offset");v.verticalFov=std::numeric_limits<float>::quiet_NaN();auto copy=p;paint(p,128,72,clip,v);check(p==copy,"Invalid FOV leaves output unchanged");
 auto world=stage::Collision::make({{0,0,0},{50,0,0},{0,0,50},{10000,0,0},{10050,0,0},{10000,0,50}},{{{0,1,2}},{{3,4,5}}});Frame terrain({24,1,1000,100});terrain.terrain(world,{0,0,0});check(terrain.stats().triangles==1&&terrain.lines().size()==3,"Bounded nearby original triangles only");
 for(auto [flags,kind]:{std::pair{stage::attribute::native_solid,Kind::terrain},std::pair{stage::attribute::player|stage::attribute::cliff,Kind::trigger},std::pair{stage::attribute::dont_fall,Kind::fallBarrier}}){
  const auto tagged=stage::Collision::make({{0,0,0},{50,0,0},{0,0,50}},{{{0,1,2},flags}});Frame frame;frame.terrain(tagged,{0,0,0});
  check(frame.lines().size()==3&&std::all_of(frame.lines().begin(),frame.lines().end(),[&](const auto& l){return l.kind==kind;}),"F12 distinguishes physical, cliff trigger and explicit fall barrier");
 }
 std::cout<<"Physics debug shapes, real bone transforms, clipping, FOV, and budgets PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
