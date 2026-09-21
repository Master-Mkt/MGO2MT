#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace mgo2mt::original_current_weapon_drop {
// Current ELF 8C9EA0, tables 8CA064/8CA1A4. These are WEAPON indices
// (also packed low-nine IDs for this 0..72 subset), never equipment IDs.
// A positive answer is this producer's ID gate, not general menu permission.
inline constexpr std::array<uint32_t,17> excluded{
 0,1,9,10,12,16,22,27,29,36,40,46,47,48,68,71,72};
inline constexpr std::array<uint32_t,16> onlineMenuProtected{
 1,22,27,40,9,72,47,71,10,48,16,29,36,70,12,68};
constexpr std::optional<bool> id_gate(uint32_t weaponIndex) noexcept {
 // The binary has a default branch beyond 72. Do not extend a reviewed
 // playable inventory contract to its internal/attack records implicitly.
 if(weaponIndex>72)return std::nullopt;
 for(const auto id:excluded)if(id==weaponIndex)return false;
 return true;
}
struct Context {
 uint32_t actorRoleAt88=0,actorTypeAt8c=0;
 bool isHost=false,currentWeaponPresent=false;
 uint32_t weaponIndex=0;
 bool groundManagerPresent=false;
 bool status110=false,status139=false,onlineFlag=false,menuContainsWeapon=false;
};
enum class Path {unknown,no_operation,local_current,host_proxy};
struct Plan {
 Path path=Path::no_operation;
 std::optional<bool> idEligible;
 bool requestsGroundRegistration=false;
 bool registrationManagerAvailable=false;
 bool callsMenuRemoval=false,menuRemovalPermitted=false;
 bool clearsProxyFields=false;
};
// Pure description, not an execution/authorization API. 729888 may fail; the
// original caller still attempts menu removal/proxy clear. Native transactions
// must conserve inventory rather than copy that failure behavior.
constexpr Plan plan(const Context& c) noexcept {
 Plan result;
 const bool proxy=c.actorTypeAt8c==8;
 if(proxy ? !c.isHost : c.actorRoleAt88==2||!c.currentWeaponPresent)return result;
 result.idEligible=id_gate(c.weaponIndex);
 if(!result.idEligible){result.path=Path::unknown;return result;}
 if(!*result.idEligible)return result;
 result.path=proxy?Path::host_proxy:Path::local_current;
 result.requestsGroundRegistration=true;
 result.registrationManagerAvailable=c.groundManagerPresent;
 if(proxy){result.clearsProxyFields=true;return result;}
 result.callsMenuRemoval=true;
 result.menuRemovalPermitted=c.menuContainsWeapon&&!c.status110&&!c.status139;
 if(c.onlineFlag)
  for(const auto id:onlineMenuProtected)
   if(c.weaponIndex==id)result.menuRemovalPermitted=false;
 return result;
}
}
