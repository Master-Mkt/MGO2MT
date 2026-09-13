#include "native_radio.h"
#include "preset_radio_wire.h"
#include <algorithm>
#include <limits>
#include <utility>

namespace mgo2win::radio {
namespace {
bool valid(Identity i) noexcept { return i.slot<24 && i.instance && i.character; }
bool valid(const Context& c) noexcept {
 if(!c.epoch || c.members.size()>24) return false;
 for(size_t i=0;i<c.members.size();++i) {
  if(!valid(c.members[i].identity) || c.members[i].team>2) return false;
  for(size_t j=0;j<i;++j) if(c.members[i].identity.slot==c.members[j].identity.slot || c.members[i].identity.character==c.members[j].identity.character) return false;
 }
 return true;
}
const Member* member(const Context& c,Identity id) noexcept {
 for(const auto& m:c.members) if(m.identity==id) return &m;
 return nullptr;
}
bool allowed(const Filter& f,const Member& s,const Member& r,uint8_t p) noexcept {
 if(!f || !s.eligible || !r.eligible || !s.life || !r.life) return false;
 try { return f(s,r,p); } catch(...) { return false; }
}
void put(Body& b,size_t p,uint64_t v,size_t n) noexcept { for(size_t i=0;i<n;++i) b[p+n-1-i]=static_cast<uint8_t>(v>>(8*i)); }
uint64_t get(std::span<const uint8_t> b,size_t p,size_t n) noexcept {uint64_t v=0;for(size_t i=0;i<n;++i)v=(v<<8)|b[p+i];return v;}
bool valid(const Record& r) noexcept {
 if(!valid(r.identity)||!r.epoch||!r.token)return false;
 if(r.kind==Kind::probe||r.kind==Kind::offer)return !r.life&&!r.sequence&&!r.preset&&!r.third;
 if(r.kind!=Kind::request&&r.kind!=Kind::notification)return false;
 return r.life&&r.sequence&&r.third==2&&mgo2::radio::wire::reviewed_id(r.preset);
}
}
bool recognized(std::span<const uint8_t> b) noexcept {return !b.empty()&&b[0]==marker;}
std::optional<Body> encode(const Record& r) noexcept {
 if(!valid(r))return std::nullopt;
 Body b{};b[0]=marker;b[1]='G';b[2]='W';b[3]='R';b[4]='A';b[5]=1;b[6]=static_cast<uint8_t>(r.kind);
 put(b,8,r.epoch,8);put(b,16,r.token,8);b[24]=r.identity.slot;put(b,25,r.identity.instance,2);put(b,27,r.identity.character,4);
 put(b,32,r.life,4);put(b,36,r.sequence,4);b[40]=r.preset;b[41]=r.third;return b;
}
std::optional<Record> decode(std::span<const uint8_t> b) noexcept {
 if(b.size()!=44||b[0]!=marker||b[1]!='G'||b[2]!='W'||b[3]!='R'||b[4]!='A'||b[5]!=1||b[7]||b[31]||b[42]||b[43])return std::nullopt;
 Record r{static_cast<Kind>(b[6]),get(b,8,8),get(b,16,8),{b[24],static_cast<uint16_t>(get(b,25,2)),static_cast<uint32_t>(get(b,27,4))},static_cast<uint32_t>(get(b,32,4)),static_cast<uint32_t>(get(b,36,4)),b[40],b[41]};
 return valid(r)?std::optional<Record>(r):std::nullopt;
}
void Service::reset(uint64_t epoch) noexcept {epoch_=epoch;peers_={};}
void Service::remove(Identity id) noexcept {if(valid(id)&&peers_[id.slot]&&peers_[id.slot]->identity==id)peers_[id.slot].reset();}
std::vector<Delivery> Service::receive(Identity id,std::span<const uint8_t> bytes,const Context& c,uint64_t now,const Filter& filter) {
 std::vector<Delivery> result;
 if(!valid(c)||c.epoch!=epoch_||!valid(id))return result;
 const auto* source=member(c,id);if(!source)return result;
 auto r=decode(bytes);if(!r||r->epoch!=epoch_||r->identity!=id)return result;
 // Prune disappeared/replaced peers before any possible broadcast.
 for(auto& p:peers_)if(p&&!member(c,p->identity))p.reset();
 auto& p=peers_[id.slot];
 if(r->kind==Kind::probe) {
  if(p&&p->token!=r->token)return result; // re-probe cannot erase rate/replay state
  if(!p)p=Peer{id,r->token};
  r->kind=Kind::offer;result.push_back({id,*encode(*r)});return result;
 }
 if(r->kind!=Kind::request||!p||p->token!=r->token||r->life!=source->life||!source->eligible||r->sequence<=p->sequence)return result;
 p->sequence=r->sequence; // A rate-denied request cannot be replayed later.
 if(p->sent&&(now<p->lastAt||now-p->lastAt<policy_.minimumIntervalMs))return result;
 p->sent=true;p->lastAt=now;
 for(const auto& recipient:c.members) {
  const auto& target=peers_[recipient.identity.slot];
  if(!target||target->identity!=recipient.identity||!allowed(filter,*source,recipient,r->preset))continue;
  auto notice=*r;notice.kind=Kind::notification;notice.token=target->token;
  result.push_back({recipient.identity,*encode(notice)});
 }
 return result;
}
void Client::bind(uint64_t epoch,Identity self) noexcept {epoch_=epoch;self_=self;token_=probeAt_=0;status_=Status::unavailable;attempted_=false;sequence_=0;seen_={};events_.clear();}
std::optional<Body> Client::probe(uint64_t token,uint64_t now) noexcept {
 if(attempted_||!epoch_||!valid(self_)||!token)return std::nullopt;
 attempted_=true;token_=token;probeAt_=now;status_=Status::probing;
 return encode({Kind::probe,epoch_,token_,self_});
}
void Client::tick(uint64_t now) noexcept {if(status_==Status::probing&&(now<probeAt_||now-probeAt_>=probe_timeout_ms))status_=Status::unavailable;}
bool Client::receive(std::span<const uint8_t> bytes,const Context& c,uint64_t now,const Filter& filter) {
 tick(now);
 if(!valid(c)||c.epoch!=epoch_||!member(c,self_)) {events_.clear();return false;}
 auto r=decode(bytes);if(!r||r->epoch!=epoch_||r->token!=token_)return false;
 if(r->kind==Kind::offer) {
  if(status_!=Status::probing||r->identity!=self_)return false;
  status_=Status::ready;return true;
 }
 if(status_!=Status::ready||r->kind!=Kind::notification)return false;
 const auto* source=member(c,r->identity);const auto* receiver=member(c,self_);
 if(!source||source->life!=r->life||!allowed(filter,*source,*receiver,r->preset))return false;
 auto& seen=seen_[r->identity.slot];
 if(seen&&seen->identity==r->identity&&r->sequence<=seen->sequence)return false;
 seen=Seen{r->identity,r->sequence};
 // Keep replay watermark even if a slow UI overflows its bounded queue.
 if(events_.size()>=history_limit)return false;
 events_.push_back({r->epoch,r->identity,r->life,r->sequence,r->preset,r->third});return true;
}
Submission Client::submit(uint8_t preset,const Context& c,uint64_t now) noexcept {
 tick(now);Submission result;
 if(status_!=Status::ready)return result;
 const auto* self=member(c,self_);
 if(!valid(c)||c.epoch!=epoch_||!self||!self->eligible||!self->life){result.result=SubmitResult::ineligible;return result;}
 if(!mgo2::radio::wire::reviewed_id(preset)){result.result=SubmitResult::invalid_preset;return result;}
 if(sequence_==std::numeric_limits<uint32_t>::max()){result.result=SubmitResult::exhausted;return result;}
 result.sequence=++sequence_;result.body=encode({Kind::request,epoch_,token_,self_,self->life,result.sequence,preset,2});result.result=SubmitResult::submitted;return result;
}
std::vector<Event> Client::drain(){return std::exchange(events_,{});}
}
