#pragma once
#include <array>
#include <cstdint>
#include <optional>
namespace mgo2mt::original_weapon {
// Generated from current ELF SHA1a55a41e..bfd13a. See audit_weapon_expansion.py.
// Damage: D3A3B8 default packed-ID branch. 82628C..94 selects HP when positive;
// therefore negative ST sentinel is not an extra stamina damage on M4/SAIGA.
// Candidate mass at spec+24 is preserved for investigation, not movement policy.
struct Ballistic {float range,speed,decayStart,decayEnd;int minimumForce,penetration;};
struct Parameters {uint16_t id,magazine;int hp,staminaRaw,intervalTicks,massCandidate;float lockRange,lockWidth,lockYaw;Ballistic bullet;};
inline constexpr std::array<Parameters,22> firearms{{
 {2,10,0,245,-1,1100,13000.f,500.f,0.52359885f,{76000.f,145000.f,76000.f,76000.f,1000,50}},
 {3,7,170,0,50,1000,11000.f,500.f,0.17453295f,{71000.f,124000.f,7100.f,20000.f,500,80}},
 {4,12,170,0,45,1100,20000.f,500.f,0.261799425f,{90000.f,124000.f,9000.f,20000.f,500,80}},
 {7,8,170,0,50,1180,10000.f,500.f,0.261799425f,{63500.f,124000.f,6350.f,20000.f,500,80}},
 {8,7,330,0,75,2053,12000.f,500.f,0.17453295f,{90000.f,212500.f,6000.f,40000.f,400,80}},
 {15,33,150,0,20,865,13000.f,500.f,0.261799425f,{60000.f,170500.f,7000.f,25000.f,500,80}},
 {18,30,150,0,20,3080,17000.f,500.f,0.52359885f,{160000.f,170500.f,17000.f,65000.f,500,80}},
 {20,50,140,0,20,3000,23000.f,500.f,0.17453295f,{135000.f,357500.f,14000.f,65000.f,700,290}},
 {23,20,150,0,25,1408,20000.f,500.f,0.261799425f,{75000.f,155000.f,10000.f,50000.f,500,80}},
 {24,30,225,-1,25,3000,7000.f,500.f,0.087266475f,{220000.f,473750.f,20000.f,90000.f,650,250}},
 {25,30,275,0,30,3000,8000.f,500.f,0.087266475f,{200000.f,473750.f,15000.f,80000.f,550,250}},
 {26,20,330,0,30,4410,9500.f,500.f,0.087266475f,{230000.f,419000.f,18000.f,75000.f,700,250}},
 {30,20,350,0,30,4064,9000.f,500.f,0.087266475f,{200000.f,419000.f,15000.f,70000.f,650,250}},
 {31,30,250,0,25,2800,10000.f,500.f,0.261799425f,{220000.f,473750.f,25000.f,100000.f,700,250}},
 {35,200,400,0,35,5750,8000.f,500.f,0.087266475f,{260000.f,419000.f,25000.f,95000.f,650,250}},
 {37,5,500,0,-1,3500,5000.f,500.f,0.52359885f,{0.f,0.f,0.f,0.f,0,0}},
 {38,8,500,-1,150,3800,5000.f,500.f,0.52359885f,{0.f,0.f,0.f,0.f,0,0}},
 {39,10,300,0,35,3410,0.f,500.f,0.f,{300000.f,2400000.f,300000.f,300000.f,1000,750}},
 {41,5,1200,0,-1,5958,0.f,500.f,0.f,{300000.f,2400000.f,250000.f,300000.f,1000,400}},
 {42,20,333,0,30,4450,0.f,500.f,0.f,{230000.f,2400000.f,50000.f,100000.f,700,290}},
 {43,5,0,800,-1,4400,0.f,500.f,0.f,{300000.f,2400000.f,300000.f,300000.f,1000,50}},
 {44,10,500,0,75,4415,0.f,500.f,0.f,{280000.f,2400000.f,100000.f,200000.f,700,400}},
}};
constexpr const Parameters* find(uint16_t id){for(const auto& p:firearms)if(p.id==id)return &p;return nullptr;}
constexpr bool autoaim(uint16_t id){auto p=find(id);return p&&p->lockRange>0;}
// Native mode policy; original fire-mode input and selector are separate from cadence.
constexpr bool automatic(uint16_t id){return id==15||id==18||id==20||id==23||id==24||id==25||id==26||id==30||id==31||id==35||id==39;}
// Same original bullet update arithmetic with the ID-specific table values.
inline int force(const Ballistic& b,float distance,int priorCost){
 if(distance>=b.decayEnd)return b.minimumForce;
 float cost=float(priorCost);if(distance>b.decayStart&&b.decayEnd>b.decayStart)cost+=(distance-b.decayStart)*float(1000-b.minimumForce)/(b.decayEnd-b.decayStart);
 return 1000-int(cost);
}
}
