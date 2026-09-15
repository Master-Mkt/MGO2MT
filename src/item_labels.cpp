#include "round_items.h"
namespace mgo2win::items {
std::string item_label(Domain domain,uint32_t id){
 if(domain==Domain::equipment){if(id==16)return "STEALTH";if(id==19)return "BINOCULARS";return "EQUIPMENT "+std::to_string(id);}
 switch(id){case 1:return "STUN KNIFE";case 2:return "MK.2";case 3:return "OPERATOR";case 22:return "PATRIOT";case 25:return "AK102";case 50:return "RPG-7";case 52:return "GRENADE";case 53:return "WHITE PHOSPHORUS";default:return "WEAPON "+std::to_string(id);}
}
}
