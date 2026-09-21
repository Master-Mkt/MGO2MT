#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string_view>
namespace mgo2mt::stage {
// Fixed local candidates, from updated lobby GCX procedure 11. A supported
// route does not certify that all assets/object contracts have been compiled.
struct Profile {uint8_t map;std::string_view stage;};
inline constexpr std::array<Profile,5> runtime_profiles{{{1,"n001a"},{4,"n004a"},{7,"n007a"},{20,"n022a"},{21,"n023a"}}};
constexpr const Profile* runtime_profile(uint8_t map) noexcept {for(const auto&p:runtime_profiles)if(p.map==map)return &p;return nullptr;}
constexpr bool runtime_stage_supported(uint8_t map) noexcept {return runtime_profile(map)!=nullptr;}
inline std::filesystem::path asset_path(const std::filesystem::path&root,uint8_t map,std::string_view suffix){
 const auto*p=runtime_profile(map);if(!p||suffix.empty()||suffix.front()!='.'||suffix.find_first_not_of(".abcdefghijklmnopqrstuvwxyz0123456789_")!=std::string_view::npos)throw std::invalid_argument("Unknown stage asset route");
 return root/(std::string(p->stage)+std::string(suffix));
}
}
