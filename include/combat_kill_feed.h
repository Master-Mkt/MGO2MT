#pragma once
#include "combat_authority.h"
#include "host_roster.h"
#include <deque>
#include <map>
#include <tuple>
#include <string>
namespace mgo2mt::combat {
// History is independent of actor life / transient effects. Identity is never
// resolved from slot alone, including when a slot has already been reused.
class KillFeed {
 using Key=std::tuple<uint8_t,uint16_t,uint32_t>;
 static Key key(Identity id){return {id.slot,id.instance,id.character};}
 uint64_t epoch_=0,cursor_=0;
 std::map<Key,std::string> names_;
 std::map<Key,uint32_t> deaths_;
public:
 struct Entry{Event event;uint64_t at;std::string source,target,text;};
 std::deque<Entry> entries;
 void scope(uint64_t epoch){if(epoch_!=epoch){*this={};epoch_=epoch;}}
 static std::string clean(std::string s){for(auto&c:s)if(uint8_t(c)<32||c==127)c=' ';if(s.size()>96)s.resize(96);return s;}
 void roster(const host::Roster&r){for(const auto&p:r.slots)if(p)names_[key({p->slot,p->instance,p->character})]=clean(p->name);if(names_.size()>512)names_.clear();}
 std::string name(Identity id)const{auto i=names_.find(key(id));return i==names_.end()||i->second.empty()?"ID "+std::to_string(id.character):i->second;}
 std::vector<Entry> consume(std::span<const Event> events,uint64_t now){
  std::vector<Entry> fresh;
  for(const auto&e:events){if(!epoch_||e.epoch!=epoch_||e.id<=cursor_)continue;cursor_=e.id;
   if(e.kind!=EventKind::death||e.target.slot>=24||!e.target.character||!e.target.instance||!e.targetLife||e.hp)continue;
   auto&life=deaths_[key(e.target)];if(e.targetLife<=life)continue;life=e.targetLife;
   Entry row{e,now,name(e.source),name(e.target),{}};
   row.text=e.source==e.target?row.target+"  [DOWN]":row.source+"  >  "+row.target;
   row.text+="  ["+std::to_string(e.weapon)+"]";entries.push_back(row);fresh.push_back(std::move(row));
  }
  while(entries.size()>5)entries.pop_front();expire(now);return fresh;
 }
 void expire(uint64_t now){while(!entries.empty()&&now>=entries.front().at&&now-entries.front().at>=8000)entries.pop_front();}
};
}
