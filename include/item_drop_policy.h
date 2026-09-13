#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
namespace mgo2win::items {
enum class Domain : uint8_t {weapon,equipment,world_item};
enum class DropOverride { original_default, deny, allow };
struct DropPolicy {
 std::optional<bool> originalDrop,originalEmptyDiscard;
 DropOverride drop=DropOverride::original_default;
 std::optional<bool> emptyDiscard;
 bool allows_drop() const noexcept {return drop==DropOverride::allow||(drop==DropOverride::original_default&&originalDrop.value_or(false));}
 bool discards_empty() const noexcept {return emptyDiscard.value_or(originalEmptyDiscard.value_or(false));}
};
struct PolicyEntry {uint32_t id=0;Domain domain=Domain::weapon;std::string name,source,originalKind;DropPolicy policy;};
class DropPolicies {
 std::map<uint64_t,PolicyEntry> entries_;
public:
 // Strict versioned JSON, max 2 MiB/65536 entries. Failed reload is atomic:
 // no current entry changes. Unknown original facts remain optional values.
 bool parse(std::string_view,std::string& error);
 bool load(const std::filesystem::path&,std::string& error);
 const PolicyEntry* find(uint32_t id,Domain domain=Domain::weapon)const noexcept;
 const std::map<uint64_t,PolicyEntry>& entries()const noexcept{return entries_;}
};
}
