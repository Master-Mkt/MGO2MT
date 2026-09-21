#pragma once
#include <array>
#include <cmath>
#include <cstdint>
namespace mgo2mt::original_first_person {
// Current ELF 1a55a41e..bfd13a: 8C9C28, 32A090, 2EE598, FB7C8, 162010.
// See notes/FIRST_PERSON_LENS_20260921.md. Coefficients are not angles.
inline constexpr float base_horizontal_lens=1.74f;
inline constexpr std::array<float,4> hawkeye_zoom{{1.f,1.1f,1.2f,1.3f}};
inline constexpr std::array<std::uint8_t,74> weapon_classes{{0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,13,3,3,3,3,3,3,3,4,4,4,4,4,1,4,4,4,4,4,4,13,5,5,6,6,6,6,6,6,10,9,2,1,10,10,10,8,8,8,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,2,2,11}};
constexpr std::uint8_t weapon_class(std::uint16_t id){return id<weapon_classes.size()?weapon_classes[id]:0;}
constexpr bool scope_capable(std::uint16_t id){return id==9||(id>=39&&id<=45)||id==49||id==50;}
constexpr float unscoped_aim_zoom(std::uint16_t id,unsigned hawkeyeLevel){
 const auto level=hawkeyeLevel<4?hawkeyeLevel:0; // Reject malformed native skill data.
 switch(weapon_class(id)){
 case 2:return hawkeye_zoom[level];
 case 3:case 5:return 1.2f*hawkeye_zoom[level];
 case 4:case 6:case 10:return 1.5f*hawkeye_zoom[level];
 default:return 1.f;
 }
}
// Scope activation itself belongs to input/state. Original scoped mode excludes HAWKEYE.
constexpr float aim_zoom(std::uint16_t id,unsigned level,bool scoped=false,bool highScope=false){
 return scoped&&scope_capable(id)?(highScope?10.f:3.f):unscoped_aim_zoom(id,level);
}
constexpr float horizontal_lens(std::uint16_t id,unsigned level,bool scoped=false,bool highScope=false){
 return base_horizontal_lens*aim_zoom(id,level,scoped,highScope);
}
inline float horizontal_fov_degrees(std::uint16_t id,unsigned level,bool scoped=false,bool highScope=false){
 return 2.f*std::atan(1.f/horizontal_lens(id,level,scoped,highScope))*57.29577951308232f;
}
// Square-pixel native adaptation: source uses -4/3*internalWidth/internalHeight.
// Pass displayed width/height once; do not also apply the source pixel-aspect factor.
inline float vertical_fov_degrees(std::uint16_t id,unsigned level,float displayAspect,bool scoped=false,bool highScope=false){
 if(!std::isfinite(displayAspect)||displayAspect<=0.f)displayAspect=16.f/9.f;
 return 2.f*std::atan(1.f/(horizontal_lens(id,level,scoped,highScope)*displayAspect))*57.29577951308232f;
}
}
