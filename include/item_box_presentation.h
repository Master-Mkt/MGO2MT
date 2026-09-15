#pragma once
#include "stage_lighting.h"
#include "item_box_profile.h"
#include "world_inventory_session.h"
#include "character_renderer.h"
#include "shadow_renderer.h"
#include <map>
namespace mgo2win::item_box {
inline constexpr size_t maximum_visible=128;
enum class Asset:uint8_t {large,medium,smallBox,unavailable};
// Visual source selection is independent of the unchanged HOST size profile.
// Category mappings below are native policy, not a recovered actor-ID cast.
Asset asset(const items::Contents&,const weapons::Catalog*);
struct Box {items::EntityKey key;stage::Vec3 position{};float yaw=0;Size size=Size::unknown;bool grounded=false;Asset asset=Asset::unavailable;};
// Position is the authoritative bottom-center of the native item box. Ground
// state below is a presentation query only; physics/pickup remain HOST-owned.
bool grounded(const items::Entity&,Profile,const stage::Collision&);
class Presentation {
 struct Track {items::Position position;Size size=Size::unknown;uint64_t groundAt=0;bool grounded=false;};
 weapons::Catalog catalog_;bool catalogReady_=false;
 uint64_t connection_=0,scene_=0,now_=0,revision_=0;items::Scope scope_;items::Actor actor_;
 std::map<uint64_t,Track> tracks_;
public:
 bool load_catalog(const std::filesystem::path&,std::string& error);
 void clear();
 std::vector<Box> update(const items::ClientState&,const stage::Collision&,uint64_t scene,uint64_t now);
};
// Required original MDN conversions, ordered large / medium / small. Missing
// or malformed assets throw; no fabricated replacement geometry is returned.
std::array<CharacterModel,3> original_models(const std::filesystem::path& itemsRoot);
// Uniform fit inside the existing native category envelope, with bottom-center
// at zero. Retains original proportions, topology, UVs, materials and images.
CharacterModel model(const CharacterModel& original,Size);
class Renderer {
 std::array<std::array<std::unique_ptr<CharacterRenderer>,3>,4> models_;
public:
 Renderer(ID3D11Device*,const std::filesystem::path& itemsRoot);
 // Opaque depth-tested/depth-writing geometry on the same stage target/camera.
 // Rejects the full invalid call before touching the render target.
 void draw(ID3D11DeviceContext*,std::span<const Box>,CharacterRenderer& surface,const WorldView&,std::span<const DynamicPointLight> lights={},const shadows::Renderer* shadow=nullptr,const stage::Lighting* environment=nullptr);
 void shadow_casters(std::vector<shadows::Caster>&,std::span<const Box>)const;
};
}
