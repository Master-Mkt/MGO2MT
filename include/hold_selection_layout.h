#pragma once
#include <cstddef>
namespace mgo2mt::hold_selection {
struct CardPosition {int x=0,y=0;};
// Native 1280x720 placement from the recorded t15/t30 L-shaped selector.
// Inventory order/number remains HOST owned; no synthetic NONE/extra slots.
inline CardPosition card_position(bool weapons,size_t count,size_t selected,size_t index){
 const int anchor=weapons?1000:70;
 if(!count||selected>=count||index>=count)return {anchor,598};
 int distance=int((index+count-selected)%count);
 if(distance>int(count/2))distance-=int(count);
 return distance<0?CardPosition{anchor,598+110*distance}:
  CardPosition{anchor+(weapons?-230:230)*distance,598};
}
}
