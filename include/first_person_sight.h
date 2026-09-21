#pragma once
#include "character_renderer.h"
#include <optional>
namespace mgo2mt::weapon_hand::connections {class Table;}
namespace mgo2mt::first_person_sight {
using Vec3=std::array<float,3>;
struct Axis {Vec3 rear{},front{};};
// Exact CNP point positions, accepted only for the corresponding original MDN.
std::optional<Axis> local_axis(uint32_t weapon,const CharacterModel&,const weapon_hand::connections::Table&);
std::optional<Axis> world_axis(const Axis&,const std::array<float,16>& attachment,float yaw,Vec3 origin);
struct Result {WorldView view;bool calibrated=false,held=false;float displacement=0,axisAngleRadians=0;};
// Native eye placement: nearest point on the CNP line, at least 100 native
// units behind its rear point. Navigation forward/FOV and HOST are unchanged.
class Controller {
 uint32_t weapon_=0;Vec3 offset_{};bool ready_=false;float angle_=0;
public:
 void reset(){*this={};}
 // Pass no axis during reload/CQC: retain the last offset in navigation space.
 // Weapon changes discard old calibration; stage/life changes call reset().
 Result update(const WorldView& baseline,uint32_t weapon,std::optional<Axis> stableAxis);
};
}
