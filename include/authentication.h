#pragma once
#include <array>
#include <atomic>
#include <string>
#include <string_view>
#include <cstdint>
namespace mgo2mt {
inline constexpr wchar_t login_url[]=L"https://openmgo2.com/index.php";
enum class AuthStatus { success,denied,network_error,protocol_error,cancelled };
struct AuthReply {
 AuthStatus status=AuthStatus::network_error;unsigned http=0;uint32_t user=0;
 std::array<uint32_t,10> slots{};std::array<uint8_t,8> session{};
 ~AuthReply(){volatile uint8_t*p=session.data();for(size_t i=0;i<session.size();++i)p[i]=0;}
};
struct AuthCredentials {
 std::array<wchar_t,65> id{},password{};
 AuthCredentials(std::wstring_view,std::wstring_view);
 ~AuthCredentials();
 AuthCredentials(const AuthCredentials&)=delete;AuthCredentials& operator=(const AuthCredentials&)=delete;
};
// Contract from OpenMGO2's local PS3-compatible handler/registration source.
std::string login_body(std::wstring_view id,std::wstring_view password,std::string_view seed);
AuthReply parse_login_reply(unsigned http,std::wstring_view type,std::string_view body);
AuthReply authenticate(const AuthCredentials&,const std::atomic_bool& cancel);
// Explicit empty-input probe: fails before DB lookup in the reviewed handler.
AuthReply probe_login_route();
}
