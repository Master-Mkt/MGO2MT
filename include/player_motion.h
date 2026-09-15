#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>
namespace mgo2win {
// Windows action labels are selected from inspected poses, not recovered MTSQ names.
enum class PlayerMotion : uint32_t {Idle,Walk,Run,CrouchIdle,CrouchWalk,ProneIdle,ProneForward,ProneBackward,SupineIdle,SupineForward,SupineBackward,PlayDeadProne,PlayDeadSupine,Aim,Reload,SelectionSalute,SelectionMagazine,SelectionBox,SelectionBoxEnter,Roll,Backstep,RollRecover,Count};
struct MotionPose {
 std::map<uint32_t,std::array<float,4>> rotations;
 std::array<float,3> root{};
 uint32_t rootBone=0;
};
struct PlayerMotionClip {
 PlayerMotion action{};uint32_t sourceKey=0,sourceIndex=0,frames=0,fps=60,rootBone=0;
 bool loop=false;
 std::vector<std::array<float,3>> roots;
 std::map<uint32_t,std::vector<std::array<float,4>>> tracks;
};
class PlayerMotionBank {
 std::map<PlayerMotion,PlayerMotionClip> clips_;
public:
 explicit PlayerMotionBank(std::span<const char>);
 bool has(PlayerMotion action)const{return clips_.contains(action);}
 size_t size()const{return clips_.size();}
 const PlayerMotionClip* find(PlayerMotion)const;
 // Root X/Z travel is removed: collision/navigation owns world displacement.
 // Vertical root posture/bob is retained. Non-looping clips clamp at their end.
 std::optional<MotionPose> sample(PlayerMotion,double seconds)const;
};
}
