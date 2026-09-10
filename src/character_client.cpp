#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include "character_client.h"
#include "character_creation.h"
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <set>
#include <cstring>
namespace mgo2win {
namespace {
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
 SOCKET s_=INVALID_SOCKET;const NetworkKeys& keys_;const std::atomic_bool& cancel_;ULONGLONG deadline_;uint32_t tx_=1,rx_=1;
 void wait(bool write){
  for(;;){if(cancel_)throw Failure{CharacterStatus::cancelled,0};if(GetTickCount64()>=deadline_)throw Failure{CharacterStatus::network_error,WSAETIMEDOUT};fd_set f,e;FD_ZERO(&f);FD_ZERO(&e);FD_SET(s_,&f);FD_SET(s_,&e);timeval t{0,50000};int n=select(0,write?nullptr:&f,write?&f:nullptr,&e,&t);if(n<0)throw Failure{CharacterStatus::network_error,unsigned(WSAGetLastError())};if(n>0){int err=0,len=sizeof(err);if(getsockopt(s_,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&err),&len)||err)throw Failure{CharacterStatus::network_error,unsigned(err?err:WSAGetLastError())};return;}}
 }
 void receive(std::span<uint8_t>b){size_t at=0;while(at<b.size()){wait(false);int n=recv(s_,reinterpret_cast<char*>(b.data()+at),int(b.size()-at),0);if(n<0&&WSAGetLastError()==WSAEWOULDBLOCK)continue;if(n<=0)throw Failure{CharacterStatus::network_error,unsigned(n?WSAGetLastError():WSAECONNRESET)};at+=n;}}
public:
 Connection(const NetworkKeys&k,const std::atomic_bool&c):keys_(k),cancel_(c),deadline_(GetTickCount64()+8000){}
 ~Connection(){if(s_!=INVALID_SOCKET)closesocket(s_);}
 void connect_to(uint16_t port){if(port!=5731&&port!=5732)throw std::runtime_error("unreviewed lobby port");s_=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(s_==INVALID_SOCKET)throw Failure{CharacterStatus::network_error,unsigned(WSAGetLastError())};u_long nonblocking=1;if(ioctlsocket(s_,FIONBIO,&nonblocking))throw Failure{CharacterStatus::network_error,unsigned(WSAGetLastError())};sockaddr_in peer{};peer.sin_family=AF_INET;peer.sin_port=htons(port);InetPtonW(AF_INET,L"49.212.132.180",&peer.sin_addr);if(connect(s_,reinterpret_cast<sockaddr*>(&peer),sizeof(peer))&&WSAGetLastError()!=WSAEWOULDBLOCK)throw Failure{CharacterStatus::network_error,unsigned(WSAGetLastError())};wait(true);}
 void send_packet(uint16_t cmd,std::span<const uint8_t> payload={}){auto b=encode_lobby(keys_,cmd,tx_++,payload);struct Wipe{std::vector<uint8_t>&v;~Wipe(){SecureZeroMemory(v.data(),v.size());}}wipe{b};size_t at=0;while(at<b.size()){wait(true);int n=send(s_,reinterpret_cast<char*>(b.data()+at),int(b.size()-at),0);if(n<0&&WSAGetLastError()==WSAEWOULDBLOCK)continue;if(n<=0)throw Failure{CharacterStatus::network_error,unsigned(n?WSAGetLastError():WSAECONNRESET)};at+=n;}}
 LobbyPacket read(){std::vector<uint8_t>b(24);receive(b);unsigned n=((b[2]^keys_.wire[2])<<8)|(b[3]^keys_.wire[3]);if(n>1023)throw std::runtime_error("lobby payload too large");b.resize(24+n);receive(std::span(b).subspan(24));return decode_lobby(keys_,b,rx_++);}
};
uint16_t gateway(Connection&c){c.connect_to(5731);c.send_packet(0x2005);auto p=c.read();if(p.command!=0x2002)throw std::runtime_error("gate start command");result(p.payload);uint16_t port=0;for(unsigned i=0;i<16;++i){p=c.read();if(p.command==0x2004){result(p.payload);if(!port)throw std::runtime_error("no reviewed account endpoint");return port;}if(p.command!=0x2003)throw std::runtime_error("gate list command");auto found=account_endpoint(p.payload);if(found){if(port)throw std::runtime_error("ambiguous account endpoint");port=found;}}throw std::runtime_error("too many gate packets");}
CharacterReply run(const std::filesystem::path&path,const AuthReply*auth,const std::atomic_bool&cancel){CharacterReply r;try{
 if(cancel)throw Failure{CharacterStatus::cancelled,0};auto keys=NetworkKeys::load(path);Wsa wsa;r.stage=CharacterStage::gate_connect;
 {Connection gate(keys,cancel);r.stage=CharacterStage::gate_list;r.account_port=gateway(gate);}
 if(auth){r.stage=CharacterStage::account_connect;Connection account(keys,cancel);account.connect_to(r.account_port);r.stage=CharacterStage::session;
  auto payload=session_payload(keys,*auth);struct Wipe{std::vector<uint8_t>&v;~Wipe(){SecureZeroMemory(v.data(),v.size());}}wipe{payload};account.send_packet(0x3003,payload);SecureZeroMemory(payload.data(),payload.size());auto p=account.read();if(p.command!=0x3004)throw std::runtime_error("session reply command");result(p.payload);
  r.stage=CharacterStage::list;account.send_packet(0x3048);p=account.read();if(p.command!=0x3049)throw std::runtime_error("character reply command");if(p.payload.size()==4)result(p.payload);r.list=parse_characters(p.payload);
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
CharacterCreateRequest character_create_request(std::wstring name,const std::array<uint8_t,28>&a,int pitch){
 if(a[0]>1||a[7]>7||pitch< -7||pitch>7||!CharacterCreation::name_error(name).empty())throw std::invalid_argument("Invalid creation draft");
 CharacterCreateRequest r;r.name=std::move(name);std::copy_n(a.begin(),27,r.wire_appearance.begin());auto&w=r.wire_appearance;
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
CharacterCreateReply exchange_character_create(const CharacterCreateRequest&r,const CharacterExchange&exchange,const std::atomic_bool&cancel){
 CharacterCreateReply reply;
 try{
  auto payload=character_create_payload(r);
  if(cancel){reply.status=CharacterCreateStatus::cancelled;return reply;}
  auto packet=exchange(0x3048,{});
  if(packet.command!=0x3049)throw std::runtime_error("create preflight reply command");
  if(packet.payload.size()==4)result(packet.payload);
  auto current=parse_characters(packet.payload);
  const auto capacity=character_slot_capacity(current.slots);
  if(!capacity||current.entries.size()>capacity)throw std::runtime_error("create slot capacity");
  if(current.entries.size()==capacity){reply.status=CharacterCreateStatus::full;return reply;}
  for(const auto&e:current.entries)if(e.name==r.name){reply.status=CharacterCreateStatus::rejected;return reply;}
  if(cancel){reply.status=CharacterCreateStatus::cancelled;return reply;}
  // Mark before exchange: a partial send or a lost reply can still create a PC.
  reply.request_may_have_been_sent=true;packet=exchange(0x3101,payload);
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
  character_create_payload(r);if(cancel){reply.status=CharacterCreateStatus::cancelled;return reply;}
  auto keys=NetworkKeys::load(path);Wsa wsa;uint16_t port;
  {Connection gate(keys,cancel);port=gateway(gate);}
  Connection account(keys,cancel);account.connect_to(port);
  auto payload=session_payload(keys,auth);struct Wipe{std::vector<uint8_t>&v;~Wipe(){SecureZeroMemory(v.data(),v.size());}}wipe{payload};
  account.send_packet(0x3003,payload);SecureZeroMemory(payload.data(),payload.size());auto p=account.read();
  if(p.command!=0x3004)throw std::runtime_error("create session reply");result(p.payload);
  return exchange_character_create(r,[&](uint16_t cmd,std::span<const uint8_t>b){account.send_packet(cmd,b);return account.read();},cancel);
 }catch(const Failure&f){reply.error=f.code;reply.status=f.status==CharacterStatus::cancelled?CharacterCreateStatus::cancelled:f.status==CharacterStatus::server_error?CharacterCreateStatus::rejected:CharacterCreateStatus::network_error;}
 catch(...){reply.status=CharacterCreateStatus::protocol_error;}
 return reply;
}
}
