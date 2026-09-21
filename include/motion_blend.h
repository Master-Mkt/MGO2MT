#pragma once
#include "player_motion.h"
namespace mgo2mt {
// Native pose-only transition. The caller owns world movement, scope and pause.
// Input must be a complete, stable skeleton (1..128 nonzero bone keys, rootBone
// present, finite root within +/-1e6, finite nonzero quaternions). Rotations are
// normalized. Different skeletons require reset(), even for a different key.
class MotionBlend {
 MotionPose displayed_,from_;
 uint64_t key_=0;
 double alpha_=1;
 bool initialized_=false,accepted_=false;
public:
 // key0 is invalid. seconds must be finite >=0, rate finite >0 (default5/s).
 // There is no arbitrary dt/rate cap: a long valid interval completes safely.
 // First valid sample is immediate. A changed key freezes the displayed pose
 // and returns it unchanged; that call consumes no dt. Later calls advance
 // alpha against each frame's current target. Interruptions restart from the
 // last displayed blend, never from a clip's old initial sample.
 // Invalid input keeps an existing pose/key/progress unchanged; invalid input
 // before initialization throws std::invalid_argument. Check accepted below.
 const MotionPose& update(uint64_t key,const MotionPose& completeTarget,
                          double seconds,float ratePerSecond=5.f);
 void reset();
 float progress()const{return static_cast<float>(alpha_);}
 bool active()const{return initialized_&&alpha_<1;}
 const MotionPose* pose()const{return initialized_?&displayed_:nullptr;}
 bool last_update_accepted()const{return accepted_;}
};
}
