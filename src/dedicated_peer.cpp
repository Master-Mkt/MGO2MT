#include <windows.h>
#include "dedicated_peer.h"
#include <algorithm>
#include <utility>
namespace mgo2win::host {
namespace {
void put(std::vector<uint8_t>&b,uint32_t n,unsigned bytes){while(bytes--){b.push_back(uint8_t(n));n>>=8;}}
uint32_t read(std::span<const uint8_t>b,size_t at,unsigned n){if(at>b.size()||n>b.size()-at)throw Invalid(Error::extent);uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(b[at+i])<<(i*8);return v;}
std::string label(std::span<const uint8_t>b,bool required){if(b.size()>23||(required&&b.empty()))throw Invalid(Error::identity);if(!b.empty()&&!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,reinterpret_cast<const char*>(b.data()),int(b.size()),nullptr,0))throw Invalid(Error::message);for(auto c:b)if(c<32||c==127)throw Invalid(Error::message);return {b.begin(),b.end()};}
}
ProfileNames profile_names(std::span<const uint8_t>b){
 if(b.size()<46||b.size()>372||b[0]!=2)throw Invalid(Error::extent);
 size_t at=39;for(unsigned i=0;i<5;++i){auto skill=read(b,at++,1);if(skill>=128)throw Invalid(Error::identity);if(skill&&read(b,at++,1)>3)throw Invalid(Error::message);}
 auto highest=read(b,at++,1);if(highest>=128||highest*2>b.size()-at)throw Invalid(Error::extent);at+=highest*2;
 auto tail=b.subspan(at);auto end=std::find(tail.begin(),tail.end(),0);if(end==tail.end())throw Invalid(Error::extent);size_t n=end-tail.begin();
 return {label(tail.first(n),true),label(tail.subspan(n+1),false),read(b,8,4)};
}
std::vector<uint8_t> roster_record(const Player&p,const Hello&h,uint32_t clanId){
 if(p.character!=h.character||p.slot>=24||h.endpoints.size()>2)throw Invalid(Error::identity);
 std::vector<uint8_t>body{p.slot};put(body,p.character,4);put(body,h.flags,4);put(body,h.extra,2);body.push_back(uint8_t(h.endpoints.size()));
 for(auto&e:h.endpoints){if(!valid_endpoint(e))throw Invalid(Error::identity);body.insert(body.end(),e.address.begin(),e.address.end());put(body,e.port,2);}
 put(body,clanId,4);body.push_back(0);body.insert(body.end(),p.name.begin(),p.name.end());body.push_back(0);body.insert(body.end(),p.clan.begin(),p.clan.end());
 std::vector<uint8_t>b{7};put(b,uint32_t(body.size()),2);b.push_back(0);put(b,p.instance,2);b.push_back(0);b.insert(b.end(),body.begin(),body.end());Roster check;update_roster(check,b);return b;
}
std::vector<uint8_t> roster_remove(uint16_t instance){return {7,2,0,0,uint8_t(instance),uint8_t(instance>>8),1,0,0};}
std::vector<uint8_t> room_snapshot(std::span<const Rotation>rotation,uint8_t generation){
 if(rotation.empty()||rotation.size()>15)throw Invalid(Error::extent);
 // Reviewed global fields only: rotation table(2), index(9), round(13),
 // generation(51). No invented gameplay phase or loading-complete flag.
 std::vector<uint8_t>b(9);b[0]=11;for(unsigned i:{2,9,13,51})b[2+i/8]|=uint8_t(1u<<(i%8));
 for(size_t i=0;i<16;++i){auto r=i<rotation.size()?rotation[i]:Rotation{};b.push_back(r.map);b.push_back(r.rule);b.push_back(r.flags);}
 b.push_back(0);b.push_back(0);b.push_back(generation);return b;
}
DedicatedPeer::DedicatedPeer(Hello local,Hello remote,uint64_t now):local_(std::move(local)),remote_(std::move(remote)),keys_{local_.seed^remote_.seed,local_.seed^remote_.seed^initial_mac},last_(now),start_(now),helloNext_(now),keepalive_(now+2000){encode_hello(local_);encode_hello(remote_);if(local_.character==remote_.character)throw Invalid(Error::identity);}
uint64_t DedicatedPeer::queue(std::vector<uint8_t>b,uint64_t now){if(closed_)return 0;if(b.empty()||pending_.size()>=32||pending_.contains(txApp_)||!nextTicket_){close();return 0;}Message m;m.channel=1;m.serial=txApp_++;m.payload=std::move(b);auto ticket=nextTicket_++;pending_.emplace(m.serial,Pending{std::move(m),now,0,ticket});return ticket;}
std::vector<std::vector<uint8_t>> DedicatedPeer::events(){if(closed_){events_.clear();return {};}return std::exchange(events_,{});}
void DedicatedPeer::receive(std::span<const uint8_t>b,uint64_t now){if(closed_)return;
 try{
  Packet packet;bool initial=false;
  try{packet=decode(b,keys_,rx_);}catch(const Invalid&e){if(e.code!=Error::integrity)return;try{packet=decode(b,{},rx_);initial=true;}catch(const Invalid&){return;}}
  if(initial&&std::any_of(packet.messages.begin(),packet.messages.end(),[](const Message&m){return m.channel!=0||!m.reliable||m.serial!=0;}))return;
  int delta=int(int16_t(packet.sequence-rx_));if(received_){if(delta<=0){unsigned age=unsigned(-delta);if(age>=64||(replay_&(uint64_t(1)<<age)))return;replay_|=uint64_t(1)<<age;}else{replay_=delta>=64?1:(replay_<<delta)|1;rx_=packet.sequence;}}else{received_=true;rx_=packet.sequence;replay_=1;}last_=now;
  for(auto&m:packet.messages){
   if(m.channel==0){if(!m.reliable||m.serial)throw Invalid(Error::message);if(m.ack){helloAcked_=true;continue;}auto h=decode_hello(m.payload,remote_.character);if(h.seed!=remote_.seed||h.endpoints!=remote_.endpoints)throw Invalid(Error::identity);acks_.push_back({0,true,true,false,0,{}});continue;}
   if(m.channel!=1)continue;if(!m.reliable)throw Invalid(Error::message);if(m.ack){auto p=pending_.find(m.serial);if(p!=pending_.end()&&p->second.tries){delivered_.insert(p->second.ticket);pending_.erase(p);while(delivered_.erase(deliveredThrough_+1))++deliveredThrough_;}continue;}
   unsigned ahead=uint8_t(m.serial-rxApp_);if(ahead>=128){acks_.push_back({1,true,true,false,m.serial,{}});continue;}if(ahead>=32)throw Invalid(Error::sequence);
   auto found=reordered_.find(m.serial);if(found!=reordered_.end()&&found->second.payload!=m.payload)throw Invalid(Error::sequence);reordered_.insert_or_assign(m.serial,m);acks_.push_back({1,true,true,false,m.serial,{}});
  }
  while(reordered_.contains(rxApp_)){auto m=std::move(reordered_.at(rxApp_));reordered_.erase(rxApp_++);if(m.payload.empty())throw Invalid(Error::message);
   auto op=m.payload[0];if((op==0||op==1||op==10)&&m.payload.size()!=1)throw Invalid(Error::extent);
   if(op==10&&profile_.empty())throw Invalid(Error::identity);
   if(op==2){profile_names(m.payload);if(!profile_.empty()){if(profile_!=m.payload)throw Invalid(Error::identity);continue;}profile_=m.payload;}
   if(events_.size()>=32)throw Invalid(Error::extent);events_.push_back(std::move(m.payload));}
  if(acks_.size()>128)throw Invalid(Error::extent);
 }catch(const Invalid&){close();}
}
std::vector<std::vector<uint8_t>> DedicatedPeer::poll(uint64_t now){
 std::vector<std::vector<uint8_t>>out;if(closed_)return out;
 if(now<last_||now<start_||now-last_>=10000||((!helloAcked_||profile_.empty())&&now-start_>=8000)){close();return out;}
 auto send=[&](std::vector<Message>m,Keys keys){out.push_back(encode({tx_++,std::move(m)},keys));};
 if(!helloAcked_&&now>=helloNext_){Message m;m.payload=encode_hello(local_);send({std::move(m)},{});helloNext_=now+500;}
 if(!acks_.empty()){send(std::move(acks_),keys_);acks_.clear();}
 if(helloAcked_&&pending_.empty()&&now>=keepalive_){queue({0},now);keepalive_=now+2000;}
 for(auto&[serial,p]:pending_)if(now>=p.next){if(p.tries++>=24){closed_=true;return out;}send({p.message},keys_);p.next=now+350;}
 return out;
}
}
