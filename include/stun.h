#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <span>
namespace mgo2win {
using StunId=std::array<uint8_t,12>;
enum class StunStatus {unchecked,success,timeout,cancelled,network_error,protocol_error,server_error};
struct StunResult {StunStatus status=StunStatus::unchecked;uint16_t mapped_port=0;std::array<uint8_t,4> address{};unsigned attempts=0;int error=0;};
// OpenMGO2's existing coturn, confirmed by read-only service/socket inspection.
inline constexpr std::array<uint8_t,4> stun_server={49,212,132,180};
inline constexpr uint16_t stun_port=3478;
std::array<uint8_t,20> stun_request(const StunId&);
StunResult parse_stun(std::span<const uint8_t>,const StunId&);
// Uses an already reserved UDP socket. No account credentials or redirects.
StunResult check_stun(uintptr_t socket,const std::atomic_bool& cancel);
}
