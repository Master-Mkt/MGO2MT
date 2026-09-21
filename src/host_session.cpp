#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include "host_session.h"
#include "host_appearance.h"
#include "native_radio_context.h"
#include <algorithm>
#include <set>
#include <utility>
#include <iostream>
#include <syncstream>
namespace mgo2mt::host {
namespace {
const char* stage_name(Stage s){switch(s){
 case Stage::connecting:return "connecting";case Stage::profile:return "profile";case Stage::synchronizing:return "synchronizing";case Stage::joined:return "joined";
 case Stage::cancelled:return "cancelled";case Stage::timeout:return "timeout";case Stage::rejected:return "rejected";case Stage::disconnected:return "disconnected";
 case Stage::network_error:return "network_error";case Stage::protocol_error:return "protocol_error";default:return "unavailable";
}}
struct ConnectionLog {
 const Result&result;uint64_t start=GetTickCount64(),sent=0,received=0,ignoredSource=0;
 Stage lastActive=Stage::unavailable;bool profileSent=false,wasJoined=false,helloReceived=false,helloAcked=false;
 void observe(const Machine&machine){auto d=machine.diagnostics();lastActive=d.last_active_stage;profileSent=d.profile_sent;wasJoined=d.was_joined;helloReceived=d.hello_received;helloAcked=d.hello_acked;}
 void begin(bool lan,uint16_t localPort,uint16_t destinationPort)const noexcept{try{
  std::osyncstream(std::cout)<<"{\"host_connection\":\"begin\",\"route\":\""<<(lan?"lan":"public")<<"\",\"local_port\":"<<localPort<<",\"destination_port\":"<<destinationPort<<"}"<<std::endl;
 }catch(...){}}
 ~ConnectionLog()noexcept{try{
  std::osyncstream(std::cout)<<"{\"host_connection\":\"end\",\"stage\":\""<<stage_name(result.stage)<<"\",\"last_active_stage\":\""<<stage_name(lastActive)
   <<"\",\"elapsed_ms\":"<<(GetTickCount64()-start)<<",\"sent_datagrams\":"<<sent<<",\"received_datagrams\":"<<received<<",\"ignored_source_datagrams\":"<<ignoredSource
   <<",\"hello_received\":"<<(helloReceived?"true":"false")<<",\"hello_acked\":"<<(helloAcked?"true":"false")<<",\"profile_sent\":"<<(profileSent?"true":"false")
   <<",\"was_joined\":"<<(wasJoined?"true":"false")<<",\"error\":"<<result.error<<"}"<<std::endl;
 }catch(...){/* Diagnostics must not change admission or cleanup behavior. */}}
};
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
Machine::Machine(Hello local,uint32_t host,std::vector<uint8_t>profile,uint64_t now,std::optional<stage::ObjectRegistry> registry):local_(std::move(local)),host_(host),profile_(std::move(profile)),start_(now),last_(now),stage_at_(now),hello_next_(now){objectRegistry_=std::move(registry);if(objectRegistry_){stage::SceneReceiver validate(*objectRegistry_,0);}if(!host||host==local_.character||profile_.empty()||profile_[0]!=2||profile_.size()>372)throw Invalid(Error::identity);encode_hello(local_);}
void Machine::fail(Stage s,unsigned error){if(active(stage_))lastActiveStage_=stage_;stage_=s;error_=error;pending_.clear();reordered_.clear();combatOffer_.reset();radioMessages_.clear();inventoryMessages_.clear();combat_.clear();sop_={};debugFlights_.clear();environment_.clear();combatEvents_.clear();combatPendingInput_.reset();preparation_.reset();combatStatus_=combat::wire::Status::awaiting_world;roster_={};match_={};placements_.clear();itemReordered_.clear();objects_.reset();objectSlot_.reset();objectPending_.clear();objectReordered_.clear();}
void Machine::queue(std::vector<uint8_t>b,uint64_t now){if(pending_.size()>=32||pending_.contains(tx_app_))throw Invalid(Error::sequence);Message m;m.channel=1;m.serial=tx_app_++;m.payload=std::move(b);pending_.emplace(m.serial,Pending{std::move(m),now,0});}
void Machine::registries(std::vector<stage::ObjectRegistry> registries){
 if(stage_!=Stage::connecting)throw Invalid(Error::message);
 for(auto& r:registries){stage::SceneReceiver validate(r,0);if(!objectRegistries_.emplace(r.map,std::move(r)).second)throw Invalid(Error::message);}
}
void Machine::sync_objects(){
 if(match_.request&&!objectRegistries_.empty()){
  auto it=objectRegistries_.find(match_.request->rotation.map);
  if(it==objectRegistries_.end()){objects_.reset();objectSlot_.reset();objectRegistry_.reset();return;}
  if(!objectRegistry_||objectRegistry_->map!=it->first){objectRegistry_=it->second;objects_.reset();objectSlot_.reset();}
 }
 if(!objectRegistry_)return;
 auto found=std::find_if(roster_.slots.begin(),roster_.slots.end(),[&](const auto&p){return p&&p->character==local_.character;});
 std::optional<uint8_t> slot;if(found!=roster_.slots.end())slot=uint8_t(found-roster_.slots.begin());
 if(slot!=objectSlot_){objects_.reset();objectSlot_=slot;if(slot)objects_.emplace(*objectRegistry_,*slot);}
 if(objects_){objects_->begin(match_.request);}
}
void Machine::application(std::span<const uint8_t>b,uint64_t now){if(b.empty())throw Invalid(Error::message);
 if(items::wire::recognized(b)){if(stage_==Stage::joined&&combatOffer_&&inventoryMessages_.size()<32&&items::wire::decode(b))inventoryMessages_.emplace_back(b.begin(),b.end());return;}
 if(radio::recognized(b)){
  // Optional native extension: malformed/late/overflow records cannot end a room.
  auto record=radio::decode(b);
  if(stage_==Stage::joined&&combatOffer_&&record&&record->epoch==combatOffer_->epoch&&
     (record->kind==radio::Kind::offer||record->kind==radio::Kind::notification)&&radioMessages_.size()<radio::history_limit)
   radioMessages_.push_back(*radio::encode(*record));
  return;
 }
 if(b[0]==appearance_opcode)update_appearance(roster_,b);
 if(b[0]==7){
  auto present=[&](uint32_t id){return std::any_of(roster_.slots.begin(),roster_.slots.end(),[&](const auto&p){return p&&p->character==id;});};
  bool hadSelf=present(local_.character),hadHost=present(host_);bool playerClass=update_roster(roster_,b);
  // Native policy: leave when our admitted identity disappears. Host migration
  // is not implemented; a removed host also ends this pinned-host session.
  if(stage_==Stage::joined&&((hadSelf&&!present(local_.character))||(hadHost&&!present(host_)))){fail(Stage::disconnected);return;}
  if(playerClass&&b[6]==3&&stage_==Stage::profile){queue({10},now);stage_=Stage::synchronizing;stage_at_=now;}
 }
 else if(b[0]==11){auto generation=update_match(match_,b);if(generation&&placements_.result().generation!=generation){placements_.begin(*generation);combatOffer_.reset();radioMessages_.clear();inventoryMessages_.clear();combat_.clear();sop_={};debugFlights_.clear();environment_.clear();combatEvents_.clear();combatPendingInput_.reset();preparation_.reset();combatStatus_=combat::wire::Status::awaiting_world;itemReordered_.clear();itemSerial_=0;generationPacket_=rx_;objectRx_=0;objectTx_=0;objectRequestAt_=0;objectReordered_.clear();objectPending_.clear();}if(generation&&stage_==Stage::synchronizing){stage_=Stage::joined;was_joined_=true;stage_at_=now;keepalive_at_=now+2000;}}
 if(combat::wire::recognized(b)){
  try{auto record=combat::wire::decode(b);
   if(auto offer=std::get_if<combat::wire::Offer>(&record)){
    if(offer->configuration!=configuration_){std::osyncstream(std::clog)<<"combat_configuration_mismatch: gameplay.json or mounted_weapons.json differs from HOST\n";fail(Stage::protocol_error,ERROR_REVISION_MISMATCH);return;}
    if(stage_!=Stage::joined||offer->self.character!=local_.character||!roster_.slots[offer->self.slot]||roster_.slots[offer->self.slot]->instance!=offer->self.instance||roster_.slots[offer->self.slot]->character!=local_.character)throw Invalid(Error::identity);
    if(combatOffer_&&offer->epoch<combatOffer_->epoch)throw Invalid(Error::sequence);
    if(!combatOffer_||*combatOffer_!=*offer){combatOffer_=*offer;radioMessages_.clear();inventoryMessages_.clear();combat_.clear();sop_={};debugFlights_.clear();environment_.clear();combatEvents_.clear();combatPendingInput_.reset();preparation_.reset();queue(combat::wire::encode(combat::wire::Accept{offer->epoch,configuration_}),now);}
   }else if(auto state=std::get_if<combat::wire::Preparation>(&record)){
    if(!combatOffer_||state->epoch!=combatOffer_->epoch||state->self!=combatOffer_->self||!match_.generation||state->generation!=*match_.generation)throw Invalid(Error::identity);
    for(const auto&p:state->players)if(p){const auto&r=roster_.slots[p->id.slot];if(!r||r->instance!=p->id.instance||r->character!=p->id.character)throw Invalid(Error::identity);}
    if(preparation_&&state->revision<preparation_->revision)throw Invalid(Error::sequence);
    preparation_=*state;
   }else if(auto setting=std::get_if<combat::wire::Environment>(&record)){
    if(combatOffer_)environment_.receive(*setting,*combatOffer_);
   }else if(auto diagnostic=std::get_if<combat::wire::DebugFlights>(&record)){
    if(combatOffer_&&combat_.state())debugFlights_.receive(*diagnostic,*combatOffer_,*combat_.state(),now);
   }else if(auto frame=std::get_if<combat::wire::Frame>(&record)){
    if(!combatOffer_||frame->snapshot.epoch!=combatOffer_->epoch)throw Invalid(Error::identity);
    for(const auto&p:frame->snapshot.players)if(p){const auto&r=roster_.slots[p->identity.slot];if(!r||r->instance!=p->identity.instance||r->character!=p->identity.character)throw Invalid(Error::identity);}
    if(frame->sop.recipient!=combat::Identity{}&&frame->sop.recipient!=combatOffer_->self)throw Invalid(Error::identity);
    if(combat_.state()&&combat_.state()->revision==frame->snapshot.revision&&sop_!=frame->sop)throw Invalid(Error::sequence);
    if(!combat_.snapshot(frame->snapshot))throw Invalid(Error::sequence);
    sop_=frame->sop;
    auto events=combat_.events(frame->events,true);if(combatEvents_.size()+events.size()>128)throw Invalid(Error::extent);
    combatEvents_.insert(combatEvents_.end(),events.begin(),events.end());combatStatus_=frame->status;debugFlights_.poll(combat_.state(),combatStatus_,now);
   }else throw Invalid(Error::message);
  }catch(const combat::wire::Invalid&){if(b.size()>=7&&(b[6]==6||b[6]==7))return;throw Invalid(Error::message);}
 }
 sync_objects();
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
  // 7382B8 / 26B778: 734 is reliable; 735 is the coalesced unreliable path.
  // Register only after identity, full load request and reviewed schema match.
  if(objects_&&match_.request&&objects_->status()!=stage::SceneSyncStatus::unverified_registry&&objects_->status()!=stage::SceneSyncStatus::idle&&int16_t(packet.sequence-generationPacket_)>=0){
   const auto request=*match_.request;const uint16_t channel=734|((request.generation&1)?0x800:0),fast=channel+1;
   for(const auto&m:packet.messages){
    if(m.channel==fast){if(m.reliable||m.ack)throw Invalid(Error::message);objects_->receive(request,m.payload);continue;}
    if(m.channel!=channel)continue;if(!m.reliable)throw Invalid(Error::message);
    if(m.ack){auto p=objectPending_.find(m.serial);if(p!=objectPending_.end()&&p->second.tries)objectPending_.erase(p);continue;}
    unsigned ahead=uint8_t(m.serial-objectRx_);if(ahead>=128){acks_.push_back({channel,true,true,false,m.serial,{}});continue;}if(ahead>=32)throw Invalid(Error::sequence);
    auto it=objectReordered_.find(m.serial);if(it!=objectReordered_.end()&&it->second.payload!=m.payload)throw Invalid(Error::sequence);
    objectReordered_.insert_or_assign(m.serial,m);acks_.push_back({channel,true,true,false,m.serial,{}});
   }
   while(objectReordered_.contains(objectRx_)){auto m=std::move(objectReordered_.at(objectRx_));objectReordered_.erase(objectRx_++);objects_->receive(request,m.payload);}
  }
  if(acks_.size()>128)throw Invalid(Error::extent);
 }catch(const Invalid&e){fail(Stage::protocol_error,unsigned(e.code));}
}
std::vector<std::vector<uint8_t>> Machine::poll(uint64_t now){std::vector<std::vector<uint8_t>>out;if(!active(stage_))return out;
 if(now<start_||now<last_){fail(Stage::protocol_error);return out;}
 if((stage_!=Stage::joined&&now-stage_at_>=8000)||(stage_==Stage::joined&&now-last_>=10000)){fail(stage_==Stage::joined?Stage::disconnected:Stage::timeout);return out;}
 debugFlights_.poll(combat_.state(),combatStatus_,now);
 flush_combat_input(now);
 auto send=[&](std::vector<Message>m,Keys k){out.push_back(encode({tx_++,std::move(m)},k));};
 // Opcode 0 is the reviewed no-op callback on both sides. A native 2s
 // heartbeat keeps this room-only implementation alive without gameplay input.
 if(stage_==Stage::joined&&now>=keepalive_at_){if(pending_.empty())queue({0},now);keepalive_at_=now+2000;}
 if(objects_&&match_.request&&now>=objectRequestAt_)if(auto request=objects_->snapshot_request()){
  // Original 739578 retries E1 every 3000 ms until addressed E0, not until ACK.
  if(objectPending_.empty()){Message m;m.channel=734|((match_.request->generation&1)?0x800:0);m.serial=objectTx_++;m.payload.assign(request->begin(),request->end());objectPending_.emplace(m.serial,Pending{std::move(m),now,0});}
  objectRequestAt_=now+3000;
 }
 if(!acks_.empty()){send(std::move(acks_),hello_received_?keys_:Keys{});acks_.clear();}
 if(stage_==Stage::connecting&&!hello_acked_&&now>=hello_next_){Message h;h.payload=encode_hello(local_);send({std::move(h)},{});hello_next_=now+500;}
 for(auto&[serial,p]:objectPending_)if(now>=p.next){if(p.tries>=24){fail(Stage::timeout);return out;}send({p.message},keys_);p.next=now+350;++p.tries;}
 for(auto&[serial,p]:pending_)if(now>=p.next){if(p.tries>=24){fail(Stage::timeout);return out;}send({p.message},keys_);p.next=now+350;++p.tries;}
 return out;
}
void Machine::flush_combat_input(uint64_t now){
 if(!combatPendingInput_)return;
 if(stage_!=Stage::joined||!combatOffer_||combatPendingInput_->epoch!=combatOffer_->epoch||combatStatus_!=combat::wire::Status::active||!combat_.state()){combatPendingInput_.reset();return;}
 const auto&player=combat_.state()->players[combatOffer_->self.slot];
 if(!player||!player->alive||player->stunned||combatPendingInput_->life!=player->life){combatPendingInput_.reset();return;}
 if(pending_.size()<8){queue(combat::wire::encode(*combatPendingInput_),now);combatPendingInput_.reset();}
}
bool Machine::combat_input(const combat::wire::Input&input,uint64_t now){
 if(!input.debugPhysics)debugFlights_.request(false,input.sequence,input.life);
 if(stage_!=Stage::joined||!combatOffer_||input.epoch!=combatOffer_->epoch||combatStatus_!=combat::wire::Status::active||!combat_.state())return false;
 const auto&player=combat_.state()->players[combatOffer_->self.slot];if(!player||!player->alive||player->stunned||input.life!=player->life)return false;
 try{
  combat::wire::encode(input); // Validate before keeping an unsent request.
  auto merged=input;
  if(combatPendingInput_&&combatPendingInput_->life!=input.life)combatPendingInput_.reset();
  if(combatPendingInput_){
   if(uint32_t(input.sequence-combatPendingInput_->sequence)>=0x80000000u||input.sequence==combatPendingInput_->sequence)return false;
   merged=combat::wire::coalesce_input(*combatPendingInput_,input);
  }
  // Bound reliable traffic while retaining one newest pose and pending action.
  // The next poll drains this after ACKs free a slot, even without another input.
  debugFlights_.request(input.debugPhysics,input.sequence,input.life);combatPendingInput_=merged;flush_combat_input(now);return true;
 }catch(const combat::wire::Invalid&){return false;}
}
bool Machine::combat_command(const combat::wire::Command&command,uint64_t now){
 if(stage_!=Stage::joined||!combatOffer_||!preparation_||command.epoch!=combatOffer_->epoch||!preparation_->players[combatOffer_->self.slot]||command.life!=preparation_->players[combatOffer_->self.slot]->life||pending_.size()>=24)return false;
 if(preparation_->phase==combat::wire::RoundPhase::ended)return false;
 if(command.kind==combat::wire::CommandKind::loaded&&command.enabled&&objects_){const auto&snapshot=objects_->snapshot();if(objects_->status()!=stage::SceneSyncStatus::ready||!snapshot||snapshot->request!=match_.request||snapshot->request.generation!=command.generation||snapshot->revision!=command.sceneRevision)return false;}
 try{queue(combat::wire::encode(command),now);return true;}catch(const combat::wire::Invalid&){return false;}
}
std::vector<combat::Event> Machine::combat_events(){return std::exchange(combatEvents_,{});}
bool Machine::radio_send(std::span<const uint8_t> bytes,uint64_t now){
 if(stage_!=Stage::joined||!combatOffer_||pending_.size()>=8||pending_.contains(tx_app_))return false;
 auto record=radio::decode(bytes);
 if(!record||record->epoch!=combatOffer_->epoch||record->identity!=combatOffer_->self||
    (record->kind!=radio::Kind::probe&&record->kind!=radio::Kind::request))return false;
 try{queue(std::vector<uint8_t>(bytes.begin(),bytes.end()),now);return true;}catch(const Invalid&){return false;}
}
bool Machine::inventory_send(std::span<const uint8_t> body,uint64_t now){
 if(!radio_writable())return false;auto record=items::wire::decode(body);if(!record)return false;
 auto h=std::visit([](const auto& v){return v.header;},*record);
 if(h.scope.epoch!=combatOffer_->epoch||h.actor.slot!=combatOffer_->self.slot||h.actor.instance!=combatOffer_->self.instance||h.actor.character!=combatOffer_->self.character||(!std::holds_alternative<items::wire::Probe>(*record)&&!std::holds_alternative<items::wire::Command>(*record)))return false;
 queue({body.begin(),body.end()},now);return true;
}
std::vector<std::vector<uint8_t>> Machine::inventory_messages(){return std::exchange(inventoryMessages_,{});}
std::vector<radio::Body> Machine::radio_messages(){return std::exchange(radioMessages_,{});}
void Machine::cancel(){if(active(stage_))fail(Stage::cancelled);}
std::optional<std::vector<uint8_t>> Machine::leave_packet(){if(!profile_sent_)return {};Message m;m.channel=1;m.serial=tx_app_++;m.payload={1};return encode({tx_++,{m}},keys_);}
Result run(const Local&local,const Admission&admission,std::span<const uint8_t>profile,const std::atomic_bool&stop,const std::atomic_bool&cancel,const std::function<void(Result)>&publish,const std::function<bool()>&lobbyAlive,std::optional<stage::ObjectRegistry> registry,const std::function<std::optional<combat::wire::Input>()>&combatInput,const std::function<std::optional<combat::wire::Command>()>&combatCommand,std::shared_ptr<radio::Session> radioSession,std::shared_ptr<items::ClientSession> inventorySession,std::vector<stage::ObjectRegistry> registries,uint64_t configuration){
 struct InventoryLeave {std::shared_ptr<items::ClientSession> session;~InventoryLeave(){if(session)session->disconnect();}} inventoryLeave{inventorySession};
 struct RadioLeave {std::shared_ptr<radio::Session> session;~RadioLeave(){if(session)session->disconnect();}} radioLeave{radioSession};
 Result out;ConnectionLog diagnostic{out};try{if(local.socket==~uintptr_t(0)||!valid_endpoint(local.public_endpoint)||!valid_endpoint(local.private_endpoint))return out;
  for(auto&e:admission.endpoints)if(!valid_endpoint(e))throw Invalid(Error::identity);if(admission.character==local.character)throw Invalid(Error::identity);
  uint32_t seed=0;if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&seed),sizeof(seed),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("host random source");
  SOCKET socket=SOCKET(local.socket);u_long nonblocking=1;if(ioctlsocket(socket,FIONBIO,&nonblocking)){out.stage=Stage::network_error;out.error=unsigned(WSAGetLastError());return out;}
  // Prefer LAN only for peers sharing our public address. Otherwise pin public.
  Endpoint peer=admission.endpoints[0];const bool lan=peer.address==local.public_endpoint.address;if(lan)peer=admission.endpoints[1];
  diagnostic.begin(lan,local.private_endpoint.port,peer.port);
  sockaddr_in destination{};destination.sin_family=AF_INET;destination.sin_port=htons(peer.port);std::copy(peer.address.begin(),peer.address.end(),reinterpret_cast<uint8_t*>(&destination.sin_addr));
  Machine machine({local.character,seed,2,2,{local.public_endpoint,local.private_endpoint}},admission.character,{profile.begin(),profile.end()},GetTickCount64(),std::move(registry));machine.registries(std::move(registries));machine.configuration(configuration);Stage reported=Stage::unavailable;uint64_t reportedRevision=0,reportedMatchRevision=0,reportedPlacementRevision=0;bool reportedPartial=false;std::optional<stage::SceneSnapshot> reportedScene;auto reportedSceneStatus=stage::SceneSyncStatus::idle;std::optional<combat::Snapshot> reportedCombat;std::optional<combat::wire::Offer> reportedOffer;auto reportedCombatStatus=combat::wire::Status::awaiting_world;
  std::optional<combat::wire::Environment> reportedEnvironment;std::optional<combat::wire::DebugFlights> reportedDebug;std::optional<combat::wire::Preparation> reportedPreparation;std::optional<combat::wire::Command> pendingCommand;
  diagnostic.observe(machine);
  while(active(machine.result().stage)){
   if(stop||cancel){machine.cancel();break;}
   if(lobbyAlive&&!lobbyAlive()){out=machine.result();out.stage=Stage::disconnected;return out;}
   auto now=GetTickCount64();
   if(!pendingCommand&&combatCommand)pendingCommand=combatCommand();
   if(pendingCommand){const auto current=machine.result();if(!current.combat_offer||pendingCommand->epoch!=current.combat_offer->epoch||!current.preparation||!current.preparation->players[current.combat_offer->self.slot]||pendingCommand->life!=current.preparation->players[current.combat_offer->self.slot]->life)pendingCommand.reset();else if(machine.combat_command(*pendingCommand,now))pendingCommand.reset();}
   if(combatInput)if(auto input=combatInput())machine.combat_input(*input,now);
   if(inventorySession){const auto current=machine.result();items::ClientContext context;
    if(current.stage==Stage::joined&&current.combat_offer&&current.match.request&&current.combat_state&&current.combat_state->epoch==current.combat_offer->epoch){
     const auto id=current.combat_offer->self;const auto& p=current.combat_state->players[id.slot];
     if(p&&p->identity==id){context.scope={current.combat_offer->epoch,current.match.request->generation};context.actor={id.slot,id.instance,id.character,p->life};context.position={p->pose.feet[0],p->pose.feet[1],p->pose.feet[2],p->pose.yaw};context.active=p->alive&&!p->stunned&&current.combat_status==combat::wire::Status::active;}
    }
    uint64_t nonce=0;if(inventorySession->state().status==items::ClientStatus::unavailable&&BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&nonce),sizeof(nonce),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("inventory nonce");
    for(auto& b:inventorySession->pump(context,now,nonce,machine.radio_writable(),machine.inventory_messages()))if(!machine.inventory_send(b,now))inventorySession->disconnect();
   }
   if(radioSession){const auto current=machine.result();const auto context=radio::client_context(current);const auto self=current.combat_offer?current.combat_offer->self:combat::Identity{};
    uint64_t nonce=0;const auto previous=radioSession->state();
    if(previous.epoch!=context.epoch||previous.self!=self||previous.status==radio::Status::unavailable)
     if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&nonce),sizeof(nonce),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)nonce=0;
    for(const auto& body:radioSession->pump(context,self,now,nonce,machine.radio_writable(),machine.radio_messages(),radio::same_team))
     if(!machine.radio_send(body,now))radioSession->disconnect();
   }
   for(auto&b:machine.poll(now)){int n=sendto(socket,reinterpret_cast<const char*>(b.data()),int(b.size()),0,reinterpret_cast<sockaddr*>(&destination),sizeof(destination));if(n==SOCKET_ERROR){auto err=WSAGetLastError();if(err!=WSAEWOULDBLOCK){out=machine.result();out.stage=Stage::network_error;out.error=unsigned(err);return out;}}else ++diagnostic.sent;}
   diagnostic.observe(machine);
   out=machine.result();if(out.environment!=reportedEnvironment||out.debug_flights!=reportedDebug||out.stage!=reported||out.roster.revision!=reportedRevision||out.match.revision!=reportedMatchRevision||out.placements.revision!=reportedPlacementRevision||out.placements.partial!=reportedPartial||out.scene!=reportedScene||out.scene_status!=reportedSceneStatus||out.combat_state!=reportedCombat||out.combat_offer!=reportedOffer||out.combat_status!=reportedCombatStatus||!out.combat_events.empty()||out.preparation!=reportedPreparation){reportedEnvironment=out.environment;reportedDebug=out.debug_flights;reportedPreparation=out.preparation;reported=out.stage;reportedRevision=out.roster.revision;reportedMatchRevision=out.match.revision;reportedPlacementRevision=out.placements.revision;reportedPartial=out.placements.partial;reportedScene=out.scene;reportedSceneStatus=out.scene_status;reportedCombat=out.combat_state;reportedOffer=out.combat_offer;reportedCombatStatus=out.combat_status;out.combat_events=machine.combat_events();publish(out);}if(!active(out.stage))break;
   fd_set read;FD_ZERO(&read);FD_SET(socket,&read);timeval wait{0,20000};int n=select(0,&read,nullptr,nullptr,&wait);if(n<0){out.stage=Stage::network_error;out.error=unsigned(WSAGetLastError());return out;}
   if(n){std::array<uint8_t,2049>b{};sockaddr_in from{};int len=sizeof(from);n=recvfrom(socket,reinterpret_cast<char*>(b.data()),int(b.size()),0,reinterpret_cast<sockaddr*>(&from),&len);if(n<0){auto err=WSAGetLastError();if(err==WSAEWOULDBLOCK||err==WSAEMSGSIZE||err==WSAECONNRESET)continue;out.stage=Stage::network_error;out.error=unsigned(err);return out;}++diagnostic.received;if(from.sin_family==AF_INET&&from.sin_port==destination.sin_port&&from.sin_addr.s_addr==destination.sin_addr.s_addr)machine.receive(std::span(b).first(size_t(n)),GetTickCount64());else ++diagnostic.ignoredSource;diagnostic.observe(machine);}
  }
  diagnostic.observe(machine);out=machine.result();if(auto leave=machine.leave_packet())if(sendto(socket,reinterpret_cast<const char*>(leave->data()),int(leave->size()),0,reinterpret_cast<sockaddr*>(&destination),sizeof(destination))!=SOCKET_ERROR)++diagnostic.sent;return out;
 }catch(const Invalid&e){out.stage=Stage::protocol_error;out.error=unsigned(e.code);}catch(...){out.stage=Stage::network_error;}return out;
}
}
