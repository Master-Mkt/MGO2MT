#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include "render_reflections.h"
namespace mgo2mt::render_backend {
// Backend-neutral choices. Defaults preserve the existing Legacy image.
struct Options {unsigned anisotropy=0;bool mipmaps=false,linearColor=false,hdr=false,softParticles=false,lod=false,reflections=false;render_reflections::Profile reflectionMaterials{};bool operator==(const Options&)const=default;};
inline bool valid(Options o){return o.anisotropy==0||o.anisotropy==2||o.anisotropy==4||o.anisotropy==8||o.anisotropy==16;}
struct Extent {unsigned width,height;bool reduced=false;};
inline Extent internal_extent(unsigned w,unsigned h,unsigned percent,bool advanced=false){
 if(!w||!h||w>8192||h>8192||percent<50||percent>200||percent%25)throw std::invalid_argument("Render extent");
 double scale=percent/100.0;
 // Extra HDR/effects buffers use a lower pixel budget. Still permits native4K.
 scale=std::min({scale,8192.0/w,8192.0/h,std::sqrt((advanced?8388608.0:16777216.0)/(double(w)*h))});
 return {std::max(1u,unsigned(std::floor(w*scale))),std::max(1u,unsigned(std::floor(h*scale))),scale+1e-8<percent/100.0};
}
}
