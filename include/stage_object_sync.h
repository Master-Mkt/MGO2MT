#pragma once
#include "host_match.h"
#include "host_placements.h"
#include <optional>
#include <filesystem>

namespace mgo2win::stage {
struct RegistryEntry {
 uint32_t bindingId=0;
 uint8_t width=0;
 host::ObjectStates::Update update=host::ObjectStates::Update::bits;
};
struct ObjectRegistry {
 uint8_t map=0;
 // Literal actor registration order, including actors without scene geometry.
 std::vector<RegistryEntry> entries;
 // Original profiles require reviewed constructors/order/widths; explicitly
 // native profiles describe a complete, deliberately bounded native schema.
 bool complete=false;
 // Narrow reviewed rule scope; nullopt is reserved for independently verified fixtures.
 std::optional<uint8_t> rule;
 // Explicit local static-only schema; no original OLObjMan registration claim.
 bool nativeStatic=false;
 bool combatRulesOnly=false;
 // Explicit BB lamp-only schema, distinct from the original full OLObj order.
 bool nativeLights=false;
};
// Strict loader for reviewed original or explicitly named native stage profiles.
ObjectRegistry load_object_registry(const std::filesystem::path&);
// Normal DM/TDM share n022a main -> proc87 registration; other rules rejected.
ObjectRegistry load_combat_object_registry(const std::filesystem::path&);
// Shared authoritative state. Call update only after host gameplay validates
// the event; a received peer record is not authorization to destroy an object.
class SceneAuthority {
 ObjectRegistry registry_;std::optional<host::LoadRequest> request_;std::vector<uint8_t> values_;
public:
 explicit SceneAuthority(ObjectRegistry);
 void begin(std::optional<host::LoadRequest>);
 const std::optional<host::LoadRequest>& request()const{return request_;}
 std::optional<std::vector<uint8_t>> snapshot(uint8_t slot)const;
 std::optional<std::vector<uint8_t>> update(uint32_t bindingId,uint8_t value);
 const ObjectRegistry& registry()const{return registry_;}
};
struct SceneObjectState {
 uint32_t bindingId=0;uint8_t current=0,initial=0;
 bool operator==(const SceneObjectState&)const=default;
};
struct SceneSnapshot {
 host::LoadRequest request;
 uint64_t revision=0;
 // Entire reviewed registry, never a partially decoded update. Initial is
 // the first published scene state; pre-snapshot deltas restore silently.
 std::vector<SceneObjectState> objects;
 bool operator==(const SceneSnapshot&)const=default;
};
enum class SceneSyncStatus {idle,unverified_registry,waiting_snapshot,ready};
class SceneReceiver {
 ObjectRegistry registry_;uint8_t slot_=0;uint64_t revision_=0;
 std::optional<host::LoadRequest> request_;
 std::optional<host::ObjectStates> states_;
 std::optional<SceneSnapshot> snapshot_;
public:
 SceneReceiver(ObjectRegistry,uint8_t localSlot);
 void begin(std::optional<host::LoadRequest>);
 // Transport must authenticate the sender and reject stale generation before
 // calling this. OR/max deltas commute, but the scene identity must match.
 bool receive(const host::LoadRequest&,std::span<const uint8_t> record);
 std::optional<std::array<uint8_t,2>> snapshot_request()const;
 SceneSyncStatus status()const;
 const std::optional<SceneSnapshot>& snapshot()const{return snapshot_;}
};
}
