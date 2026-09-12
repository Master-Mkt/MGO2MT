#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include "host_session.h"
#include <algorithm>
#include <set>
namespace mgo2win::host {
namespace {
uint32_t number(std::span<const uint8_t>b,size_t at,unsigned n=4,bool little=false){if(at>b.size()||n>b.size()-at)throw Invalid(Error::extent);uint32_t v=0;for(unsigned i=0;i<n;++i){if(little)v|=uint32_t(b[at+i])<<(8*i);else v=(v<<8)|b[at+i];}return v;}
void put(std::vector<uint8_t>&b,uint32_t v,unsigned n){while(n--){b.push_back(uint8_t(v));v>>=8;}}
std::vector<uint8_t> text(std::span<const uint8_t>b,bool required){auto end=std::find(b.begin(),b.end(),0);int n=int(end-b.begin());if(required&&!n)throw Invalid(Error::identity);if(n&&!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,reinterpret_cast<const char*>(b.data()),n,nullptr,0))throw Invalid(Error::message);for(int i=0;i<n;++i)if(b[i]<32||b[i]==127)throw Invalid(Error::message);return {b.begin(),end};}
}
bool active(Stage s){return s==Stage::connecting||s==Stage::profile||s==Stage::synchronizing||s==Stage::joined;}
std::vector<uint8_t> profile_payload(uint32_t id,std::span<const uint8_t>info,std::span<const uint8_t>personal,std::span<const uint8_t>skills){
 // F08A18/F09A48/F096AC -> 97C39C -> 270E00. Values come from this PC's
 // authenticated 4100 replies, including equipped skill levels and clothing.
 if(!id||info.size()!=579||number(info,0)!=id||personal.size()!=245||skills.size()<4)throw Invalid(Error::identity);
 auto name=text(info.subspan(4,16),true),clan=text(personal.subspan(4,16),false);auto count=number(skills,0);if(count>128||skills.size()!=4+count*4)throw Invalid(Error::extent);
 std::array<uint16_t,128> values{};std::set<uint8_t> ids;unsigned highest=0;
 for(size_t at=4;at<skills.size();at+=4){auto key=skills[at];if(!key||key>=128||!ids.insert(key).second)throw Invalid(Error::identity);values[key]=uint16_t(number(skills,at+1,2));if(values[key])highest=std::max(highest,unsigned(key));}
 std::vector<uint8_t>b{2};put(b,number(info,28),2);put(b,number(info,574),2);
 b.push_back(0); // Native online status: normal; no PS3 social/voice flags.
 b.push_back(personal[239]);b.push_back(personal[240]);put(b,personal[20]==1||personal[20]==2?number(personal,0):0,4);put(b,number(personal,241),4);
 b.insert(b.end(),personal.begin()+62,personal.begin()+74);b.insert(b.end(),personal.begin()+49,personal.begin()+58);b.insert(b.end(),personal.begin()+74,personal.begin()+76);
 for(unsigned i=0;i<5;++i){auto skill=personal[76+i],level=personal[81+i];if(skill>=128||level>3)throw Invalid(Error::message);b.push_back(skill);if(skill)b.push_back(level);}
 b.push_back(uint8_t(highest));for(unsigned i=1;i<=highest;++i)put(b,values[i],2);b.insert(b.end(),name.begin(),name.end());b.push_back(0);b.insert(b.end(),clan.begin(),clan.end());if(b.size()>372)throw Invalid(Error::extent);return b;
}
Machine::Machine(Hello local,uint32_t host,std::vector<uint8_t>profile,uint64_t now):local_(std::move(local)),host_(host),profile_(std::move(profile)),start_(now),last_(now),stage_at_(now),hello_next_(now){if(!host||host==local_.character||profile_.empty()||profile_[0]!=2||profile_.size()>372)throw Invalid(Error::identity);encode_hello(local_);}
void Machine::fail(Stage s,unsigned error){stage_=s;error_=error;pending_.clear();reordered_.clear();roster_={};match_={};placements_.clear();itemReordered_.clear();}
void Machine::queue(std::vector<uint8_t>b,uint64_t now){if(pending_.size()>=32||pending_.contains(tx_app_))throw Invalid(Error::sequence);Message m;m.channel=1;m.serial=tx_app_++;m.payload=std::move(b);pending_.emplace(m.serial,Pending{std::move(m),now,0});}
void Machine::application(std::span<const uint8_t>b,uint64_t now){if(b.empty())throw Invalid(Error::message);
 if(b[0]==7){
  auto present=[&](uint32_t id){return std::any_of(roster_.slots.begin(),roster_.slots.end(),[&](const auto&p){return p&&p->character==id;});};
  bool hadSelf=present(local_.character),hadHost=present(host_);bool playerClass=update_roster(roster_,b);
  // Native policy: leave when our admitted identity disappears. Host migration
  // is not implemented; a removed host also ends this pinned-host session.
  if(stage_==Stage::joined&&((hadSelf&&!present(local_.character))||(hadHost&&!present(host_)))){fail(Stage::disconnected);return;}
  if(playerClass&&b[6]==3&&stage_==Stage::profile){queue({10},now);stage_=Stage::synchronizing;stage_at_=now;}
 }
 else if(b[0]==11){auto generation=update_match(match_,b);if(generation&&placements_.result().generation!=generation){placements_.begin(*generation);itemReordered_.clear();itemSerial_=0;generationPacket_=rx_;}if(generation&&stage_==Stage::synchronizing){stage_=Stage::joined;was_joined_=true;stage_at_=now;keepalive_at_=now+2000;}}
 // Other channels/application records belong to gameplay; no gameplay action
 // is synthesized. Room entry requires the original explicit global update.
}
void Machine::receive(std::span<const uint8_t>raw,uint64_t now){if(!active(stage_))return;
 try{Packet packet;bool initial=false;try{packet=decode(raw,hello_received_?keys_:Keys{},rx_);}catch(const Invalid&e){if(e.code!=Error::integrity)throw;if(!hello_received_)return;try{packet=decode(raw,{},rx_);initial=true;}catch(const Invalid&){return;}}
  // Late initial-key packets may only acknowledge/repeat the known handshake.
  if(initial&&std::any_of(packet.messages.begin(),packet.messages.end(),[](const Message&m){return m.channel!=0||!m.reliable||m.serial!=0;}))return;
  int delta=int(int16_t(packet.sequence-rx_));if(received_){if(delta<=0){unsigned age=unsigned(-delta);if(age>=64||(replay_&(uint64_t(1)<<age)))return;replay_|=uint64_t(1)<<age;}else{replay_=delta>=64?1:(replay_<<delta)|1;rx_=packet.sequence;}}else{received_=true;rx_=packet.sequence;replay_=1;}
  last_=now;
  for(auto&m:packet.messages){if(m.channel==0){if(!m.reliable||m.serial!=0)throw Invalid(Error::message);if(m.ack){hello_acked_=true;continue;}if(m.payload.size()==5&&number(m.payload,0,4,true)==host_&&m.payload[4]==0){fail(Stage::rejected);return;}auto peer=decode_hello(m.payload,host_);if(hello_received_&&peer.seed!=peer_seed_)throw Invalid(Error::identity);peer_seed_=peer.seed;keys_={local_.seed^peer.seed,local_.seed^peer.seed^initial_mac};hello_received_=true;acks_.push_back({0,true,true,false,0,{}});continue;}
   if(!hello_received_)throw Invalid(Error::message);
   if(m.channel!=1)continue;
   if(!m.reliable)throw Invalid(Error::message);
   if(m.ack){pending_.erase(m.serial);continue;}
   unsigned ahead=uint8_t(m.serial-rx_app_);if(ahead>=128){acks_.push_back({1,true,true,false,m.serial,{}});continue;}if(ahead>=32)throw Invalid(Error::sequence);
   if(reordered_.size()>=32&&!reordered_.contains(m.serial))throw Invalid(Error::sequence);auto found=reordered_.find(m.serial);if(found!=reordered_.end()&&found->second.payload!=m.payload)throw Invalid(Error::sequence);reordered_.insert_or_assign(m.serial,m);acks_.push_back({1,true,true,false,m.serial,{}});
  }
  if(hello_received_&&hello_acked_&&stage_==Stage::connecting){stage_=Stage::profile;stage_at_=now;queue(profile_,now);profile_sent_=true;}
  while(active(stage_)&&reordered_.contains(rx_app_)){auto m=std::move(reordered_.at(rx_app_));reordered_.erase(rx_app_++);application(m.payload,now);}
  // Original fixed item object 592; channels >63 carry round parity (261AB0).
  // Wait for a validated global generation. No ACK means the reliable sender
  // can retry records which arrived before that registration was available.
  if(auto generation=placements_.result().generation;generation&&int16_t(packet.sequence-generationPacket_)>=0){
   uint16_t channel=item_channel|((*generation&1)?0x800:0);
   for(auto&m:packet.messages){if(m.channel!=channel||m.ack)continue;if(!m.reliable)throw Invalid(Error::message);
    unsigned ahead=uint8_t(m.serial-itemSerial_);if(ahead>=128){acks_.push_back({channel,true,true,false,m.serial,{}});continue;}if(ahead>=32)throw Invalid(Error::sequence);
    auto it=itemReordered_.find(m.serial);if(it!=itemReordered_.end()&&it->second.payload!=m.payload)throw Invalid(Error::sequence);
    itemReordered_.insert_or_assign(m.serial,m);acks_.push_back({channel,true,true,false,m.serial,{}});
   }
   while(itemReordered_.contains(itemSerial_)){auto m=std::move(itemReordered_.at(itemSerial_));itemReordered_.erase(itemSerial_++);placements_.receive(*generation,m.payload);}
  }
  if(acks_.size()>128)throw Invalid(Error::extent);
 }catch(const Invalid&e){fail(Stage::protocol_error,unsigned(e.code));}
}
std::vector<std::vector<uint8_t>> Machine::poll(uint64_t now){std::vector<std::vector<uint8_t>>out;if(!active(stage_))return out;
 if(now<start_||now<last_){fail(Stage::protocol_error);return out;}
 if((stage_!=Stage::joined&&now-stage_at_>=8000)||(stage_==Stage::joined&&now-last_>=10000)){fail(stage_==Stage::joined?Stage::disconnected:Stage::timeout);return out;}
 auto send=[&](std::vector<Message>m,Keys k){out.push_back(encode({tx_++,std::move(m)},k));};
 // Opcode 0 is the reviewed no-op callback on both sides. A native 2s
 // heartbeat keeps this room-only implementation alive without gameplay input.
 if(stage_==Stage::joined&&now>=keepalive_at_){if(pending_.empty())queue({0},now);keepalive_at_=now+2000;}
 if(!acks_.empty()){send(std::move(acks_),hello_received_?keys_:Keys{});acks_.clear();}
 if(stage_==Stage::connecting&&!hello_acked_&&now>=hello_next_){Message h;h.payload=encode_hello(local_);send({std::move(h)},{});hello_next_=now+500;}
 for(auto&[serial,p]:pending_)if(now>=p.next){if(p.tries>=24){fail(Stage::timeout);return out;}send({p.message},keys_);p.next=now+350;++p.tries;}
 return out;
}
void Machine::cancel(){if(active(stage_))fail(Stage::cancelled);}
std::optional<std::vector<uint8_t>> Machine::leave_packet(){if(!profile_sent_)return {};Message m;m.channel=1;m.serial=tx_app_++;m.payload={1};return encode({tx_++,{m}},keys_);}
Result run(const Local&local,const Admission&admission,std::span<const uint8_t>profile,const std::atomic_bool&stop,const std::atomic_bool&cancel,const std::function<void(Result)>&publish,const std::function<bool()>&lobbyAlive){
 Result out;try{if(local.socket==~uintptr_t(0)||!valid_endpoint(local.public_endpoint)||!valid_endpoint(local.private_endpoint))return out;
  for(auto&e:admission.endpoints)if(!valid_endpoint(e))throw Invalid(Error::identity);if(admission.character==local.character)throw Invalid(Error::identity);
  uint32_t seed=0;if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&seed),sizeof(seed),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("host random source");
  SOCKET socket=SOCKET(local.socket);u_long nonblocking=1;if(ioctlsocket(socket,FIONBIO,&nonblocking))return {Stage::network_error,unsigned(WSAGetLastError())};
  // Prefer LAN only for peers sharing our public address. Otherwise pin public.
  Endpoint peer=admission.endpoints[0];if(peer.address==local.public_endpoint.address)peer=admission.endpoints[1];
  sockaddr_in destination{};destination.sin_family=AF_INET;destination.sin_port=htons(peer.port);std::copy(peer.address.begin(),peer.address.end(),reinterpret_cast<uint8_t*>(&destination.sin_addr));
  Machine machine({local.character,seed,2,2,{local.public_endpoint,local.private_endpoint}},admission.character,{profile.begin(),profile.end()},GetTickCount64());Stage reported=Stage::unavailable;uint64_t reportedRevision=0,reportedMatchRevision=0,reportedPlacementRevision=0;bool reportedPartial=false;
  while(active(machine.result().stage)){
   if(stop||cancel){machine.cancel();break;}
   if(lobbyAlive&&!lobbyAlive()){out=machine.result();out.stage=Stage::disconnected;return out;}
   auto now=GetTickCount64();
   for(auto&b:machine.poll(now)){int n=sendto(socket,reinterpret_cast<const char*>(b.data()),int(b.size()),0,reinterpret_cast<sockaddr*>(&destination),sizeof(destination));if(n==SOCKET_ERROR){auto err=WSAGetLastError();if(err!=WSAEWOULDBLOCK){out=machine.result();out.stage=Stage::network_error;out.error=unsigned(err);return out;}}}
   out=machine.result();if(out.stage!=reported||out.roster.revision!=reportedRevision||out.match.revision!=reportedMatchRevision||out.placements.revision!=reportedPlacementRevision||out.placements.partial!=reportedPartial){reported=out.stage;reportedRevision=out.roster.revision;reportedMatchRevision=out.match.revision;reportedPlacementRevision=out.placements.revision;reportedPartial=out.placements.partial;publish(out);}if(!active(out.stage))break;
   fd_set read;FD_ZERO(&read);FD_SET(socket,&read);timeval wait{0,20000};int n=select(0,&read,nullptr,nullptr,&wait);if(n<0){out.stage=Stage::network_error;out.error=unsigned(WSAGetLastError());return out;}
   if(n){std::array<uint8_t,2049>b{};sockaddr_in from{};int len=sizeof(from);n=recvfrom(socket,reinterpret_cast<char*>(b.data()),int(b.size()),0,reinterpret_cast<sockaddr*>(&from),&len);if(n<0){auto err=WSAGetLastError();if(err==WSAEWOULDBLOCK||err==WSAEMSGSIZE||err==WSAECONNRESET)continue;out.stage=Stage::network_error;out.error=unsigned(err);return out;}if(from.sin_family==AF_INET&&from.sin_port==destination.sin_port&&from.sin_addr.s_addr==destination.sin_addr.s_addr)machine.receive(std::span(b).first(size_t(n)),GetTickCount64());}
  }
  out=machine.result();if(auto leave=machine.leave_packet())sendto(socket,reinterpret_cast<const char*>(leave->data()),int(leave->size()),0,reinterpret_cast<sockaddr*>(&destination),sizeof(destination));return out;
 }catch(const Invalid&e){out.stage=Stage::protocol_error;out.error=unsigned(e.code);}catch(...){out.stage=Stage::network_error;}return out;
}
}
