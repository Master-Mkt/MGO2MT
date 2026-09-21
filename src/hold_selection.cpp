#include "hold_selection.h"
#include <algorithm>
#include <set>

namespace mgo2mt::hold_selection {
bool State::valid(const Snapshot& value){
 const auto& s=value.scope;
 if(!s[0]||!s[1]||!s[2]||s[3]>=24||!s[4]||s[4]>65535||!s[5]||s[5]>UINT32_MAX||!s[6]||s[6]>UINT32_MAX||value.weapons.size()+value.equipment.size()>255)return false;
 std::set<uint8_t> slots;
 for(const auto* rows:{&value.weapons,&value.equipment})for(const auto& row:*rows)if(row.slot==255||!row.item||!row.revision||!row.quantity||!slots.insert(row.slot).second)return false;
 auto selected=[](const auto& rows,std::optional<uint8_t> slot){return !slot||std::any_of(rows.begin(),rows.end(),[&](const auto& row){return row.slot==*slot;});};
 return selected(value.weapons,value.selectedWeapon)&&selected(value.equipment,value.selectedEquipment);
}
void State::cancel(){blocking_|=visible();kind_=Kind::none;selected_=0;frozen_={};blocked_=true;left_=right_=drop_=false;}
Events State::step(const Snapshot& snapshot,Input input){
 Events result;const bool neutral=!input.weapons&&!input.equipment&&!input.left&&!input.right&&!input.drop;
 if(neutral)blocking_=false;else if(input.weapons||input.equipment)blocking_=true;
 if(!input.active||input.otherMenu||!snapshot.eligible||!valid(snapshot)){
  result.cancelled=visible();cancel();return result;
 }
 if(scope_!=snapshot.scope){result.cancelled=visible();cancel();scope_=snapshot.scope;}
 if(input.weapons&&input.equipment){result.cancelled=visible();cancel();return result;}
 if(visible()&&frozen_!=snapshot){result.cancelled=true;cancel();return result;}
 if(blocked_){if(neutral)blocked_=false;return result;}
 if(!visible()){
  if(!input.weapons&&!input.equipment)return result;
  frozen_=snapshot;kind_=input.weapons?Kind::weapons:Kind::equipment;selected_=0;
  const auto selected=kind_==Kind::weapons?snapshot.selectedWeapon:snapshot.selectedEquipment;
  for(size_t i=0;selected&&i<choices().size();++i)if(choices()[i].slot==*selected)selected_=i;
  left_=input.left;right_=input.right;drop_=input.drop;result.opened=true;return result;
 }
 const bool ownHeld=kind_==Kind::weapons?input.weapons:input.equipment;
 const bool otherHeld=kind_==Kind::weapons?input.equipment:input.weapons;
 if(otherHeld){result.cancelled=true;cancel();return result;}
 if(ownHeld&&input.drop&&!drop_){if(const auto* item=choice())result.confirm=Request{scope_,kind_,*item,Action::drop};else result.cancelled=true;cancel();return result;}
 if(!ownHeld&&input.drop){result.cancelled=true;cancel();return result;}
 if(!ownHeld){if(const auto* item=choice())result.confirm=Request{scope_,kind_,*item};else result.cancelled=true;cancel();return result;}
 if(input.left!=input.right&&choices().size()>1){
  if(input.left&&!left_){selected_=(selected_+choices().size()-1)%choices().size();result.cursor=true;}
  else if(input.right&&!right_){selected_=(selected_+1)%choices().size();result.cursor=true;}
 }
 left_=input.left;right_=input.right;drop_=input.drop;return result;
}
}
