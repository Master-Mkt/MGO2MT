#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace mgo2mt::hold_selection {
enum class Kind {none,weapons,equipment};
enum class Action {equip,drop};
// connection, epoch, inventory generation, slot, instance, character, life.
using Scope=std::array<uint64_t,7>;
struct Owned {
 uint8_t slot=255;uint32_t item=0;uint64_t revision=0;
 uint32_t quantity=0,magazine=0,reserve=0,charges=0;bool ammunition=false;
 bool operator==(const Owned&)const=default;
};
struct Snapshot {
 Scope scope{};bool eligible=false;
 std::vector<Owned> weapons,equipment;
 std::optional<uint8_t> selectedWeapon,selectedEquipment;
 bool operator==(const Snapshot&)const=default;
};
struct Input {bool weapons=false,equipment=false,left=false,right=false,active=true,otherMenu=false,drop=false;};
struct Request {Scope scope{};Kind kind=Kind::none;Owned item;Action action=Action::equip;bool operator==(const Request&)const=default;};
struct Events {std::optional<Request> confirm;bool cursor=false,cancelled=false,opened=false;};
// Pure preview: release requests equip; a fresh A edge while held requests drop.
// The caller still
// checks current HOST holdings and submits it; no ownership/equip is changed here.
class State {
 Snapshot frozen_;Kind kind_=Kind::none;size_t selected_=0;bool blocked_=true,blocking_=false,left_=false,right_=false,drop_=false;
 Scope scope_{};
public:
 static bool valid(const Snapshot&);
 Events step(const Snapshot&,Input);
 void cancel(); // also requires neutral triggers/directions before another open
 bool visible()const{return kind_!=Kind::none;}
 bool blocks_gameplay()const{return visible()||blocking_;}
 Kind kind()const{return kind_;}
 size_t selected()const{return selected_;}
 const std::vector<Owned>& choices()const{return kind_==Kind::equipment?frozen_.equipment:frozen_.weapons;}
 const Owned* choice()const{return visible()&&selected_<choices().size()?&choices()[selected_]:nullptr;}
};
}
