#include "chat_session.h"
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace mgo2mt::chat;
static void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
static std::vector<uint8_t> cap(uint64_t n,uint32_t room=17,uint32_t pc=7,uint8_t encoding=1,uint8_t flags=1){auto p=capability_payload(n,room,pc);p.resize(28);p[5]=0;p[6]=encoding;p[7]=flags;return p;}
static void join(Session&s){s.connect(7);s.enter(17);s.roster({{7,"LOCAL",1},{8,"PEER",1},{9,"OTHER",2}},true);}
int main(){try{
 Session s;s.connect(7);s.enter(17);check(!s.request_capability(1,1),"capability waits for completed host admission");join(s);auto g=s.state().generation;
 check(s.submit(g,"hello",true,10)==Submit::unsupported_team,"legacy team cannot leak to all");
 check(s.submit(g,"日本語",false,10)==Submit::unknown_encoding,"legacy non-ASCII refused without encoding guess");
 check(s.submit(g,"  /all bad",false,10)==Submit::command_disabled,"slash commands never invoked implicitly");
 check(s.submit(g,"  ",false,10)==Submit::invalid_text,"empty input refused");
 check(s.submit(g,"hello",false,10)==Submit::accepted,"explicit ASCII submission");
 check(s.state().lines.empty()&&s.state().delivery==Delivery::queued,"no optimistic history");
 auto send=s.take(11);check(send&&send->payload.size()==129&&send->payload[0]==0&&send->payload[1]=='0',"original route and body");
 check(!s.take(12),"no duplicate take");
 check(!s.receive({100,0,"injected"},12),"unknown member refused");
 check(!s.receive({9,1,"private"},12),"enemy team message hidden");
 check(s.receive({8,0,"hello"},12)&&s.state().delivery==Delivery::awaiting_echo,"other sender does not acknowledge own draft");
 check(s.receive({7,0,"hello"},13)&&s.state().delivery==Delivery::echo_received,"matching self echo completes observation");
 check(s.receive({8,0,"OpenMGO2 | impersonation"},14)&&s.state().lines.back().name=="PEER","body prefix cannot forge server sender label");
 check(s.submit(g,"next",false,100)==Submit::busy,"bounded native rate");
 check(s.submit(g,"next",false,1000)==Submit::accepted&&s.take(1000).has_value(),"next manual message");
 s.tick(6000);check(s.state().delivery==Delivery::unconfirmed&&!s.take(6001),"timeout keeps uncertain outcome without retry");
 check(s.request_capability(11,6100).has_value()&&!s.request_capability(12,6101),"single capability attempt per room");
 check(!s.receive_capability(cap(12),6101)&&!s.receive_capability(cap(11,18),6101)&&!s.receive_capability(cap(11,17,8),6101),"nonce room identity must all match");
 auto bad=cap(11);bad[27]=1;check(!s.receive_capability(bad,6101),"reserved capability byte refused");
 check(s.receive_capability(cap(11),6101)&&s.state().encoding==Encoding::utf8&&!s.state().teamSupported,"matched capability enables UTF8 only");
 check(!s.receive_capability(cap(11),6102),"capability cannot replay or alter policy");
 check(s.submit(g,"日本語",false,6200)==Submit::accepted,"negotiated Japanese send");
 s.leave();check(!s.take(6201)&&s.state().lines.empty()&&!s.state().encoding,"leave cancels pending and scoped capability");
 s.enter(17);s.roster({{7,"LOCAL",1},{8,"PEER",1}},true);check(s.submit(g,"stale",false,7000)==Submit::not_joined,"old UI generation rejected after same-room reentry");
 check(s.request_capability(12,7100).has_value()&&!s.receive_capability(cap(11),7101),"old room nonce rejected");
 check(!s.receive_capability(cap(12),10100)&&s.state().encoding==Encoding::utf8,"deadline boundary keeps previously established charset");
 join(s);g=s.state().generation;s.capabilities(Encoding::latin1,false);
 check(s.submit(g,"日本語",false,1)==Submit::invalid_text,"ENG does not silently corrupt Japanese");
 check(s.submit(g,"café",false,1)==Submit::accepted,"representable Latin1 supported");
 auto latin=s.take(2);check(latin&&latin->payload[5]==0xe9,"actual Latin1 payload");
 s.failed(latin->serial+1);check(s.state().delivery==Delivery::awaiting_echo,"stale failure serial ignored");s.failed(latin->serial);check(s.state().delivery==Delivery::unconfirmed,"matching network failure retained");
 for(unsigned i=0;i<100;++i)check(s.receive({8,0,"bounded history"},3+i),"valid room message");check(s.state().lines.size()==64,"history capped");
 // Concurrent UI snapshots and worker updates use the same session lock.
 std::thread reader([&]{for(unsigned i=0;i<1000;++i){auto state=s.state();if(state.lines.size()>64)std::terminate();}});
 for(unsigned i=0;i<1000;++i)s.receive({8,0,"worker"},200+i);reader.join();
 s.roster({{7,"LOCAL",1},{7,"DUPLICATE",1}},true);check(!s.state().joined&&!s.take(1300),"malformed roster invalidates stale membership and pending work");
 s.roster({{8,"PEER",1}},true);check(!s.state().joined&&!s.take(1300),"self removal disables submission");s.disconnect();check(!s.state().self&&s.state().lines.empty(),"logout clears history");
 // Existing JP NOMADPX works without deploying the optional GWCH extension.
 s.connect(7,EndpointProfile::nomad_jp);s.enter(17);s.roster({{7,"日本語の名前",1},{8,"日本語の相手",1}},true);g=s.state().generation;
 check(s.state().encoding==Encoding::utf8&&!s.state().teamSupported,"known JP connection sets only UTF8 policy");
 check(s.submit(g,"日本語チャット",false,1)==Submit::accepted,"existing JP supports Japanese without GWCH");
 send=s.take(2);check(send&&receive_payload(std::vector<uint8_t>{0,0,0,7,'0',0xe6,0x97,0xa5,0},Encoding::utf8).text=="日","known JP uses UTF8 bytes");
 check(s.receive({7,0,"日本語チャット"},3)&&s.state().lines.back().name=="日本語の名前","Japanese sender and echoed body stay separate");
 s.display_name(7,"日本語の完全なキャラクター名");check(s.state().lines.back().name=="日本語の完全なキャラクター名","late canonical response updates existing history by ID");
 s.display_name(99,"unknown");check(s.state().lines.back().name!="unknown","unknown ID cannot relabel chat history");
 check(s.request_capability(99,4).has_value()&&!s.receive_capability(cap(98),5),"known JP still verifies discovery nonce");
 s.tick(3004);check(s.state().encoding==Encoding::utf8,"optional discovery timeout preserves existing JP support");
 s.leave();check(!s.state().encoding&&s.state().lines.empty(),"leave clears visible room state");
 s.enter(17);s.roster({{7,"LOCAL",1},{8,"PEER",1}},true);g=s.state().generation;
 check(s.state().encoding==Encoding::utf8,"known endpoint profile survives same-connection room change");
 check(s.submit(g,"日本語",false,4000)==Submit::accepted&&s.request_capability(100,4001).has_value(),"Japanese queued before optional discovery");
 check(s.receive_capability(cap(100,17,7,2),4002)&&s.state().encoding==Encoding::latin1&&!s.take(4003)&&s.state().delivery==Delivery::unconfirmed,"actual charset reply wins and cancels incompatible queued bytes");
 check(s.submit(g,"日本語",false,5000)==Submit::invalid_text,"actual ENG reply does not corrupt Japanese");
 s.leave();s.enter(18);check(s.state().encoding==Encoding::latin1,"explicit server charset persists across rooms on that connection");
 s.disconnect();s.connect(7);s.enter(17);s.roster({{7,"LOCAL",1}},true);g=s.state().generation;
 check(!s.state().encoding&&s.submit(g,"日本語",false,1)==Submit::unknown_encoding,"JP policy never leaks into a different unknown connection");
 std::cout<<"chat codec/session lifecycle, privacy gate, encoding, delivery uncertainty and concurrency PASS\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
