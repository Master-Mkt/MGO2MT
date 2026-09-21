#include "tournament_invitation.h"
#include "native_character_names.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace mgo2mt::invitations {
namespace {
[[noreturn]]void invalid(){throw std::invalid_argument("invalid invitation payload");}
uint32_t u(std::span<const uint8_t>b,size_t at,unsigned n){uint32_t v=0;for(unsigned i=0;i<n;++i)v=(v<<8)|b[at+i];return v;}
uint64_t deadline(uint64_t now,uint64_t delta){return now>UINT64_MAX-delta?UINT64_MAX:now+delta;}
std::string name(std::span<const uint8_t>b,chat::Encoding encoding){
 auto zero=std::find(b.begin(),b.end(),0);size_t n=size_t(zero-b.begin());
 // All 16 bytes can be occupied in the original fixed string; bytes after NUL
 // must be padding, never an invisible second identity or trailing instruction.
 if(zero!=b.end()&&std::any_of(zero,b.end(),[](uint8_t c){return c!=0;}))invalid();
 std::string s;for(size_t i=0;i<n;++i){auto c=b[i];if(encoding==chat::Encoding::latin1&&c>=128){s.push_back(char(0xc0|(c>>6)));s.push_back(char(0x80|(c&63)));}else s.push_back(char(c));}
 if(!names::detail::display_name(s))invalid();
 // Preserve Japanese/Unicode without permitting display-direction controls.
 for(auto bad:{"\xe2\x80\xaa","\xe2\x80\xab","\xe2\x80\xac","\xe2\x80\xad","\xe2\x80\xae","\xe2\x81\xa6","\xe2\x81\xa7","\xe2\x81\xa8","\xe2\x81\xa9"})if(s.find(bad)!=s.npos)invalid();
 return s;
}
}
Notification parse_notification(std::span<const uint8_t>b,chat::Encoding encoding){
 if(b.size()!=32||(encoding!=chat::Encoding::utf8&&encoding!=chat::Encoding::latin1))invalid();
 Notification n;n.lobby=uint16_t(u(b,0,2));n.id=u(b,2,4);n.serverTime=u(b,6,4);n.state=b[10];n.kind=b[11];n.opaque=u(b,12,4);
 if(!n.id||n.state>4)invalid();n.name=name(b.subspan(16,16),encoding);return n;
}
AnswerReply parse_answer_reply(std::span<const uint8_t>b){
 if(b.size()!=9)invalid();AnswerReply r{u(b,0,4),u(b,4,4),b[8]};if(r.state>4||(!r.result&&(!r.id||(r.state!=2&&r.state!=4))))invalid();return r;
}
std::array<uint8_t,5> answer_payload(uint32_t id,bool accept){
 if(!id)invalid();return {uint8_t(id>>24),uint8_t(id>>16),uint8_t(id>>8),uint8_t(id),uint8_t(accept?2:4)};
}
std::string marquee_utf8(const Notification& n){
 return n.name+"さんから"+(n.kind==4?"サバイバルロビー":"トーナメント登録ロビー")+"（"+std::to_string(n.lobby)+"）に招待されています。START メニューから承諾・辞退を選択できます。";
}
Session::Session(Policy p):policy_(std::move(p)){
 if(!policy_.ttlMs||policy_.ttlMs>300000||!policy_.answerTimeoutMs||policy_.answerTimeoutMs>30000)invalid();
}
void Session::bind(uint64_t scope,uint32_t self){std::lock_guard lock(mutex_);if(scope_==scope&&self_==self)return;scope_=scope;self_=self;clock_=pendingAt_=0;pendingId_=0;entries_.clear();seen_.clear();}
void Session::tick_locked(uint64_t now){
 // UI/network calls can arrive out of order; never regress the shared clock or
 // extend an existing deadline using an older caller timestamp.
 clock_=(std::max)(clock_,now);
 for(auto&e:entries_){
  if(e.state==State::sending&&clock_>=deadline(pendingAt_,policy_.answerTimeoutMs)){e.state=State::outcome_unknown;e.respondable=false;pendingId_=0;}
  else if((e.state==State::pending||e.state==State::queued)&&clock_>=e.expiresAt){if(e.state==State::queued)pendingId_=0;e.state=State::expired;e.respondable=false;}
 }
}
void Session::tick(uint64_t now){std::lock_guard lock(mutex_);tick_locked(now);}
std::vector<Entry> Session::view(uint64_t now){std::lock_guard lock(mutex_);tick_locked(now);auto result=entries_;if(pendingId_)for(auto&e:result)e.respondable=false;return result;}
bool Session::receive_notification(uint64_t scope,std::span<const uint8_t>b,chat::Encoding encoding,uint64_t now){
 Notification n;try{n=parse_notification(b,encoding);}catch(const std::exception&){return false;}
 std::lock_guard lock(mutex_);if(!scope_||!self_||scope!=scope_)return false;tick_locked(now);
 if(n.state!=1||seen_.contains(n.id)||seen_.size()>=512)return false;
 // Three simultaneous invitations, matching the original bounded cache. Keep
 // an outstanding answer; evict only a terminal entry. No expiry refresh on replay.
 if(entries_.size()==3){auto at=std::find_if(entries_.begin(),entries_.end(),[](const Entry&e){return e.state!=State::pending&&e.state!=State::queued&&e.state!=State::sending;});if(at==entries_.end())return false;entries_.erase(at);}
 const bool can=policy_.allowReceivedKinds||std::find(policy_.respondableKinds.begin(),policy_.respondableKinds.end(),n.kind)!=policy_.respondableKinds.end();
 seen_.insert(n.id);entries_.push_back({std::move(n),scope_,clock_,deadline(clock_,policy_.ttlMs),State::pending,can,0});return true;
}
std::optional<std::array<uint8_t,5>> Session::prepare_locked(uint64_t scope,uint32_t id,bool accept,uint64_t now,bool queued){
 if(!scope_||!self_||scope!=scope_)return {};tick_locked(now);if(pendingId_)return {};
 auto at=std::find_if(entries_.begin(),entries_.end(),[&](const Entry&e){return e.notification.id==id;});
 if(at==entries_.end()||at->state!=State::pending||!at->respondable)return {};
 at->state=queued?State::queued:State::sending;at->respondable=false;pendingId_=id;pendingAccept_=accept;pendingAt_=queued?0:clock_;return answer_payload(id,accept);
}
std::optional<std::array<uint8_t,5>> Session::answer(uint64_t scope,uint32_t id,bool accept,uint64_t now){std::lock_guard lock(mutex_);return prepare_locked(scope,id,accept,now,false);}
bool Session::submit(uint64_t scope,uint32_t id,bool accept,uint64_t now){std::lock_guard lock(mutex_);return prepare_locked(scope,id,accept,now,true).has_value();}
std::optional<std::array<uint8_t,5>> Session::take(uint64_t now){
 std::lock_guard lock(mutex_);tick_locked(now);if(!pendingId_)return {};
 for(auto&e:entries_)if(e.notification.id==pendingId_&&e.state==State::queued){e.state=State::sending;pendingAt_=clock_;return answer_payload(pendingId_,pendingAccept_);}return {};
}
bool Session::receive_answer(uint64_t scope,std::span<const uint8_t>b,uint64_t now){
 AnswerReply r;try{r=parse_answer_reply(b);}catch(const std::exception&){return false;}
 std::lock_guard lock(mutex_);if(!scope_||scope!=scope_)return false;tick_locked(now);
 if(!pendingId_||r.id!=pendingId_||(!r.result&&r.state!=(pendingAccept_?2:4)))return false;
 auto at=std::find_if(entries_.begin(),entries_.end(),[&](const Entry&e){return e.notification.id==pendingId_&&e.state==State::sending;});if(at==entries_.end())return false;
 at->result=r.result;at->state=r.result?State::rejected:pendingAccept_?State::accepted:State::declined;pendingId_=0;
 // Retail success marks the other invitations state4. Keep the native history
 // explicit; this local cancellation never sends additional responses.
 if(!r.result)for(auto&e:entries_)if(&e!=&*at&&e.state==State::pending){e.state=State::expired;e.respondable=false;}
 return true;
}
void Session::submission_unknown(uint64_t scope,uint32_t id){std::lock_guard lock(mutex_);if(scope!=scope_||id!=pendingId_)return;for(auto&e:entries_)if(e.notification.id==id&&(e.state==State::queued||e.state==State::sending)){e.state=State::outcome_unknown;e.respondable=false;}pendingId_=0;}
}
