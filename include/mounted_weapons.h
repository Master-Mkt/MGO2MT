#pragma once
#include "stage_collision.h"
#include <filesystem>
#include <string>
#include <vector>
namespace mgo2mt::mounted {
using Vec3=stage::Vec3;
enum class Kind:uint8_t {gun,mortar,catapult};
// Native emplaced-weapon adapter. JSON coordinates use the game's millimetres;
// angle limits use radians. These are not original channel-764 actor packets.
struct Type {
 std::string id,name,model;uint16_t weapon=0;
 Vec3 pivot{},muzzle{},operatorOffset{};
 float yawMin=-1.57f,yawMax=1.57f,pitchMin=-.6f,pitchMax=.6f,useRadius=1500;
 bool infiniteAmmo=true;
 std::string rig,maleMotion,femaleMotion;
 Vec3 collisionCenter{},collisionHalfExtents{}; // Zero disables the explicit native base box.
 Vec3 eye{0,1250,-600};
 bool operatorOrbit=false;
 Vec3 ejection{},ejectionDirection{};
 Kind kind=Kind::gun;
 std::string projectileModel;
 float bindPitch=0,initialPitch=0;
 Vec3 launchDirection{0,0,1};
 float launchSpeed=10000,gravity=9800,blastRadius=6000;
 uint32_t cooldownMs=2000,maxFlightMs=10000;
};
struct Instance {uint8_t map=0;uint16_t id=0;std::string type;Vec3 origin{};float yaw=0;};
enum class Action:uint8_t {none,mount,dismount};
struct Intent {Action action=Action::none;uint16_t instance=0;uint32_t request=0;bool operator==(const Intent&)const=default;};
inline bool valid(const Intent&i){return i.action==Action::none?!i.instance&&!i.request:i.request&&(i.action==Action::mount?(i.instance&&i.instance<=32767):i.action==Action::dismount&&!i.instance);}
bool valid(const Type&);bool valid(const Instance&);
struct Registry {
 std::vector<Type> types;std::vector<Instance> placements;
 bool load(const std::filesystem::path&,std::string& error);
 const Type* find(const std::string&)const;
 const Instance* find(uint8_t map,uint16_t instance)const;
 std::vector<Instance> scene(uint8_t map)const;
};
Vec3 rotate(Vec3,float yaw,float pitch=0);
Vec3 operator_position(const Instance&,const Type&);
Vec3 operator_position(const Instance&,const Type&,float yaw);
// Shared client hint/HOST admission range. Inclusive, three-dimensional
// distance from the configured initial operator position, in millimetres.
bool within_use_range(const Instance&,const Type&,Vec3 feet);
// Ordered feet samples along the configured yaw arc; excludes the old position.
std::vector<Vec3> operator_path(const Instance&,const Type&,float fromYaw,float toYaw);
Vec3 pivot_position(const Instance&,const Type&);
Vec3 muzzle_position(const Instance&,const Type&,float yaw,float pitch);
Vec3 launch_direction(const Type&,float yaw,float pitch);
stage::Collision with_collision(const stage::Collision&,const Registry&,uint8_t map);
void clamp_aim(const Instance&,const Type&,float&yaw,float&pitch);
}
