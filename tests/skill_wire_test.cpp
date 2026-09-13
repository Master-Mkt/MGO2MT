#include "skill_wire.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2win::skills;
static void check(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
template<class F>static void rejects(F action,const char*message){try{action();}catch(const std::runtime_error&){return;}throw std::runtime_error(message);}
static std::vector<uint8_t> reply(uint32_t token=7,uint32_t character=123){return {
 0x47,0x57,0x53,0x4b,1,0,0,4,uint8_t(token>>24),uint8_t(token>>16),uint8_t(token>>8),uint8_t(token),
 0,0,0,13,2,1,0,0,uint8_t(character>>24),uint8_t(character>>16),uint8_t(character>>8),uint8_t(character),0,5,2,0};}
int main(){try{
 RemoteRequest get{0x81234567};check(remote_payload(get)==std::vector<uint8_t>({0x47,0x57,0x53,0x4b,1,0,0,0,0x81,0x23,0x45,0x67}),"GET golden big-endian layout");
 RemoteRequest put{7,12,3,true,{{{5,2}}}};check(remote_payload(put)==std::vector<uint8_t>({0x47,0x57,0x53,0x4b,1,3,0,0,0,0,0,7,0,0,0,12,1,0,0,0,0,5,2,0}),"PUT matches candidate Java codec");
 auto bytes=reply();auto profile=remote_reply(bytes);check(profile.token==7&&profile.revision==13&&profile.character==123&&profile.used==2&&profile.loadout==Loadout{{{5,2}}},"reply field offsets match Java golden response");
 for(size_t size=0;size<bytes.size();++size)rejects([&]{remote_reply(std::span(bytes).first(size));},"truncated reply accepted");
 for(size_t at:{size_t(0),size_t(4),size_t(18),size_t(19),size_t(27)}){auto bad=bytes;bad[at]^=0x80;rejects([&]{remote_reply(bad);},"magic/version/reserved mutation accepted");}
 for(auto pair:{std::pair<size_t,uint8_t>{5,7},{6,4},{7,3},{7,9},{16,5},{17,9},{26,0}}){auto bad=bytes;bad[pair.first]=pair.second;rejects([&]{remote_reply(bad);},"reply bounds accepted");}
 auto trailing=bytes;trailing.push_back(0);rejects([&]{remote_reply(trailing);},"trailing reply bytes accepted");
 auto other=bytes;other[24]=1;rejects([&]{remote_reply(other);},"out-of-range server skill ID accepted");
 auto zeroChar=bytes;for(size_t i=20;i<24;++i)zeroChar[i]=0;rejects([&]{remote_reply(zeroChar);},"successful reply without character accepted");
 auto duplicate=bytes;duplicate[17]=2;duplicate.insert(duplicate.end(),bytes.begin()+24,bytes.end());rejects([&]{remote_reply(duplicate);},"duplicate reply skill accepted");
 rejects([&]{remote_payload({0});},"zero request token accepted");rejects([&]{remote_payload({1,0,4});},"invalid set accepted");
 rejects([&]{remote_payload({1,0,0,true,{{{256,1}}}});},"out-of-range outgoing skill ID accepted");
 rejects([&]{remote_payload({1,0,0,true,{{{1,1},{1,2}}}});},"duplicate outgoing skill accepted");
 Remote remote;remote.character(123);check(!remote.save({})&&!remote.fetch(),"offline profile cannot save or fetch");remote.connected();auto read=remote.take(100);check(read&&!read->write&&remote.state().status==RemoteStatus::reading,"connection only offers GET capability probe");
 check(!remote.save({})&&!remote.take(100),"no save or duplicate flight before positive GET");
 auto response=remote_reply(reply(read->token,123));auto wrong=response;wrong.token++;remote.receive(wrong,false);check(remote.in_flight(),"wrong token ignored");wrong=response;wrong.character=124;remote.receive(wrong,false);check(remote.in_flight(),"other character reply ignored");remote.receive(response,true);check(remote.in_flight(),"wrong operation reply ignored");
 remote.receive(response,false);check(remote.state().status==RemoteStatus::ready&&!remote.in_flight(),"positive GET establishes capability");
 Loadout draft{{{1,3},{2,1}}};check(remote.save(draft),"validated context can queue draft");auto write=remote.take(300);check(write&&write->write&&write->revision==13&&write->loadout==draft,"save uses fetched compare-and-set revision");check(remote.state().profile->loadout==response.loadout,"unacknowledged draft never replaces accepted profile");
 RemoteProfile conflict{};conflict.character=123;conflict.token=write->token;conflict.status=4;remote.receive(conflict,true);check(remote.state().status==RemoteStatus::conflict&&!remote.save(draft),"conflict requires refresh instead of overwriting newer profile");
 check(remote.fetch(),"conflict can refresh");read=remote.take(500);response.token=read->token;response.revision=14;remote.receive(response,false);check(remote.state().status==RemoteStatus::ready&&remote.save(draft),"refreshed revision permits deliberate retry");write=remote.take(600);
 response.token=write->token;response.revision=15;response.loadout=draft;response.used=4;remote.receive(response,true);check(remote.state().profile->loadout==draft&&remote.state().profile->revision==15,"only matching acknowledgment commits displayed profile");
 check(remote.fetch(),"explicit refresh queues");read=remote.take(1000);remote.tick(3999);check(remote.in_flight(),"timeout boundary inclusive");remote.tick(4000);check(remote.state().status==RemoteStatus::unsupported&&!remote.save(draft),"unanswered server request never claims a save");
 remote.disconnected();check(remote.state().status==RemoteStatus::offline&&!remote.save(draft),"disconnected profile cannot write");remote.connected();read=remote.take(5000);check(!remote.state().profile,"reconnect discards stale capability");
 remote.character(999);response.token=read->token;remote.receive(response,false);check(remote.state().character==999&&!remote.state().profile,"old character reply cannot repopulate new character state");
 remote.connected();read=remote.take(6000);response.character=999;response.token=read->token;response.capacity=8;response.used=8;response.loadout.entries.clear();for(uint16_t i=1;i<=8;++i)response.loadout.entries.push_back({i,1});remote.receive(response,false);check(remote.save(response.loadout),"explicit eight-capacity profile supports all eight entries");
 auto nine=response.loadout;nine.entries.push_back({9,1});rejects([&]{remote_payload({1,0,0,true,nine});},"nine-entry request accepted");
 std::cout<<"Native skill wire extent, identity, capability, revision, timeout and eight-entry boundaries passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
