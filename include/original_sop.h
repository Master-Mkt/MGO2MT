#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>

namespace mgo2win::original_sop {
// Current ELF 81A028; original coordinate units, 65536 yaw units/turn.
enum class Branch { status22, status140 };
struct Position { float x{}, y{}, z{}; };
inline constexpr float vertical_limit = 2000.f;
inline constexpr float facing_exemption_radius = 2000.f;
inline constexpr int unavailable_grace_ticks = 300;
inline constexpr float range(Branch b) { return b == Branch::status22 ? 5000.f : 20000.f; }
inline constexpr bool yaw_window(std::uint16_t bearing, std::uint16_t facing, Branch b) {
    const unsigned half = b == Branch::status22 ? 0x2000u : 0x1555u;
    return static_cast<std::uint16_t>(unsigned(bearing) - unsigned(facing) + half) <= 2u * half;
}
// Scalar port of 81A610..81A698's atan approximation. The original uses
// PPC fres + two refinement steps: our correctly rounded reciprocal seed
// is an explicit native arithmetic adapter, not a bit-exact fres emulator.
inline std::optional<std::uint16_t> bearing(Position d) {
    if (!std::isfinite(d.x) || !std::isfinite(d.z)) return {};
    const float ax = std::abs(d.x), den = ax + std::abs(d.z);
    if (!(den > 0.f) || !std::isfinite(den)) return {};
    auto f = [](std::uint32_t v) { return std::bit_cast<float>(v); };
    float inv = 1.f / den;
    inv = std::fma(inv, std::fma(-den, inv, 1.f), inv);
    inv = std::fma(inv, std::fma(-den, inv, 1.f), inv);
    const float u = (d.z >= 0.f ? d.z - ax : d.z + ax) * inv;
    const float q = u * u;
    float p = std::fma(q, f(0x3b3bd74a), -f(0x3c846e02));
    float low = std::fma(q, f(0xbe117fc7), f(0x3e4cbbe5));
    p = std::fma(q, p, f(0x3d2fc1fe));
    low = std::fma(q, low, -f(0x3eaaaa6c));
    p = std::fma(q, p, -f(0x3d9a3174));
    const float q2 = q * q, q4 = q2 * q2;
    const float lo = std::fma(u * q, low, u);
    p = std::fma(q, p, f(0x3dda3d83));
    float a = (d.z >= 0.f ? f(0x3f490fdb) : f(0x4016cbe4)) - std::fma(u * q4, p, lo);
    if (d.x < 0.f) a = -a;
    const float scaled = a * f(0x4622f983);
    if (!std::isfinite(scaled)) return {};
    return static_cast<std::uint16_t>(static_cast<int>(scaled));
}
inline bool geometry(Position source, Position target, std::uint16_t facing, Branch branch) {
    const Position d{target.x-source.x, target.y-source.y, target.z-source.z};
    if (!std::isfinite(d.x) || !std::isfinite(d.y) || !std::isfinite(d.z) || std::abs(d.y)>vertical_limit) return false;
    const float h = std::sqrt(std::abs(std::fma(d.x,d.x,d.z*d.z)));
    if (!std::isfinite(h) || h>range(branch)) return false;
    if (h<=facing_exemption_radius) return true;
    const auto angle = bearing(d);
    return angle && yaw_window(*angle,facing,branch);
}
// +58 state and relation are raw original values. Callers must resolve their
// own team/life/status data; this does not label unknown status160/136 as stun.
inline constexpr bool participant_state(int state, int rule) {
    return state!=1 && state!=3 && state!=6 && !(state==0 && rule==4);
}
struct Peer { int slot{}, state{}, group{}; bool active{}, status160{}, status136{}, status22{}, status140{}; };
inline constexpr bool eligible(Peer s, Peer t, int rule, int original_relation) {
    return s.slot>=0 && s.slot<24 && t.slot>=0 && t.slot<24 && s.slot!=t.slot &&
        s.active && t.active && !s.status160 && !s.status136 && !t.status160 && !t.status136 &&
        s.group!=t.group && original_relation==2 && participant_state(s.state,rule) &&
        participant_state(t.state,rule) && (s.status22 || s.status140);
}
inline constexpr int suppressed_level(int source_level, int target_skill23) {
    return (std::max)(source_level-target_skill23,0);
}
// Membership projection of 81A6C4..81A9F4. Scan-owner/duration records are
// deliberately outside this helper. Pair order is source -> target.
struct Groups {
    std::array<int,24> value{};
    constexpr Groups() { reset(); }
    constexpr void reset() { for(int i=0;i<24;++i) value[i]=i; }
    constexpr void clear(int slot) { if(slot>=0 && slot<24) value[slot]=slot; }
    constexpr int group(int slot) const { return slot>=0 && slot<24 && value[slot]>23 ? value[slot] : -1; }
    constexpr bool merge(int source,int target) {
        if(source<0 || source>=24 || target<0 || target>=24 || source==target) return false;
        const int a=value[source],b=value[target];
        if(a<0 || b<0 || a==b) return false;
        if(a!=source) {
            if(b!=target) { for(auto& v:value) if(v==a) v=b; }
            else value[target]=a;
        } else if(b!=target) value[source]=b;
        else {
            int fresh=24;
            for(;fresh<=63;++fresh) { bool used=false; for(auto v:value) if(v==fresh) used=true; if(!used) break; }
            if(fresh>63) return false; // impossible for 24 valid members; safe native guard.
            value[source]=value[target]=fresh;
        }
        return true;
    }
    constexpr void dissolve_singletons() {
        for(int i=0;i<24;++i) if(value[i]>23) {
            bool other=false; for(int j=0;j<24;++j) if(i!=j && value[i]==value[j]) other=true;
            if(!other) value[i]=i;
        }
    }
};
} // namespace mgo2win::original_sop
