#pragma once
#include "world_inventory.h"
#include <filesystem>
#include <map>
#include <string>
namespace mgo2win::items {
struct WeaponOverride {DropOverride drop=DropOverride::original_default;std::optional<bool> emptyDiscard;bool operator==(const WeaponOverride&)const=default;};
struct Settings {
 Capacity capacity{64,64};bool recoverOthers=true;
 // Native initial policy only, not a recovered original drop flag.
 std::map<uint32_t,WeaponOverride> weapons{{25,{DropOverride::allow,{}}}};
 bool valid()const noexcept;
 bool parse(std::string_view,std::string& error);
 bool load(const std::filesystem::path&,std::string& error);
 bool save(const std::filesystem::path&,std::string& error)const;
 DropPolicy policy(uint32_t id,const DropPolicy* original=nullptr)const noexcept;
};
}
