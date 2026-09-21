#pragma once
#include "stage_collision.h"
#include <cstdint>
#include <optional>
#include <cmath>
namespace mgo2mt::combat::cover {
using Vec3=stage::Vec3;
enum class Action:uint8_t {none,attach,detach};
struct Intent {uint32_t request=0;Action action=Action::none;int8_t lean=0;bool firstPerson=false;bool operator==(const Intent&)const=default;};
struct State {bool attached=false;int8_t lean=0;float normalYaw=0;bool operator==(const State&)const=default;};
struct Policy {float slideSpeed=620,coverLean=420,freeLean=125,probeGap=140,upperRadius=85;};
inline constexpr Policy native_policy{}; // Native extension, not recovered MGO2 constants.
struct Contact {Vec3 normal{},point{};};
inline bool valid(const State&s)noexcept{return s.lean>=-1&&s.lean<=1&&std::isfinite(s.normalYaw)&&std::abs(s.normalYaw)<=3.14159274f&&(s.attached||s.normalYaw==0);}
inline bool valid(const Intent&i)noexcept{return unsigned(i.action)<=unsigned(Action::detach)&&((i.action==Action::none)==(i.request==0))&&i.lean>=-1&&i.lean<=1;}
std::optional<Contact> acquire(const stage::Collision&,Vec3 feet,stage::Capsule,float yaw);
Vec3 projected_destination(const stage::Collision&,Vec3 feet,Vec3 requested,stage::Capsule,const State&);
State evaluate(const stage::Collision&,Vec3 feet,stage::Capsule,float yaw,State,int8_t requestedLean,bool firstPerson);
Vec3 eye_offset(const State&,float yaw);
Vec3 eye(Vec3 feet,stage::Capsule,const State&,float yaw);
bool fire_allowed(const stage::Collision&,Vec3 feet,stage::Capsule,float yaw,const State&);
}
