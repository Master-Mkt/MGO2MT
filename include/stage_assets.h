#pragma once
#include "host_match.h"
#include "character_model.h"
#include "stage_collision.h"
#include "stage_round.h"
#include "stage_cbox.h"
#include "stage_lighting.h"
#include "host_placements.h"
#include "stage_object_sync.h"
#include <filesystem>
#include <string_view>
#include <memory>
#include <mutex>
#include <thread>
namespace mgo2win::stage {
std::string_view name(uint8_t map);
enum class Status {idle,loading,preview_ready,unknown_map,unavailable,invalid,graphics_error};
struct LightChange {unsigned key=0,id=~0u;bool enabled=true;Vec3 center{};float radius=0;};
struct ObjectTransform {Vec3 position{},degrees{};};
struct ObjectPartBinding {
 uint32_t componentId=0;uint8_t mask=0,value=0;
 std::shared_ptr<const CharacterModel> model;std::shared_ptr<const Collision> collision;
 // Individual bottle instances carry absolute transforms within one registry actor.
 std::optional<ObjectTransform> placement;
 bool hitOnly=false; // GM_HIT targets do not obstruct player navigation.
};
struct ObjectLightRule {uint8_t mask=0,value=0;LightChange light;};
struct ObjectBinding {uint32_t bindingId=0;uint8_t width=0;Vec3 position{},degrees{};std::vector<ObjectPartBinding> parts;std::vector<ObjectLightRule> lights;int cboxOrdinal=-1;};
std::vector<ObjectBinding> read_object_bindings(const std::filesystem::path& root,bool loadModels=true,uint8_t map=20);
struct Result {Status status=Status::idle;std::optional<host::LoadRequest> request;std::shared_ptr<const CharacterModel> model,debugModel,receivedModel;std::shared_ptr<const Collision> collision;uint64_t generation=0;Round round;std::vector<std::shared_ptr<const CharacterModel>> props;std::shared_ptr<const Lighting> lighting,authoredLighting;std::map<uint8_t,std::shared_ptr<const CharacterModel>> itemModels;std::optional<host::Placements> received;size_t missingItemModels=0;
 std::shared_ptr<const Collision> authoredCollision;std::shared_ptr<const CharacterModel> objectModel;
 std::shared_ptr<const Collision> objectHitCollision;
 std::optional<SceneSnapshot> objectSnapshot;size_t activeObjectComponents=0;
 std::shared_ptr<const std::vector<ObjectBinding>> objectBindings;
 // Selection is known independently of OLObjMan's full registration order.
 // Do not assign snapshot indexes from candidateIndex or render intact boxes
 // before the initial object state and model/collision binding are verified.
 std::shared_ptr<const CboxLayout> cboxLayout;std::vector<CboxPlacement> cboxes;
};
struct ObjectLightBinding {size_t index=0;unsigned key=0;Vec3 center{};float radius=0;};
// Local shape preview only. No network traffic or loading-complete notification.
class Assets {
 std::filesystem::path root_;std::jthread worker_;mutable std::mutex mutex_;Result result_;
 uint64_t generation_=0;
public:
 explicit Assets(std::filesystem::path root):root_(std::move(root)){}
 ~Assets();
 void select(std::optional<host::LoadRequest>);
 void reset();
 // Applies only the current host generation, retaining instance identities.
 // Rendering available item models never implies all scene state is ready.
 void receive(const std::optional<host::Placements>&);
 bool light_states(const host::LoadRequest&,const std::vector<LightChange>&);
 // Requires the caller's reviewed GCX registration order. Never use a
 // geometry-property enumeration as the network actor index. This adapter
 // accepts one-bit breakables only, not bottle bitsets or CBOX max states.
 bool object_lights(const host::LoadRequest&,const host::ObjectStates&,const std::vector<ObjectLightBinding>&);
 // All-or-nothing scene revision, after the registry receiver has accepted a
 // complete snapshot. Initial restoration causes no break sound/explosion.
 bool object_states(const SceneSnapshot&,std::span<const ObjectBinding>);
 bool object_states(const SceneSnapshot&);
 Result result()const;
};
}
