#pragma once
#include "stage_lighting.h"
#include <cstdint>
#include <optional>
namespace mgo2win::stage {
struct CollisionTriangle {std::array<unsigned,3> vertices{};uint64_t attribute=0;unsigned polygonAttribute=0;};
struct CollisionHit {float distance=0;Vec3 position{};size_t triangle=0;};
// Geometry queries only; actor movement, collision masks and responses are separate.
struct Collision {
 std::vector<Vec3> vertices;std::vector<CollisionTriangle> triangles;
 static Collision read(std::istream&);
 std::optional<CollisionHit> ray(Vec3 origin,Vec3 direction,float maximum)const;
};
}
