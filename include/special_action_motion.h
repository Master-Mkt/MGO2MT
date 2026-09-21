#pragma once
#include "player_motion.h"
#include "original_reload_timing.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mgo2mt::player {
enum class SpecialPhase : uint8_t {start,hold,end};
// Original male normal-PC slot 6, indices 0/1/2, distinct from lobby selection.
// Native caller must not use this bank for female PCs, Instructor/trainer,
// rule 10, or skill 25 alternate gestures; those resources are not restored.
// These are deadlines under the existing native nominal 5-tick frame clock;
// they are not measurements of PS3 wall time. Only hold publishes flag 22.
inline constexpr uint32_t special_start_ms=651,special_end_ms=735;
inline constexpr uint32_t special_source_hash=0x65735a;
class SpecialMotionBank {
 PlayerMotionBank bank_;
public:
 explicit SpecialMotionBank(std::span<const char> bytes):bank_(bytes){
  constexpr std::array<uint32_t,3> keys{0xcd9e86,0x6a1cd4,0xf47aa6};
  constexpr std::array<uint32_t,3> frames{40,120,45};
  if(bank_.size()!=3)throw std::runtime_error("Invalid special motion clip count");
  for(unsigned i=0;i<3;++i){const auto*c=bank_.find(PlayerMotion(i));
   if(!c||c->sourceKey!=keys[i]||c->sourceIndex!=i||c->frames!=frames[i]||c->loop!=(i==1))
    throw std::runtime_error("Unreviewed special motion source");
  }
 }
 // GWT1 slots 0..2 are private storage labels, never cast a Control::Motion or
 // exposed as the ordinary Idle/Walk/Run bank. No global enum ordinal changes.
 std::optional<MotionPose> sample(SpecialPhase phase,double seconds)const{
  const auto index=unsigned(phase);if(index>2||!std::isfinite(seconds)||seconds<0)return {};
  const double last=index==0?39.0:44.0;
  if(index!=1)seconds=(std::min)(seconds,last/original::nominal_motion_fps);
  return bank_.sample(PlayerMotion(index),seconds/1.001);
 }
};
}
