#pragma once
#include "skill_settings.h"
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
namespace mgo2win::skills {
// Native candidate server contract; never use these opcodes as original MGO2.
struct RemoteProfile {uint32_t character=0,revision=0;unsigned capacity=4,used=0;uint8_t set=0,status=0;uint32_t token=0;Loadout loadout;};
struct RemoteRequest {uint32_t token=0,revision=0;uint8_t set=0;bool write=false;Loadout loadout;};
std::vector<uint8_t> remote_payload(const RemoteRequest&);
RemoteProfile remote_reply(std::span<const uint8_t>);
enum class RemoteStatus {offline,reading,ready,saving,unsupported,rejected,conflict,outcome_unknown};
struct RemoteState {RemoteStatus status=RemoteStatus::offline;std::optional<RemoteProfile> profile;uint32_t character=0;uint8_t error=0;uint64_t serial=0;};
class Remote {
 mutable std::mutex mutex_;RemoteState state_;std::optional<RemoteRequest> queued_,flight_;uint32_t token_=0;uint64_t deadline_=0;
public:
 void character(uint32_t id);
 void connected();void disconnected();
 bool fetch();bool save(const Loadout&);
 RemoteState state()const;
 std::optional<RemoteRequest> take(uint64_t now);
 void receive(const RemoteProfile&,bool writeReply);void tick(uint64_t now);
 bool in_flight()const;
};
}
