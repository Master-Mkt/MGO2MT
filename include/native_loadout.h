#pragma once
#include <array>
#include <cstdint>
namespace mgo2win::weapons::native_loadout {
inline constexpr std::array<uint16_t,3> initial{25,3,52};
// IDs are original; the user's fixed initial grant is a native policy. These
// two identities currently support selection only, with no attack fallback.
constexpr bool held_only(uint16_t id) { return id==1||id==3||id==52; }
constexpr bool attack_supported(uint16_t id) { return id==25||id==2||id==50||id==53||(id>=128&&id<=131); }
}
