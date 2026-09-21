#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>
namespace mgo2mt::combat::decals {
using Vec3=std::array<float,3>;
struct Scope {uint64_t epoch=0,scene=0;bool operator==(const Scope&)const=default;};
// This classification must come from a current, matching collision query.
// A bare network impact is insufficient to assert a static solid surface.
enum class Surface:uint8_t {unknown,static_solid,water,sky,body,dynamic};
struct Impact {Scope scope;uint64_t eventId=0;Vec3 position{},normal{};uint32_t material=0;Surface surface=Surface::unknown;};
// All values are explicit native presentation policy. No original duration,
// radius or pool count is inferred. Radius/offset use decoded world units (mm).
struct Policy {size_t capacity;uint64_t lifetimeMs,fadeMs;float radius,surfaceOffset;};
struct Decal {
 Scope scope;uint64_t eventId=0;Vec3 position{},normal{},tangent{},bitangent{};
 uint32_t material=0;float radius=0,alpha=0;
};
class Pool {
 struct Entry {Decal decal;uint64_t born=0;};
 Policy policy_;Scope scope_;std::deque<Entry> entries_;
 uint64_t played_=0,watermark_=0,lastNow_=0;bool established_=false;
 bool expire(uint64_t now);
public:
 static constexpr size_t maximumCapacity=65536;
 static bool valid(const Policy&)noexcept;
 explicit Pool(Policy); // Throws invalid_argument for invalid native policy.
 bool configure(Policy); // Failed validation preserves policy and entries.
 void clear();
 // Establish first watermark as baseline; old events do not create marks.
 // Call before dispatching events with the newest committed event watermark.
 // Epoch/scene changes clear all marks; zero scope means unavailable/clear.
 void synchronize(Scope,uint64_t eventWatermark,uint64_t now);
 // Ordered committed events, once each. Rejected surface/geometry consumes
 // its event ID too, so a later reinterpretation cannot re-play that event.
 bool emit(const Impact&,uint64_t now);
 std::vector<Decal> sample(uint64_t now);
 size_t size()const noexcept{return entries_.size();}
 const Policy& policy()const noexcept{return policy_;}
};
}
