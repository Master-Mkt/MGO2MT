#pragma once
#include "original_hit_regions.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
namespace mgo2mt::host_hit {
using Vec3=std::array<float,3>;
enum class Stance : uint8_t {standing,crouching,prone};
// Local static proxy, not the original animated collision controller. No
// original model/animation transforms are embedded in the program.
struct BoneTransform {uint32_t key;std::array<Vec3,3> axes;Vec3 origin;};
struct Hit {float distance;uint8_t bone;uint8_t regionIndex;uint32_t nameHash;Vec3 position;};
enum class ResourceStatus : uint8_t {native_fallback,local_resource,invalid_resource};
// Supply data/character/hit_geometry.gwhit explicitly. Missing resources select
// the authored simple human proxy. Invalid resources report an error and also
// reset to that proxy; a previous local table is never silently retained.
// GWHIT1: 8-byte magic, LE u32 version/poses/bones/reserved (1/6/21/0),
// then 126 rows of LE u32 key, 9 IEEE float32 axes and 3 float32 origins.
ResourceStatus configure(const std::filesystem::path& file,std::string& error) noexcept;
bool using_local_resource() noexcept;
// The returned span is retained per thread until its next pose() call. Gender
// 0/1 and three stances select local tables, or the same authored shape for both
// genders. Local tables do not establish original female animation selection.
std::span<const BoneTransform> pose(uint8_t gender,Stance stance) noexcept;
// Direction must be unit length. Unknown/invalid inputs fail closed.
std::optional<Hit> query(const Vec3& origin,const Vec3& direction,float maxDistance,
 const Vec3& feet,float yaw,uint8_t gender,Stance stance) noexcept;
// Pure oriented-box primitive, also available for boundary verification.
std::optional<float> intersect(const Vec3& origin,const Vec3& direction,float maxDistance,
 const BoneTransform& bone,const Vec3& offset,const Vec3& halfExtent) noexcept;
}
