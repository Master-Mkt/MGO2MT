#include "lobby_keepalive.h"
#include "character_client.h"
#include <array>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
static void require(bool b){if(!b)throw std::runtime_error("lobby keepalive regression");}
template<class F>void rejects(F f,LobbyDisconnectReason reason){bool failed=false;try{f();}catch(const LobbyBeaconError&e){failed=e.reason==reason;}require(failed);}
int main(){try{
 const std::array<uint8_t,4> ok{};
 LobbyKeepalive boundary(0);require(boundary.poll(0));require(!boundary.poll(0));require(boundary.receive(5,ok,10)&&boundary.ping_ms()==10u);require(!boundary.poll(14999));require(boundary.poll(15000));require(!boundary.receive(0x4b4b,ok,30009));require(boundary.pending());rejects([&]{boundary.poll(30010);},LobbyDisconnectReason::beacon_timeout);rejects([&]{boundary.receive(5,ok,30010);},LobbyDisconnectReason::beacon_timeout);
 LobbyKeepalive unsolicited(0);rejects([&]{unsolicited.receive(5,ok,1);},LobbyDisconnectReason::invalid_beacon);
 for(auto payload:{std::vector<uint8_t>{},std::vector<uint8_t>{0},std::vector<uint8_t>{0,0,0,1},std::vector<uint8_t>{0,0,0,0,0}}){LobbyKeepalive invalid(0);require(invalid.poll(0));rejects([&]{invalid.receive(5,payload,1);},LobbyDisconnectReason::invalid_beacon);require(!invalid.ping_ms());}
 LobbyKeepalive delayed(0);rejects([&]{delayed.poll(90000);},LobbyDisconnectReason::beacon_timeout);LobbyKeepalive slow(0);require(slow.poll(0));require(!slow.poll(15000));require(slow.receive(5,ok,29999)&&slow.ping_ms()==29999u);require(slow.poll(29999));require(slow.receive(5,ok,30000));require(!slow.poll(30001));rejects([&]{slow.poll(29999);},LobbyDisconnectReason::invalid_beacon);
 LobbyKeepalive lastReply(0);require(lastReply.poll(0)&&lastReply.receive(5,ok,1));require(lastReply.poll(15000)&&lastReply.receive(5,ok,15123));require(lastReply.poll(30000));lastReply.check(45122);rejects([&]{lastReply.check(45123);},LobbyDisconnectReason::beacon_timeout);
 LobbyKeepalive nearEnd(UINT64_MAX-20000);require(nearEnd.poll(UINT64_MAX-20000));nearEnd.check(UINT64_MAX); // Relative arithmetic does not wrap to an expired deadline.
 LobbyMonitor monitor;std::atomic_bool joinCancelled=false;monitor.begin(1,&joinCancelled);monitor.beacon(1,12,100);require(monitor.state().pingMs==12u);monitor.begin(2,&joinCancelled);monitor.disconnect(1,LobbyDisconnectReason::beacon_timeout,&joinCancelled);require(!joinCancelled);monitor.beacon(1,99,999);require(monitor.state().connected&&!monitor.state().pingMs&&monitor.state().reason==LobbyDisconnectReason::none);monitor.disconnect(2,LobbyDisconnectReason::beacon_timeout);monitor.disconnect(2);monitor.beacon(2,1,200);require(!monitor.state().connected&&monitor.state().reason==LobbyDisconnectReason::beacon_timeout&&!monitor.state().pingMs);
 NetworkKeys keys{};for(unsigned i=0;i<16;++i)keys.hmac[i]=uint8_t(i*7+3);keys.wire={0x12,0x34,0x56,0x78};LobbyKeepalive live(0);uint64_t lastServerRead=0;uint32_t tx=1,rx=1;unsigned sent=0,sideReplies=0;
 for(uint64_t now=0;now<=600000;now+=100){require(now-lastServerRead<120000);if(live.poll(now)){auto request=decode_lobby(keys,encode_lobby(keys,5,tx,{}),tx);++tx;require(request.command==5&&request.payload.empty());lastServerRead=now;++sent;for(uint16_t command:{uint16_t(0x41e1),uint16_t(0x4b4b)}){auto side=decode_lobby(keys,encode_lobby(keys,command,rx,ok),rx);++rx;require(!live.receive(side.command,side.payload,now));++sideReplies;}auto response=decode_lobby(keys,encode_lobby(keys,5,rx,ok),rx);++rx;require(live.receive(response.command,response.payload,now));}}
 require(sent==41&&sideReplies==82&&600000-lastServerRead<120000);std::cout<<"Lobby monitor PASS: first immediate/15sec cadence, exact 30sec since valid beacon, RTT, one pending/no burst, malformed/unsolicited/late/clock, generation teardown, 41 framed pings with82 unrelated replies\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}


