#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>
namespace mgo2mt::gameplay {
// Accidental configuration-drift detection, not authentication. Both peers
// hash the same exact configuration and optional hit-shape files before admission.
inline std::optional<uint64_t> fingerprint(const std::filesystem::path& data){
 uint64_t value=14695981039346656037ull;bool present=false;
 auto byte=[&](uint8_t b){value^=b;value*=1099511628211ull;};
 for(std::string_view name:{"gameplay.json","mounted_weapons.json","character/hit_geometry.gwhit","motion/evade_travel.gwet","special/gekko_jump.gwjc"}){
  std::error_code error;const auto path=data/std::filesystem::path(name);const bool exists=std::filesystem::exists(path,error);if(error)return {};
  for(unsigned char c:name)byte(c);byte(0);byte(exists?1:0);if(!exists)continue;present=true;
  std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in)return {};const auto count=in.tellg();if(count<=0||count>1024*1024)return {};in.seekg(0);for(uint64_t n=uint64_t(count),i=0;i<8;++i)byte(uint8_t(n>>(i*8)));
  char buffer[4096];while(in){in.read(buffer,sizeof(buffer));for(std::streamsize n=0;n<in.gcount();++n)byte(uint8_t(buffer[n]));}if(!in.eof())return {};
 }
 return present?(value?value:1):0;
}
}
