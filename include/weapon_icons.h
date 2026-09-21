#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <vector>
namespace mgo2mt::weapons {
// Local presentation data. An icon never grants permission to equip a weapon.
std::map<uint16_t,std::string> read_icon_index(const std::filesystem::path&);
struct Icon {unsigned width=0,height=0;std::vector<uint32_t> bgra;};
class Icons {
 std::map<uint16_t,Icon> images_;
public:
 bool load(const std::filesystem::path&,std::string& error);
 // Optional JSON icon overrides are applied atomically; existing index icons
 // remain available for all IDs without an override.
 bool override_paths(const std::filesystem::path& dataRoot,const std::map<uint16_t,std::string>&,std::string& error);
 const Icon* find(uint16_t id)const;
 size_t size()const{return images_.size();}
};
// Source and destination are straight-alpha BGRA, as used by the menu overlay.
void paint_icon(const Icon&,std::span<uint32_t>,int width,int height,int x,int y,int w,int h,bool muted=false,double displayWidth=0,double displayHeight=0);
}
