#pragma once
#include "weapon_effect_config.h"
#include "combat_audio.h"
#include <memory>
#include <set>
namespace mgo2mt::weapon_effect {
// Local presentation only. Accepted HOST events select sounds; peers never
// provide paths or timing. User imported PCM is checked before scheduling.
class Audio {
 struct Pending {Sound sound;combat::Identity owner;uint32_t life=0;uint16_t weapon=0;uint64_t due=0,reload=0;combat::Vec3 origin{};bool world=false;};
 struct Reload {combat::Identity owner;uint32_t life=0;uint16_t weapon=0;uint64_t deadline=0;};
 std::shared_ptr<const Config> config_;
 std::map<std::string,std::filesystem::path,std::less<>> files_;
 std::vector<Pending> pending_;
 std::array<Reload,24> reloads_{};
 std::set<uint64_t> seen_,contacts_;
 uint64_t epoch_=0,scene_=0,floor_=0,now_=0,lastClick_=0;
 void queue(const Sound&,combat::Identity,uint32_t,uint16_t,combat::Vec3,uint64_t,uint64_t reload=0,bool world=false);
public:
 bool configure(std::shared_ptr<const Config>,const std::filesystem::path& dataRoot,const combat::Effects&,std::string& error);
 void clear();
 void synchronize(uint64_t epoch,uint64_t scene,uint64_t watermark,uint64_t now);
 // Returns events whose existing original audio should still be dispatched.
 std::vector<combat::Event> dispatch(std::span<const combat::Event>,const combat::Snapshot&,uint64_t now);
 void reloads(const combat::Snapshot&,uint64_t now);
 bool replaces_reload(uint16_t weapon)const{return config_&&config_->sound(weapon,"reload");}
 void click(const combat::Player&,uint64_t now);
 void casing(const combat::Event& contact,const combat::Snapshot&,uint64_t now);
 void sample(const combat::Snapshot&,uint64_t now,combat::Vec3 listener,combat::Effects&,const std::function<void(const combat::Sound&)>&);
 size_t pending()const noexcept{return pending_.size();}
};
bool validate_sound_file(const std::filesystem::path&,std::string& error);
}
