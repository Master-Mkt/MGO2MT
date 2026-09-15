#pragma once
#include "player_motion.h"
#include "player_control.h"
namespace mgo2win {
// Selection clips occupy IDs15..18; gameplay enums must never be raw-cast.
inline PlayerMotion render_motion(player::Motion motion){
 switch(motion){
 case player::Motion::idle:return PlayerMotion::Idle;
 case player::Motion::walk:return PlayerMotion::Walk;
 case player::Motion::run:return PlayerMotion::Run;
 case player::Motion::crouch_idle:return PlayerMotion::CrouchIdle;
 case player::Motion::crouch_walk:return PlayerMotion::CrouchWalk;
 case player::Motion::prone_idle:return PlayerMotion::ProneIdle;
 case player::Motion::prone_forward:return PlayerMotion::ProneForward;
 case player::Motion::prone_backward:return PlayerMotion::ProneBackward;
 case player::Motion::supine_idle:return PlayerMotion::SupineIdle;
 case player::Motion::supine_forward:return PlayerMotion::SupineForward;
 case player::Motion::supine_backward:return PlayerMotion::SupineBackward;
 case player::Motion::dead_prone:return PlayerMotion::PlayDeadProne;
 case player::Motion::dead_supine:return PlayerMotion::PlayDeadSupine;
 case player::Motion::aim:return PlayerMotion::Aim;
 case player::Motion::reload:return PlayerMotion::Reload;
 case player::Motion::roll:return PlayerMotion::Roll;
 case player::Motion::backstep:return PlayerMotion::Backstep;
 }
 return PlayerMotion::Idle;
}
}
