#pragma once
#include "stage_collision.h"
#include <cstdint>
#include <optional>
#include <span>
#include <cmath>
#include <filesystem>
#include <vector>
namespace mgo2win::ladder {
// Native safe traversal of explicitly reviewed stage anchors. This is not a
// decoder for arbitrary ladder-looking triangles or original ladder physics.
struct Anchor {uint16_t id=0;stage::Vec3 bottom{},top{},bottomExit{},topExit{};float facingYaw=0;bool operator==(const Anchor&)const=default;};
struct Body {stage::Vec3 feet{};stage::Capsule capsule{};};
enum class Action:uint8_t {none,enter,leave};
struct Intent {Action action=Action::none;uint16_t anchorId=0;float axis=0;bool operator==(const Intent&)const=default;};
struct State {Anchor anchor;stage::Vec3 feet{};stage::Capsule capsule{};bool active=false;};
inline constexpr float speed=600,maximum_enter_distance=650,endpoint_tolerance=5;
bool valid(const Anchor&);
inline bool valid(const Intent&i){return unsigned(i.action)<=unsigned(Action::leave)&&std::isfinite(i.axis)&&std::abs(i.axis)<=1&&(i.action!=Action::enter||i.anchorId!=0);}
// Explicit local configuration only. No unverified stage coordinates are added.
// GWLA1 <count>, then: id bottomXYZ topXYZ bottomExitXYZ topExitXYZ yawRadians.
std::vector<Anchor> load(const std::filesystem::path&);
std::optional<State> enter(const Anchor&,stage::Vec3 feet,stage::Capsule,const stage::Collision&,std::span<const Body> peers={});
// Only the vertical axis is accepted. Invalid input leaves the state intact;
// collision is an accepted stop, never a teleport to the far side.
bool advance(State&,float axis,double seconds,const stage::Collision&,std::span<const Body> peers={});
// Either endpoint must be reached. Exit has real support, headroom and a clear
// swept route. Failure retains the attached state, allowing movement away.
bool leave(State&,const stage::Collision&,std::span<const Body> peers={});
}
