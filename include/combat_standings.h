#pragma once
#include "combat_wire.h"
#include <algorithm>
namespace mgo2mt::combat {
struct Standing {Identity id;uint32_t kills=0,deaths=0;unsigned rank=0;};
// Local DM scoring: one confirmed enemy elimination, no inferred original
// headshot bonus or death tie-break. Equal kills share the same place.
inline std::vector<Standing> standings(const wire::Preparation&p){
 std::vector<Standing> out;if(!p.freeForAll)return out;
 for(const auto&x:p.players)if(x)out.push_back({x->id,x->kills,x->deaths});
 std::stable_sort(out.begin(),out.end(),[](const auto&a,const auto&b){return a.kills>b.kills;});
 for(size_t i=0;i<out.size();++i)out[i].rank=i&&out[i].kills==out[i-1].kills?out[i-1].rank:unsigned(i+1);
 return out;
}
}
