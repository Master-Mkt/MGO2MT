#pragma once
#include "combat_wire.h"
namespace mgo2mt::combat {
// Native control mapping: unraised firearms punch; equipped knife keeps its
// dedicated attack. No input here selects or equips a different weapon.
inline void select_attack(wire::Input& input,bool raised,bool firstPerson,bool specialPc,bool coverAttached){
 if(specialPc||input.weapon==1||raised||firstPerson)return;
 input.meleePressed=input.weapon&&input.firePressed&&!input.suspended&&!input.reload&&!input.specialPressed&&!input.specialHeld&&!input.evadeRequest&&!input.cover.request&&!input.specialPc.request&&input.ladder==ladder::Intent{}&&!coverAttached;
 input.fire=input.firePressed=input.aiming=false;
}
}
