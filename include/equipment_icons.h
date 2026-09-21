#include "product_identity.h"
#pragma once
#include "weapon_icons.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <stdexcept>
namespace mgo2mt::equipment {
struct Extent{double width=0,height=0;};
// Separate equipment namespace: equipment 22 is ENVG, never weapon 22.
class Icons {
 weapons::Icons images_;std::map<uint16_t,Extent> extents_;
public:
 bool load(const std::filesystem::path& folder,std::string& error){
  images_={};extents_.clear();
  try{
   if(!images_.load(folder/"index.tsv",error))return false;
   if(std::filesystem::file_size(folder/"display.tsv")>8192)throw std::runtime_error("Equipment display extent");
   std::ifstream f(folder/"display.tsv");std::string line;
   if(!std::getline(f,line)||line!=mgo2mt::brand::Format{"MGO2MT_EQUIPMENT_DISPLAY 1"})throw std::runtime_error("Equipment display version");
   while(std::getline(f,line)){
    std::istringstream row(line);unsigned id;Extent e;std::string extra;
    if(!(row>>id>>e.width>>e.height)||row>>extra||id>255||!images_.find(uint16_t(id))||
       !std::isfinite(e.width)||!std::isfinite(e.height)||e.width<=0||e.height<=0||e.width>1024||e.height>512||
       !extents_.emplace(uint16_t(id),e).second)throw std::runtime_error("Equipment display row");
   }
   if(!f.eof()||extents_.size()!=images_.size())throw std::runtime_error("Equipment display missing row");
   error.clear();return true;
  }catch(const std::exception& e){images_={};extents_.clear();error=e.what();return false;}
 }
 const weapons::Icon* find(uint16_t id)const{return images_.find(id);}
 const Extent* extent(uint16_t id)const{auto i=extents_.find(id);return i==extents_.end()?nullptr:&i->second;}
 size_t size()const{return images_.size();}
 void paint(uint16_t id,std::span<uint32_t> pixels,int x,int y,int w,int h)const{
  const auto* icon=find(id);const auto* e=extent(id);if(!icon||!e||w<=0||h<=0)return;
  // Preserve original LA2 nonuniform scale; only shrink to fit the native card.
  const double scale=(std::min)({1.,w/e->width,h/e->height});
  const int dw=(std::max)(1,int(std::round(e->width*scale))),dh=(std::max)(1,int(std::round(e->height*scale)));
  weapons::paint_icon(*icon,pixels,1280,720,x+(w-dw)/2,y+(h-dh)/2,dw,dh,false,dw,dh);
 }
};
}
