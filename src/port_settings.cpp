#include "product_identity.h"
#include <winsock2.h>
#include <windows.h>
#include "port_settings.h"
#include <fstream>
#include <string>
#include <stdexcept>
namespace mgo2mt {
bool parse_port(std::wstring_view s,uint16_t& result){
 if(s.empty()||s.size()>5)return false;unsigned n=0;
 for(auto c:s){if(c<L'0'||c>L'9')return false;n=n*10+unsigned(c-L'0');}
 if(n<1024||n>65535)return false;result=static_cast<uint16_t>(n);return true;
}
bool load_ports(const std::filesystem::path& path,PortSettings& settings){
 if(!std::filesystem::exists(path))return false;
 if(std::filesystem::file_size(path)>128)throw std::runtime_error("Oversized port settings");
 std::ifstream f(path,std::ios::binary);std::string tag,version,automatic,port,bandwidth,extra;
 if(!(f>>tag>>version>>automatic>>port)||tag!=mgo2mt::brand::Format{"MGO2MT.NETWORK"}||(version!="1"&&version!="2")||(automatic!="0"&&automatic!="1"))throw std::runtime_error("Invalid port settings");
 unsigned speed=512;
 if(version=="2"){
  if(!(f>>bandwidth)||bandwidth.size()>4)throw std::runtime_error("Invalid bandwidth");
  speed=0;for(auto c:bandwidth){if(c<'0'||c>'9')throw std::runtime_error("Invalid bandwidth");speed=speed*10+unsigned(c-'0');}
  if(!valid_bandwidth(speed))throw std::runtime_error("Invalid bandwidth");
 }
 if(f>>extra||!f.eof())throw std::runtime_error("Invalid port settings");
 uint16_t n=0;if(!parse_port(std::wstring(port.begin(),port.end()),n))throw std::runtime_error("Invalid saved port");
 settings={automatic=="1",n,static_cast<uint16_t>(speed)};return true;
}
void save_ports(const std::filesystem::path& path,const PortSettings& s){
 if(s.port<1024||!valid_bandwidth(s.bandwidth_kbps))throw std::runtime_error("Invalid network settings");
 std::filesystem::create_directories(path.parent_path());auto temp=path;temp+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
 try{
  {std::ofstream f(temp,std::ios::binary|std::ios::trunc);f<<"MGO2MT.NETWORK 2\n"<<unsigned(s.automatic)<<' '<<s.port<<' '<<s.bandwidth_kbps<<'\n';f.close();if(!f)throw std::runtime_error("Port settings write failure");}
  if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Port settings replace failure");
 }catch(...){std::error_code ec;std::filesystem::remove(temp,ec);throw;}
}
PortReservation::~PortReservation(){reset();if(started_)WSACleanup();}
void PortReservation::reset(){if(socket_!=~uintptr_t(0)){closesocket(static_cast<SOCKET>(socket_));socket_=~uintptr_t(0);}}
PortResult PortReservation::check(const PortSettings& settings){
 reset();PortResult r;if(settings.port<1024){r.status=PortStatus::failed;r.error=WSAEINVAL;return r;}
 if(!started_){WSADATA data{};int e=WSAStartup(MAKEWORD(2,2),&data);if(e){r.status=PortStatus::failed;r.error=e;return r;}started_=true;}
 auto attempt=[&](uint16_t port){
  SOCKET s=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(s==INVALID_SOCKET){r.error=WSAGetLastError();return false;}
  BOOL exclusive=TRUE;
  if(setsockopt(s,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&exclusive),sizeof(exclusive))==SOCKET_ERROR){r.error=WSAGetLastError();closesocket(s);return false;}
  sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_ANY);address.sin_port=htons(port);
  if(bind(s,reinterpret_cast<const sockaddr*>(&address),sizeof(address))==SOCKET_ERROR){r.error=WSAGetLastError();closesocket(s);return false;}
  int size=sizeof(address);if(getsockname(s,reinterpret_cast<sockaddr*>(&address),&size)==SOCKET_ERROR){r.error=WSAGetLastError();closesocket(s);return false;}
  socket_=static_cast<uintptr_t>(s);r.port=ntohs(address.sin_port);r.error=0;return true;
 };
 if(attempt(settings.port)||(settings.automatic&&(r.error==WSAEADDRINUSE||r.error==WSAEACCES)&&attempt(0)))r.status=PortStatus::available;
 else r.status=r.error==WSAEADDRINUSE?PortStatus::in_use:r.error==WSAEACCES?PortStatus::denied:PortStatus::failed;
 return r;
}
}
