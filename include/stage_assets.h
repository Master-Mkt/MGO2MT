#pragma once
#include "host_match.h"
#include "character_model.h"
#include "stage_collision.h"
#include "stage_round.h"
#include "stage_cbox.h"
#include "stage_lighting.h"
#include "host_placements.h"
#include <filesystem>
#include <string_view>
#include <memory>
#include <mutex>
#include <thread>
namespace mgo2win::stage {
std::string_view name(uint8_t map);
enum class Status {idle,loading,preview_ready,unknown_map,unavailable,invalid,graphics_error};
struct Result {Status status=Status::idle;std::optional<host::LoadRequest> request;std::shared_ptr<const CharacterModel> model,debugModel,receivedModel;std::shared_ptr<const Collision> collision;uint64_t generation=0;Round round;std::vector<std::shared_ptr<const CharacterModel>> props;std::shared_ptr<const Lighting> lighting,authoredLighting;std::map<uint8_t,std::shared_ptr<const CharacterModel>> itemModels;std::optional<host::Placements> received;size_t missingItemModels=0;
 // Selection is known independently of OLObjMan's full registration order.
 // Do not assign snapshot indexes from candidateIndex or render intact boxes
 // before the initial object state and model/collision binding are verified.
 std::shared_ptr<const CboxLayout> cboxLayout;std::vector<CboxPlacement> cboxes;
};
struct LightChange {unsigned key=0,id=~0u;bool enabled=true;Vec3 center{};float radius=0;};
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
 Result result()const;
};
}
