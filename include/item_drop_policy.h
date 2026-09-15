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
enum class PolicyBasis { unresolved, original_fact, local_override };
struct PolicyDecision {bool value=false;PolicyBasis basis=PolicyBasis::unresolved;};
struct DropPolicy {
 std::optional<bool> originalDrop,originalEmptyDiscard;
 DropOverride drop=DropOverride::original_default;
 std::optional<bool> emptyDiscard;
 // Unknown original facts fail closed at runtime, but must not be displayed as
 // a verified original prohibition. Explicit native settings stay distinguishable.
 PolicyDecision drop_decision() const noexcept {
  if(drop!=DropOverride::original_default)return {drop==DropOverride::allow,PolicyBasis::local_override};
  return {originalDrop.value_or(false),originalDrop?PolicyBasis::original_fact:PolicyBasis::unresolved};
 }
 PolicyDecision empty_decision() const noexcept {
  if(emptyDiscard)return {*emptyDiscard,PolicyBasis::local_override};
  return {originalEmptyDiscard.value_or(false),originalEmptyDiscard?PolicyBasis::original_fact:PolicyBasis::unresolved};
 }
 bool allows_drop() const noexcept {return drop_decision().value;}
 bool discards_empty() const noexcept {return empty_decision().value;}
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
