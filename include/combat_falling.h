#pragma once
#include <cstdint>
#include <optional>
namespace mgo2win::combat::falling {
struct Scope {uint64_t epoch=0;uint8_t slot=255;uint16_t instance=0;uint32_t character=0,life=0;bool operator==(const Scope&)const=default;};
// User-requested native thresholds in existing world units (1000 = one metre).
// No claim that these values restore original MGO2 fall damage.
struct Policy {float safeHeight=3000,severeHeight=7000,fatalHeight=10000;uint32_t severePermille=900;};
inline constexpr Policy native_policy{};
bool valid(Policy)noexcept;
std::optional<uint32_t> damage(float height,uint32_t maxHp,Policy=native_policy)noexcept;
struct Input {Scope scope;float feetY=0;uint64_t nowMs=0;bool grounded=false,eligible=false,ladder=false;};
struct Landing {float height=0;uint32_t damage=0;bool fatal=false;uint64_t serial=0;};
// Inputs are HOST-accepted positions. grounded means verified supporting solid
// geometry, never a client flag. Scene/world replacement is not a reset.
// Caller disables eligibility on death, water
// exemption, teleport, scene loading and other non-falling transport.
class Tracker {
 Policy policy_;Scope scope_;std::optional<Scope> fatalScope_;float previousY_=0,peakY_=0;uint64_t at_=0,serial_=0;
 bool clock_=false,primed_=false,airborne_=false,observedGround_=false;
 void reset_tracking();
public:
 explicit Tracker(Policy=native_policy);
 void clear(); // Trusted transport reset. Same-life fatal latch and serial survive.
 std::optional<Landing> update(const Input&,uint32_t maxHp);
 bool airborne()const noexcept{return airborne_;}
 bool primed()const noexcept{return primed_;}
};
}
