#pragma once
#include "combat_authority.h"
#include <filesystem>
#include <functional>
#include <set>
namespace mgo2mt::combat {
struct Sound {uint32_t cue;std::filesystem::path file;float gain;Vec3 origin;};
// Only locally registered original cue/WAV mappings can play. A network peer
// never supplies a filename; missing combat audio is silent, not a menu sound.
class Effects {
 std::map<uint32_t,std::filesystem::path> files_;uint64_t epoch_=0,floor_=0;std::set<uint64_t> played_;size_t missing_=0;
public:
 bool load(const std::filesystem::path& directory);
 bool load_manifest(const std::filesystem::path& manifest);
 const std::map<uint32_t,std::filesystem::path>& files()const noexcept{return files_;}
 void dispatch(std::span<const Event>,Vec3 listener,const std::function<void(const Sound&)>&);
 bool play_cue(uint32_t cue,Vec3 origin,Vec3 listener,const std::function<void(const Sound&)>&);
 size_t missing()const{return missing_;}
 void clear(){epoch_=floor_=0;played_.clear();}
};
}
