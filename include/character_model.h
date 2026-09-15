#pragma once
#include <cstdint>
#include <span>
#include <vector>
#include <array>
#include <string>
namespace mgo2win {
enum class ModelExtent {normal,sky};
// GWM1 magic, versions 1/2: geometry and embedded BC1/BC3 images. Version 2
// additionally preserves authored MDN COLOR0 independently of sampled light.
struct ModelVertex {float x,y,z,nx,ny,nz,u,v,u1=0,v1=0;float lr=1,lg=1,lb=1,lit=0;float ar=0,ag=0,ab=0,aa=1;};
inline constexpr uint32_t noMaterialTexture=0xffffffffu;
// Source records are big endian and immutable. Runtime parameters are the
// MGO2 0x115AC0 loader result, not IEEE half (including signed zero).
struct OriginalTexture {
 std::array<uint8_t,32> raw{};
 uint32_t image=noMaterialTexture;
 std::string provenance;
};
struct OriginalMaterial {
 bool present=false;
 std::array<uint8_t,112> raw{};
 std::array<uint8_t,48> vertexDeclaration{};
 uint32_t sourceIndex=0,key=0,nameHash=0,parameterCount=0;
 std::array<std::array<uint16_t,4>,8> half{};
 std::array<std::array<float,4>,8> parameters{};
 // Explicit provisional requests, never inferred from a bit of the key.
 // 1=AG normal, 2=the five audited extra-normal pairs, 4=palette UV,
 // 8=audited 0x1001 three-color mask (also requires explicit draw c467.x).
 uint32_t requestedRules=0,normalSlot=noMaterialTexture,extraNormalSlot=noMaterialTexture;
 uint32_t reflectionSlot=noMaterialTexture,paletteSlot=noMaterialTexture;
 std::string mdnPath,mdnSha256,packagePath,packageSha256,vertexProgramSha256,fragmentProgramSha256,ruleId,fallbackReason;
 std::vector<OriginalTexture> textures;
};
struct ModelPart {uint32_t first,count,texture,flags;uint32_t materialShader=0;std::array<float,3> tint{1,1,1};OriginalMaterial original;};
struct ModelTexture {uint32_t width,height,codec;std::vector<uint8_t> pixels;};
struct CharacterModel {
 std::array<float,6> bounds{};std::vector<ModelVertex> vertices;
 std::array<float,6> overviewBounds{};bool hasOverviewBounds=false;
 std::vector<uint32_t> indices;std::vector<ModelPart> parts;std::vector<ModelTexture> textures;
 explicit CharacterModel(std::span<const char> bytes,ModelExtent extent=ModelExtent::normal);
 CharacterModel()=default;
};
}
