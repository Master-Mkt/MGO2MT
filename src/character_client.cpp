#include "lobby_keepalive.h"
#include "invitation_lobby.h"
#include "combat_service.h"
#include "combat_world.h"
#include "combat_spawn.h"
#include "combat_initial_profile.h"
#include "combat_cycle.h"
#include "native_radio_context.h"
#include "host_appearance.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include "character_client.h"
#include "world_inventory_host.h"
#include "clan_download_queue.h"
#include "lobby_groups.h"
#include "character_creation.h"
#include "stun.h"
#include "dedicated_service.h"
#include "dedicated_peer.h"
#include "host_briefing.h"
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <set>
#include <cstring>
#include <thread>
#include <chrono>
#include <iostream>
#include <syncstream>
namespace mgo2win {
namespace {
const char* stun_status_name(StunStatus status){switch(status){
 case StunStatus::success:return "success";case StunStatus::timeout:return "timeout";case StunStatus::cancelled:return "cancelled";
 case StunStatus::network_error:return "network_error";case StunStatus::protocol_error:return "protocol_error";case StunStatus::server_error:return "server_error";default:return "unchecked";
}}
struct DedicatedTime {
 uint64_t ms;uint32_t subMsNs;
 static DedicatedTime read(){
  const auto ns=uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
  return {ns/1000000,uint32_t(ns%1000000)};
 }
};
// Local timer, no process-wide timer-resolution change. The high-resolution
// flag is supported on recent Windows; older systems keep a safe slower wait.
struct DedicatedPollWait {
 HANDLE timer=CreateWaitableTimerExW(nullptr,nullptr,0x2,TIMER_MODIFY_STATE|SYNCHRONIZE);
 ~DedicatedPollWait(){if(timer)CloseHandle(timer);}
 void wait(bool active){
  const int ms=active?1:10;
  LARGE_INTEGER due;due.QuadPart=-int64_t(ms)*10000;
  if(timer&&SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE)&&WaitForSingleObject(timer,50)==WAIT_OBJECT_0)return;
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
 }
};
uint32_t be(std::span<const uint8_t> b,size_t at,unsigned n=4){if(at+n>b.size())throw std::runtime_error("truncated lobby data");uint32_t r=0;for(unsigned i=0;i<n;++i)r=(r<<8)|b[at+i];return r;}
void put(std::span<uint8_t>b,size_t at,uint32_t v,unsigned n=4){if(at+n>b.size())throw std::runtime_error("lobby extent");while(n){b[at+--n]=uint8_t(v);v>>=8;}}
std::array<uint8_t,16> mac(const NetworkKeys& k,std::span<const uint8_t> b){
 BCRYPT_ALG_HANDLE a=nullptr;BCRYPT_HASH_HANDLE h=nullptr;std::array<uint8_t,16> out{};
 auto ok=[](NTSTATUS s){if(s<0)throw std::runtime_error("lobby checksum provider");};
 try{ok(BCryptOpenAlgorithmProvider(&a,BCRYPT_MD5_ALGORITHM,nullptr,BCRYPT_ALG_HANDLE_HMAC_FLAG));ok(BCryptCreateHash(a,&h,nullptr,0,const_cast<PUCHAR>(k.hmac.data()),16,0));ok(BCryptHashData(h,const_cast<PUCHAR>(b.data()),8,0));if(b.size()>24)ok(BCryptHashData(h,const_cast<PUCHAR>(b.data()+24),ULONG(b.size()-24),0));ok(BCryptFinishHash(h,out.data(),16,0));}
 catch(...){if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);throw;}
 BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);return out;
}
struct Failure {CharacterStatus status;unsigned code;};
void result(std::span<const uint8_t> b){if(b.size()!=4)throw std::runtime_error("lobby result extent");auto r=be(b,0);if(r!=0&&r!=0xc0ffee00)throw Failure{CharacterStatus::server_error,r};}
struct Wsa {Wsa(){WSADATA d{};int e=WSAStartup(MAKEWORD(2,2),&d);if(e)throw Failure{CharacterStatus::network_error,unsigned(e)};}~Wsa(){WSACleanup();}};
class Connection {
 SOCKET s_=INVALID_SOCKET;const NetworkKeys& keys_;const std::atomic_bool& cancel_;ULONGLONG deadline_;uint32_t tx_=1,rx_=1;bool cleanup_=false;LobbyKeepalive keepalive_{GetTickCount64()};
 void wait(bool write){
  for(;;){if(cancel_&&!cleanup_)throw Failure{CharacterStatus::cancelled,0};if(GetTickCount64()>=deadline_)throw Failure{CharacterStatus::network_error,WSAETIMEDOUT};fd_set f,e;FD_ZERO(&f);FD_ZERO(&e);FD_SET(s_,&f);FD_SET(s_,&e);timeval t{0,50000};int n=select(0,write?nullptr:&f,write?&f:nullptr,&e,&t);if(n<0)throw Failure{CharacterStatus::network_error,unsigned(WSAGetLastError())};if(n>0){int err=0,len=sizeof(err);if(getsockopt(s_,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&err),&len)||err)throw Failure{CharacterStatus::network_error,unsigned(err?err:WSAGetLastError())};return;}}
 }
 void receive(std::span<uint8_t>b){size_t at=0;while(at<b.size()){wait(false);int n=recv(s_,reinterpret_cast<char*>(b.data()+at),int(b.size()-at),0);if(n<0&&WSAGetLastError()==WSAEWOULDBLOCK)continue;if(n<=0)throw Failure{CharacterStatus::network_error,unsigned(n?WSAGetLastError():WSAECONNRESET)};at+=n;}}
