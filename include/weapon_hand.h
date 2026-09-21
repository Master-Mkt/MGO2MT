#pragma once
#include "character_catalog.h"
namespace mgo2mt::weapon_hand {
struct Point {uint32_t bone=0x5c0243;std::array<float,3> position{};std::array<float,4> rotation{0,0,0,1};};
struct Sample {MotionPose pose;Point point;uint32_t weapon=0,index=0,key=0;std::optional<Point> magazine;double seconds=0;bool cqc=false,sighting=false;};
struct Clip {uint32_t weapon,index,key,frames,fps,rootBone,bone;std::vector<std::array<float,3>> roots,positions;std::vector<std::array<float,4>> rotations;std::map<uint32_t,std::vector<std::array<float,4>>> tracks;uint32_t magazineBone=0;std::vector<std::array<float,3>> magazinePositions;std::vector<std::array<float,4>> magazineRotations;};
// Indices reference original source clips. UINT32_MAX means unavailable.
struct Selection {std::array<uint32_t,3> hold,aim,reload,fire,cqc;};
class Bank {
 std::map<std::pair<uint32_t,uint32_t>,Clip> clips_;
 std::map<uint32_t,Selection> selections_;
public:
 explicit Bank(std::span<const char>);
 std::optional<Sample> sample(uint32_t weapon,uint32_t index,double seconds,bool loop)const;
 std::optional<Sample> select(uint32_t weapon,PlayerMotion motion,double seconds,bool aiming=false,double fireSeconds=-1,double cqcSeconds=-1,int posture=-1)const;
 std::optional<double> duration(uint32_t weapon,uint32_t index)const;
 std::vector<uint32_t> weapons()const;
 size_t size()const{return clips_.size();}
};
// Preserve locomotion/root/legs. Apply original upper-body and finger tracks
// before the existing pose transition blender, never after skinning.
void upper_body(MotionPose&,const Sample&,std::span<const CatalogBone>);
// Native camera-pitch adapter, applied once after catalog.pose and before
// held-weapon attachment. Positive radians looks upward in actor-local space.
bool aim_pitch(PreparedCharacter&,std::span<const CatalogBone>,float radians);
// D2E9D08 -> D34368 -> D37940: hand bone * MTP local, no inverse CNP grip.
std::optional<std::array<float,16>> frame(const PreparedCharacter&,const Point&);
void transform(std::span<const ModelVertex> bind,std::span<ModelVertex> out,const std::array<float,16>&);
}
