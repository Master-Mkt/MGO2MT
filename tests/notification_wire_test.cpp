#include "notification_wire.h"
#include "notification_bridge.h"
#include "server_disconnect_screen.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
static void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
static std::vector<uint8_t> bytes(std::string_view s){return {s.begin(),s.end()};}
static std::vector<uint8_t> file(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);check(bool(f),"fixture open");return {std::istreambuf_iterator<char>(f),{}};}
static void replace(std::string& s,std::string_view a,std::string_view b){auto n=s.find(a);check(n!=s.npos,"replace fixture");s.replace(n,a.size(),b);}
static void refused(std::string_view json,const char* message){bool rejected=false;try{notices::parse(bytes(json));}catch(...){rejected=true;}check(rejected,message);}
int main(int argc,char**argv){try{
 check(argc==2,"fixture argument");const auto root=std::filesystem::path(argv[1]);auto changed=file(root/"changed.json");auto reply=notices::parse(changed);
 check(reply.nonce==0x0102030405060708ull&&reply.character==10&&reply.snapshot->mailCount==3&&reply.snapshot->alert->text=="更新のお知らせ\n日本語通知","real Java handler UTF-8 decode");
 check(notices::request(reply.nonce,10,22,{})==file(root/"request.bin"),"actual Java request vector: ping22, selectedPC10, nonzero64bit nonce, ASCII zero digest");
 check(reply.serverTime==1789344010000ull&&reply.snapshot->alert->publishedAt==1789344000000ull&&reply.snapshot->alert->expiresAt==1789344300000ull&&reply.snapshot->latestMailId==99,"Java UTC milliseconds and MAX unread ID values");
 const auto unchanged=notices::parse(file(root/"unchanged.json"));check(!unchanged.changed&&!unchanged.snapshot&&unchanged.version==reply.version&&unchanged.nonce==reply.nonce&&unchanged.serverTime==1789344010001ull,"actual Java unchanged response fields");
 auto unknownPing=notices::request(reply.nonce,10,{},{});check(unknownPing.size()==88&&std::all_of(unknownPing.begin()+20,unknownPing.begin()+24,[](uint8_t v){return v==255;}),"unknown ping wire sentinel");
 check(notices::request(reply.nonce,10,0,{}).size()==88&&notices::request(reply.nonce,10,30000,{}).size()==88,"known ping inclusive bounds");
 bool invalidPing=false;try{notices::request(reply.nonce,10,30001,{});}catch(...){invalidPing=true;}check(invalidPing,"ping beyond beacon interval rejected");
 check(notices::request(reply.nonce,INT32_MAX,{},{}).size()==88,"selected PC signed DB maximum accepted");
 for(uint32_t pc:{0u,uint32_t(INT32_MAX)+1u,UINT32_MAX}){bool bad=false;try{notices::request(reply.nonce,pc,{},{});}catch(...){bad=true;}check(bad,"selected PC outside positive signed DB range rejected");}
 const std::string golden(changed.begin(),changed.end());
 auto maxPc=golden;replace(maxPc,"\"character\":10","\"character\":2147483647");check(notices::parse(bytes(maxPc)).character==uint32_t(INT32_MAX),"reply selected PC signed DB maximum accepted");
 for(auto pc:{"0","2147483648","4294967295"}){auto badPc=golden;replace(badPc,"\"character\":10",std::string("\"character\":")+pc);refused(badPc,"reply selected PC outside signed DB range rejected");}
 auto maxMail=golden;replace(maxMail,"\"count\":3","\"count\":4294967295");replace(maxMail,"\"latestId\":99","\"latestId\":4294967295");
 const auto maximum=notices::parse(bytes(maxMail));check(maximum.snapshot->mailCount==UINT32_MAX&&maximum.snapshot->latestMailId==UINT32_MAX,"aggregate mail full uint32 range preserved");
 auto invalid=golden;replace(invalid,"\"count\":3","\"count\":4294967296");refused(invalid,"count overflow rejected");
 invalid=golden;replace(invalid,"\"latestId\":99","\"latestId\":4294967296");refused(invalid,"mail ID overflow rejected");
 invalid=golden;replace(invalid,"\"count\":3","\"count\":0");refused(invalid,"nonzero latest requires unread count");
 invalid=golden;replace(invalid,"\"latestId\":99","\"latestId\":0");refused(invalid,"unread count requires latest ID");
 auto emptyMail=golden;replace(emptyMail,"\"count\":3","\"count\":0");replace(emptyMail,"\"latestId\":99","\"latestId\":0");check(notices::parse(bytes(emptyMail)).snapshot->mailCount==0,"empty aggregate accepted");
 invalid=golden;replace(invalid,"\"expiresAt\":1789344300000","\"expiresAt\":1789344010000");refused(invalid,"expiry equality is already expired");
 invalid=golden;replace(invalid,"\"publishedAt\":1789344000000","\"publishedAt\":1789344010001");refused(invalid,"future publication rejected");
 notices::Session s;s.connect(reply.nonce-1,10,0);check(s.take(0,37).has_value(),"initial poll");check(!s.take(1,{}),"only one pending request");check(s.receive(changed,100),"initial snapshot");check(s.state().ready&&s.state().serial==1,"atomic ready");check(!s.receive(changed,101),"replay rejected");check(!s.take(14999,{}),"15 second cadence");check(s.take(15000,10).has_value(),"next poll");
 auto altered=std::string(changed.begin(),changed.end());replace(altered,"0102030405060708","0102030405060709");auto rollback=altered;replace(rollback,"1789344010000","1789344009999");check(!s.receive(bytes(rollback),15010),"server clock rollback rejected");auto rewritten=altered;replace(rewritten,"\"count\":3","\"count\":4");check(!s.receive(bytes(rewritten),15011),"same version cannot rewrite snapshot");check(s.receive(bytes(altered),15012),"invalid replies do not consume nonce");
 check(s.take(30000,{}).has_value(),"third poll");check(!s.take(37999,{}),"8 second extension deadline");check(!s.take(38000,{}),"supported extension waits for scheduled retry");check(s.take(45000,{}).has_value(),"extension retries after timeout");s.disconnect();check(!s.state().connected&&!s.state().ready&&!s.take(45001,{}),"disconnect clears data");
 notices::Session legacy;legacy.connect(1,1,0);check(legacy.take(0,{}).has_value(),"legacy probe");check(!legacy.take(8000,{})&&!legacy.take(999999,{}),"legacy ignores unknown opcode once without disconnect");
 for(auto name:{"../a.wav","C:a.wav","\\a.wav","a..wav","CON.any.wav","COM1.foo.wav","nul.wav","a.WAV"})check(!notices::valid_media({name,std::string(64,'a'),44},false),"unsafe media descriptor");
 check(notices::valid_media({"notice-1.wav",std::string(64,'a'),44},false),"safe descriptor");
 for(auto bad:std::vector<std::string>{"{}","[]",std::string(8193,'a'),"{\"v\":1,\"v\":1}","{\"v\":01}","{\"v\":18446744073709551616}"}){bool refused=false;try{notices::parse(bytes(bad));}catch(...){refused=true;}check(refused,"malformed JSON refused");}
 for(size_t n=0;n<changed.size();++n){bool refused=false;try{notices::parse(std::span(changed).first(n));}catch(...){refused=true;}check(refused,"every truncation refused");}
 notifications::Presentation presentation;notices::State state;state.connected=true;state.scope=22;state.generation=1;state.character=10;
 notifications::update_session(presentation,state,{},100,1000);check(!presentation.take_ping(),"new empty invitation scope baseline");
 invitations::Entry invite;invite.scope=22;invite.notification.id=31;invite.notification.kind=4;invite.notification.state=1;invite.expiresAt=1000;
 auto v=notifications::update_session(presentation,state,std::span(&invite,1),101,1001);check(v.badges[2]&&presentation.take_ping(),"legacy server survival push works without DB extension");
 state.ready=true;state.serverTime=1000;state.receivedAt=100;state.snapshot.mailCount=3;state.snapshot.latestMailId=99;
 v=notifications::update_session(presentation,state,std::span(&invite,1),102,999999);check(v.badges[0]&&!presentation.take_ping(),"first asynchronous DB mail establishes quiet baseline");
 state.snapshot.latestMailId=100;v=notifications::update_session(presentation,state,std::span(&invite,1),103,999999);check(presentation.take_ping(),"new mail announces once");
 state.snapshot.latestMailId=99;v=notifications::update_session(presentation,state,std::span(&invite,1),104,999999);check(v.badges[0]&&!presentation.take_ping(),"reading latest mail reveals older unread ID without new-arrival sound");
 v=notifications::update_session(presentation,state,std::span(&invite,1),1000,999999);check(!v.badges[2]&&!presentation.take_ping(),"retail monotonic expiry independent of client wall clock");
 state.connected=false;v=notifications::update_session(presentation,state,{},1001,999999);check(v.badges==std::array{false,false,false}&&!v.news,"disconnect removes entire view");
 notices::State alertState;alertState.connected=alertState.ready=true;alertState.scope=25;alertState.generation=2;alertState.character=10;alertState.serverTime=reply.serverTime;alertState.receivedAt=100;alertState.snapshot=*reply.snapshot;
 notifications::Presentation alertView;auto news=notifications::update_session(alertView,alertState,{},100,1);check(news.news&&news.news->id==42,"actual Java current alert reaches presentation independently of local wall clock");
 check(notifications::server_now(alertState,1100,1)==reply.serverTime+1000,"server UTC advances using receive monotonic delta");
 news=notifications::update_session(alertView,alertState,{},100+reply.snapshot->alert->expiresAt-reply.serverTime,1);check(!news.news,"retained snapshot alert expires exactly at server-anchored deadline");
 std::vector<uint32_t> pixels(1280*720);paint_server_disconnect(pixels,LobbyDisconnectReason::beacon_timeout);check(std::count_if(pixels.begin(),pixels.end(),[](auto v){return (v&0xffffff)!=0;})>1000,"Japanese START error pixels");
 std::cout<<"notification wire + Java vectors + runtime bridge + disconnect UI PASS\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