public:
 std::function<void(const LobbyPacket&)> nativeSkills,nativeHostSkills,nativeChat,nativeNames,nativeInvitations;
 Connection(const NetworkKeys&k,const std::atomic_bool&c):keys_(k),cancel_(c),deadline_(GetTickCount64()+8000){}
 ~Connection(){if(s_!=INVALID_SOCKET)closesocket(s_);}
 void reset_deadline(bool cleanup=false){cleanup_=cleanup;deadline_=GetTickCount64()+(cleanup?1500:8000);}
 std::array<uint8_t,4> local_address()const{sockaddr_in a{};int n=sizeof(a);if(getsockname(s_,reinterpret_cast<sockaddr*>(&a),&n))throw Failure{CharacterStatus::network_error,unsigned(WSAGetLastError())};std::array<uint8_t,4>b;std::copy_n(reinterpret_cast<const uint8_t*>(&a.sin_addr),4,b.begin());return b;}
 void connect_to(uint16_t port){if(port<5731||port>5739)throw std::runtime_error("unreviewed lobby port");s_=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(s_==INVALID_SOCKET)throw Failure{CharacterStatus::network_error,unsigned(WSAGetLastError())};u_long nonblocking=1;if(ioctlsocket(s_,FIONBIO,&nonblocking))throw Failure{CharacterStatus::network_error,unsigned(WSAGetLastError())};sockaddr_in peer{};peer.sin_family=AF_INET;peer.sin_port=htons(port);InetPtonW(AF_INET,L"49.212.132.180",&peer.sin_addr);if(connect(s_,reinterpret_cast<sockaddr*>(&peer),sizeof(peer))&&WSAGetLastError()!=WSAEWOULDBLOCK)throw Failure{CharacterStatus::network_error,unsigned(WSAGetLastError())};wait(true);}
 void send_packet(uint16_t cmd,std::span<const uint8_t> payload={}){auto b=encode_lobby(keys_,cmd,tx_++,payload);struct Wipe{std::vector<uint8_t>&v;~Wipe(){SecureZeroMemory(v.data(),v.size());}}wipe{b};size_t at=0;while(at<b.size()){wait(true);int n=send(s_,reinterpret_cast<char*>(b.data()+at),int(b.size()-at),0);if(n<0&&WSAGetLastError()==WSAEWOULDBLOCK)continue;if(n<=0)throw Failure{CharacterStatus::network_error,unsigned(n?WSAGetLastError():WSAECONNRESET)};at+=n;}}
 bool alive()const{char value;int n=recv(s_,&value,1,MSG_PEEK);return n>0||(n==SOCKET_ERROR&&WSAGetLastError()==WSAEWOULDBLOCK);}
 LobbyPacket read_raw(){std::vector<uint8_t>b(24);receive(b);unsigned n=((b[2]^keys_.wire[2])<<8)|(b[3]^keys_.wire[3]);if(n>1023)throw std::runtime_error("lobby payload too large");b.resize(24+n);receive(std::span(b).subspan(24));return decode_lobby(keys_,b,rx_++);}
 bool common_reply(const LobbyPacket&p){if(!keepalive_.receive(p.command,p.payload,GetTickCount64()))return false;try{std::osyncstream(std::cout)<<"{\"lobby_keepalive\":\"received\"}"<<std::endl;}catch(...){}return true;}
 bool chat_reply(const LobbyPacket&p){if(p.command!=chat::receive_opcode&&p.command!=chat::capability_reply_opcode)return false;if(nativeChat)nativeChat(p);return true;}
 bool name_reply(const LobbyPacket&p){if(p.command!=names::account_reply_opcode&&p.command!=names::game_reply_opcode)return false;if(nativeNames)nativeNames(p);return true;}
 bool invitation_reply(const LobbyPacket&p){if(!invitations::asynchronous(p.command))return false;if(nativeInvitations)nativeInvitations(p);return true;}
 LobbyPacket read(){for(;;){auto p=read_raw();if(common_reply(p)||chat_reply(p)||name_reply(p)||invitation_reply(p))continue;if(nativeSkills&&(p.command==0x41e1||p.command==0x41e3||p.command==0x4b4b)){nativeSkills(p);continue;}if(nativeHostSkills&&p.command==host_skills::reply_opcode){nativeHostSkills(p);continue;}return p;}}
 void poll_skills(){for(unsigned i=0;i<8;++i){
  u_long available=0;if(ioctlsocket(s_,FIONREAD,&available))throw Failure{CharacterStatus::network_error,unsigned(WSAGetLastError())};
  if(available<24)return;std::array<uint8_t,24> header{};if(recv(s_,reinterpret_cast<char*>(header.data()),24,MSG_PEEK)!=24)return;
  const unsigned cmd=((header[0]^keys_.wire[0])<<8)|(header[1]^keys_.wire[1]);
  if(!invitations::asynchronous(uint16_t(cmd))&&cmd!=names::account_reply_opcode&&cmd!=names::game_reply_opcode&&cmd!=chat::receive_opcode&&cmd!=chat::capability_reply_opcode&&cmd!=0x0005&&!(nativeSkills&&(cmd==0x41e1||cmd==0x41e3||cmd==0x4b4b))&&!(nativeHostSkills&&cmd==host_skills::reply_opcode))return;
  const unsigned n=((header[2]^keys_.wire[2])<<8)|(header[3]^keys_.wire[3]);if(n>1023)throw std::runtime_error("async lobby packet size");if(available<24+n)return;
  // A complete asynchronous frame can arrive long after the preceding RPC.
  // It needs its own bounded receive deadline, not the expired RPC deadline.
  reset_deadline();auto packet=read_raw();
  if(!common_reply(packet)&&!chat_reply(packet)&&!name_reply(packet)&&!invitation_reply(packet)){if(packet.command==host_skills::reply_opcode)nativeHostSkills(packet);else nativeSkills(packet);}
 }}
 void keep_alive(){bool send=false;try{send=keepalive_.poll(GetTickCount64());}catch(...){try{std::osyncstream(std::cout)<<"{\"lobby_keepalive\":\"reply_timeout\"}"<<std::endl;}catch(...){}throw;}if(send){reset_deadline();send_packet(0x0005);try{std::osyncstream(std::cout)<<"{\"lobby_keepalive\":\"sent\"}"<<std::endl;}catch(...){}}}

};
uint64_t name_nonce(){uint64_t value=0;do{if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&value),sizeof(value),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("name nonce provider");}while(!value);return value;}
std::string name_utf8(std::wstring_view value){const auto n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);if(n<=0||n>64)throw std::runtime_error("name encoding extent");std::string text(n,0);if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),text.data(),n,nullptr,nullptr)!=n)throw std::runtime_error("name encoding");return text;}
std::wstring name_wide(std::string_view value){const auto n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);if(n<=0||n>32)throw std::runtime_error("name display extent");std::wstring text(n,0);if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),text.data(),n)!=n)throw std::runtime_error("name display encoding");return text;}
class AccountNames {
 Connection& connection_;uint64_t nonce_;std::optional<names::Reply> reply_;
public:
 explicit AccountNames(Connection& connection):connection_(connection),nonce_(name_nonce()){
  connection_.nativeNames=[this](const LobbyPacket&p){if(p.command!=names::account_reply_opcode||reply_)return;try{auto reply=names::parse(p.payload);if(reply.status||reply.op!=names::Operation::own||reply.nonce!=nonce_||!(reply.capabilities&names::capability_own))return;reply_=std::move(reply);}catch(const std::exception&){}};
  connection_.send_packet(names::account_request_opcode,names::query(names::Operation::own,nonce_));
 }
 ~AccountNames(){connection_.nativeNames={};}
 bool native_ready()const{return reply_&&(reply_->capabilities&names::capability_create);}
 void apply(CharacterEntry& entry)const{if(!reply_)return;for(const auto&record:reply_->records)if(record.id==entry.id){if(entry.legacy_name.empty())entry.legacy_name=entry.name;entry.name=name_wide(record.name);entry.main=record.main;break;}}
 void apply(CharacterList& list)const{if(!reply_)return;for(const auto&record:reply_->records)if(std::none_of(list.entries.begin(),list.entries.end(),[&](const auto&e){return e.id==record.id;}))return;for(auto&entry:list.entries)apply(entry);list.unicode_names=native_ready();}
};
uint16_t gateway(Connection&c){c.connect_to(5731);c.send_packet(0x2005);auto p=c.read();if(p.command!=0x2002)throw std::runtime_error("gate start command");result(p.payload);uint16_t port=0;for(unsigned i=0;i<16;++i){p=c.read();if(p.command==0x2004){result(p.payload);if(!port)throw std::runtime_error("no reviewed account endpoint");return port;}if(p.command!=0x2003)throw std::runtime_error("gate list command");auto found=account_endpoint(p.payload);if(found){if(port)throw std::runtime_error("ambiguous account endpoint");port=found;}}throw std::runtime_error("too many gate packets");}
CharacterReply run(const std::filesystem::path&path,const AuthReply*auth,const std::atomic_bool&cancel){CharacterReply r;try{
 if(cancel)throw Failure{CharacterStatus::cancelled,0};auto keys=NetworkKeys::load(path);Wsa wsa;r.stage=CharacterStage::gate_connect;
 {Connection gate(keys,cancel);r.stage=CharacterStage::gate_list;r.account_port=gateway(gate);}
 if(auth){r.stage=CharacterStage::account_connect;Connection account(keys,cancel);account.connect_to(r.account_port);r.stage=CharacterStage::session;
  auto payload=session_payload(keys,*auth);struct Wipe{std::vector<uint8_t>&v;~Wipe(){SecureZeroMemory(v.data(),v.size());}}wipe{payload};account.send_packet(0x3003,payload);SecureZeroMemory(payload.data(),payload.size());auto p=account.read();if(p.command!=0x3004)throw std::runtime_error("session reply command");result(p.payload);
  AccountNames names(account);r.stage=CharacterStage::list;account.send_packet(0x3048);p=account.read();if(p.command!=0x3049)throw std::runtime_error("character reply command");if(p.payload.size()==4)result(p.payload);r.list=parse_characters(p.payload);names.apply(r.list);
 }
 r.status=CharacterStatus::success;
 }catch(const Failure&f){r.status=f.status;r.error=f.code;}catch(...){r.status=CharacterStatus::protocol_error;}return r;}
}
NetworkKeys NetworkKeys::load(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f||f.tellg()!=8380)throw std::runtime_error("GNK extent");std::vector<uint8_t>b(8380);f.seekg(0);if(!f.read(reinterpret_cast<char*>(b.data()),b.size())||memcmp(b.data(),"GNK1",4)||be(b,4)!=1042||be(b,8)!=2||be(b,12))throw std::runtime_error("GNK header");NetworkKeys k;for(size_t i=0;i<1042;++i){k.packet[i]=be(b,16+4*i);k.auth[i]=be(b,4184+4*i);}std::copy_n(b.begin()+8352,16,k.hmac.begin());std::copy_n(b.begin()+8368,4,k.wire.begin());std::copy_n(b.begin()+8372,8,k.salt.begin());return k;}
void network_block(std::span<uint8_t>b,const std::array<uint32_t,1042>&t,bool encrypt){if(b.empty()||b.size()%8)throw std::runtime_error("block extent");auto f=[&](uint32_t x){return ((t[18+(x>>24)]+t[274+((x>>16)&255)])^t[530+((x>>8)&255)])+t[786+(x&255)];};for(size_t i=0;i<b.size();i+=8){uint32_t l=be(b,i)^t[encrypt?0:17],r=be(b,i+4);for(unsigned j=0;j<8;++j){r^=t[encrypt?1+2*j:16-2*j]^f(l);l^=t[encrypt?2+2*j:15-2*j]^f(r);}r^=t[encrypt?17:0];put(b,i,r);put(b,i+4,l);}}
std::vector<uint8_t> encode_lobby(const NetworkKeys&k,uint16_t cmd,uint32_t seq,std::span<const uint8_t>p){if(!cmd||!seq||p.size()>1023)throw std::runtime_error("packet range");std::vector<uint8_t>b(24+p.size());put(b,0,cmd,2);put(b,2,uint32_t(p.size()),2);put(b,4,seq);std::copy(p.begin(),p.end(),b.begin()+24);auto h=mac(k,b);std::copy(h.begin(),h.end(),b.begin()+8);for(size_t i=0;i<b.size();++i)b[i]^=k.wire[i%4];return b;}
LobbyPacket decode_lobby(const NetworkKeys&k,std::span<const uint8_t>wire,uint32_t expected){if(wire.size()<24||wire.size()>1047)throw std::runtime_error("packet extent");std::vector<uint8_t>b(wire.begin(),wire.end());for(size_t i=0;i<b.size();++i)b[i]^=k.wire[i%4];if(!be(b,0,2)||be(b,2,2)!=b.size()-24||!expected||be(b,4)!=expected)throw std::runtime_error("packet header");auto h=mac(k,b);unsigned diff=0;for(unsigned i=0;i<16;++i)diff|=h[i]^b[8+i];if(diff)throw std::runtime_error("packet checksum");return {uint16_t(be(b,0,2)),be(b,4),{b.begin()+24,b.end()}};}
std::vector<uint8_t> session_payload(const NetworkKeys&k,const AuthReply&a){if(a.status!=AuthStatus::success||!a.user||std::any_of(a.session.begin()+4,a.session.end(),[](uint8_t x){return x!=0;}))throw std::runtime_error("unsupported HTTP session contract");std::array<uint8_t,8>plain{};constexpr char hex[]="0123456789abcdef";for(unsigned i=0;i<4;++i){plain[2*i]=hex[a.session[i]>>4];plain[2*i+1]=hex[a.session[i]&15];}network_block(plain,k.auth,false);std::vector<uint8_t>b(24);put(b,0,a.user);for(unsigned i=0;i<8;++i)b[4+i]=plain[i]^k.salt[i];SecureZeroMemory(plain.data(),plain.size());network_block(b,k.packet,true);return b;}
uint16_t account_endpoint(std::span<const uint8_t>b){if(b.empty()||b.size()%46)throw std::runtime_error("gate record extent");uint16_t out=0;for(size_t i=0;i<b.size();i+=46){if(be(b,i+4)!=1)continue;std::string ip(reinterpret_cast<const char*>(b.data()+i+24),15);ip.resize(ip.find('\0')==std::string::npos?ip.size():ip.find('\0'));if(ip!="49.212.132.180"||be(b,i+39,2)!=5732||out)throw std::runtime_error("unreviewed account destination");out=5732;}return out;}
CharacterList parse_characters(std::span<const uint8_t>b){if(b.size()!=471||be(b,0)!=0||b[5]>8)throw std::runtime_error("character list extent");CharacterList out;out.slots=b[4];size_t at=7;std::set<uint32_t>ids;for(unsigned i=0;i<b[5];++i){if(i&&be(b,at)!=i)throw std::runtime_error("character index");at+=i?4:17;CharacterEntry e;e.id=be(b,at);if(!e.id||!ids.insert(e.id).second)throw std::runtime_error("character ID");auto str=b.subspan(at+4,16);auto end=std::find(str.begin(),str.end(),0);int n=int(end-str.begin());if(!n)throw std::runtime_error("empty character name");int len=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,reinterpret_cast<const char*>(str.data()),n,nullptr,0);if(!len)throw std::runtime_error("character name encoding");e.name.resize(len);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,reinterpret_cast<const char*>(str.data()),n,e.name.data(),len);for(auto c:e.name)if(c<32||c==127)throw std::runtime_error("character name controls");e.main=e.name.front()==L'*';if(e.main)e.name.erase(0,1);std::copy_n(b.begin()+at+20,28,e.appearance.begin());out.entries.push_back(std::move(e));at+=48;}return out;}
CharacterReply fetch_characters(const std::filesystem::path&p,const AuthReply&a,const std::atomic_bool&c){return run(p,&a,c);}
CharacterReply probe_character_gate(const std::filesystem::path&p,const std::atomic_bool&c){return run(p,nullptr,c);}
LobbyDirectory read_lobby_directory(const std::function<LobbyPacket()>&read){
 LobbyDirectory out;auto p=read();if(p.command!=0x2002)throw std::runtime_error("directory start");result(p.payload);
 std::set<uint16_t> ids;unsigned records=0;
 auto string=[](std::span<const uint8_t>b){auto end=std::find(b.begin(),b.end(),0);int n=int(end-b.begin());if(!n)throw std::runtime_error("empty directory string");int len=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,reinterpret_cast<const char*>(b.data()),n,nullptr,0);if(!len)throw std::runtime_error("directory encoding");std::wstring s(len,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,reinterpret_cast<const char*>(b.data()),n,s.data(),len);for(auto c:s)if(c<32||c==127)throw std::runtime_error("directory controls");return s;};
 // Bounded multi-packet stream; absence of the terminator is never success.
 for(unsigned packets=0;packets<16;++packets){p=read();if(p.command==0x2004){result(p.payload);if(!out.account_port)throw std::runtime_error("directory account missing");return out;}
  if(p.command!=0x2003||p.payload.empty()||p.payload.size()%46)throw std::runtime_error("directory record extent");
  for(size_t at=0;at<p.payload.size();at+=46){auto b=std::span(p.payload).subspan(at,46);if(++records>256||be(b,0)!=records-1)throw std::runtime_error("directory record index");auto type=be(b,4);if(type>2)throw std::runtime_error("directory type");
   auto name=string(b.subspan(8,16)),ip=string(b.subspan(24,15));auto port=uint16_t(be(b,39,2)),id=uint16_t(be(b,43,2));if(!port||!id||!ids.insert(id).second)throw std::runtime_error("directory identity");
   if(type==1){if(out.account_port||ip!=L"49.212.132.180"||port!=5732)throw std::runtime_error("unreviewed account endpoint");out.account_port=port;}
   if(type==2){if(ip!=L"49.212.132.180")throw std::runtime_error("unreviewed game host");out.games.push_back({id,port,uint16_t(be(b,41,2)),std::move(name),b[45]});}
  }
 }
 throw std::runtime_error("directory packet limit");
}
CharacterSelectionReply exchange_character_selection(uint32_t id,const CharacterExchange&exchange,const std::atomic_bool&cancel,CharacterSelectionContract contract){
 CharacterSelectionReply reply;
 if(contract!=CharacterSelectionContract::channel_snapshot_v1){reply.status=CharacterSelectionStatus::unavailable;return reply;}
 try{
  if(!id)throw std::runtime_error("selection requires stable ID");
  if(cancel){reply.status=CharacterSelectionStatus::cancelled;return reply;}
  auto packet=exchange(0x3048,{});if(packet.command!=0x3049)throw std::runtime_error("selection preflight command");if(packet.payload.size()==4)result(packet.payload);
  auto current=parse_characters(packet.payload);auto found=std::find_if(current.entries.begin(),current.entries.end(),[id](const auto&e){return e.id==id;});
  if(found==current.entries.end()){reply.status=CharacterSelectionStatus::missing;return reply;}
  reply.character=*found;const uint8_t index=uint8_t(found-current.entries.begin());
  if(cancel){reply.status=CharacterSelectionStatus::cancelled;return reply;}
  reply.request_may_have_been_sent=true;packet=exchange(0x3103,std::span(&index,1));
  if(packet.command!=0x3104||packet.payload.size()!=4)throw std::runtime_error("selection reply command/extent");
  reply.error=be(packet.payload,0);reply.status=reply.error?CharacterSelectionStatus::rejected:CharacterSelectionStatus::success;
 }catch(const Failure&f){reply.error=f.code;reply.status=reply.request_may_have_been_sent?CharacterSelectionStatus::outcome_unknown:f.status==CharacterStatus::cancelled?CharacterSelectionStatus::cancelled:f.status==CharacterStatus::server_error?CharacterSelectionStatus::rejected:CharacterSelectionStatus::network_error;}
 catch(...){reply.status=reply.request_may_have_been_sent?CharacterSelectionStatus::outcome_unknown:CharacterSelectionStatus::protocol_error;}
 return reply;
}
CharacterSelectionReply select_character(const std::filesystem::path&path,const AuthReply&auth,uint32_t id,const std::atomic_bool&cancel,CharacterSelectionContract contract){
 CharacterSelectionReply reply;
 // No setting or server-advertised field can lift this deployment prerequisite.
 if(contract!=CharacterSelectionContract::channel_snapshot_v1){reply.status=CharacterSelectionStatus::unavailable;return reply;}
 try{
  if(!id)throw std::runtime_error("selection requires ID");if(cancel){reply.status=CharacterSelectionStatus::cancelled;return reply;}
  auto membership=load_lobby_membership(path.parent_path()/L"lobbies.cfg");
  auto keys=NetworkKeys::load(path);Wsa wsa;LobbyDirectory directory;
  {Connection gate(keys,cancel);gate.connect_to(5731);gate.send_packet(0x2005);directory=read_lobby_directory([&]{return gate.read();});}
  apply_lobby_membership(directory.games,membership);
  Connection account(keys,cancel);account.connect_to(directory.account_port);
  auto payload=session_payload(keys,auth);struct Wipe{std::vector<uint8_t>&v;~Wipe(){SecureZeroMemory(v.data(),v.size());}}wipe{payload};
  account.send_packet(0x3003,payload);SecureZeroMemory(payload.data(),payload.size());auto p=account.read();if(p.command!=0x3004)throw std::runtime_error("selection session reply");result(p.payload);
  AccountNames names(account);reply=exchange_character_selection(id,[&](uint16_t cmd,std::span<const uint8_t>b){account.send_packet(cmd,b);return account.read();},cancel,contract);names.apply(reply.character);
  if(reply.status==CharacterSelectionStatus::success)reply.lobbies=std::move(directory.games);
 }catch(const Failure&f){reply.error=f.code;reply.status=f.status==CharacterStatus::cancelled?CharacterSelectionStatus::cancelled:f.status==CharacterStatus::server_error?CharacterSelectionStatus::rejected:CharacterSelectionStatus::network_error;}
 catch(...){reply.status=CharacterSelectionStatus::protocol_error;}
 return reply;
}
std::vector<uint8_t> game_session_payload(const NetworkKeys&keys,const AuthReply&auth,uint32_t id){
 if(!id)throw std::runtime_error("game session requires selected PC");AuthReply selected=auth;selected.user=id;return session_payload(keys,selected);
}
std::vector<RoomEntry> read_room_directory(const std::function<LobbyPacket()>&read){
 auto p=read();if(p.command!=0x4301)throw std::runtime_error("room list start");result(p.payload);
 std::vector<RoomEntry> rooms;std::set<uint32_t> ids;
 // Original F117F4 consumes 55 wire bytes into a 68-byte internal record,
 // maximum 1000 entries; deployed Games.getList emits at most 18 per packet.
 for(unsigned packet=0;packet<64;++packet){p=read();if(p.command==0x4303){result(p.payload);return rooms;}
  if(p.command!=0x4302||p.payload.empty()||p.payload.size()%55||p.payload.size()>990)throw std::runtime_error("room list extent");
  for(size_t at=0;at<p.payload.size();at+=55){auto b=std::span(p.payload).subspan(at,55);RoomEntry room;room.id=be(b,0);
   if(!room.id||!ids.insert(room.id).second||rooms.size()>=1000)throw std::runtime_error("room list identity/limit");
   auto str=b.subspan(4,16);auto end=std::find(str.begin(),str.end(),0);int n=int(end-str.begin());if(!n)throw std::runtime_error("empty room name");
   int len=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,reinterpret_cast<const char*>(str.data()),n,nullptr,0);if(!len)throw std::runtime_error("room name encoding");
   room.name.resize(len);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,reinterpret_cast<const char*>(str.data()),n,room.name.data(),len);
   for(auto c:room.name)if(c<32||c==127)throw std::runtime_error("room name controls");
   room.password=(b[20]&1)!=0;room.rule=b[22];room.map=b[23];room.capacity=b[25];room.players=b[29];rooms.push_back(std::move(room));
  }
 }throw std::runtime_error("room list terminator missing");
}
void run_game_lobby(const std::filesystem::path&path,const AuthReply&auth,uint32_t id,const GameLobbyEntry&lobby,const std::atomic_bool&cancel,std::atomic_bool&refresh,const RoomPublish&publish,RoomRequests& requests,uintptr_t udpSocket){
 struct ChatDisconnect {std::shared_ptr<chat::Session> session;~ChatDisconnect(){session->disconnect();}} chatDisconnect{requests.chatSession};
 struct InvitationDisconnect {std::shared_ptr<invitations::Session> session;~InvitationDisconnect(){session->bind(0,0);}} invitationDisconnect{requests.invitationSession};
 struct NameDisconnect {std::shared_ptr<names::Directory> directory;~NameDisconnect(){directory->connect(0);}} nameDisconnect{requests.nameDirectory};
 struct SkillDisconnect {std::shared_ptr<skills::Remote> remote;~SkillDisconnect(){remote->disconnected();}} skillDisconnect{requests.skills};
 try{
  if(cancel)return;auto rows=load_lobby_membership(path.parent_path()/L"lobbies.cfg");
  if(lobby.port<5733||lobby.port>5739||std::none_of(rows.begin(),rows.end(),[&](const auto&r){return r.id==lobby.id&&r.port==lobby.port&&r.subtype==lobby.subtype;}))throw std::runtime_error("unreviewed game endpoint");
  auto keys=NetworkKeys::load(path);Wsa wsa;Connection game(keys,cancel);game.connect_to(lobby.port);
  auto payload=game_session_payload(keys,auth,id);struct Wipe{std::vector<uint8_t>&v;~Wipe(){SecureZeroMemory(v.data(),v.size());}}wipe{payload};
  game.send_packet(0x3003,payload);SecureZeroMemory(payload.data(),payload.size());auto p=game.read();if(p.command!=0x3004)throw std::runtime_error("game session reply");result(p.payload);
  // This connection is restricted above/to connect_to's fixed OpenMGO2
  // destination. Its existing NOMADPX JP deployment already speaks UTF-8;
  // GWCH is optional discovery, not a prerequisite for Japanese chat.
  requests.chatSession->connect(id,chat::EndpointProfile::nomad_jp);
  const auto invitationScope=name_nonce();requests.invitationSession->bind(invitationScope,id);
  game.nativeInvitations=[session=requests.invitationSession,chat=requests.chatSession,invitationScope](const LobbyPacket&p){
   const auto now=GetTickCount64();
   invitations::dispatch(*session,invitationScope,p.command,p.payload,chat->state().encoding.value_or(chat::Encoding::utf8),now);
  };
  requests.nameDirectory->connect(id);
  game.nativeNames=[directory=requests.nameDirectory,session=requests.chatSession](const LobbyPacket&p){if(p.command==names::game_reply_opcode&&directory->receive(p.payload,GetTickCount64()))for(const auto&[character,name]:directory->resolved())session->display_name(character,name);};
  game.nativeChat=[session=requests.chatSession](const LobbyPacket&p){const auto now=GetTickCount64();if(p.command==chat::capability_reply_opcode){session->receive_capability(p.payload,now);return;}try{session->receive(chat::receive_payload(p.payload,session->state().encoding.value_or(chat::Encoding::utf8)),now);}catch(const std::exception&){/* Drop malformed chat; keep the room transport alive. */}};
  requests.skills->character(id);requests.skills->connected();
  requests.clanEmblem->want(0);requests.enemyClanEmblem->want(0);clan::DownloadQueue emblemQueue(requests.clanEmblem,requests.enemyClanEmblem);
  game.nativeSkills=[&](const LobbyPacket& packet){
   if(packet.command==0x4b4b){emblemQueue.tick(GetTickCount64());emblemQueue.receive(packet.payload);}
   else requests.skills->receive(skills::remote_reply(packet.payload),packet.command==0x41e3);
  };
  uint64_t chatCapabilityGeneration=0;
  auto pumpSkills=[&]{
   game.poll_skills();game.keep_alive();auto now=GetTickCount64();
   requests.invitationSession->tick(now);
   if(auto answer=requests.invitationSession->take(now)){try{game.reset_deadline();game.send_packet(invitations::answer_opcode,*answer);}catch(...){requests.invitationSession->submission_unknown(invitationScope,be(*answer,0));throw;}}
   if(requests.nameDirectory->needs_request(now))if(auto request=requests.nameDirectory->take(name_nonce(),now)){game.reset_deadline();game.send_packet(names::game_request_opcode,*request);}
   requests.chatSession->tick(now);if(requests.cancel_join&&requests.chatSession->state().room)requests.chatSession->leave();
   const auto chatState=requests.chatSession->state();if(chatState.joined&&chatState.generation!=chatCapabilityGeneration){chatCapabilityGeneration=chatState.generation;
    uint64_t nonce=0;if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&nonce),sizeof(nonce),BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0)if(auto cap=requests.chatSession->request_capability(nonce,now)){game.reset_deadline();game.send_packet(chat::capability_request_opcode,*cap);}}
   if(auto message=requests.chatSession->take(now)){try{game.reset_deadline();game.send_packet(chat::send_opcode,message->payload);}catch(...){requests.chatSession->failed(message->serial);throw;}}
   if(auto request=emblemQueue.take(now)){game.reset_deadline();game.send_packet(0x4b4a,*request);}
   requests.skills->tick(now);if(auto request=requests.skills->take(now)){game.reset_deadline();auto bytes=skills::remote_payload(*request);game.send_packet(request->write?0x41e2:0x41e0,bytes);}return game.alive();
  };
  while(!cancel){refresh=false;game.reset_deadline();constexpr uint8_t normalList[]={0,0,0,2};game.send_packet(0x4300,normalList);
   auto rooms=read_room_directory([&]{return game.read();});if(cancel)return;publish({RoomStatus::ready,rooms,0});
   auto next=GetTickCount64()+10000;while(!cancel&&!refresh&&GetTickCount64()<next){
    if(!pumpSkills())throw Failure{CharacterStatus::network_error,WSAECONNRESET};
    if(auto action=requests.take()){
     if(std::none_of(rooms.begin(),rooms.end(),[&](const RoomEntry&r){return r.id==action->id;})){RoomReply r;r.event=action->event;r.requested_room=action->id;r.status=RoomStatus::rejected;r.error=0xc0ffee03;publish(std::move(r));continue;}
     host::Local local;std::vector<uint8_t> profile;
     if(action->event==RoomEvent::join){
      RoomReply progress;progress.event=RoomEvent::join;progress.requested_room=action->id;progress.status=RoomStatus::connecting;progress.join_status=RoomJoinStatus::host_connecting;publish(progress);
      // Refresh the mapping on the SAME reserved socket before advertising it.
      if(udpSocket==~uintptr_t(0)){progress.status=RoomStatus::ready;progress.join_status=RoomJoinStatus::host_unavailable;publish(progress);continue;}
      sockaddr_in diagnosticBound{};int diagnosticSize=sizeof(diagnosticBound);uint16_t diagnosticPort=0;
      if(!getsockname(SOCKET(udpSocket),reinterpret_cast<sockaddr*>(&diagnosticBound),&diagnosticSize))diagnosticPort=ntohs(diagnosticBound.sin_port);
      const auto stunStarted=GetTickCount64();
      auto logStun=[&](const char*phase,const StunResult*result=nullptr)noexcept{try{
       std::osyncstream log(std::cout);log<<"{\"join_stun\":\""<<phase<<"\",\"local_port\":"<<diagnosticPort;
       if(result)log<<",\"status\":\""<<stun_status_name(result->status)<<"\",\"attempts\":"<<result->attempts<<",\"elapsed_ms\":"<<(GetTickCount64()-stunStarted)<<",\"error\":"<<result->error;
       log<<"}"<<std::endl;
      }catch(...){/* Diagnostics must not change admission behavior. */}};
      logStun("begin");auto stun=check_stun(udpSocket,cancel);logStun("end",&stun);
      if(cancel)return;if(requests.cancel_join){progress.status=RoomStatus::ready;progress.join_status=RoomJoinStatus::host_cancelled;publish(progress);continue;}
      if(stun.status!=StunStatus::success){progress.status=RoomStatus::ready;progress.join_status=stun.status==StunStatus::timeout?RoomJoinStatus::host_timeout:RoomJoinStatus::host_network_error;progress.error=unsigned(stun.error);publish(progress);continue;}
      sockaddr_in bound{};int boundSize=sizeof(bound);if(getsockname(SOCKET(udpSocket),reinterpret_cast<sockaddr*>(&bound),&boundSize))throw Failure{CharacterStatus::network_error,unsigned(WSAGetLastError())};
      local={udpSocket,{game.local_address(),ntohs(bound.sin_port)},{stun.address,stun.mapped_port},id};
      if(!host::valid_endpoint(local.private_endpoint)||!host::valid_endpoint(local.public_endpoint))throw std::runtime_error("local UDP endpoint");
      game.reset_deadline();game.send_packet(0x4100);
      std::vector<uint8_t> info,personal,skills;unsigned macros=0;bool settings=false,gear=false,skillSets=false,gearSets=false;
      for(unsigned i=0;i<12;++i){auto packet=game.read();if(packet.payload.size()==4)result(packet.payload);
       switch(packet.command){case 0x4101:if(!info.empty())throw std::runtime_error("duplicate profile");info=std::move(packet.payload);break;
       case 0x4120:if(settings)throw std::runtime_error("duplicate settings");settings=true;break;
       case 0x4121:if(++macros>2)throw std::runtime_error("duplicate macros");break;
       case 0x4122:if(!personal.empty())throw std::runtime_error("duplicate personal");personal=std::move(packet.payload);break;
       case 0x4124:if(gear)throw std::runtime_error("duplicate gear");gear=true;break;
       case 0x4125:if(!skills.empty())throw std::runtime_error("duplicate skills");skills=std::move(packet.payload);break;
       case 0x4140:if(skillSets)throw std::runtime_error("duplicate skill sets");skillSets=true;break;
       case 0x4142:if(gearSets)throw std::runtime_error("duplicate gear sets");gearSets=true;break;
       default:throw std::runtime_error("profile response command");}
       if(!info.empty()&&!personal.empty()&&!skills.empty()&&settings&&macros==2&&gear&&skillSets&&gearSets)break;
      }
      if(!settings||macros!=2||!gear||!skillSets||!gearSets)throw std::runtime_error("incomplete profile replies");profile=host::profile_payload(id,info,personal,skills);
      if(cancel)return;if(requests.cancel_join){progress.status=RoomStatus::ready;progress.join_status=RoomJoinStatus::host_cancelled;publish(progress);continue;}
      // F0Dxxx/Characters.updateConnectionInfo: private portBE + IP16 + public
      // portBE + reserved16; encrypted to 24 bytes. Public IP is server-observed.
      std::vector<uint8_t> connection(24);put(connection,0,local.private_endpoint.port,2);char ip[16]{};IN_ADDR ipAddress{};std::copy(local.private_endpoint.address.begin(),local.private_endpoint.address.end(),reinterpret_cast<uint8_t*>(&ipAddress));if(!InetNtopA(AF_INET,&ipAddress,ip,sizeof(ip)))throw std::runtime_error("private IP format");std::copy_n(reinterpret_cast<const uint8_t*>(ip),strlen(ip),connection.begin()+2);put(connection,18,local.public_endpoint.port,2);network_block(connection,keys.packet,true);game.reset_deadline();game.send_packet(0x4700,connection);auto updated=game.read();if(updated.command!=0x4701)throw std::runtime_error("endpoint update command");result(updated.payload);
     }
     auto reply=exchange_room_action(*action,[&](uint16_t command,std::span<const uint8_t> plain){game.reset_deadline(command==0x4322||command==0x4380);auto wire=room_action_wire_payload(keys,command,plain);Wipe wipe{wire};game.send_packet(command,wire);return game.read();},*requests.uncertain,requests.cancel_join,action->event==RoomEvent::join?HostConnect([&](const host::Admission& admission){
      if(admission.character==id)return host::Result{host::Stage::unavailable};
      requests.chatSession->enter(action->id);
      struct ChatLeave {std::shared_ptr<chat::Session> session;~ChatLeave(){session->leave();}} chatLeave{requests.chatSession};
      auto progress=[&](host::Result result){if(!host::active(result.stage))return;
       std::vector<uint32_t> nameIds;for(const auto&entry:result.roster.slots)if(entry)nameIds.push_back(entry->character);requests.nameDirectory->want(nameIds);
       std::vector<chat::Member> chatMembers;for(const auto&entry:result.roster.slots)if(entry){uint8_t team=0;
        if(result.combat_state)for(const auto&p:result.combat_state->players)if(p&&p->identity.character==entry->character)team=p->team;
        if(result.preparation)for(const auto&p:result.preparation->players)if(p&&p->id.character==entry->character)team=p->team;
        chatMembers.push_back({entry->character,requests.nameDirectory->display(entry->character,entry->name),team});}
       requests.chatSession->roster(std::move(chatMembers),result.stage==host::Stage::joined);
{uint32_t clanId=0;for(const auto&entry:result.roster.slots)if(entry&&entry->character==id){clanId=entry->clanId;break;}requests.clanEmblem->want(clanId);}RoomReply r;r.event=RoomEvent::join;r.requested_room=action->id;r.status=result.stage==host::Stage::joined?RoomStatus::ready:RoomStatus::connecting;r.join_status=room_host_status(result.stage);r.host_roster=std::move(result.roster);r.host_match=std::move(result.match);r.host_placements=std::move(result.placements);r.host_scene=std::move(result.scene);r.scene_status=result.scene_status;r.combat_offer=result.combat_offer;r.combat_state=std::move(result.combat_state);r.combat_sop=result.combat_sop;r.combat_status=result.combat_status;r.combat_events=std::move(result.combat_events);r.preparation=std::move(result.preparation);publish(std::move(r));};
      std::vector<stage::ObjectRegistry> registries;for(auto map:{1,4,20,21}){auto registryPath=path.parent_path()/"stage"/(std::string(stage::name(uint8_t(map)))+".objects.cfg");if(std::filesystem::exists(registryPath))registries.push_back(stage::load_combat_object_registry(registryPath));}
       return host::run(local,admission,profile,cancel,requests.cancel_join,progress,pumpSkills,std::nullopt,[&]{return requests.take_combat();},[&]{return requests.take_command();},requests.radioSession,requests.inventorySession,std::move(registries));
     }):HostConnect{});
     std::vector<uint32_t> nameIds;if(reply.detail)for(const auto&entry:reply.detail->roster)nameIds.push_back(entry.id);requests.nameDirectory->want(nameIds);
     bool lost=reply.status==RoomStatus::protocol_error&&reply.join_status!=RoomJoinStatus::invalid_input;publish(std::move(reply));if(lost||*requests.uncertain)return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
   }
  }
 }catch(const Failure&f){if(!cancel)publish({f.status==CharacterStatus::server_error?RoomStatus::rejected:RoomStatus::network_error,{},f.code});}
 catch(...){if(!cancel)publish({RoomStatus::protocol_error,{},0});}
}
void run_dedicated_lobby(const std::filesystem::path&path,const AuthReply&auth,const CharacterEntry&character,const GameLobbyEntry&lobby,const host::Settings&settings,const std::atomic_bool&cancel,const DedicatedPublish&publish,uintptr_t udpSocket){
 DedicatedReply progress;
 try{
  auto rows=load_lobby_membership(path.parent_path()/L"lobbies.cfg");
  if(!character.id||udpSocket==~uintptr_t(0)||lobby.port<5733||lobby.port>5739||lobby.subtype!=1||std::none_of(rows.begin(),rows.end(),[&](const auto&r){return r.id==lobby.id&&r.port==lobby.port&&r.subtype==1;}))throw std::runtime_error("dedicated lobby identity");
  host::settings_payload(settings); // Validate all settings before connecting.
  auto keys=NetworkKeys::load(path);Wsa wsa;Connection game(keys,cancel);publish(progress);game.connect_to(lobby.port);
  auto session=game_session_payload(keys,auth,character.id);struct Wipe{std::vector<uint8_t>&v;~Wipe(){SecureZeroMemory(v.data(),v.size());}}wipe{session};
  game.send_packet(0x3003,session);SecureZeroMemory(session.data(),session.size());auto packet=game.read();if(packet.command!=0x3004)throw std::runtime_error("dedicated session reply");result(packet.payload);
  progress.status=DedicatedStatus::mapping;publish(progress);auto stun=check_stun(udpSocket,cancel);
  if(cancel){progress.status=DedicatedStatus::cancelled;publish(progress);return;}
  if(stun.status!=StunStatus::success){progress.status=DedicatedStatus::network_error;progress.error=stun.error;publish(progress);return;}
  sockaddr_in bound{};int size=sizeof(bound);if(getsockname(SOCKET(udpSocket),reinterpret_cast<sockaddr*>(&bound),&size))throw Failure{CharacterStatus::network_error,unsigned(WSAGetLastError())};
  host::Endpoint privateEndpoint{game.local_address(),ntohs(bound.sin_port)},publicEndpoint{stun.address,stun.mapped_port};
  progress.local_port=privateEndpoint.port;progress.public_port=publicEndpoint.port;
  if(!host::valid_endpoint(privateEndpoint)||!host::valid_endpoint(publicEndpoint))throw std::runtime_error("dedicated mapping");
  std::vector<uint8_t>connection(24);put(connection,0,privateEndpoint.port,2);char ip[16]{};IN_ADDR address{};std::copy(privateEndpoint.address.begin(),privateEndpoint.address.end(),reinterpret_cast<uint8_t*>(&address));if(!InetNtopA(AF_INET,&address,ip,sizeof(ip)))throw std::runtime_error("dedicated IP");std::copy_n(reinterpret_cast<const uint8_t*>(ip),strlen(ip),connection.begin()+2);put(connection,18,publicEndpoint.port,2);network_block(connection,keys.packet,true);game.reset_deadline();game.send_packet(0x4700,connection);packet=game.read();if(packet.command!=0x4701)throw std::runtime_error("dedicated endpoint reply");result(packet.payload);
  host::Lifecycle room;
  auto exchange=[&](uint16_t command,std::span<const uint8_t>plain){game.reset_deadline(command==0x4380);auto wire=host::host_room_wire_payload(keys,command,plain);Wipe wipe{wire};game.send_packet(command,wire);return game.read();};
  auto close=[&]{if(room.room_may_exist()){auto reply=room.close(exchange);progress.room_may_exist=room.room_may_exist();if(reply.status!=host::RoomControlStatus::success){progress.status=DedicatedStatus::outcome_unknown;progress.error=reply.error;}}};
  try{
   progress.status=DedicatedStatus::creating;publish(progress);auto made=room.create(settings,exchange,cancel);progress.room=made.room;progress.error=made.error;progress.room_may_exist=made.room_may_exist;
   if(made.status!=host::RoomControlStatus::success){progress.status=made.status==host::RoomControlStatus::outcome_unknown?DedicatedStatus::outcome_unknown:made.status==host::RoomControlStatus::cancelled?DedicatedStatus::cancelled:DedicatedStatus::rejected;close();publish(progress);return;}
   uint32_t seed=0;if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&seed),4,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("dedicated RNG");
   host::Hello hello{character.id,seed,2,1,{publicEndpoint,privateEndpoint}};
   auto text=[](std::wstring_view s){int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);if(n<=0||n>23)throw std::runtime_error("dedicated PC name");std::string out(n,0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),int(s.size()),out.data(),n,nullptr,nullptr);return out;};
   host::Player own{0,0x100,character.id,text(character.legacy_name.empty()?character.name:character.legacy_name),{}};
   auto catalog=std::make_shared<weapons::Catalog>();std::string catalogError;if(!catalog->load(path.parent_path()/"weapon_catalog.tsv",catalogError))catalog.reset();
   combat::Cycle::Options cycleOptions;cycleOptions.stageRoot=path.parent_path()/"stage";cycleOptions.rotations=settings.rotations;cycleOptions.capacity=settings.capacity;cycleOptions.combat.friendlyFire=settings.friendly_fire;
   cycleOptions.round={uint64_t(settings.briefing_minutes)*60000,1,settings.rotations.front().flags==2,settings.auto_assign,settings.weapon_restrictions};cycleOptions.round.roundDurationMs=host::round_duration_ms(settings,settings.rotations.front().rule);
   std::string itemError;auto settingsPath=path.parent_path()/"item_settings.cfg";if(std::filesystem::exists(settingsPath)&&!cycleOptions.items.load(settingsPath,itemError))throw std::runtime_error("Invalid local item settings");if(!cycleOptions.itemFacts.load(path.parent_path()/"item_drop_policy.json",itemError))throw std::runtime_error("Item catalog unavailable");
   combat::Cycle combatCycle(cycleOptions,catalog,[]{combat::spawn::Random random;if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(random.words.data()),ULONG(sizeof(random.words)),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("spawn RNG");return random;});
   items::HostSession inventoryHost;
   radio::Service radioService({1000});radioService.reset(combatCycle.epoch());
   auto skillCatalog=std::make_shared<skills::Catalog>();std::string skillError;
   std::shared_ptr<host_skills::Receiver> skillReceiver;
   if(skillCatalog->load(path.parent_path()/"skill_catalog.tsv",skillError)){
    uint64_t nonceSeed=0;do{if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&nonceSeed),8,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("host skills RNG");}while(!nonceSeed||nonceSeed==UINT64_MAX);
    skillReceiver=std::make_shared<host_skills::Receiver>(skillCatalog,nonceSeed);skillReceiver->begin(made.room,character.id,combatCycle.epoch());
   }
   // The callback owns its receiver through room-close cleanup as well. This
   // authenticated TCP path is separate from GWSK UI and all UDP peer input.
   game.nativeHostSkills=[skillReceiver](const LobbyPacket&p){if(skillReceiver)skillReceiver->receive(p.payload,DedicatedTime::read().ms);};
   auto installSkills=[&]{if(skillReceiver){
     for(auto&v:skillReceiver->drain()){const bool installed=combatCycle.service().install_loadout(v);std::osyncstream(std::cout)<<"{\"host_skills\":\""<<(installed?"verified_pre_spawn":"late_or_stale_ignored")<<"\",\"pc\":"<<v.scope().character<<",\"revision\":"<<v.profile().revision<<"}"<<std::endl;}
   }};
   bool roundStarted=false;DedicatedPollWait pollWait;
   struct Peer {sockaddr_in address;host::DedicatedPeer wire;std::optional<host::Player> player;uint32_t clan=0;bool synced=false;uint64_t syncTicket=0;bool snapshotReceived=false;std::optional<uint8_t> serverTeam;};
   std::map<uint32_t,Peer>peers;uint16_t instance=0x101;uint64_t lastNew=0,heartbeat=DedicatedTime::read().ms+5000;
   u_long nonblocking=1;if(ioctlsocket(SOCKET(udpSocket),FIONBIO,&nonblocking))throw Failure{CharacterStatus::network_error,unsigned(WSAGetLastError())};
   auto send=[&](Peer&p,std::span<const uint8_t>b){int n=sendto(SOCKET(udpSocket),reinterpret_cast<const char*>(b.data()),int(b.size()),0,reinterpret_cast<const sockaddr*>(&p.address),sizeof(p.address));if(n==SOCKET_ERROR&&WSAGetLastError()!=WSAEWOULDBLOCK)throw Failure{CharacterStatus::network_error,unsigned(WSAGetLastError())};};
   auto notify=[&]{progress.combat_status=combatCycle.service().current_status();progress.players.clear();progress.synchronized_players=0;for(auto&[id,p]:peers)if(p.player){progress.players.push_back(*p.player);if(p.snapshotReceived)++progress.synchronized_players;}progress.phase=roundStarted?host::RoundPhase::preparing:host::RoundPhase::waiting;progress.briefing_remaining_ms.reset();
    for(auto&[id,p]:peers)if(p.player&&p.synced){if(auto state=combatCycle.service().preparation({p.player->slot,p.player->instance,p.player->character},DedicatedTime::read().ms);state&&state->countdown)progress.briefing_remaining_ms=state->remainingMs;break;}
    publish(progress);};
   uint64_t nextBriefingPublish=0;
   progress.status=DedicatedStatus::hosting;notify();
   while(!cancel){
    auto now=DedicatedTime::read().ms;if(!game.alive())throw Failure{CharacterStatus::network_error,WSAECONNRESET};
    game.poll_skills();game.keep_alive();
    if(skillReceiver){
     const auto previous=skillReceiver->availability();skillReceiver->tick(DedicatedTime::read().ms);
     if(previous!=host_skills::Availability::unsupported&&skillReceiver->availability()==host_skills::Availability::unsupported)std::osyncstream(std::cout)<<"{\"host_skills\":\"session_unavailable_baseline\"}"<<std::endl;
     installSkills();
     if(auto request=skillReceiver->take(DedicatedTime::read().ms)){game.reset_deadline();game.send_packet(host_skills::request_opcode,host_skills::encode_request(*request));}
    }
    if(now>=heartbeat){auto r=room.heartbeat(exchange);if(r.status!=host::RoomControlStatus::success)throw std::runtime_error("dedicated lease reply");heartbeat=DedicatedTime::read().ms+5000;now=DedicatedTime::read().ms;}
    std::array<uint8_t,2049>bytes{};sockaddr_in from{};int len=sizeof(from);int n=recvfrom(SOCKET(udpSocket),reinterpret_cast<char*>(bytes.data()),int(bytes.size()),0,reinterpret_cast<sockaddr*>(&from),&len);
    if(n>0&&from.sin_family==AF_INET){auto raw=std::span(bytes).first(size_t(n));auto peer=std::find_if(peers.begin(),peers.end(),[&](auto&p){return p.second.address.sin_addr.s_addr==from.sin_addr.s_addr&&p.second.address.sin_port==from.sin_port;});
     if(peer==peers.end()&&peers.size()<size_t(settings.capacity-1)&&now-lastNew>=250){
      try{auto p=host::decode(raw);if(p.messages.size()!=1)throw host::Invalid(host::Error::message);auto&m=p.messages[0];if(m.channel||!m.reliable||m.ack||m.serial||m.payload.size()<16)throw host::Invalid(host::Error::message);uint32_t id=uint32_t(m.payload[0])|(uint32_t(m.payload[1])<<8)|(uint32_t(m.payload[2])<<16)|(uint32_t(m.payload[3])<<24);auto remote=host::decode_hello(m.payload,id);if(id==character.id||peers.contains(id))throw host::Invalid(host::Error::identity);lastNew=now;peer=peers.emplace(id,Peer{from,host::DedicatedPeer(hello,remote,now)}).first;}catch(const host::Invalid&){}
     }
     if(peer!=peers.end())peer->second.wire.receive(raw,now);
    }else if(n==SOCKET_ERROR){auto error=WSAGetLastError();if(error!=WSAEWOULDBLOCK&&error!=WSAEMSGSIZE&&error!=WSAECONNRESET)throw Failure{CharacterStatus::network_error,unsigned(error)};}
    for(auto it=peers.begin();it!=peers.end();){auto&peer=it->second;bool remove=peer.wire.closed();
     for(auto&event:peer.wire.events()){
      if(event[0]==1){remove=true;break;}
      if(event[0]==2&&!peer.player){auto names=host::profile_names(event);auto admitted=room.player_connected(it->first,exchange);if(admitted.status!=host::RoomControlStatus::success){if(admitted.status!=host::RoomControlStatus::rejected)throw std::runtime_error("uncertain dedicated admission");remove=true;break;}
       uint8_t slot=1;while(slot<17&&std::any_of(peers.begin(),peers.end(),[&](const auto&p){return p.second.player&&p.second.player->slot==slot;}))++slot;
       peer.player=host::Player{slot,instance++,it->first,names.name,names.clan};peer.clan=names.clanId;
       peer.player->appearance=names.appearance;
       peer.wire.queue(host::roster_record(own,hello),now);
       for(auto&[id,p]:peers)if(p.player)peer.wire.queue(host::roster_record(*p.player,p.wire.hello(),p.clan),now);
       peer.wire.queue({7,0,0,0,0,0,3},now);
       std::vector<host::Player> appearances;for(auto&[id,p]:peers)if(p.player&&p.player->appearance)appearances.push_back(*p.player);
       peer.wire.queue(host::appearance_record(appearances),now);
       for(auto&[id,p]:peers)if(id!=it->first&&p.player)p.wire.queue(host::roster_record(*peer.player,peer.wire.hello(),peer.clan),now);
       for(auto&[id,p]:peers)if(id!=it->first&&p.player)p.wire.queue(host::appearance_record(std::span(&*peer.player,1)),now);
       notify();
      }else if(event[0]==10&&event.size()==1&&peer.player&&!peer.synced){if(combatCycle.objects()&&combatCycle.objects()->request())peer.wire.objects(combatCycle.objects()->registry(),*combatCycle.objects()->request(),peer.player->slot);peer.wire.queue(host::room_snapshot(settings.rotations,combatCycle.request().generation,0,combatCycle.request().round),now);peer.syncTicket=peer.wire.queue(host::phase_update(roundStarted?2:0),now);peer.synced=true;if(!combatCycle.admit({peer.player->slot,peer.player->instance,peer.player->character},now))throw std::runtime_error("Combat admission identity");if(skillReceiver)skillReceiver->want(peer.player->slot,peer.player->instance,peer.player->character);}
      else if(combat::wire::recognized(event)&&peer.player&&peer.synced){now=DedicatedTime::read().ms;installSkills();combatCycle.service().receive({peer.player->slot,peer.player->instance,peer.player->character},event,now);}
      else if(items::wire::recognized(event)&&peer.player&&peer.synced){inventoryHost.receive(combatCycle.service().authority(),{peer.player->slot,peer.player->instance,peer.player->character},event,DedicatedTime::read().ms);}
      else if(radio::recognized(event)&&peer.player&&peer.synced){
       std::vector<radio::Identity> admitted;
       for(const auto&[id,p]:peers)if(p.player&&p.synced&&!p.wire.closed())admitted.push_back({p.player->slot,p.player->instance,p.player->character});
       const auto context=radio::host_context(combatCycle,admitted,now);
       for(const auto& delivery:radioService.receive({peer.player->slot,peer.player->instance,peer.player->character},event,context,now,radio::same_team)){
        auto recipient=peers.find(delivery.recipient.character);
        if(recipient!=peers.end()&&recipient->second.player&&recipient->second.synced&&
           recipient->second.player->slot==delivery.recipient.slot&&recipient->second.player->instance==delivery.recipient.instance)
         recipient->second.wire.optional_queue(delivery.body,now);
       }
      }
     }
     for(auto slot:peer.wire.snapshot_requests())if(combatCycle.objects()&&peer.player&&slot==peer.player->slot){
      if(auto image=combatCycle.objects()->snapshot(slot))peer.wire.queue_object(std::move(*image),now);
     }
     if(!peer.snapshotReceived&&peer.wire.delivery_complete(peer.syncTicket)){peer.snapshotReceived=true;notify();}
     if(peer.player&&peer.synced){auto state=combatCycle.service().preparation({peer.player->slot,peer.player->instance,peer.player->character},now);
      if(state){const auto&self=state->players[peer.player->slot];if(self&&(state->freeForAll||self->team)){auto team=state->freeForAll?uint8_t(0):uint8_t(self->team-1);if(peer.serverTeam!=team){
       auto response=room.player_team(peer.player->character,team,exchange);if(response.status!=host::RoomControlStatus::success)throw std::runtime_error("dedicated team reply");peer.serverTeam=team;
      }}}
     }
     for(auto&b:peer.wire.poll(DedicatedTime::read().ms))send(peer,b);
     if(peer.wire.closed()&&peer.wire.close_reason()==host::PeerCloseReason::mandatory_overflow){std::osyncstream(std::cout)<<"{\"host_peer_close\":\"mandatory_backlog_overflow\"}\n";}
     remove|=peer.wire.closed();if(remove){if(peer.player){auto r=room.player_disconnected(it->first,exchange);if(r.status!=host::RoomControlStatus::success)throw std::runtime_error("dedicated removal reply");combatCycle.remove({peer.player->slot,peer.player->instance,peer.player->character});radioService.remove({peer.player->slot,peer.player->instance,peer.player->character});inventoryHost.remove({peer.player->slot,peer.player->instance,peer.player->character});if(skillReceiver)skillReceiver->forget(peer.player->slot,peer.player->instance,peer.player->character);for(auto&[id,p]:peers)if(id!=it->first&&p.player)p.wire.queue(host::roster_remove(peer.player->instance),DedicatedTime::read().ms);}it=peers.erase(it);notify();}else ++it;
    }
    const auto combatNow=DedicatedTime::read();now=combatNow.ms;
    installSkills();combatCycle.poll(now,combatNow.subMsNs);inventoryHost.poll(combatCycle.service().authority(),now);
    for(auto&[character,peer]:peers)if(peer.player&&peer.synced){auto id=combat::Identity{peer.player->slot,peer.player->instance,peer.player->character};if(auto body=inventoryHost.front(id);body&&peer.wire.optional_queue(*body,now))inventoryHost.pop(id);}
    // DedicatedPeer owns the single bounded mandatory FIFO, including the
    // metadata below. Reliable ACK gaps must not drop or reorder combat frames.
    for(auto&delivery:combatCycle.service().deliveries())if(auto peer=peers.find(delivery.recipient.character);peer!=peers.end()&&peer->second.player&&peer->second.player->instance==delivery.recipient.instance)peer->second.wire.queue(std::move(delivery.payload),now);
    // The native coordinator waits for actual applied scene readiness and reversible
    // READY. Phase 2 retains its reviewed original preparation meaning.
    if(combatCycle.service().take_round_start()){
     auto started=room.round_started(0,exchange);if(started.status!=host::RoomControlStatus::success)throw std::runtime_error("dedicated round start reply");
     roundStarted=true;for(auto&[id,p]:peers)if(p.player&&p.synced)p.wire.queue(host::phase_update(2),now);notify();
    }
    // Native single-rotation continuation. Existing UDP peers/roster remain;
    // only the round epoch, scene generation, spawn grants and READY reset.
    if(combatCycle.advance(now)){
     radioService.reset(combatCycle.epoch());inventoryHost.clear();
     if(skillReceiver)skillReceiver->begin(made.room,character.id,combatCycle.epoch());
     roundStarted=false;
     for(auto&[id,p]:peers)if(p.player&&p.synced){
      if(auto objects=combatCycle.objects();objects&&objects->request())p.wire.objects(objects->registry(),*objects->request(),p.player->slot);
      p.wire.queue(host::next_round_snapshot(combatCycle.request().generation,combatCycle.request().round),now);
      if(skillReceiver)skillReceiver->want(p.player->slot,p.player->instance,p.player->character);
      p.syncTicket=p.wire.queue(host::phase_update(0),now);p.snapshotReceived=false;
     }
     // New offers are emitted on the next poll, after the global generation.
     notify();
    }
    if(now>=nextBriefingPublish){nextBriefingPublish=now+1000;notify();}
    pollWait.wait(combatCycle.service().authority().active());
   }
   for(auto&[id,peer]:peers)if(peer.player){peer.wire.queue(host::roster_remove(own.instance),DedicatedTime::read().ms);for(auto&b:peer.wire.poll(DedicatedTime::read().ms))send(peer,b);}
   progress.status=DedicatedStatus::closed;close();publish(progress);
  }catch(...){close();throw;}
 }catch(const Failure&f){progress.status=progress.room_may_exist?DedicatedStatus::outcome_unknown:cancel?DedicatedStatus::cancelled:DedicatedStatus::network_error;progress.error=f.code;publish(progress);}
 catch(...){progress.status=progress.room_may_exist?DedicatedStatus::outcome_unknown:DedicatedStatus::protocol_error;publish(progress);}
}
CharacterCreateRequest character_create_request(std::wstring name,const std::array<uint8_t,28>&a,int pitch,bool unicode){
 if(a[0]>1||a[7]>7||pitch< -7||pitch>7||!CharacterCreation::name_error(name,unicode).empty())throw std::invalid_argument("Invalid creation draft");
 CharacterCreateRequest r;r.name=std::move(name);r.unicode_name=unicode;std::copy_n(a.begin(),27,r.wire_appearance.begin());auto&w=r.wire_appearance;
 w[7]=uint8_t(a[7]+(a[0]?16:7));w[8]=uint8_t(pitch+15);for(unsigned i=9;i<=12;++i)w[i]=0;
 // GWC adds synthetic skin palettes for bare hands; original ID46 has only
 // wire palette 0 (both gender tables). Skin matching belongs to rendering.
 if(w[15]==46)w[22]=0;
 // Native optional value 0 denotes the original unequipped table entries.
 for(auto [field,sentinel]:{std::pair{13u,28u},{14u,68u},{16u,86u},{18u,102u},{19u,102u}})if(!w[field]){w[field]=uint8_t(sentinel);w[field+7]=0;}
 return r;
}
std::vector<uint8_t> character_create_payload(const CharacterCreateRequest&r){
 if(!CharacterCreation::name_error(r.name).empty())throw std::runtime_error("invalid character name");
 if(r.wire_appearance[0]>1||std::any_of(r.wire_appearance.begin()+9,r.wire_appearance.begin()+13,[](uint8_t x){return x!=0;}))throw std::runtime_error("invalid appearance reserved fields");
 auto n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,r.name.data(),int(r.name.size()),nullptr,0,nullptr,nullptr);
 if(n<1||n>16)throw std::runtime_error("character name extent");
 std::vector<uint8_t>b(43);if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,r.name.data(),int(r.name.size()),reinterpret_cast<char*>(b.data()),16,nullptr,nullptr)!=n)throw std::runtime_error("character name conversion");
 std::copy(r.wire_appearance.begin(),r.wire_appearance.end(),b.begin()+16);return b;
}
CharacterCreateReply exchange_character_create(const CharacterCreateRequest&r,const CharacterExchange&exchange,const std::atomic_bool&cancel,const std::function<bool()>&nativeReady){
 CharacterCreateReply reply;
 try{
  if(!CharacterCreation::name_error(r.name,r.unicode_name).empty()||r.wire_appearance[0]>1||std::any_of(r.wire_appearance.begin()+9,r.wire_appearance.begin()+13,[](uint8_t x){return x!=0;}))throw std::runtime_error("invalid creation request");
  const auto nonce=r.unicode_name?name_nonce():0;
  auto payload=r.unicode_name?names::create(nonce,name_utf8(r.name),r.wire_appearance):character_create_payload(r);
  if(cancel){reply.status=CharacterCreateStatus::cancelled;return reply;}
  auto packet=exchange(0x3048,{});
  if(packet.command!=0x3049)throw std::runtime_error("create preflight reply command");
  if(packet.payload.size()==4)result(packet.payload);
  auto current=parse_characters(packet.payload);
  if(r.unicode_name&&(!nativeReady||!nativeReady())){reply.status=CharacterCreateStatus::rejected;return reply;}
  const auto capacity=character_slot_capacity(current.slots);
  if(!capacity||current.entries.size()>capacity)throw std::runtime_error("create slot capacity");
  if(current.entries.size()==capacity){reply.status=CharacterCreateStatus::full;return reply;}
  for(const auto&e:current.entries)if(e.name==r.name){reply.status=CharacterCreateStatus::rejected;return reply;}
  if(cancel){reply.status=CharacterCreateStatus::cancelled;return reply;}
  // Mark before exchange: a partial send or a lost reply can still create a PC.
  reply.request_may_have_been_sent=true;packet=exchange(r.unicode_name?names::create_request_opcode:0x3101,payload);
  if(r.unicode_name){
   if(packet.command!=names::create_reply_opcode||packet.payload.size()!=32)throw std::runtime_error("native create reply command/extent");
   auto created=names::parse(packet.payload);if(created.op!=names::Operation::create||created.nonce!=nonce)throw std::runtime_error("native create reply identity");
   reply.error=created.result;if(created.status){reply.status=created.status==3?CharacterCreateStatus::outcome_unknown:CharacterCreateStatus::rejected;return reply;}
   reply.created_id=created.createdId;reply.status=CharacterCreateStatus::success;return reply;
  }
  if(packet.command!=0x3102||(packet.payload.size()!=4&&packet.payload.size()!=8))throw std::runtime_error("create reply extent/command");
  reply.error=be(packet.payload,0);
  if(reply.error){if(packet.payload.size()!=4)throw std::runtime_error("create rejection extent");reply.status=CharacterCreateStatus::rejected;return reply;}
  if(packet.payload.size()!=8||!(reply.created_id=be(packet.payload,4)))throw std::runtime_error("create ID missing");
  reply.status=CharacterCreateStatus::success;
 }catch(const Failure&f){reply.error=f.code;reply.status=reply.request_may_have_been_sent?CharacterCreateStatus::outcome_unknown:f.status==CharacterStatus::cancelled?CharacterCreateStatus::cancelled:f.status==CharacterStatus::server_error?CharacterCreateStatus::rejected:CharacterCreateStatus::network_error;}
 catch(...){reply.status=reply.request_may_have_been_sent?CharacterCreateStatus::outcome_unknown:CharacterCreateStatus::protocol_error;}
 return reply;
}
CharacterCreateReply create_character(const std::filesystem::path&path,const AuthReply&auth,const CharacterCreateRequest&r,const std::atomic_bool&cancel){
 CharacterCreateReply reply;
 try{
  if(!CharacterCreation::name_error(r.name,r.unicode_name).empty())throw std::runtime_error("invalid creation name");if(cancel){reply.status=CharacterCreateStatus::cancelled;return reply;}
  auto keys=NetworkKeys::load(path);Wsa wsa;uint16_t port;
  {Connection gate(keys,cancel);port=gateway(gate);}
  Connection account(keys,cancel);account.connect_to(port);
  auto payload=session_payload(keys,auth);struct Wipe{std::vector<uint8_t>&v;~Wipe(){SecureZeroMemory(v.data(),v.size());}}wipe{payload};
  account.send_packet(0x3003,payload);SecureZeroMemory(payload.data(),payload.size());auto p=account.read();
  if(p.command!=0x3004)throw std::runtime_error("create session reply");result(p.payload);
  AccountNames names(account);return exchange_character_create(r,[&](uint16_t cmd,std::span<const uint8_t>b){account.send_packet(cmd,b);return account.read();},cancel,[&]{return names.native_ready();});
 }catch(const Failure&f){reply.error=f.code;reply.status=f.status==CharacterStatus::cancelled?CharacterCreateStatus::cancelled:f.status==CharacterStatus::server_error?CharacterCreateStatus::rejected:CharacterCreateStatus::network_error;}
 catch(...){reply.status=CharacterCreateStatus::protocol_error;}
 return reply;
}
}
