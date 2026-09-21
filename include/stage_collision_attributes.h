#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace mgo2mt::stage {
// GEOM surface attributes are 64-bit. These names describe the user supplied
// surface table, not the separate primitive header or material flags.
namespace attribute {
inline constexpr uint64_t none=0, reserved_0=1, recoil=2, floor=4, sound=8,
 player=0x10, enemy=0x20, bullet=0x40, missile=0x80, bomb=0x100,
 radar=0x200, blood=0x400, ik=0x800, stairway=0x1000, stop_eye=0x2000,
 cliff=0x4000, through=0x8000, lean=0x10000, dont_fall=0x20000,
 camera=0x40000, shadow=0x80000, intrude=0x100000, attack_guard=0x200000,
 rail=0x400000, height_limit=0x1000000, bullet_mark=0x800000,
 no_behind=0x2000000, behind_through=0x4000000, reserved_27=0x8000000,
 reserved_28=0x10000000, reserved_29=0x20000000, water=0x40000000,
 reserved_31=0x80000000, reserved_32=0x100000000ULL, reserved_33=0x200000000ULL;
inline constexpr uint64_t known=0x7fffffeULL|water;
// Explicit tags for locally generated solid boxes/test fixtures. Never used
// to reinterpret a zero attribute in decoded GEOM data.
inline constexpr uint64_t native_solid=floor|sound|player|enemy|bullet|missile|bomb|ik|stop_eye|camera|shadow|attack_guard|bullet_mark;
inline constexpr uint64_t native_hit_target=bullet|missile|bomb|attack_guard|bullet_mark;
inline constexpr std::array<std::string_view,34> names{
 "Unknown 0","Type Recoil","Floor","Sound","Player","Enemy","Bullet","Missile",
 "Bomb","Radar","Blood","IK","Stairway","Stop Eye","Cliff","Type Through",
 "Lean","Don't Fall","Camera","Shadow","Intrude","Attack Guard","Rail","Bullet Mark",
 "Height Limit","No Behind","Behind Through","Unknown 27","Unknown 28","Unknown 29",
 "Water","Unknown 31","Unknown 32","Unknown 33"};
constexpr bool has(uint64_t value,uint64_t bits){return (value&bits)==bits;}
}
// Empty query means raw geometry (tools, inspection and explicit test worlds).
// A purpose query is strict: None/unknown bits never imply every collision type.
struct CollisionQuery {
 uint64_t required=0,any=0,excluded=0;
 bool playerMovement=false;
 constexpr bool matches(uint64_t value)const{
  // Native ledge policy: Cliff-only actor bands are traversal metadata, not
  // walls. Actual floors and explicit Don't Fall barriers remain physical.
  const bool ledgeBand=(value&attribute::cliff)&&!(value&(attribute::floor|attribute::dont_fall));
  return (value&required)==required&&(!any||(value&any))&&!(value&excluded)&&!(playerMovement&&ledgeBand);
 }
};
namespace query {
inline constexpr CollisionQuery raw{},player{0,attribute::player|attribute::dont_fall,attribute::recoil|attribute::through,true},
 enemy{attribute::enemy},floor{attribute::floor,0,attribute::recoil|attribute::through},player_floor{attribute::floor|attribute::player,0,attribute::recoil|attribute::through},
 sound{attribute::sound},bullet{attribute::bullet},missile{attribute::missile},bomb{attribute::bomb},
 radar{attribute::radar},blood{attribute::blood},ik{attribute::ik},stairway{attribute::stairway},
 stop_eye{attribute::stop_eye},cliff{attribute::cliff},through{attribute::through},lean{attribute::lean},
 dont_fall{attribute::dont_fall},camera{attribute::camera},shadow{attribute::shadow},
 intrude{attribute::intrude},attack_guard{attribute::attack_guard},rail{attribute::rail},
 bullet_mark{attribute::bullet_mark},height_limit{attribute::height_limit},
 no_behind{attribute::no_behind},behind_through{attribute::behind_through},water{attribute::water};
}
}
