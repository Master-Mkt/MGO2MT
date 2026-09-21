#pragma once
#include "world_inventory_wire.h"
#include <mutex>
namespace mgo2mt::items {
struct ClientContext {Scope scope;Actor actor;Position position;bool active=false;};
enum class ClientStatus {unavailable,probing,ready};
enum class Delivery {none,pending,confirmed,rejected,unconfirmed};
struct ClientState {
 ClientStatus status=ClientStatus::unavailable;Delivery delivery=Delivery::none;
 ClientContext context;uint64_t connection=0;uint32_t capabilities=0;
 std::optional<wire::Held> held;std::optional<SnapshotState> world;
 ResultCode result=ResultCode::ok;
};
class ClientSession {
 mutable std::mutex mutex_;ClientState state_;wire::Header header_;wire::Receiver receiver_;
 uint64_t next_=0,probeAt_=0,sentAt_=0;bool attempted_=false;
 std::optional<wire::Command> queued_,pending_;
 void reset();
 bool submit_locked(wire::Action,uint8_t,uint64_t);
public:
 void disconnect();ClientState state()const;
 bool submit(wire::Action,uint8_t slot,uint64_t entity=0);
 // Commits the exact hold-menu candidate atomically with scope/life and the
 // local connection incarnation; never substitutes a newly replaced slot.
 bool submit_equip(uint64_t expectedConnection,Scope,Actor,uint8_t slot,uint32_t expectedItem,uint64_t expectedRevision);
 bool submit_drop(uint64_t expectedConnection,Scope,Actor,uint8_t slot,uint32_t expectedItem,uint64_t expectedRevision);
 // Transport calls once per tick; mutation commands are never automatically
 // re-submitted after an unknown result. Outer reliable retransmission remains.
 std::vector<std::vector<uint8_t>> pump(ClientContext,uint64_t now,uint64_t nonce,bool writable,
                                      std::span<const std::vector<uint8_t>> incoming);
};
}
