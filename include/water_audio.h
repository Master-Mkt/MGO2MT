#pragma once
#include "stage_navigation.h"
#include "combat_audio.h"
#include <map>
#include <span>
#include <vector>
namespace mgo2win::water_audio {
// Explicit native presentation, NOT a recovered original human-water cue.
inline constexpr const wchar_t* filename=L"native_water_step.wav";
struct Scope {uint64_t epoch=0,scene=0;bool operator==(const Scope&)const=default;};
struct Actor {uint64_t identity=0,life=0;stage::Vec3 feet{};stage::NavigationWaterState water;bool alive=true,grounded=true;};
// Shared presentation/contact guard; horizontalScale zero is a valid blocked-prone contact.
bool valid_wet(const Actor&);
struct Step {uint64_t identity=0,life=0;stage::Vec3 position{};float gain=0;};
class Steps {
 struct Track {uint64_t life=0,lastEmission=0;stage::Vec3 feet{};float level=0,distance=0;bool wet=false,emitted=false;};
 Scope scope_{};uint64_t lastNow_=0;bool initialized_=false;std::map<uint64_t,Track> tracks_;
public:
 static constexpr size_t maximumActors=24,maximumEvents=4;
 static constexpr uint64_t minimumIntervalMs=240,maximumGapMs=250;
 static constexpr float stepDistance=500.f,maximumDisplacement=2000.f;
 void reset();
 // Returned events are consumed once, not retained or replayed later. Filter
 // membership/life using the latest admitted roster; stale snapshots are inactive.
 std::vector<Step> update(Scope,std::span<const Actor>,uint64_t now,bool active);
};
// Deterministic mono PCM16/22050Hz, 0.24s, bounded moderate amplitude. The caller
// writes this local generated asset at packaging/startup; no original cue ID.
std::vector<uint8_t> native_step_wav();
// The trusted local filename is explicit; no peer supplies an audio path.
// Gain includes native distance attenuation and can use existing playCombatSound.
std::optional<combat::Sound> make_sound(const Step&,const std::filesystem::path&,stage::Vec3 listener);
}
