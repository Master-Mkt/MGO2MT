#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <optional>

namespace mgo2win::original_hit_regions {
using Vec3=std::array<float,3>;
// Current 3F47C0 -> 63F80 -> 19C680 / 17FDA0. Original coordinate units.
// The parent dot (radius 1000) is a broad-phase parent, not a damage region.
struct Box {uint8_t bone;Vec3 halfExtent,offset;uint32_t nameHash;};
inline constexpr std::array<Box,12> boxes{{
 {4,{95,110,98},{0,68,2},0x293c7c},
 {3,{65,40,65},{0,60,-20},0x2c3cc3},
 {2,{180,210,100},{0,90,0},0xc3f36d},
 {0,{180,120,110},{0,10,20},0x15b1da},
 {10,{160,60,60},{-80,0,0},0x7d29c0},
 {11,{180,40,40},{-120,0,0},0xc491b2},
 {6,{160,60,60},{80,0,0},0x7d2840},
 {7,{180,40,40},{120,0,0},0x6491b2},
 {17,{80,240,100},{0,-110,-10},0x71c85b},
 {18,{60,300,60},{0,-180,-40},0x9b0e46},
 {13,{80,240,100},{0,-110,-10},0x71c6db},
 {14,{60,300,60},{0,-180,-40},0x9b0e3a}
}};
enum class Region:uint8_t {body,headOrNeck,limb};
constexpr Region region(uint8_t rawJoint){return rawJoint<=2?Region::body:rawJoint<=4?Region::headOrNeck:Region::limb;}
// 826AC8 / 8272DC: shared game +4C bit 1, target 8185E0()==4
// exception and original RNG sign. These are explicit inputs, not native defaults.
constexpr bool headshot_allowed(uint32_t sharedFlags,uint32_t targetClass,int32_t nextRandom){
 return (sharedFlags&2)!=0&&(targetClass!=4||nextRandom<0);
}
struct Result {int32_t damage=0;bool headshot=false;};
// Normal AK packed-weapon branch (ID25, 0x219 plus allowed projectile flags).
// Input baseDamage is AFTER D3A3B8's force/1000 integer calculation. Result is
// BEFORE the original virtual damage cap, friendly-team /2 and HP subtraction.
// Current 826AE4/826B04/826B14/826B24/826B9C, 827168 and 11C715C.
// forceWord is the raw metadata uint16; 0 is NOT substituted with 1000 here.
inline std::optional<Result> ak102_region_damage(int32_t baseDamage,int32_t maximumHp,
 uint8_t rawJoint,uint16_t forceWord,bool projectileHeadshotFlag,bool headshotAllowed){
 if(baseDamage<0||maximumHp<=0)return {};
 float scale=1;int32_t value=baseDamage;bool hs=false;
 switch(region(rawJoint)){
  case Region::limb:scale=0.60000002384185791015625f;break;
  case Region::headOrNeck:
   if(projectileHeadshotFlag&&headshotAllowed){hs=true;if(forceWord==1000){if(value<maximumHp)value=maximumHp;}else scale=4;}
   break;
  case Region::body:break;
 }
 // PPC fcfid/frsp/fmuls/fctiwz, including binary32 rounding before truncation.
 float result=float(value)*scale;
 if(double(result)>double(std::numeric_limits<int32_t>::max()))return {};
 return Result{int32_t(result),hs};
}
}
