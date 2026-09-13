#pragma once
#include "stage_collision.h"
namespace mgo2win::physics {
using stage::Vec3;
using Quat=std::array<float,4>;
Vec3 add(Vec3,Vec3);Vec3 sub(Vec3,Vec3);Vec3 mul(Vec3,float);
float dot(Vec3,Vec3);Vec3 cross(Vec3,Vec3);float length(Vec3);Vec3 unit(Vec3);
Quat normalize(Quat);Quat multiply(Quat,Quat);Quat conjugate(Quat);Vec3 rotate(Quat,Vec3);Quat between(Vec3,Vec3);
bool finite(Vec3);bool finite(Quat);
// Native CPU approximation in millimetres, seconds and kilograms. This is not
// the original PH integrator/solver. Its policies are documented separately.
struct ContactImpact {Vec3 position{},normal{};float approachSpeed=0;stage::CollisionMaterial material;uint32_t object=0;};
struct RigidBody {
 Vec3 position{},velocity{},angularVelocity{};
 Quat rotation{0,0,0,1};
 float mass=1,radius=100,halfLength=0,friction=.5f,restitution=.2f;
 float linearDamping=.15f,angularDamping=.3f;
 bool sleeping=false;float quietTime=0;
 bool valid()const;
 std::pair<Vec3,Vec3> segment()const;
 Vec3 inverse_inertia(Vec3 worldTorque)const;
 void impulse(Vec3 worldImpulse,Vec3 worldPoint);
 void wake(){sleeping=false;quietTime=0;}
 // At most 0.2 s is advanced per call; invalid/negative time leaves state intact.
 void step(const stage::Collision&,float seconds,Vec3 gravity={0,-9800,0},std::vector<ContactImpact>* impacts=nullptr);
};
// Exposed for articulated solver substeps; dt must be finite and <= 1/120 s.
void integrate(RigidBody&,const stage::Collision&,float dt,Vec3 gravity,std::vector<ContactImpact>* impacts=nullptr);
void solve_static(RigidBody&,const stage::Collision&,float dt,bool allowBounce=true,std::vector<ContactImpact>* impacts=nullptr);
}
