#pragma once
#include "original_reload_timing.h"
namespace mgo2mt::original_weapon {
// Original selectors and MTAR headers, normal actor mode0 / nonempty-magazine.
// Refill uses verified585238 events when compatible; otherwise native clip-end.
// Specialized shell-by-shell M870 and RPG actions are not this generic table.
inline std::optional<original::ReloadMotion> reload_motion(uint16_t id,uint8_t posture=0){if(posture>2)return {};switch(id){
case 2:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,114,405,1.f},original::ReloadMotion{5,114,405,1.f},original::ReloadMotion{5,114,405,1.f}};return m[posture];}
case 3:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,114,405,1.f},original::ReloadMotion{5,114,405,1.f},original::ReloadMotion{5,114,405,1.f}};return m[posture];}
case 4:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,114,405,1.f},original::ReloadMotion{5,114,405,1.f},original::ReloadMotion{5,114,405,1.f}};return m[posture];}
case 7:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,114,405,1.f},original::ReloadMotion{5,114,405,1.f},original::ReloadMotion{5,114,405,1.f}};return m[posture];}
case 8:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,114,405,1.f},original::ReloadMotion{5,114,405,1.f},original::ReloadMotion{5,114,405,1.f}};return m[posture];}
case 15:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,114,405,1.f},original::ReloadMotion{5,114,405,1.f},original::ReloadMotion{5,114,405,1.f}};return m[posture];}
case 18:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,235,1170,1.f},original::ReloadMotion{5,235,1170,1.f},original::ReloadMotion{5,235,1170,1.f}};return m[posture];}
case 20:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,210,735,1.f},original::ReloadMotion{5,210,735,1.f},original::ReloadMotion{5,210,735,1.f}};return m[posture];}
case 23:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,210,660,1.f},original::ReloadMotion{5,210,660,1.f},original::ReloadMotion{5,210,660,1.f}};return m[posture];}
case 24:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,50,245,1.f},original::ReloadMotion{5,100,495,1.f},original::ReloadMotion{5,100,495,1.f}};return m[posture];}
case 25:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,210,650,1.f},original::ReloadMotion{5,210,650,1.f},original::ReloadMotion{5,210,650,1.f}};return m[posture];}
case 26:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,240,1195,1.f},original::ReloadMotion{5,240,1195,1.f},original::ReloadMotion{5,240,1195,1.f}};return m[posture];}
case 30:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,210,655,1.f},original::ReloadMotion{5,210,655,1.f},original::ReloadMotion{5,210,655,1.f}};return m[posture];}
case 31:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,210,705,1.f},original::ReloadMotion{5,210,705,1.f},original::ReloadMotion{5,210,705,1.f}};return m[posture];}
case 35:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,333,1660,1.f},original::ReloadMotion{5,333,1660,1.f},original::ReloadMotion{5,333,1660,1.f}};return m[posture];}
case 38:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,210,650,1.f},original::ReloadMotion{5,210,650,1.f},original::ReloadMotion{5,210,650,1.f}};return m[posture];}
case 39:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,210,1045,1.f},original::ReloadMotion{5,210,1045,1.f},original::ReloadMotion{5,210,1045,1.f}};return m[posture];}
case 41:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,237,1180,1.f},original::ReloadMotion{5,237,1180,1.f},original::ReloadMotion{5,237,1180,1.f}};return m[posture];}
case 42:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,90,445,1.f},original::ReloadMotion{5,100,495,1.f},original::ReloadMotion{5,100,495,1.f}};return m[posture];}
case 43:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,189,550,1.f},original::ReloadMotion{5,189,550,1.f},original::ReloadMotion{5,189,550,1.f}};return m[posture];}
case 44:{constexpr original::ReloadMotion m[]{original::ReloadMotion{5,210,1045,1.f},original::ReloadMotion{5,210,1045,1.f},original::ReloadMotion{5,210,1045,1.f}};return m[posture];}
default:return {};}}
}
