#pragma once
#include "original_hit_regions.h"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
namespace mgo2win::host_hit {
using Vec3=std::array<float,3>;
enum class Stance : uint8_t {standing,crouching,prone};
// Static frame-zero native proxy, not the original animated collision controller.
// Full bone axes and origin use CharacterCatalog's row-vector parent composition.
struct BoneTransform {uint32_t key;std::array<Vec3,3> axes;Vec3 origin;};
struct Hit {float distance;uint8_t bone;uint8_t regionIndex;uint32_t nameHash;Vec3 position;};
// gender 0/1 uses the existing GWC skeleton and shared GWMOT action tracks.
// This does not establish the original female animation selection.
std::span<const BoneTransform> pose(uint8_t gender,Stance stance) noexcept;
// Direction must be unit length. Unknown/invalid inputs fail closed.
std::optional<Hit> query(const Vec3& origin,const Vec3& direction,float maxDistance,
 const Vec3& feet,float yaw,uint8_t gender,Stance stance) noexcept;
// Pure oriented-box primitive, also available for boundary verification.
std::optional<float> intersect(const Vec3& origin,const Vec3& direction,float maxDistance,
 const BoneTransform& bone,const Vec3& offset,const Vec3& halfExtent) noexcept;
}
