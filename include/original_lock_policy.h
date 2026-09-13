#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>

namespace mgo2win::original_lock {
// Current MGO2.ELF SHA 1a55a41e...bfd13a, not the older debug table.
// Distances are unscaled original stage-coordinate units; no SI conversion is
// performed. Angles use current PPC float constants and signed 16-bit turns.
inline constexpr uint32_t ak102_table_row=0x1293840;
inline constexpr uint32_t range_bonus_actor_flag=0x04000000;
inline constexpr float units_per_radian=std::bit_cast<float>(0x4622f983u);
inline constexpr float radians_per_unit=std::bit_cast<float>(0x38c90fdbu);
inline constexpr float ak102_yaw=std::bit_cast<float>(0x3db2b8c4u);
inline constexpr float vertical_limit=std::bit_cast<float>(0x3f9c61abu);
inline constexpr std::array<float,4> surveyor_range_multiplier{
 std::bit_cast<float>(0x3f666666u),1.f,std::bit_cast<float>(0x3f99999au),std::bit_cast<float>(0x3fb33333u)};
enum class Phase { Acquire, Retain, CrosshairCandidate };
struct Limits {float range,width,yaw,pitch;};
struct Parameters {
 Limits acquire,retain,crosshair;
 const Limits& at(Phase p)const{
  return p==Phase::Retain?retain:p==Phase::CrosshairCandidate?crosshair:acquire;
 }
};

// C05C38's original environment/scalar value must be supplied explicitly.
// Its semantic name is unresolved. There is deliberately no assumed zero.
// Actor flags are the original weapon-info flags queried by D34C90; they are
// not inventory flags, room flags, skill levels, or native identity bits.
// Online 8CC0E0 also applies skill ID7 (SURVEYOR), including the 0.9 factor
// when absent. Level must be supplied explicitly; no skill is inferred/granted.
inline std::optional<Parameters> ak102_parameters(unsigned weaponId,
 uint32_t actorInfoFlags,float originalRangeModifier,unsigned surveyorLevel){
 if(weaponId!=25||surveyorLevel>3||!std::isfinite(originalRangeModifier))return {};
 const float weaponRange=(8000.f*((actorInfoFlags&range_bonus_actor_flag)?1.25f:1.f))*surveyor_range_multiplier[surveyorLevel];
 const float multiplier=std::fma(1.f-originalRangeModifier,.5f,.5f);
 const float unclamped=weaponRange*multiplier;
 if(!std::isfinite(multiplier)||!std::isfinite(unclamped))return {};
 const float acquireRange=std::max(3000.f,unclamped);
 if(!std::isfinite(acquireRange)||!std::isfinite(acquireRange+1000.f))return {};
 return Parameters{{acquireRange,500.f,ak102_yaw,vertical_limit},
  {acquireRange+1000.f,500.f,ak102_yaw+ak102_yaw,vertical_limit},
  {weaponRange,500.f,ak102_yaw,vertical_limit}};
}

inline bool valid(const Limits&p){
 return std::isfinite(p.range)&&p.range>0&&std::isfinite(p.width)&&p.width>=0&&
  std::isfinite(p.yaw)&&p.yaw>=0&&p.yaw<1.5707964f&&
  std::isfinite(p.pitch)&&p.pitch>=0&&p.pitch<1.5707964f;
}
// fctiwz followed by signed halfword extraction. Geometric input below has
// forward>=0, so both angles are within +/-pi/2 and cannot wrap int16.
inline int angular_units(float radians){return static_cast<int>(radians*units_per_radian);}
struct Angles {int unadjustedPitch,widthAdjustedPitch,widthAdjustedYaw;};

// Exact scalar inequalities recovered from CCDB38 / CCC5F0. Values at range,
// width, and signed angle bounds are included; behind-view and zero-distance
// candidates are excluded. Callers must still enforce class/identity/visibility.
inline bool accepts_quantized(const Limits&p,float distance,float right,float forward,Angles a){
 if(!valid(p)||!std::isfinite(distance)||!std::isfinite(right)||!std::isfinite(forward)||
  distance<=0||distance>p.range||forward<0)return false;
 const int yaw=angular_units(p.yaw),pitch=angular_units(p.pitch);
 auto within=[](int value,int bound){return value>=-bound&&value<=bound;};
 if(std::abs(right)<=p.width)return within(a.unadjustedPitch,pitch);
 return within(a.widthAdjustedPitch,pitch)&&within(a.widthAdjustedYaw,yaw);
}
struct Geometry {bool accepted=false;float distance=0;Angles angles{};};

// Input is target-relative coordinates transformed into the ORIGINAL aim
// frame: +x right, +y up, +z forward. The aim frame need not be the eye camera.
// Portable angular evaluation uses atan2; original PPC uses a polynomial and
// reciprocal estimate. Scalar bounds/quantization match, but a last angular
// quantum at a threshold is not promised bit-identical to that PS3 arithmetic.
inline Geometry evaluate_local(const Limits&p,std::array<float,3> local){
 Geometry result;
 if(!valid(p)||!std::isfinite(local[0])||!std::isfinite(local[1])||!std::isfinite(local[2]))return result;
 const float x=local[0],y=local[1],z=local[2];
 result.distance=std::sqrt(std::fma(y,y,x*x)+z*z);
 if(!std::isfinite(result.distance)||result.distance<=0||result.distance>p.range||z<0)return result;
 const float adjusted=x>p.width?x-p.width:x< -p.width?x+p.width:0.f;
 result.angles.unadjustedPitch=angular_units(std::atan2(-y,std::hypot(x,z)));
 result.angles.widthAdjustedPitch=angular_units(std::atan2(-y,std::hypot(adjusted,z)));
 result.angles.widthAdjustedYaw=angular_units(std::atan2(adjusted,z));
 result.accepted=accepts_quantized(p,result.distance,x,z,result.angles);
 return result;
}

// CCDB38 mode0: choose shortest original distance within the first eligible
// class group. Equal distance retains the earlier enumerated candidate.
// Native roster order is an adapter decision, not proof of original class order.
inline bool prefer_mode0(float distance,std::optional<float> previous){
 return std::isfinite(distance)&&distance>0&&(!previous||distance<*previous);
}
} // namespace mgo2win::original_lock
