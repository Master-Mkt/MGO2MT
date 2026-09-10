#include <winsock2.h>
#include <windows.h>
#include <bcrypt.h>
#include "stun.h"
#include <algorithm>
namespace mgo2win {
namespace {
uint16_t u16(std::span<const uint8_t>b,size_t p){return uint16_t(b[p]*256+b[p+1]);}
uint32_t u32(std::span<const uint8_t>b,size_t p){return uint32_t(b[p])<<24|uint32_t(b[p+1])<<16|uint32_t(b[p+2])<<8|b[p+3];}
uint32_t crc(std::span<const uint8_t>b){uint32_t c=0xffffffff;for(auto x:b){c^=x;for(int i=0;i<8;++i)c=(c>>1)^((c&1)?0xedb88320:0);}return ~c;}
}
std::array<uint8_t,20> stun_request(const StunId& id){std::array<uint8_t,20> b={0,1,0,0,0x21,0x12,0xa4,0x42};std::copy(id.begin(),id.end(),b.begin()+8);return b;}
StunResult parse_stun(std::span<const uint8_t>b,const StunId& id){
 StunResult bad;bad.status=StunStatus::protocol_error;
 if(b.size()<20||b.size()>2048||u32(b,4)!=0x2112a442||u16(b,2)%4||size_t(u16(b,2))+20!=b.size()||!std::equal(id.begin(),id.end(),b.begin()+8))return bad;
 auto type=u16(b,0);if(type!=0x0101&&type!=0x0111)return bad;
 StunResult mapped,xored;bool haveMapped=false,haveXor=false,haveError=false;int error=0;
 for(size_t p=20;p<b.size();){
  if(p+4>b.size())return bad;auto attr=u16(b,p),len=u16(b,p+2);size_t end=p+4+len,next=p+4+((size_t(len)+3)&~size_t(3));if(next>b.size())return bad;
  if(attr==0x0001||attr==0x0020){
   if(len!=8||b[p+4]!=0||b[p+5]!=1)return bad;auto&r=attr==0x0020?xored:mapped;bool&seen=attr==0x0020?haveXor:haveMapped;if(seen)return bad;seen=true;
   r.mapped_port=u16(b,p+6)^(attr==0x0020?0x2112:0);if(!r.mapped_port)return bad;
   for(size_t i=0;i<4;++i)r.address[i]=b[p+8+i]^(attr==0x0020?b[4+i]:0);
   if(r.address[0]==0||r.address[0]>=224)return bad;
  }else if(attr==0x0009){if(haveError||len<4||b[p+4]||b[p+5]||(b[p+6]&0xf8)||b[p+6]<3||b[p+6]>6||b[p+7]>99)return bad;haveError=true;error=b[p+6]*100+b[p+7];}
  else if(attr==0x8028){if(len!=4||next!=b.size()||u32(b,p+4)!=(crc(b.first(p))^0x5354554e))return bad;}
  else if(attr<0x8000)return bad; // Unknown required or unsolicited integrity/auth attributes.
  p=next;(void)end;
 }
 if(type==0x0111){if(!haveError||haveMapped||haveXor)return bad;StunResult r;r.status=StunStatus::server_error;r.error=error;return r;}
 if(haveError||(!haveXor&&!haveMapped))return bad;
 auto r=haveXor?xored:mapped;r.status=StunStatus::success;return r;
}
StunResult check_stun(uintptr_t handle,const std::atomic_bool& cancel){
 StunResult result;result.status=StunStatus::network_error;if(cancel){result.status=StunStatus::cancelled;return result;}
 SOCKET s=static_cast<SOCKET>(handle);if(s==INVALID_SOCKET){result.error=WSAENOTSOCK;return result;}
 StunId id{};if(BCryptGenRandom(nullptr,id.data(),ULONG(id.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)return result;
 const auto request=stun_request(id);sockaddr_in peer{};peer.sin_family=AF_INET;peer.sin_port=htons(stun_port);std::copy(stun_server.begin(),stun_server.end(),reinterpret_cast<uint8_t*>(&peer.sin_addr));
 u_long nonblocking=1;if(ioctlsocket(s,FIONBIO,&nonblocking)){result.error=WSAGetLastError();return result;}
 // Bounded UI diagnostic: three sends at 0/500/1500ms, total 3.5s. No keepalive.
 const auto start=GetTickCount64();unsigned sent=0;bool malformed=false;const unsigned due[]={0,500,1500};
 while(GetTickCount64()-start<3500){
  if(cancel){result.status=StunStatus::cancelled;return result;}
  if(sent<3&&GetTickCount64()-start>=due[sent]){
   int n=sendto(s,reinterpret_cast<const char*>(request.data()),int(request.size()),0,reinterpret_cast<sockaddr*>(&peer),sizeof(peer));
   if(n!=int(request.size())){result.error=WSAGetLastError();return result;}result.attempts=++sent;
  }
  fd_set reads;FD_ZERO(&reads);FD_SET(s,&reads);timeval wait{0,50000};int ready=select(0,&reads,nullptr,nullptr,&wait);
  if(ready==SOCKET_ERROR){result.error=WSAGetLastError();return result;}if(!ready)continue;
  std::array<uint8_t,2048> bytes{};sockaddr_in from{};int size=sizeof(from);int n=recvfrom(s,reinterpret_cast<char*>(bytes.data()),int(bytes.size()),0,reinterpret_cast<sockaddr*>(&from),&size);
  if(n==SOCKET_ERROR){auto e=WSAGetLastError();if(e==WSAEWOULDBLOCK||e==WSAEMSGSIZE)continue;result.error=e;return result;}
  if(size!=sizeof(from)||from.sin_family!=AF_INET||from.sin_port!=peer.sin_port||from.sin_addr.s_addr!=peer.sin_addr.s_addr)continue;
  if(n<20||!std::equal(id.begin(),id.end(),bytes.begin()+8))continue;
  auto parsed=parse_stun(std::span(bytes).first(size_t(n)),id);
  if(parsed.status==StunStatus::protocol_error){malformed=true;continue;}
  parsed.attempts=sent;return parsed;
 }
 result.status=malformed?StunStatus::protocol_error:StunStatus::timeout;return result;
}
}
