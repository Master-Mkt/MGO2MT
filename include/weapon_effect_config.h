#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mgo2mt::weapon_effect {
using Vec3=std::array<float,3>;
using Curve=std::vector<std::array<float,2>>;
struct Atlas {uint32_t columns=1,rows=1,frames=1;float fps=0;bool loop=false,randomStart=false;};
struct Emitter {
 bool enabled=true,additive=false;
 uint32_t texture=0,count=1,delayMs=0,emissionMs=0,lifetimeMs=500;
 std::string texturePath;
 Atlas atlas;
 float radius=100,speed=0,spread=0,gravity=0,rotation=0,rotationSpeed=0;
 float sizeRandom=0,stretchRandom=0,alphaRandom=0,rotationRandom=0;
 std::array<float,2> stretch{1,1};
 std::array<float,4> color{1,1,1,1};
 Vec3 velocity{};
 Curve sizeCurve{{0,1},{1,1}},alphaCurve{{0,1},{1,0}};
};
struct Light {bool enabled=true;uint32_t delayMs=0,durationMs=80;float radius=2600,intensity=2;Vec3 color{1,.64f,.25f};Curve intensityCurve{{0,1},{1,0}};};
struct Sound {bool enabled=true;uint32_t cue=0,delayMs=0;float gain=1;std::string path;};
struct Reticle {bool enabled=true,centerDot=true;std::array<float,4> color{1,.65f,.17f,1};float thickness=2,length=10,minGap=6;};
struct Definition {
 std::map<std::string,std::vector<Emitter>,std::less<>> particles;
 std::map<std::string,Light,std::less<>> lights;
 std::map<std::string,Sound,std::less<>> sounds;
 std::optional<Reticle> reticle;
};
class Config {
 Definition defaults_;
 std::map<uint16_t,Definition> weapons_;
 std::map<std::string,uint32_t,std::less<>> textures_;
public:
 bool load(const std::filesystem::path&,std::string& error);
 bool load_text(std::string_view,std::string& error);
 const std::vector<Emitter>* particles(uint16_t,std::string_view channel)const;
 const Light* light(uint16_t,std::string_view event)const;
 const Sound* sound(uint16_t,std::string_view event)const;
 const Reticle* reticle(uint16_t)const;
 const auto& textures()const noexcept{return textures_;}
 const auto& weapons()const noexcept{return weapons_;}
};
bool relative_path(std::string_view) noexcept;
float curve(const Curve&,float age) noexcept;
uint64_t duration(const std::vector<Emitter>&) noexcept;
struct ParticleSample {
 Vec3 position{};
 float radius=1,rotation=0;
 std::array<float,2> stretch{1,1};
 std::array<float,4> rgba{1,1,1,1},uv{0,0,1,1};
 uint32_t texture=0;bool additive=false;
};
// The editor and the live game share this evaluator. Random choices are stable
// for an event/layer/particle seed; sampling twice never changes the variation.
std::vector<ParticleSample> sample(const std::vector<Emitter>&,Vec3 origin,Vec3 direction,uint64_t seed,uint64_t ageMs);
}
