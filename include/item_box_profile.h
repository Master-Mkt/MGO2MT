#pragma once
#include "world_inventory.h"
#include "weapon_catalog.h"
#include "stage_collision.h"
namespace mgo2mt::item_box {
enum class Size:uint8_t {primary,secondary,reserve,equipment,unknown};
struct Profile {Size size=Size::unknown;stage::Vec3 halfExtent{150,150,150};};
// Native box dimensions, not a recovered original item-mesh/model namespace.
inline constexpr Profile profile(Size size){switch(size){
 case Size::primary:return {size,{300,130,130}};
 case Size::secondary:return {size,{160,110,110}};
 case Size::reserve:return {size,{120,120,120}};
 case Size::equipment:return {size,{150,150,150}};
 default:return {Size::unknown,{150,150,150}};
}}
inline Profile profile(const items::Contents& item,const weapons::Catalog* catalog){
 if(item.domain==items::Domain::equipment)return profile(Size::equipment);
 if(item.domain!=items::Domain::weapon||!item.item||item.item>65535||!catalog)return profile(Size::unknown);
 const weapons::Entry* found=nullptr;
 for(auto category:{weapons::Category::primary,weapons::Category::secondary,weapons::Category::support})if(auto row=catalog->find(category,uint16_t(item.item))){if(found)return profile(Size::unknown);found=row;}
 if(!found)return profile(Size::unknown);
 return profile(found->category==weapons::Category::primary?Size::primary:found->category==weapons::Category::secondary?Size::secondary:Size::reserve);
}
}
