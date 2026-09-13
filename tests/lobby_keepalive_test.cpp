#include "lobby_keepalive.h"
#include "character_client.h"
#include <array>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
static void require(bool b){if(!b)throw std::runtime_error("lobby keepalive regression");}
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::runtime_error&){failed=true;}require(failed);}
int main(){
 const std::array<uint8_t,4> ok{};
 LobbyKeepalive boundary(0);require(!boundary.poll(29999));require(boundary.poll(30000));require(!boundary.poll(30000));
 require(!boundary.receive(0x4b4b,ok,30001));require(boundary.pending());
 require(boundary.receive(5,ok,30001));require(!boundary.poll(59999));require(boundary.poll(60000));
 require(!boundary.poll(67999));rejects([&]{boundary.poll(68000);});rejects([&]{boundary.receive(5,ok,68000);});
 LobbyKeepalive unsolicited(0);rejects([&]{unsolicited.receive(5,ok,1);});
 for(auto payload:{std::vector<uint8_t>{},std::vector<uint8_t>{0},std::vector<uint8_t>{0,0,0,1},std::vector<uint8_t>{0,0,0,0,0}}){
  LobbyKeepalive invalid(0);require(invalid.poll(30000));rejects([&]{invalid.receive(5,payload,30001);});
 }
 LobbyKeepalive delayed(0);require(delayed.poll(90000));require(delayed.receive(5,ok,90001));require(!delayed.poll(90002));
 // Model Nomad's observed 120-second read inactivity rule against real lobby
 // framing/HMAC and independent sequence numbers. No socket or account is used.
 NetworkKeys keys{};for(unsigned i=0;i<16;++i)keys.hmac[i]=uint8_t(i*7+3);keys.wire={0x12,0x34,0x56,0x78};
 LobbyKeepalive live(0);uint64_t lastServerRead=0;uint32_t tx=1,rx=1;unsigned sent=0,sideReplies=0;
 for(uint64_t now=0;now<=600000;now+=100){
  require(now-lastServerRead<120000); // Old client with no writes fails at 120000.
  if(live.poll(now)){
   auto request=decode_lobby(keys,encode_lobby(keys,5,tx,{}),tx);++tx;
   require(request.command==5&&request.payload.empty());lastServerRead=now;++sent;
   for(uint16_t command:{uint16_t(0x41e1),uint16_t(0x4b4b)}){
    auto side=decode_lobby(keys,encode_lobby(keys,command,rx,ok),rx);++rx;
    require(!live.receive(side.command,side.payload,now));++sideReplies;
   }
   auto response=decode_lobby(keys,encode_lobby(keys,5,rx,ok),rx);++rx;
   require(live.receive(response.command,response.payload,now));
  }
 }
 require(sent==20&&sideReplies==40&&600000-lastServerRead<120000);
 std::cout<<"10-minute framed lobby session survived 20 pings; 40 interleaved skill/emblem replies preserved; missing/late/unsolicited/malformed replies and no-burst boundaries passed.\n";
}
