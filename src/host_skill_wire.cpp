#include "host_skill_wire.h"
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>
namespace mgo2win::host_skills {
namespace {
void require(bool b){if(!b)throw std::invalid_argument("native host skill snapshot");}
bool valid(const Scope&s){return s.room&&s.host&&s.character&&s.character!=s.host&&s.epoch&&s.slot<24&&s.instance;}
uint64_t get(std::span<const uint8_t>b,size_t at,unsigned n){require(at<=b.size()&&n<=b.size()-at);uint64_t v=0;while(n--)v=(v<<8)|b[at++];return v;}
void put(std::vector<uint8_t>&b,uint64_t v,unsigned n){while(n)b.push_back(uint8_t(v>>(8*--n)));}
}
std::vector<uint8_t> encode_request(const Request&r){
 require(valid(r.scope)&&r.nonce);std::vector<uint8_t>b{'G','W','S','H',1,1,0,0};
 put(b,r.nonce,8);put(b,r.scope.room,4);put(b,r.scope.character,4);return b;
}
Reply decode_reply(std::span<const uint8_t>b){
 require(b.size()>=72&&b.size()<=104&&get(b,0,4)==0x47575348&&b[4]==1&&b[5]<=7&&!b[6]&&!b[7]&&!b[39]&&b[38]<=8&&b.size()==72+4*size_t(b[38]));
 Reply r;r.status=b[5];r.nonce=get(b,8,8);r.room=uint32_t(get(b,16,4));r.character=uint32_t(get(b,20,4));r.host=uint32_t(get(b,24,4));r.membership=uint32_t(get(b,28,4));r.revision=uint32_t(get(b,32,4));r.capacity=b[36];r.used=b[37];std::copy_n(b.begin()+40,32,r.catalog.begin());
 if(r.status){require(b.size()==72&&!r.host&&!r.membership&&!r.revision&&!r.capacity&&!r.used&&r.catalog==std::array<uint8_t,32>{});return r;}
 require(r.nonce&&r.room&&r.character&&r.host&&r.host!=r.character&&r.membership&&r.revision&&r.capacity>=4&&r.capacity<=8&&r.used<=r.capacity&&r.catalog==catalog_digest);
 std::set<uint16_t> ids;for(size_t at=72;at<b.size();at+=4){skills::Choice c{uint16_t(get(b,at,2)),b[at+2]};require(c.id>=1&&c.id<=25&&c.level>=1&&c.level<=3&&!b[at+3]&&ids.insert(c.id).second);r.loadout.entries.push_back(c);}return r;
}
Receiver::Receiver(std::shared_ptr<const skills::Catalog> catalog,uint64_t seed):catalog_(std::move(catalog)),nonce_(seed){require(bool(catalog_)&&catalog_->entries().size()==75&&seed&&seed<std::numeric_limits<uint64_t>::max());}
void Receiver::begin(uint32_t room,uint32_t host,uint64_t epoch){
 require(room&&host&&epoch);if(room==room_&&host==host_&&epoch==epoch_)return;
 room_=room;host_=host;epoch_=epoch;queued_.clear();wanted_.clear();flight_.reset();ready_.clear();deadline_=0;
}
bool Receiver::want(uint8_t slot,uint16_t instance,uint32_t character){
 Scope s{room_,host_,character,epoch_,slot,instance};if(!valid(s)||availability_==Availability::unsupported)return false;
 if(auto it=wanted_.find(character);it!=wanted_.end())return it->second==s;
 if(wanted_.size()>=24||std::any_of(wanted_.begin(),wanted_.end(),[&](const auto&p){return p.second.slot==slot;}))return false;
 wanted_.emplace(character,s);queued_.push_back(s);return true;
}
void Receiver::forget(uint8_t slot,uint16_t instance,uint32_t character){
 Scope s{room_,host_,character,epoch_,slot,instance};auto it=wanted_.find(character);if(it==wanted_.end()||it->second!=s)return;
 wanted_.erase(it);std::erase(queued_,s);std::erase_if(ready_,[&](const auto&p){return p.scope()==s;});if(flight_&&flight_->scope==s)flight_.reset();
}
std::optional<Request> Receiver::take(uint64_t now){
 tick(now);if(availability_==Availability::unsupported||flight_||queued_.empty())return {};
 if(nonce_==std::numeric_limits<uint64_t>::max()||now>std::numeric_limits<uint64_t>::max()-3000){availability_=Availability::unsupported;queued_.clear();return {};}
 flight_=Request{queued_.front(),++nonce_};queued_.pop_front();deadline_=now+3000;return flight_;
}
bool Receiver::receive(std::span<const uint8_t>b,uint64_t now){
 tick(now);if(!flight_)return false;
 Reply r;try{r=decode_reply(b);}catch(const std::invalid_argument&){return false;}
 const auto expected=*flight_;
 if(r.nonce!=expected.nonce||r.room!=expected.scope.room||r.character!=expected.scope.character)return false;
 if(!r.status){
  if(r.host!=expected.scope.host)return false;
  auto checked=skills::validate(*catalog_,r.loadout,r.capacity);if(!checked||checked.used!=r.used)return false;
  auto previous=revisions_.find(r.character);
  if(previous!=revisions_.end()&&(r.revision<previous->second.revision||(r.revision==previous->second.revision&&r.loadout!=previous->second.loadout)))return false;
  // A bounded session cache cannot evict old PCs and silently forget a floor.
  if(previous==revisions_.end()&&revisions_.size()>=256){availability_=Availability::unsupported;flight_.reset();queued_.clear();return false;}
  revisions_.insert_or_assign(r.character,Revision{r.revision,r.loadout});
  ready_.push_back(Verified{expected.scope,std::move(r)});
 }
 availability_=Availability::supported;flight_.reset();deadline_=0;return true;
}
void Receiver::tick(uint64_t now){
 if(now<lastNow_||(flight_&&now>=deadline_)){availability_=Availability::unsupported;flight_.reset();queued_.clear();ready_.clear();}
 lastNow_=now;
}
std::vector<Verified> Receiver::drain(){return std::exchange(ready_,{});}
}
