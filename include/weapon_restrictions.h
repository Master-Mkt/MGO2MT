#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace mgo2win::restrictions {
using Bits=std::array<uint8_t,16>;
enum class Category : uint8_t { primary,secondary,support,custom,items };
enum class LockState { unlocked,mixed,locked };
struct Entry {
 std::string_view key;
 Category category;
 std::string_view display_name;
 // Original inventory ID where proved; item/custom rows may have no one ID.
 std::optional<uint16_t> weapon_id;
 Bits mask;
 // True means absent from the ordinary TDM DP-off list. It is a menu hint,
 // never proof that the current player owns/can equip this entry.
 std::optional<bool> dp_only;
};
// This catalog describes HOST restrictions, independently of player eligibility.
std::span<const Entry> catalog();
const Entry* find(std::string_view key);
const char* category_name(Category);
bool enabled(const Bits&);
void set_enabled(Bits&,bool);
LockState state(const Bits&,const Entry&);
// Stored choice, even with master disabled; a partially set composite is locked.
bool locked(const Bits&,const Entry&);
bool effective_locked(const Bits&,const Entry&);
void set_locked(Bits&,const Entry&,bool);
// Only catalog mask bits are changed. Master, other categories and unknown bits
// are preserved. No qualification, point balance or original asset is modified.
void set_category_locked(Bits&,Category,bool);
void set_all_locked(Bits&,bool);
Bits category_mask(Category);
}
