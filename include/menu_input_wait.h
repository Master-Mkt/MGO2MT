#pragma once
#include "controller_input.h"
#include <cstdint>
namespace mgo2win {
// Native menu timing. Gameplay levels/edges and text editing never pass through
// this clock. Rejected input is discarded, never queued for the next screen.
class MenuInputWait {
 uint64_t context_=0,until_=0;bool initialized_=false;
public:
 enum class Kind { none, move, decision };
 static constexpr uint64_t decision_ms=300,move_ms=100;
 void context(uint64_t value,uint64_t now){
  if(initialized_&&value!=context_)wait(now);
  context_=value;initialized_=true;
 }
 void wait(uint64_t now){until_=now+decision_ms;}
 bool accept(Kind kind,uint64_t now,bool repeat=false){
  if(kind==Kind::none)return true;
  if((repeat&&kind==Kind::decision)||now<until_)return false;
  until_=now+(kind==Kind::decision?decision_ms:move_ms);return true;
 }
 static Kind key(unsigned key,bool textEntry=false){
  if(key==VK_RETURN||key==VK_ESCAPE)return Kind::decision;
  if(textEntry)return Kind::none;
  if(key==VK_SPACE||key==VK_BACK||(key>=VK_F1&&key<=VK_F9))return Kind::decision;
  if((key>=VK_PRIOR&&key<=VK_DOWN)||key==VK_TAB)return Kind::move;
  return Kind::none;
 }
};
}
