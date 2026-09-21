#pragma once
#include "stage_lighting.h"
#include <cstdint>
#include <istream>
#include <vector>

namespace mgo2mt::stage {
// GEOM child hashes may repeat. sourceOffset identifies the authored node;
// the position in anchors preserves 1A1468/1A1348 iteration order.
struct CboxAnchor {
 uint32_t sourceOffset=0,key=0;Vec3 position{};
 bool operator==(const CboxAnchor&)const=default;
};
struct CboxPlacement {
 CboxAnchor anchor;uint32_t candidateIndex=0;int16_t rotationUnits=0;
 // Original scalar supplied to the vector rotation code. Matrix/actor state
 // construction remains separate; this is not a gameplay-ready actor.
 float rotationRadians=0;
 bool operator==(const CboxPlacement&)const=default;
};
struct CboxLayout {
 uint32_t count=0;std::vector<CboxAnchor> anchors;
 static CboxLayout read(std::istream&);
 // 73DAD0 seeds from global object 0 at +0xA1, the host round generation.
 // Never substitute Assets' local reload counter or a locally random seed.
 std::vector<CboxPlacement> select(uint8_t hostGeneration)const;
};
}
