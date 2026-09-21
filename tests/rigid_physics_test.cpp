#include "rigid_physics.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt;using namespace physics;
static void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
static stage::Collision floor(float friction=.5f,float restitution=.5f){return stage::Collision::make({{-100000,0,-100000},{100000,0,-100000},{100000,0,100000},{-100000,0,100000}},{{{0,1,2},0,0,0},{{0,2,3},0,0,0}},{{7,friction,restitution,true}});}
int main(){try{
 auto world=floor();RigidBody body;body.position={30,1500,75};for(int i=0;i<360;++i)body.step(world,1.f/120);check(body.valid()&&body.position[1]>=99&&body.position[1]<104&&length(body.velocity)<16,"fall rests above terrain without penetration");check(body.sleeping,"settled body sleeps");
 RigidBody audible;audible.position={30,1500,75};std::vector<ContactImpact> impacts;for(int i=0;i<120;++i)audible.step(world,1.f/120,{0,-9800,0},&impacts);check(impacts.size()==1&&impacts[0].material.id==7&&impacts[0].approachSpeed>300&&std::abs(impacts[0].position[1])<2,"bounded strongest material contact reports pre-impulse impact");
 impacts.clear();for(int i=0;i<240;++i)audible.step(world,1.f/120);audible.step(world,.2f,{0,-9800,0},&impacts);check(impacts.empty(),"resting contact does not repeat impact events");
 auto before=body.position;auto empty=stage::Collision::make({},{});body.step(empty,.1f);check(!body.sleeping&&body.position[1]<before[1]-20,"removed supporting collision wakes a body");
 auto bounce=[&](float e){RigidBody b;b.position={500,1000,700};b.restitution=e;b.linearDamping=0;float rebound=0;for(int i=0;i<220;++i){b.step(floor(.0f,e),1.f/240);rebound=std::max(rebound,b.velocity[1]);check(b.valid()&&b.position[1]>=99,"bouncing body stays above floor");}return rebound;};check(bounce(.8f)>2000&&bounce(0)<1,"material restitution controls rebound");
 auto slide=[&](float f){RigidBody b;b.position={30,101,75};b.velocity={1800,0,0};b.friction=f;b.linearDamping=0;b.angularDamping=0;for(int i=0;i<120;++i)b.step(floor(f,0),1.f/120);check(b.position[0]>300,"floor contact preserves horizontal travel");return b.velocity[0];};float smooth=slide(0),rough=slide(.9f);check(smooth>1750&&rough<smooth*.9f,"material friction slows sliding");
 RigidBody spin;spin.position={0,1000,0};spin.halfLength=200;spin.mass=5;spin.impulse({0,0,5000},{100,1200,0});auto original=spin.rotation;spin.step(empty,.1f,{0,0,0});check(length(spin.angularVelocity)>.1f&&spin.rotation!=original&&finite(spin.rotation),"off-centre impulse rotates an arbitrary capsule");
 auto wall=stage::Collision::make({{2000,-5000,-5000},{2000,5000,-5000},{2000,5000,5000},{2000,-5000,5000}},{{{0,1,2}},{{0,2,3}}});RigidBody fast;fast.position={0,500,0};fast.velocity={50000,0,0};fast.restitution=0;fast.step(wall,.1f,{0,0,0});check(fast.position[0]<=1900.5f&&fast.position[0]>1800,"high speed translation cannot cross a thin wall");
 // Sleep must depend on support, not any remaining contact after a scene update.
 auto corner=stage::Collision::combine(world,std::array{stage::CollisionInstance{1,std::make_shared<const stage::Collision>(wall),{}, {}}});
 RigidBody supported;supported.position={1900,101,0};supported.sleeping=true;supported.step(corner,.1f);check(supported.sleeping&&supported.position[1]==101,"side wall does not hide valid floor support");
 supported.step(wall,.1f);check(!supported.sleeping&&supported.position[1]<81,"floor removal wakes body despite remaining side wall");
 auto ceiling=stage::Collision::make({{-5000,1101,-5000},{5000,1101,-5000},{5000,1101,5000},{-5000,1101,5000}},{{{0,1,2}},{{0,2,3}}});
 RigidBody overhead;overhead.position={0,1000,0};overhead.sleeping=true;overhead.step(ceiling,.1f);check(!overhead.sleeping&&overhead.position[1]<980,"ceiling-only contact cannot suspend a sleeping body");
 RigidBody zero;zero.position={0,1000,0};zero.sleeping=true;zero.step(empty,.1f,{0,0,0});check(zero.sleeping&&zero.position[1]==1000,"zero gravity needs no static support to preserve sleep");
 RigidBody side;side.position={1900,1000,0};side.sleeping=true;side.step(wall,.1f,{9800,0,0});check(side.sleeping&&side.position[0]==1900,"sleep support is measured against supplied gravity");
 auto slope=[](float up){Vec3 n{std::sqrt(1-up*up),up,0},t{-up,n[0],0};auto u=mul(t,5000),v=Vec3{0,0,5000};return stage::Collision::make({add(u,v),sub(u,v),mul(add(u,v),-1),sub(v,u)},{{{0,1,2}},{{0,2,3}}});};
 for(float up:{.39f,.41f}){RigidBody s;s.position=mul(Vec3{std::sqrt(1-up*up),up,0},101);s.sleeping=true;s.step(slope(up),1.f/240);check(s.sleeping==(up>.4f),"existing native strict support cosine threshold retained");}
 auto saved=spin.position;for(float dt:{-1.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})spin.step(empty,dt);check(spin.position==saved,"invalid time leaves simulation unchanged");spin.impulse({std::numeric_limits<float>::quiet_NaN(),0,0},{0,0,0});check(spin.valid(),"invalid impulse rejected");
 auto q=between({0,1,0},{0,-1,0});check(length(sub(rotate(q,{0,1,0}),{0,-1,0}))<.0001f,"opposite capsule axis is well defined");
 std::cout<<"Rigid native drop, restitution, friction, torque, sweep, support removal and rejection passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
