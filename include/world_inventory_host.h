#pragma once
#include "combat_authority.h"
#include "world_inventory_wire.h"
#include <deque>
namespace mgo2win::items {
// Optional native transport adapter. Authority owns all inventory mutations.
class HostSession {
 struct Peer {wire::Header header;uint64_t worldRevision=0,nextPublish=0;std::optional<wire::Held> held;std::deque<std::vector<uint8_t>> pending,priority;};
 std::map<uint32_t,Peer> peers_;
public:
 void clear(){peers_.clear();}
 void remove(combat::Identity id){auto it=peers_.find(id.character);if(it!=peers_.end()&&it->second.header.actor.instance==id.instance&&it->second.header.actor.slot==id.slot)peers_.erase(it);}
 bool receive(combat::Authority&,combat::Identity,std::span<const uint8_t>,uint64_t now);
 void poll(combat::Authority&,uint64_t now);
 const std::vector<uint8_t>* front(combat::Identity)const;
 void pop(combat::Identity);
};
}
