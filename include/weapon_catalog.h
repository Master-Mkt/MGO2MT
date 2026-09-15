#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace mgo2win::weapons {
enum class Category : uint8_t { primary=0, secondary=1, support=2 };
struct Entry {
 uint16_t id=0;
 Category category=Category::primary;
 std::string display_name;
 // Unknown evidence is distinct from a free weapon or an allowed weapon.
 std::optional<uint32_t> dp_cost;
 std::optional<bool> available_without_dp;
 // Bit number in the original 16-byte room restriction field, if proved.
 std::optional<uint8_t> restriction_bit;
};
enum class Access { allowed, restricted, dp_disabled, insufficient_dp, unverified };
struct SelectionContext {
 bool dp_enabled=false;
 uint32_t dp_balance=0;
 std::array<uint8_t,16> room_restrictions{};
 // Explicit native initial-loadout exception requested by the user. Original
 // catalog rows and room restriction bits remain authoritative and unchanged.
 bool native_operator_grant=false;
};
class Catalog {
 std::vector<Entry> entries_;
 std::optional<uint32_t> initial_dp_;
public:
 // Strict, bounded UTF-8 TSV. A failed reload preserves the current catalog.
 bool load(const std::filesystem::path&,std::string& error);
 const std::vector<Entry>& entries()const{return entries_;}
 const Entry* find(Category,uint16_t id)const;
 std::optional<uint32_t> initial_dp()const{return initial_dp_;}
 std::vector<const Entry*> choices(Category,const SelectionContext&,bool include_unavailable=true)const;
};
Access access(const Entry&,const SelectionContext&);
struct Quote { Access access=Access::allowed;uint64_t cost=0; };
// Rejects repeated categories, unverified prices and restricted choices.
Quote quote(std::span<const Entry* const>,const SelectionContext&);
const char* category_name(Category);
}
