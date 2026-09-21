#pragma once
#include "combat_authority.h"
#include "weapon_catalog.h"
#include <filesystem>
#include <string_view>
namespace mgo2mt::gameplay {
struct Resources {
 std::string handsPath,modelsIndexPath,modelRoot,audioManifest,effectsManifest;
 std::string damageEffectsManifest; // Optional additional original body-hit images.
 std::string weaponEffectsManifest; // Optional editor-controlled effect/audio timeline JSON.
};
// Paths are relative to the data directory. Textures/materials are contained
// in each GWM, so replacing modelPath replaces those original resources too.
struct Visual {
 uint16_t id=0,motionId=0,effectId=0;
 std::string modelPath,secondaryModelPath,iconPath; // Optional data-relative PNG for the weapon menu.
 uint32_t flags=0;
 uint32_t flashTexture=0,smokeTexture=0; // zero retains the selected effect source default
 std::array<float,3> muzzle{},magPosition{};
 std::array<float,4> magRotation{0,0,0,1};
 bool autoAim=false,tracer=false;
 float lockRange=0,lockWidth=0,lockYaw=0;
};
struct Definition {combat::Weapon weapon;Visual visual;std::string name,provenance;std::optional<int> massCandidate;bool stageMaterialAudio=false;};
class Config {
 Resources resources_;
 std::vector<Definition> definitions_;
 std::vector<weapons::Entry> entries_;
 std::optional<uint32_t> initialDp_;
public:
 // Strict UTF-8, <=1 MiB, unknown/duplicate fields and IDs are errors.
 // A failed load preserves the entire previous configuration.
 bool load(const std::filesystem::path&,std::string& error);
 bool load_text(std::string_view,std::string& error);
 const Resources& resources()const{return resources_;}
 const std::vector<Definition>& definitions()const{return definitions_;}
 const std::vector<weapons::Entry>& entries()const{return entries_;}
 std::optional<uint32_t> initial_dp()const{return initialDp_;}
 const Visual* visual(uint16_t id)const;
 const Definition* find(uint16_t id)const;
 uint16_t source_id(uint16_t id)const{auto d=find(id);return d&&d->weapon.behaviorSourceId?d->weapon.behaviorSourceId:id;}
 std::vector<combat::Weapon> profiles(uint8_t map)const;
};
// Reject traversal, absolute/UNC/drive paths, alternate streams and Windows
// device names. Does not require an asset to exist on the HOST.
bool relative_resource_path(std::string_view);
}
