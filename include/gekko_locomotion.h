#pragma once
#include <array>
#include <cstdint>
namespace mgo2mt::gekko_locomotion {
struct Scope {uint64_t epoch=0,scene=0;uint32_t character=0,life=0;uint16_t instance=0;uint8_t slot=255;bool operator==(const Scope&)const=default;};
struct Policy {
 float maximumSpeed=4224,acceleration=12000,deceleration=24000,yawRate=6.28318530718f;
 float reverseCosine=-.5f;uint32_t reverseWaitMs=180,maximumGapMs=250;
};
struct Intent {float yaw=0,speed=0;}; // world direction; speed0 releases movement
enum class Phase {stopped,moving,braking,waiting};
struct Step {
 std::array<float,2> displacement{},velocity{}; // world X,Z, displacement over this call
 float yaw=0;Phase phase=Phase::stopped;bool accepted=false,rebaselined=false;
};
bool valid(const Policy&);
// Native easing only. No original MGS4 acceleration/turn timing is claimed.
// Fixed1ms steps give identical output for equal timestamp partitions and
// unchanged input. Feed raw intent once; never ease an already-eased HOST pose.
class State {
 Scope scope_{};uint64_t at_=0;bool ready_=false;float speed_=0,travelYaw_=0,yaw_=0;
 Phase phase_=Phase::stopped;uint32_t waitMs_=0;
public:
 void reset();
 Step update(Scope,Intent,uint64_t nowMs,const Policy& policy={});
 float speed()const{return speed_;}float yaw()const{return yaw_;}Phase phase()const{return phase_;}
};
}
