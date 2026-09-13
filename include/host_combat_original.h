#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace mgo2win::combat::original {
// Retail MGO2 8101A0: ten channels per player actor, not per NT object.
constexpr uint16_t channel_base=64;
constexpr uint8_t player_limit=24,channel_count=10;
constexpr bool reliable=false;
enum class Lane:uint8_t {damage=0,upper_action=3,vitals=4,pose=5,camera=9};
struct Channel {uint8_t player=0,lane=0;bool operator==(const Channel&)const=default;};
enum class Error {extent,opcode,bounds};
struct Invalid:std::runtime_error {
 Error code;explicit Invalid(Error e):std::runtime_error("original combat payload"),code(e){}
};
uint16_t channel(uint8_t player,Lane);
std::optional<Channel> decode_channel(uint16_t);
struct Position {
 int16_t x=0,y=0,z=0;
 bool operator==(const Position&)const=default;
};
struct Angles {
 int16_t pitch=0,yaw=0;
 bool operator==(const Angles&)const=default;
};
// The numeric claims and flags are preserved, not authorized by decoding.
// Damage positions are (world - the game's origin) / 10, truncated to i16.
struct Damage {
 uint16_t flags=0,amount=0;uint8_t attacker=0,weapon=0;
 Position position;std::optional<Angles> direction;
 bool operator==(const Damage&)const=default;
};
std::vector<uint8_t> encode_damage(const Damage&);
Damage decode_damage(std::span<const uint8_t>);
// Retail sends ceil(value / 4), clamped to 1..250 while positive, or 0.
struct Vitals {
 uint8_t life=0,stamina=0;
 uint16_t life_value()const{return uint16_t(life)*4;}
 uint16_t stamina_value()const{return uint16_t(stamina)*4;}
 bool operator==(const Vitals&)const=default;
};
uint8_t quantize_vital(uint32_t);
std::vector<uint8_t> encode_vitals(const Vitals&);
Vitals decode_vitals(std::span<const uint8_t>);
// Fixed ordinary pose body only. Action-specific tails require their own
// reviewed decoder and are rejected here. This is not a feet/aim validator.
// Pose XYZ are the transmitted control position / 10. Damage XYZ also use
// an origin subtraction; do not silently apply that to this separate body.
struct Pose {
 uint16_t action=0;Position position;int16_t yaw=0;
 // PL_PLG_NORMAL look-move values, not shot pitch/yaw.
 int8_t look_lr=0;uint8_t look_u=0;
 bool operator==(const Pose&)const=default;
};
std::vector<uint8_t> encode_pose(const Pose&);
Pose decode_pose(std::span<const uint8_t>);
}
