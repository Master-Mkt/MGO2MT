#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mgo2win::skills {
inline constexpr unsigned base_capacity=4, maximum_capacity=8;
struct Choice {uint16_t id=0;uint8_t level=1;bool operator==(const Choice&)const=default;};
struct Loadout {std::vector<Choice> entries;bool operator==(const Loadout&)const=default;};
struct Entry {
 uint16_t id=0;uint8_t level=1,cost=1;std::string name,display_name;
};
// Original skill facts are provided by the reviewed local catalog. This model
// validates selection cost; it does not grant experience, purchases or effects.
class Catalog {
 std::vector<Entry> entries_;
public:
 bool load(const std::filesystem::path&,std::string& error);
 const std::vector<Entry>& entries()const{return entries_;}
 const Entry* find(uint16_t,uint8_t)const;
 std::vector<uint16_t> ids()const;
 std::vector<const Entry*> levels(uint16_t)const;
};
enum class Validation {valid, invalid_capacity, too_many, unknown_skill, unknown_level, duplicate, over_budget};
struct Check {Validation result=Validation::valid;unsigned used=0;explicit operator bool()const{return result==Validation::valid;}};
Check validate(const Catalog&,const Loadout&,unsigned capacity=base_capacity);
class Editor {
 std::shared_ptr<const Catalog> catalog_;Loadout original_,draft_;unsigned capacity_=base_capacity;
public:
 Editor(std::shared_ptr<const Catalog>,Loadout,unsigned capacity=base_capacity);
 Check set(uint16_t id,uint8_t level); // Level zero removes a skill; failures leave draft intact.
 Check status()const;
 const Loadout& draft()const{return draft_;}
 const Loadout& original()const{return original_;}
 unsigned capacity()const{return capacity_;}
 bool changed()const{return draft_!=original_;}
 // Refresh the authoritative baseline without discarding an in-progress edit.
 void synchronize(const Loadout& value,unsigned capacity,bool preserveDraft){if(!preserveDraft)draft_=value;original_=value;capacity_=capacity;}
 void reset(){draft_=original_;}
 void clear(){draft_.entries.clear();}
};
// characterId is the server's numeric character identity. Capacity is never
// persisted: the caller supplies a verified entitlement, defaulting to four.
bool save(const std::filesystem::path& directory,uint64_t characterId,const Catalog&,const Loadout&,unsigned capacity,std::string& error);
std::optional<Loadout> load(const std::filesystem::path& directory,uint64_t characterId,const Catalog&,unsigned capacity,std::string& error);
}
