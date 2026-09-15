#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
namespace mgo2win::render_backend {
// Backend-neutral choices. Defaults preserve the existing Legacy image.
struct Options {unsigned anisotropy=0;bool mipmaps=false,linearColor=false;bool operator==(const Options&)const=default;};
inline bool valid(Options o){return o.anisotropy==0||o.anisotropy==2||o.anisotropy==4||o.anisotropy==8||o.anisotropy==16;}
struct Extent {unsigned width,height;bool reduced=false;};
inline Extent internal_extent(unsigned w,unsigned h,unsigned percent){
 if(!w||!h||w>8192||h>8192||percent<50||percent>200||percent%25)throw std::invalid_argument("Render extent");
 double scale=percent/100.0;
 // Bound color+depth to 128 MiB, also avoiding dimensions beyond 8192.
 scale=std::min({scale,8192.0/w,8192.0/h,std::sqrt(16777216.0/(double(w)*h))});
 return {std::max(1u,unsigned(std::floor(w*scale))),std::max(1u,unsigned(std::floor(h*scale))),scale+1e-8<percent/100.0};
}
}
