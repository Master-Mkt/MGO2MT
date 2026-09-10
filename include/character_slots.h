#pragma once
#include "character_client.h"
#include <algorithm>
namespace mgo2win {
// Windows UI rules supplied by the user, not a decompiled original function.
class CharacterSlots {
 unsigned count_=0,selected_=0,capacity_=0;std::vector<uint32_t> ids_;
 bool held_=false,tracking_=false,dialog_=false,yes_=false;
 uint64_t began_=0;uint32_t target_=0;
public:
 void load(const CharacterList& list){reset_hold();dialog_=false;yes_=false;target_=0;ids_.clear();for(auto&e:list.entries)ids_.push_back(e.id);capacity_=character_slot_capacity(list.slots);count_=std::min(8u,std::max(unsigned(ids_.size()),capacity_?capacity_+(capacity_<8):0));selected_=0;}
 unsigned count()const{return count_;}unsigned selected()const{return selected_;}
 unsigned capacity()const{return capacity_;}
 bool purchase_required()const{return !occupied()&&capacity_&&selected_>=capacity_;}
 bool can_create()const{return !occupied()&&selected_<capacity_;}
 bool occupied()const{return selected_<ids_.size()&&ids_[selected_]!=0;}
 uint32_t preview_id()const{return occupied()?ids_[selected_]:0;}
 const wchar_t* action()const{return occupied()?L"PCを選択":purchase_required()?L"PCスロットを購入":L"PCを新規登録";}
 bool select(unsigned i){if(dialog_||i>=count_||i==selected_)return false;reset_hold();selected_=i;return true;}
 void reset_hold(){held_=false;tracking_=false;began_=0;}
 void press(uint64_t now,bool repeat){if(repeat||held_||dialog_||!occupied())return;held_=tracking_=true;began_=now;target_=preview_id();}
 void release(){reset_hold();}
 bool tick(uint64_t now){if(!tracking_||dialog_||!occupied()||target_!=preview_id())return false;if(now<began_){reset_hold();return false;}if(now-began_<3000)return false;tracking_=false;dialog_=true;yes_=false;return true;}
 unsigned held_ms(uint64_t now)const{return tracking_&&now>=began_?unsigned(std::min<uint64_t>(3000,now-began_)):0;}
 bool dialog()const{return dialog_;}bool yes()const{return yes_;}
 void choose(bool yes){if(dialog_)yes_=yes;}
 void cancel_dialog(){dialog_=false;yes_=false;tracking_=false;}
 // UI intent only: consuming this does not issue a network request.
 uint32_t confirm(){uint32_t id=dialog_&&yes_&&occupied()&&target_==preview_id()?target_:0;cancel_dialog();return id;}
};
}
