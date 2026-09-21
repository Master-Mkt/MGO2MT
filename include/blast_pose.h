#pragma once
#include "mounted_flight_presentation.h"
#include "player_motion.h"
namespace mgo2mt::combat::presentation {
// Original down/recovery clips reused by a native blast transition. This is
// not a claim that the original explosion reaction-state graph was recovered.
inline std::optional<MotionPose> blast_pose(const PlayerMotionBank& bank,const mounted::FlightFrame& frame){
 if(!frame.blast||!std::isfinite(frame.seconds)||frame.seconds<0)return {};
 return bank.sample(frame.landing?PlayerMotion::RollRecover:PlayerMotion::PlayDeadSupine,frame.seconds);
}
}
