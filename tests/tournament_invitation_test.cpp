#include "tournament_invitation.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
using namespace mgo2win::invitations;
static void check(bool ok,const char*s){if(!ok)throw std::runtime_error(s);}
template<class F>void rejects(F f){try{f();}catch(const std::exception&){return;}throw std::runtime_error("malformed accepted");}
std::vector<uint8_t> notice(uint32_t id=123,uint8_t kind=4,uint8_t state=1,std::string name="DOLL-02"){
 std::vector<uint8_t>b(32);b[1]=6;for(unsigned i=0;i<4;++i)b[2+i]=uint8_t(id>>(24-i*8));b[8]=1;b[9]=200;b[10]=state;b[11]=kind;
 if(name.size()>16)throw std::runtime_error("fixture name too long");std::copy(name.begin(),name.end(),b.begin()+16);return b;
}
std::vector<uint8_t> reply(uint32_t id,uint8_t state,uint32_t result=0){std::vector<uint8_t>b(9);for(unsigned i=0;i<4;++i){b[i]=uint8_t(result>>(24-i*8));b[i+4]=uint8_t(id>>(24-i*8));}b[8]=state;return b;}
int main(){try{
 const auto utf8=chat::Encoding::utf8;auto b=notice();auto n=parse_notification(b,utf8);
 check(n.lobby==6&&n.id==123&&n.serverTime==456&&n.state==1&&n.kind==4&&n.opaque==0&&n.name=="DOLL-02","Java 49C1 exact golden fixture");
 auto a=answer_payload(0x12345678,true);check(a==std::array<uint8_t,5>{0x12,0x34,0x56,0x78,2},"original yes5B");check(answer_payload(123,false)[4]==4,"original no4");
 n=parse_notification(notice(1,4,1,"日本語招待"),utf8);check(n.name=="日本語招待","JP UTF8 retained");
 check(parse_notification(notice(1,4,1,"1234567890123456"),utf8).name.size()==16,"full fixed field without NUL");
 check(parse_notification(notice(1,4,1,"\xe9"),chat::Encoding::latin1).name=="\xc3\xa9","explicit Latin1 conversion");
 b=notice();b.pop_back();rejects([&]{parse_notification(b,utf8);});b=notice();b.push_back(0);rejects([&]{parse_notification(b,utf8);});
 for(auto name:{std::string("\xc0\x80"),std::string("\xed\xa0\x80"),std::string("\xf4\x90\x80\x80"),std::string("a\nb"),std::string("\xe2\x80\xaetest"),std::string()})rejects([&]{parse_notification(notice(1,4,1,name),utf8);});
 b=notice(1,4,1,"a");b[18]='x';rejects([&]{parse_notification(b,utf8);});rejects([&]{parse_notification(notice(0),utf8);});rejects([&]{parse_notification(notice(1,4,5),utf8);});rejects([&]{answer_payload(0,true);});rejects([&]{parse_answer_reply(reply(1,1));});
 Session s(Policy{90000,5000,{4}});s.bind(11,7);
 check(!s.receive_notification(10,notice(),utf8,100),"old connection rejected");check(s.receive_notification(11,notice(),utf8,100),"incoming accepted");
 check(!s.receive_notification(11,notice(),utf8,1000)&&s.view(1000)[0].expiresAt==90100,"replay does not extend expiry");
 check(!s.receive_notification(11,notice(123,4,4),utf8,1000)&&s.view(1000)[0].state==State::pending,"outgoing state with same integer cannot cancel incoming");
 check(s.receive_notification(11,notice(124,99),utf8,1000)&&!s.view(1000)[1].respondable&&!s.answer(11,124,true,1000),"unknown type visible but cannot send");
 check(s.receive_notification(11,notice(125),utf8,1000)&&!s.receive_notification(11,notice(126),utf8,1000),"bounded three active invitations");
 check(!s.answer(10,123,true,1000)&&s.answer(11,123,true,1000).has_value(),"scope checked send");
 check(s.view(1000)[0].state==State::sending&&!s.answer(11,123,true,1001)&&!s.answer(11,125,false,1001),"single flight no repeat");
 check(!s.receive_answer(11,reply(125,2),1001)&&!s.receive_answer(10,reply(123,2),1001)&&!s.receive_answer(11,reply(123,4),1001),"id scope and choice response correlation");
 check(s.receive_answer(11,reply(123,2),1002)&&s.view(1002)[0].state==State::accepted,"ack required for accepted");
 check(s.view(1002)[2].state==State::expired&&!s.receive_answer(11,reply(123,2),1003),"success expires other pending and duplicate ACK ignored");
 check(!s.receive_notification(11,notice(),utf8,1003),"completed invitation cannot replay");
 s.bind(12,7);check(s.view(0).empty()&&s.receive_notification(12,notice(),utf8,0),"connection reset clears scope and seen IDs");
 check(s.answer(12,123,false,0).has_value()&&!s.receive_answer(12,reply(123,4),5000)&&s.view(5000)[0].state==State::outcome_unknown,"timeout late ACK no fabricated success");
 check(!s.answer(12,123,false,5001),"uncertain response never retries");
 s.bind(13,8);check(s.receive_notification(13,notice(),utf8,0)&&s.answer(13,123,true,0).has_value(),"new authenticated identity scope");
 check(s.receive_answer(13,reply(123,2,0xfffffdf8),1)&&s.view(1)[0].state==State::rejected&&s.view(1)[0].result==0xfffffdf8,"server refusal preserved");
 s.bind(14,8);s.receive_notification(14,notice(),utf8,100);check(!s.answer(14,123,true,90100)&&s.view(90100)[0].state==State::expired,"exact expiry boundary");
 s.bind(15,8);s.receive_notification(15,notice(),utf8,100);s.answer(15,123,true,100);s.submission_unknown(14,123);check(s.view(100)[0].state==State::sending,"old-scope queue failure ignored");s.submission_unknown(15,123);check(s.view(100)[0].state==State::outcome_unknown,"queue uncertainty terminal");
 s.bind(0,0);check(!s.receive_notification(0,notice(),utf8,0)&&s.view(0).empty(),"logout rejects all notifications");
 Session queue(Policy{90000,5000,{},true});queue.bind(91,6);check(queue.receive_notification(91,notice(42,5,1,"日本語招待"),utf8,100),"valid received other kind");
 check(queue.submit(91,42,true,100)&&!queue.submit(91,42,true,101)&&queue.view(101)[0].state==State::queued,"UI submit queues only once");
 check(!queue.receive_answer(91,reply(42,2),101),"ACK before actual send ignored");
 check(queue.view(6000)[0].state==State::queued,"answer timeout starts at take not enqueue");
 check(queue.take(6000).has_value()&&!queue.take(6000)&&queue.view(6000)[0].state==State::sending,"network drain exactly once");
 check(queue.receive_answer(91,reply(42,2),6001)&&queue.view(6001)[0].state==State::accepted,"queued answer correlated");
 queue.bind(92,6);queue.receive_notification(92,notice(),utf8,0);check(queue.submit(92,123,false,0),"second queue");queue.bind(93,6);check(!queue.take(0)&&!queue.submit(92,123,true,0),"scope change discards queue and old UI decision");
 queue.receive_notification(93,notice(),utf8,0);queue.submit(93,123,true,0);check(!queue.take(90000)&&queue.view(90000)[0].state==State::expired,"unsent queue expires before send");
 check(marquee_utf8(parse_notification(notice(1,4,1,"日本語招待"),utf8)).find("サバイバル")!=std::string::npos&&marquee_utf8(parse_notification(notice(1,5),utf8)).find("トーナメント")!=std::string::npos,"original kind4 versus other UI classification");
 std::cout<<"invitations: exact Java/PPC wire, Unicode, scope, 3-entry bound, response correlation, expiry, replay and queued outcomes passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
