#pragma once
#include <cstdint>
#include <span>
#include <vector>
#include <array>
namespace mgo2win {
// GWM1: little-endian, static bind-pose geometry and embedded BC1/BC3 images.
struct ModelVertex {float x,y,z,nx,ny,nz,u,v,u1=0,v1=0;};
struct ModelPart {uint32_t first,count,texture,flags;uint32_t materialShader=0;std::array<float,3> tint{1,1,1};};
struct ModelTexture {uint32_t width,height,codec;std::vector<uint8_t> pixels;};
struct CharacterModel {
 std::array<float,6> bounds{};std::vector<ModelVertex> vertices;
 std::vector<uint32_t> indices;std::vector<ModelPart> parts;std::vector<ModelTexture> textures;
 explicit CharacterModel(std::span<const char> bytes);
 CharacterModel()=default;
};
}
