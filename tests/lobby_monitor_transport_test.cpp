// Compile the real internal Connection in this translation unit to exercise
// socket framing/waits/watchdog without exposing a production endpoint override.
#include "../src/character_client.cpp"
#include <future>
using namespace mgo2win;
namespace {
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
struct Socket {SOCKET value=INVALID_SOCKET;~Socket(){if(value!=INVALID_SOCKET)closesocket(value);}};
struct Sockets {
 SOCKET client=INVALID_SOCKET;Socket server;
 Sockets(){Socket listener;listener.value=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);check(listener.value!=INVALID_SOCKET,"local listener");sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);check(!bind(listener.value,reinterpret_cast<sockaddr*>(&address),sizeof(address))&&!listen(listener.value,1),"loopback ephemeral bind");int length=sizeof(address);check(!getsockname(listener.value,reinterpret_cast<sockaddr*>(&address),&length),"local endpoint");client=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);check(client!=INVALID_SOCKET&&!connect(client,reinterpret_cast<sockaddr*>(&address),sizeof(address)),"loopback connect");server.value=accept(listener.value,nullptr,nullptr);check(server.value!=INVALID_SOCKET,"local accept");DWORD timeout=3000;setsockopt(server.value,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));}
 ~Sockets(){if(client!=INVALID_SOCKET)closesocket(client);}
 SOCKET take(){return std::exchange(client,INVALID_SOCKET);}
};
void send_all(SOCKET socket,std::span<const uint8_t> bytes){size_t at=0;while(at<bytes.size()){int n=send(socket,reinterpret_cast<const char*>(bytes.data()+at),int(bytes.size()-at),0);check(n>0,"loopback send");at+=n;}}
std::vector<uint8_t> read_n(SOCKET socket,size_t size){std::vector<uint8_t> out(size);size_t at=0;while(at<size){int n=recv(socket,reinterpret_cast<char*>(out.data()+at),int(size-at),0);check(n>0,"loopback read");at+=n;}return out;}
LobbyPacket request(SOCKET socket,const NetworkKeys&keys,uint32_t sequence){auto bytes=read_n(socket,24);const unsigned count=((bytes[2]^keys.wire[2])<<8)|(bytes[3]^keys.wire[3]);auto body=read_n(socket,count);bytes.insert(bytes.end(),body.begin(),body.end());return decode_lobby(keys,bytes,sequence);}
template<class F>void until(F predicate){const auto end=GetTickCount64()+2000;while(!predicate()&&GetTickCount64()<end)Sleep(1);check(predicate(),"watchdog/callback deadline");}
template<class F>void fails(F function){bool failed=false;try{function();}catch(const Failure&){failed=true;}check(failed,"expected real transport failure");}
NetworkKeys keys(){NetworkKeys k{};k.hmac.fill(0x31);k.wire={0x13,0x24,0x35,0x46};return k;}
void rpc_interleave(){
 auto k=keys();Sockets pair;std::atomic_bool cancel=false,joinCancel=false;std::atomic<uint64_t> now=0;auto monitor=std::make_shared<LobbyMonitor>();Connection c(k,cancel,[&]{return now.load();});c.adopt_connected_socket(pair.take());c.start_monitor(monitor,1,&joinCancel);check(request(pair.server.value,k,1).command==5,"immediate real0005");now=10;send_all(pair.server.value,encode_lobby(k,5,1,std::array<uint8_t,4>{}));c.poll_skills();check(monitor->state().pingMs==10u,"first real HMAC/sequence beacon RTT");
 unsigned chat=0,notifications=0;c.nativeChat=[&](const LobbyPacket&){++chat;};c.nativeNotifications=[&](const LobbyPacket&){++notifications;};now=14990;c.reset_deadline();c.send_packet(0x4300);check(request(pair.server.value,k,2).command==0x4300,"RPC sequence before interleaved ping");
 auto server=std::async(std::launch::async,[&]{now=15000;check(request(pair.server.value,k,3).command==5,"read wait emits due ping with correct sequence");send_all(pair.server.value,encode_lobby(k,chat::receive_opcode,2,{}));send_all(pair.server.value,encode_lobby(k,notices::reply_opcode,3,{}));now=15025;send_all(pair.server.value,encode_lobby(k,5,4,std::array<uint8_t,4>{}));send_all(pair.server.value,encode_lobby(k,0x4301,5,std::array<uint8_t,4>{}));});auto reply=c.read();server.get();check(reply.command==0x4301&&chat==1&&notifications==1&&monitor->state().pingMs==25u&&monitor->state().lastBeaconMs==15025,"RPC async demultiplex preserves correct response and latest RTT");
 now=45024;Sleep(20);check(monitor->state().connected,"deadline based on last valid response rather than last send");now=45025;until([&]{return !monitor->state().connected;});check(monitor->state().reason==LobbyDisconnectReason::beacon_timeout&&joinCancel&&c.cancellation(),"watchdog stops room/STUN even when owner does no IO");fails([&]{c.send_packet(0x4380);});check(!cancel,"beacon does not mutate outer user cancellation token");
}
void rpc_timeout(){
 auto k=keys();Sockets pair;std::atomic_bool cancel=false;std::atomic<uint64_t> now=0;auto monitor=std::make_shared<LobbyMonitor>();Connection c(k,cancel,[&]{return now.load();});c.adopt_connected_socket(pair.take());c.start_monitor(monitor,2);request(pair.server.value,k,1);send_all(pair.server.value,encode_lobby(k,5,1,std::array<uint8_t,4>{}));c.poll_skills();now=29000;c.keep_alive();check(request(pair.server.value,k,2).command==5,"one pending late probe");c.reset_deadline();c.send_packet(0x4300);check(request(pair.server.value,k,3).command==0x4300,"RPC starts before beacon deadline");
 auto server=std::async(std::launch::async,[&]{now=29999;send_all(pair.server.value,encode_lobby(k,notices::reply_opcode,2,{}));Sleep(20);now=30000;});fails([&]{c.read();});server.get();check(monitor->state().reason==LobbyDisconnectReason::beacon_timeout,"unrelated packet cannot extend beacon deadline during RPC");
}
void rpc_deadline_preserved(){
 auto k=keys();Sockets pair;std::atomic_bool cancel=false;std::atomic<uint64_t> now=0;auto monitor=std::make_shared<LobbyMonitor>();Connection c(k,cancel,[&]{return now.load();});c.adopt_connected_socket(pair.take());c.start_monitor(monitor,3);request(pair.server.value,k,1);send_all(pair.server.value,encode_lobby(k,5,1,std::array<uint8_t,4>{}));c.poll_skills();now=14999;c.reset_deadline();c.send_packet(0x4300);request(pair.server.value,k,2);
 auto server=std::async(std::launch::async,[&]{now=15000;request(pair.server.value,k,3);now=22998;send_all(pair.server.value,encode_lobby(k,5,2,std::array<uint8_t,4>{}));until([&]{return monitor->state().lastBeaconMs==22998;});now=22999;});fails([&]{c.read();});server.get();check(monitor->state().reason==LobbyDisconnectReason::network_error&&monitor->state().lastBeaconMs==22998,"beacon cannot reset original eight-second RPC deadline");
}
void invalid_and_cancel(){
 auto k=keys();for(bool malformed:{false,true}){Sockets pair;std::atomic_bool cancel=false;std::atomic<uint64_t> now=0;auto monitor=std::make_shared<LobbyMonitor>();Connection c(k,cancel,[&]{return now.load();});c.adopt_connected_socket(pair.take());c.start_monitor(monitor,4);request(pair.server.value,k,1);auto bytes=encode_lobby(k,5,1,malformed?std::array<uint8_t,4>{0,0,0,1}:std::array<uint8_t,4>{});if(!malformed)bytes[8]^=1;send_all(pair.server.value,bytes);bool failed=false;try{c.poll_skills();}catch(...){failed=true;}check(failed&&!monitor->state().pingMs,"nonzero BE32 or invalid HMAC never becomes a valid RTT sample");if(malformed)check(monitor->state().reason==LobbyDisconnectReason::invalid_beacon,"explicit malformed beacon disconnect reason");}
 auto monitor=std::make_shared<LobbyMonitor>();{Sockets pair;std::atomic_bool cancel=false;std::atomic<uint64_t> now=0;Connection c(k,cancel,[&]{return now.load();});c.adopt_connected_socket(pair.take());c.start_monitor(monitor,5);request(pair.server.value,k,1);cancel=true;until([&]{return c.cancellation().load();});}check(!monitor->state().connected&&monitor->state().reason==LobbyDisconnectReason::none,"explicit logout closes safely without timeout rebound");
}
void notification_extents(){
 const auto k=keys();
 auto reject=[](auto action){bool bad=false;try{action();}catch(...){bad=true;}check(bad,"notification extent/checksum rejection");};
 for(bool asynchronous:{false,true})for(size_t size:{size_t(1024),size_t(8192)}){
  Sockets pair;std::atomic_bool cancel=false;std::atomic<uint64_t> now=0;
  auto monitor=std::make_shared<LobbyMonitor>();Connection c(k,cancel,[&]{return now.load();});c.adopt_connected_socket(pair.take());c.start_monitor(monitor,20);request(pair.server.value,k,1);
  send_all(pair.server.value,encode_lobby(k,5,1,std::array<uint8_t,4>{}));c.poll_skills();
  const std::vector<uint8_t> payload(size,0x41);unsigned notifications=0;
  c.nativeNotifications=[&](const LobbyPacket&p){check(p.payload==payload,"complete large authenticated notification");++notifications;};
  const auto bytes=encode_lobby(k,notices::reply_opcode,2,payload);
  check(decode_lobby(k,bytes,2).payload==payload,"large codec payload roundtrip");
  auto corrupt=bytes;corrupt.back()^=1;reject([&]{decode_lobby(k,corrupt,2);});reject([&]{decode_lobby(k,bytes,3);});
  if(asynchronous){
   send_all(pair.server.value,std::span(bytes).first(24));c.poll_skills();check(notifications==0,"header alone does not dispatch partial notification");
   send_all(pair.server.value,std::span(bytes).subspan(24));until([&]{c.poll_skills();return notifications==1;});
  }else{
   c.reset_deadline();c.send_packet(0x4300);request(pair.server.value,k,2);
   send_all(pair.server.value,bytes);send_all(pair.server.value,encode_lobby(k,0x4301,3,{}));
   check(c.read().command==0x4301&&notifications==1,"large notification interleaves synchronous RPC");
  }
  check(monitor->state().connected&&monitor->state().lastBeaconMs==0,"notification never advances beacon clock");
 }
 // Oversized header is rejected immediately, even before any body is buffered.
 for(bool asynchronous:{false,true})for(uint16_t command:{notices::reply_opcode,chat::receive_opcode}){
  const size_t size=command==notices::reply_opcode?8193:1024;
  reject([&]{encode_lobby(k,command,1,std::vector<uint8_t>(size));});
  std::vector<uint8_t> invalid(24+size);put(invalid,0,command,2);put(invalid,2,uint32_t(size),2);put(invalid,4,1);
  const auto digest=mac(k,invalid);std::copy(digest.begin(),digest.end(),invalid.begin()+8);for(size_t i=0;i<invalid.size();++i)invalid[i]^=k.wire[i%4];
  reject([&]{decode_lobby(k,invalid,1);});
  Sockets pair;std::atomic_bool cancel=false;std::atomic<uint64_t> now=0;auto monitor=std::make_shared<LobbyMonitor>();Connection c(k,cancel,[&]{return now.load();});c.adopt_connected_socket(pair.take());c.start_monitor(monitor,21);request(pair.server.value,k,1);
  send_all(pair.server.value,std::span(invalid).first(24));
  if(asynchronous)reject([&]{c.poll_skills();});else reject([&]{c.read();});
  check(monitor->state().reason==LobbyDisconnectReason::network_error,"oversized header terminates monitored connection");
 }
}
}
int main(){try{Wsa winsock;rpc_interleave();rpc_timeout();rpc_deadline_preserved();invalid_and_cancel();notification_extents();std::cout<<"Lobby transport PASS: real loopback HMAC/sequence0005 RTT, RPC async demux, watchdog shutdown, last-good30sec, RPC8sec preserved, malformed rejection, safe cancel/close, 44F1-only 8192B capacity\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}catch(const Failure&f){std::cerr<<"transport failure "<<f.code<<'\n';return 1;}}

