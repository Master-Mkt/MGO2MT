#pragma once
#include "character_catalog.h"
#include "rigid_physics.h"
namespace mgo2mt::player {
class Ragdoll {
 struct Bone {uint32_t key;int parent;physics::Vec3 bind;physics::Quat local;};
 struct Link {unsigned child,parent;physics::Vec3 childAnchor,parentAnchor;physics::Quat rest;float cone;};
 std::vector<Bone> skeleton_;
 std::array<physics::RigidBody,13> bodies_;
 std::array<unsigned,13> modelIndices_{};
 std::array<physics::Quat,13> axes_{};
 std::array<physics::Vec3,13> offsets_{};
 std::array<Link,12> links_{};
 bool active_=false;float originY_=0;
 void constraints();
public:
 // Original MGO2 table topology and bone hashes, native shape/solver policies.
 // All requested bodies must map; unsupported skeletons fail without activation.
 bool start(const CharacterCatalog&,unsigned gender,const MotionPose&,physics::Vec3 actorOrigin,float yaw);
 void step(const stage::Collision&,float seconds);
 void impulse(physics::Vec3 worldImpulse,physics::Vec3 worldPoint);
 // HOST blast velocity, distributed to all bodies to preserve the joints.
 bool launch(physics::Vec3 worldVelocity);
 MotionPose pose()const;
 physics::Vec3 origin()const;
 physics::Vec3 root_position()const;
 bool supine()const;
 bool active()const{return active_;}
 void stop(){active_=false;}
 std::span<const physics::RigidBody> bodies()const{return active_?std::span<const physics::RigidBody>(bodies_):std::span<const physics::RigidBody>{};}
 float maximum_joint_error()const;
};
}
