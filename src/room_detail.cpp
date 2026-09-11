#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "character_client.h"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace mgo2win {
namespace {
uint32_t number(std::span<const uint8_t>b,size_t at,unsigned n=4){if(at+n>b.size())throw std::runtime_error("room extent");uint32_t v=0;while(n--)v=(v<<8)|b[at++];return v;}
std::wstring string(std::span<const uint8_t>b,bool lines=false){auto end=std::find(b.begin(),b.end(),0);int n=int(end-b.begin());if(!n)return {};int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,reinterpret_cast<const char*>(b.data()),n,nullptr,0);if(!count)throw std::runtime_error("room UTF8");std::wstring s(count,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,reinterpret_cast<const char*>(b.data()),n,s.data(),count);for(auto c:s)if((c<32&&!(lines&&(c==10||c==13||c==9)))||c==127)throw std::runtime_error("room text control");return s;}
struct Rejected {uint32_t code;};
void response(const LobbyPacket&p,uint16_t command,size_t successSize){if(p.command!=command||p.payload.size()<4)throw std::runtime_error("room response command");auto result=number(p.payload,0);if(result){if(p.payload.size()!=4)throw std::runtime_error("room error extent");throw Rejected{result};}if(p.payload.size()!=successSize)throw std::runtime_error("room response extent");}
std::array<uint8_t,4> id_payload(uint32_t id){return {uint8_t(id>>24),uint8_t(id>>16),uint8_t(id>>8),uint8_t(id)};}
host::Endpoint endpoint(std::span<const uint8_t>b){auto s=string(b.first(16));IN_ADDR address{};if(s.empty()||InetPtonW(AF_INET,s.c_str(),&address)!=1)throw std::runtime_error("host endpoint");host::Endpoint e;std::copy_n(reinterpret_cast<const uint8_t*>(&address),4,e.address.begin());e.port=uint16_t(number(b,16,2));if(!host::valid_endpoint(e))throw std::runtime_error("invalid host endpoint");return e;}
}
RoomDetail parse_room_detail(std::span<const uint8_t>b,uint32_t expected){
 // F1256C: result + ID/name/comment/flags, HostGameEnv204, player18*28, tail.
 if(b.size()!=877||number(b,0)||!expected||number(b,4)!=expected)throw std::runtime_error("room detail identity/extent");
 RoomDetail d;d.id=expected;d.name=string(b.subspan(8,16));d.comment=string(b.subspan(24,128),true);
 if(d.name.empty()||b[152]>1||b[153]>1)throw std::runtime_error("room detail flags");d.password=b[152]!=0;d.dedicated=b[153]!=0;d.subtype=b[154];
 constexpr size_t env=168,players=372;d.capacity=b[env+66];d.players=b[env+67];if(!d.capacity||d.capacity>18||d.players>18)throw std::runtime_error("room player capacity");
 std::set<uint32_t> ids;
 for(size_t i=0;i<18;++i){auto at=players+i*28;auto id=number(b,at);if(!id){if(i==0)throw std::runtime_error("host must occupy slot zero");continue;}if(!ids.insert(id).second)throw std::runtime_error("duplicate room player");auto name=string(b.subspan(at+4,16));if(name.empty())throw std::runtime_error("empty player name");d.roster.push_back({id,std::move(name)});}
 return d;
}
std::vector<uint8_t> room_join_payload(uint32_t id,uint8_t subtype,std::wstring_view password){
 if(!id||!subtype||password.size()>16)throw std::invalid_argument("join input");for(auto c:password)if(c<32||c==127)throw std::invalid_argument("join password control");
 int n=password.empty()?0:WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,password.data(),int(password.size()),nullptr,0,nullptr,nullptr);
 if(!password.empty()&&(n<3||n>16))throw std::invalid_argument("join password extent");
 std::vector<uint8_t>b(21);auto bytes=id_payload(id);std::copy(bytes.begin(),bytes.end(),b.begin());b[20]=subtype;
 if(n)WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,password.data(),int(password.size()),reinterpret_cast<char*>(b.data()+4),16,nullptr,nullptr);return b;
}
std::vector<uint8_t> room_action_wire_payload(const NetworkKeys&keys,uint16_t command,std::span<const uint8_t> plain){
 if((command==0x4320&&plain.size()!=21)||(command==0x4312&&plain.size()!=4)||((command==0x4322||command==0x4380)&&!plain.empty())||(command!=0x4320&&command!=0x4312&&command!=0x4322&&command!=0x4380))throw std::invalid_argument("room wire request");
 std::vector<uint8_t> out(plain.begin(),plain.end());if(command==0x4320){out.resize(24);network_block(out,keys.packet,true);}return out;
}
RoomJoinStatus room_host_status(host::Stage s){switch(s){case host::Stage::connecting:return RoomJoinStatus::host_connecting;case host::Stage::profile:return RoomJoinStatus::host_profile;case host::Stage::synchronizing:return RoomJoinStatus::host_sync;case host::Stage::joined:return RoomJoinStatus::joined;case host::Stage::cancelled:return RoomJoinStatus::host_cancelled;case host::Stage::timeout:return RoomJoinStatus::host_timeout;case host::Stage::rejected:return RoomJoinStatus::host_rejected;case host::Stage::disconnected:return RoomJoinStatus::host_disconnected;case host::Stage::network_error:return RoomJoinStatus::host_network_error;case host::Stage::protocol_error:return RoomJoinStatus::host_protocol_error;default:return RoomJoinStatus::host_unavailable;}}
RoomReply exchange_room_action(const RoomAction&a,const CharacterExchange&exchange,std::atomic_bool&uncertain,const std::atomic_bool&cancel,const HostConnect&connect){
 RoomReply out;out.event=a.event;out.requested_room=a.id;out.status=RoomStatus::ready;
 try{
  if(!a.id||a.event==RoomEvent::list)throw std::invalid_argument("room action");if(uncertain){out.join_status=RoomJoinStatus::outcome_unknown;out.status=RoomStatus::protocol_error;return out;}if(cancel){out.status=RoomStatus::cancelled;return out;}
  auto id=id_payload(a.id);auto p=exchange(0x4312,id);response(p,0x4313,877);out.detail=parse_room_detail(p.payload,a.id);
  if(a.event==RoomEvent::detail)return out;
  // Refresh the same room immediately before sending, never use a stale UI subtype.
  if(out.detail->subtype!=a.subtype)throw std::runtime_error("room subtype changed");
  if(out.detail->players>=out.detail->capacity){out.status=RoomStatus::rejected;out.error=0xc0ffee10;out.join_status=RoomJoinStatus::rejected;return out;}
  auto end=std::find(a.password.begin(),a.password.end(),0);std::wstring_view password(a.password.data(),size_t(end-a.password.begin()));
  if(out.detail->password&&password.empty())throw std::invalid_argument("room password required");
  auto payload=room_join_payload(a.id,out.detail->subtype,password);struct Wipe{std::vector<uint8_t>&b;~Wipe(){SecureZeroMemory(b.data(),b.size());}}wipe{payload};
  if(cancel){out.status=RoomStatus::cancelled;return out;}
  uncertain=true;p=exchange(0x4320,payload);SecureZeroMemory(payload.data(),payload.size());
  try{response(p,0x4321,43);}catch(const Rejected&r){uncertain=false;throw;}
  host::Admission admission;admission.character=out.detail->roster.front().id;
  host::Result hostResult;bool threw=false,invalidEndpoint=false;
  try{admission.endpoints={endpoint(std::span(p.payload).subspan(4,18)),endpoint(std::span(p.payload).subspan(22,18))};}
  catch(...){invalidEndpoint=true;hostResult.stage=host::Stage::protocol_error;}
  if(connect&&!invalidEndpoint){try{hostResult=connect(admission);}catch(...){threw=true;hostResult.stage=host::Stage::network_error;hostResult.profile_sent=true;}}
  // 4322 clears the pending reservation. Once a profile may have reached the
  // host, 4380 also requests removal of a possible active player (not the host).
  p=exchange(0x4322,{});response(p,0x4323,4);
  if(connect&&hostResult.profile_sent){p=exchange(0x4380,{});response(p,0x4381,4);}
  uncertain=false;out.join_status=connect||invalidEndpoint?room_host_status(hostResult.stage):RoomJoinStatus::permission_checked;out.error=hostResult.error;
  if(threw)out.join_status=RoomJoinStatus::host_network_error;
 }catch(const Rejected&r){out.error=r.code;out.status=RoomStatus::rejected;out.join_status=uncertain?RoomJoinStatus::outcome_unknown:RoomJoinStatus::rejected;}
 catch(const std::invalid_argument&){out.status=RoomStatus::protocol_error;out.join_status=RoomJoinStatus::invalid_input;}
 catch(...){out.status=RoomStatus::protocol_error;out.join_status=uncertain?RoomJoinStatus::outcome_unknown:RoomJoinStatus::none;}
 return out;
}
}
