#pragma once
#include <array>
#include <cstdint>
#include <cmath>
namespace mgo2mt {struct WorldView;}
namespace mgo2mt::shadows {
struct Settings {
 bool enabled=false;unsigned cascades=4,resolution=2048,pcfRadius=1;
 float depthBias=.0003f,normalBias=20,slopeBias=2,distance=60000;
 bool visualize=false;
 bool operator==(const Settings&)const=default;
};
inline bool valid(const Settings&s){return s.cascades>=2&&s.cascades<=6&&(s.resolution==1024||s.resolution==2048||s.resolution==4096||s.resolution==8192)&&s.pcfRadius<=2&&std::isfinite(s.depthBias)&&s.depthBias>=0&&s.depthBias<=.01f&&std::isfinite(s.normalBias)&&s.normalBias>=0&&s.normalBias<=200&&std::isfinite(s.slopeBias)&&s.slopeBias>=0&&s.slopeBias<=8&&std::isfinite(s.distance)&&s.distance>=10000&&s.distance<=200000;}
struct Allocation {unsigned cascades=0,resolution=0;uint64_t bytes=0;bool reduced=false;};
Allocation allocation(const Settings&,uint64_t budget,unsigned maxDimension=8192);
struct Plan {
 std::array<std::array<float,16>,6> matrices{};
 std::array<float,6> splits{},texelSize{};
 std::array<float,3> eye{},forward{},direction{},sun{};
 unsigned count=0,resolution=0;float nearPlane=10;
};
// Stable world-aligned light basis, quantized sphere extents and texel centers.
// Bounds include off-camera casters; simulation and camera FOV stay unchanged.
Plan plan(const WorldView&,std::array<float,3> direction,std::array<float,3> sun,
          const std::array<float,6>& sceneBounds,const Settings&,Allocation);
}
