#pragma once
#include "stage_collision.h"
#include "gekko_jump_curve.h"
#include <cstdint>
namespace mgo2mt::special_pc {
enum class Kind:uint8_t {human,gekko};
constexpr bool weapon(uint16_t id){return id>=128&&id<=131;}
enum class Action:uint8_t {none,jump,kick,salute,climb};
struct Intent {Action action=Action::none;uint32_t request=0;bool operator==(const Intent&)const=default;};
struct State {Kind kind=Kind::human;bool nameVisible=true;Action action=Action::none;uint32_t serial=0;uint16_t elapsedMs=0;bool operator==(const State&)const=default;};
// All gameplay values below are native prototype policy, not recovered MGS4
// damage/physics parameters. The authored model is approximately 4243 units high.
struct Profile {
 stage::Capsule capsule{800,4200,2};uint32_t hp=1000,stamina=5000;
 float walkSpeed=1885,runSpeed=4224,modelFeetOffset=2926.016357421875f;
 uint16_t jumpMs=5024,kickMs=2084,kickHitMs=900;
 float jumpHeight=10000,kickRange=2700,kickRadius=600;uint32_t kickDamage=400;
};
inline constexpr Profile native_gekko{};
inline constexpr uint16_t salute_ms=2334; // Native greeting selection, authored 140 frames at 60 Hz.
inline constexpr uint16_t climb_ms=2600; // Native bounded mantle; original action not recovered.
inline constexpr uint16_t duration(Action a){return a==Action::climb?climb_ms:a==Action::jump?native_gekko.jumpMs:a==Action::salute?salute_ms:native_gekko.kickMs;}
inline constexpr uint16_t kick_event_weapon=65534; // Native event-only identity; not an original weapon ID.
inline constexpr bool valid(Kind k){return k==Kind::human||k==Kind::gekko;}
inline constexpr bool valid(const Intent&i){return unsigned(i.action)<=unsigned(Action::climb)&&((i.action==Action::none)==(!i.request));}
inline constexpr bool valid(const State&s){return valid(s.kind)&&unsigned(s.action)<=unsigned(Action::climb)&&(s.action==Action::none?(!s.serial&&!s.elapsedMs):(s.kind==Kind::gekko&&s.serial&&s.elapsedMs<duration(s.action)))&&(s.kind!=Kind::human||s.nameVisible);}
// Bounded pose-only trajectory for offline presentation. HOST collision must
// sweep each destination; a client cannot supply an impulse or elapsed time.
// Physical trajectory is intentionally separate from the original root-Y
// compensation table used by GekkoMotionBank.
inline float gameplay_jump_height_ms(uint32_t elapsed){
 if(elapsed<=600||elapsed>=3458)return 0;
 const float t=float(elapsed-600)/2858.f;return native_gekko.jumpHeight*4*t*(1-t);
}
inline float jump_height(uint16_t elapsed){return gameplay_jump_height_ms(elapsed);}
}
