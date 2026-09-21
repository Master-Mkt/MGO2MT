#pragma once
#include <cstdint>
#include <filesystem>
#include <string_view>
namespace mgo2mt {
// Native setting policy. Original default 5730 / >1023 guard: AA19E0..AA19F0.
// User-requested native bandwidth choices, kbps. Not an inferred PS3 wire value.
constexpr bool valid_bandwidth(unsigned n){return n>=256&&n<=2048&&n%256==0;}
struct PortSettings {bool automatic=false;uint16_t port=5730;uint16_t bandwidth_kbps=512;};
bool parse_port(std::wstring_view,uint16_t&);
bool load_ports(const std::filesystem::path&,PortSettings&);
void save_ports(const std::filesystem::path&,const PortSettings&);
enum class PortStatus {unchecked,available,in_use,denied,failed};
struct PortResult {PortStatus status=PortStatus::unchecked;uint16_t port=0;int error=0;};
// Holds the tested UDP/IPv4 socket exclusively until reset/destruction.
// A successful local bind makes no statement about firewall/NAT reachability.
class PortReservation {
 uintptr_t socket_=~uintptr_t(0);bool started_=false;
public:
 PortReservation()=default;~PortReservation();
 PortReservation(const PortReservation&)=delete;PortReservation& operator=(const PortReservation&)=delete;
 void reset();PortResult check(const PortSettings&);
 uintptr_t native_socket()const{return socket_;}
};
}
