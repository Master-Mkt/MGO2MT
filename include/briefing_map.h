#pragma once
#include "weapon_icons.h"
#include <stdexcept>

namespace mgo2mt::briefing {
// Native presentation of the two original n022a online_map texture planes.
// Shared square placement and amber tint are Windows UI choices; original
// alpha and geometry stay unchanged. World-to-map/player markers are unknown.
class Map {
    weapons::Icon map_,grid_;
    static void tint(weapons::Icon& icon){
        for(auto& pixel:icon.bgra){
            const auto blue=(pixel&255)*100/255;
            const auto green=((pixel>>8)&255)*193/255;
            pixel=(pixel&0xffff0000)|(green<<8)|blue; // Original red *255/255.
        }
    }
public:
    bool load(const std::filesystem::path& indexPath,std::string& error){
        try{
            weapons::Icons icons;
            if(!icons.load(indexPath,error)){map_={};grid_={};return false;}
            const auto* map=icons.find(1);const auto* grid=icons.find(2);
            if(icons.size()!=2||!map||!grid||map->width!=512||map->height!=512||grid->width!=512||grid->height!=512)
                throw std::runtime_error("Briefing map requires two original 512-square planes");
            map_=*map;grid_=*grid;tint(map_);tint(grid_);error.clear();return true;
        }catch(const std::exception& e){map_={};grid_={};error=e.what();return false;}
    }
    bool paint(std::span<uint32_t> destination,int width,int height,unsigned mapID,int x,int y,int size)const{
        if(mapID!=20||map_.bgra.empty()||grid_.bgra.empty()||width<=0||height<=0||
           destination.size()<size_t(width)*height||size<=0||size>1024||
           x>=width||y>=height||int64_t(x)+size<=0||int64_t(y)+size<=0)return false;
        weapons::paint_icon(map_,destination,width,height,x,y,size,size);
        weapons::paint_icon(grid_,destination,width,height,x,y,size,size);
        return true;
    }
};
}
