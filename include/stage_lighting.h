#pragma once
#include <array>
#include <vector>
#include <istream>
#include "environment_light.h"
namespace mgo2win::stage {
using Vec3=std::array<float,3>;
struct LightIdentity {
 unsigned groupFlags=0,id=0,key=0;
 Vec3 groupMinimum{},groupMaximum{};
};
struct Hemisphere {
 Vec3 maximum{},minimum{},center{},extent{},positive{},negative{},direction{};
 std::array<float,4> quaternion{0,0,0,1};LightIdentity identity;
 Vec3 front{},back{};std::array<float,2> rotation{};unsigned flags=0;
};
struct LightSample {Vec3 color{};float hemisphereWeight=0;unsigned volumes=0;};
struct PointLight {Vec3 position{},color{};float range=0,extendedRange=0,importance=0;unsigned flags=0;LightIdentity identity;};
// Original LT3 96-byte families. Parameters remain uninterpreted source values;
// native sampling approximates type 2 as a bounded spot and type 4 as a bounded point.
struct AuthoredLight {
 unsigned kind=0;Vec3 minimum{},maximum{};std::array<float,4> position{},direction{};
 Vec3 color{};std::array<float,3> parameters{};unsigned flags=0;LightIdentity identity;
};
struct TransientLights {
 struct Entry {PointLight light;double expires=0;};std::vector<Entry> entries;
 bool add(PointLight,double now,double lifetime);
 void expire(double now);
 void clear(){entries.clear();}
 Vec3 sample(Vec3 position,Vec3 normal,double now)const;
};
struct Lighting {
 Vec3 direction{0,-1,0},direct{},front{},back{},axis{0,1,0},ambientScale{1,1,1};
 std::array<float,6> cameraBounds{};std::vector<Hemisphere> hemispheres;
 std::vector<PointLight> points;
 std::vector<AuthoredLight> authored;
 static Lighting read(std::istream&);
 // DG_GetLight 0x1250B8: strict AABB, active/disabled flags, oriented feather box.
 static float weight(const Hemisphere&,Vec3);
 static Vec3 point_sample(const PointLight&,Vec3 position,Vec3 normal);
 static Vec3 authored_sample(const AuthoredLight&,Vec3 position,Vec3 normal);
 // 11B6B0: group active bit, per-record key/id, disabled bit 0x8000.
 size_t enable(unsigned key,unsigned id,bool enabled);
 // 120400 / 11CE10: strict sphere at each light's reference position,
 // preceded by authored group-AABB overlap. Unsupported LT3 families are not lit.
 size_t enable_sphere(Vec3 center,float radius,bool enabled);
 LightSample sample(Vec3 position,Vec3 normal)const;
 EnvironmentLight environment(Vec3 position)const;
};
}
