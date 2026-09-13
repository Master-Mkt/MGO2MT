#include "chat_session.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace mgo2win::chat {
namespace {bool ascii(std::string_view s){return std::none_of(s.begin(),s.end(),[](unsigned char c){return c>=128;});}}
void Session::connect(uint32_t self,EndpointProfile profile){std::lock_guard lock(mutex_);const auto generation=state_.generation+1;state_={};state_.generation=generation;state_.self=self;endpointEncoding_=self&&profile==EndpointProfile::nomad_jp?std::optional{Encoding::utf8}:std::nullopt;members_.clear();queued_.reset();flight_.reset();deadline_=lastSend_=lastNow_=sequence_=0;everSent_=false;capNonce_=capDeadline_=0;capRequested_=false;}
void Session::capabilities(Encoding encoding,bool team){std::lock_guard lock(mutex_);state_.encoding=encoding;state_.teamSupported=team;}
std::optional<std::vector<uint8_t>> Session::request_capability(uint64_t nonce,uint64_t now){
 std::lock_guard lock(mutex_);if(capRequested_||!state_.joined||!state_.room||!state_.self||!nonce||now>std::numeric_limits<uint64_t>::max()-3000)return {};
 auto payload=capability_payload(nonce,state_.room,state_.self);capRequested_=true;capNonce_=nonce;capDeadline_=now+3000;return payload;
}
bool Session::receive_capability(std::span<const uint8_t> payload,uint64_t now){
 std::lock_guard lock(mutex_);if(!capRequested_||!capNonce_||now>=capDeadline_)return false;
 try{auto c=capability_reply(payload);if(c.nonce!=capNonce_||c.room!=state_.room||c.character!=state_.self)return false;
 capNonce_=0;if(c.status||!(c.flags&1))return false;
 // An explicit server reply supersedes the known deployment profile. Never
 // send already encoded queued text under a different charset policy.
 if(state_.encoding&&state_.encoding!=c.encoding&&(queued_||flight_)){queued_.reset();flight_.reset();deadline_=0;state_.delivery=Delivery::unconfirmed;}
 state_.encoding=c.encoding;endpointEncoding_=c.encoding;state_.teamSupported=(c.flags&2)!=0;return true;
 }catch(const std::exception&){return false;}
}
void Session::enter(uint32_t room){std::lock_guard lock(mutex_);++state_.generation;state_.room=room;state_.joined=false;state_.encoding=room?endpointEncoding_:std::nullopt;state_.teamSupported=false;capNonce_=capDeadline_=0;capRequested_=false;state_.lines.clear();state_.delivery=Delivery::none;state_.submittedText.clear();members_.clear();queued_.reset();flight_.reset();deadline_=0;}
void Session::roster(std::vector<Member> members,bool joined){
 std::lock_guard lock(mutex_);auto invalid=[&]{members_.clear();state_.joined=false;queued_.reset();flight_.reset();state_.delivery=Delivery::none;};
 if(!state_.room||!state_.self||members.size()>24){invalid();return;}
 std::map<uint32_t,Member> checked;for(auto&m:members)if(!m.character||m.name.empty()||m.name.size()>128||m.team>2||!checked.emplace(m.character,std::move(m)).second){invalid();return;}
 members_=std::move(checked);state_.joined=joined&&members_.contains(state_.self);if(!state_.joined){queued_.reset();flight_.reset();state_.delivery=Delivery::none;}
}
void Session::leave(){enter(0);}
void Session::display_name(uint32_t character,std::string name){std::lock_guard lock(mutex_);auto found=members_.find(character);if(found==members_.end()||name.empty()||name.size()>64)return;found->second.name=name;for(auto&line:state_.lines)if(line.character==character)line.name=name;}
void Session::disconnect(){connect(0);}
State Session::state()const{std::lock_guard lock(mutex_);return state_;}
void Session::expire(uint64_t now){if(now<lastNow_||(flight_&&now>=deadline_)){queued_.reset();flight_.reset();if(state_.delivery==Delivery::queued||state_.delivery==Delivery::awaiting_echo)state_.delivery=Delivery::unconfirmed;}lastNow_=now;}
Submit Session::submit(uint64_t generation,std::string text,bool team,uint64_t now){
 std::lock_guard lock(mutex_);expire(now);
 if(generation!=state_.generation||!state_.joined||!state_.room||!state_.self)return Submit::not_joined;
 if(queued_||flight_||(everSent_&&(now<lastSend_||now-lastSend_<750)))return Submit::busy;
 if(team&&(!state_.teamSupported||!members_.contains(state_.self)||!members_.at(state_.self).team))return Submit::unsupported_team;
 if(!state_.encoding&&!ascii(text))return Submit::unknown_encoding;
 auto first=text.find_first_not_of(" ");if(first==std::string::npos)return Submit::invalid_text;
 if(text[first]=='/')return Submit::command_disabled;
 std::vector<uint8_t> payload;try{payload=room_payload(text,state_.encoding.value_or(Encoding::utf8));}catch(const std::exception&){return Submit::invalid_text;}
 if(team){payload[0]=1;payload[1]='1';}
 if(state_.serial==std::numeric_limits<uint64_t>::max())return Submit::busy;
 queued_=Send{generation,++state_.serial,state_.room,state_.self,text,uint8_t(team?1:0),std::move(payload)};state_.submittedText=std::move(text);state_.delivery=Delivery::queued;return Submit::accepted;
}
std::optional<Send> Session::take(uint64_t now){std::lock_guard lock(mutex_);expire(now);if(!queued_||flight_||!state_.joined)return {};if(now>std::numeric_limits<uint64_t>::max()-5000){queued_.reset();state_.delivery=Delivery::unconfirmed;return {};}flight_=std::move(queued_);queued_.reset();lastSend_=now;everSent_=true;deadline_=now+5000;state_.delivery=Delivery::awaiting_echo;return flight_;}
bool Session::receive(const Message&m,uint64_t now){
 std::lock_guard lock(mutex_);expire(now);if(!state_.joined||!m.character||m.text.empty()||m.text.size()>4096)return false;
 if(!state_.encoding&&!ascii(m.text))return false;
 auto member=members_.find(m.character);
 if(member==members_.end())return false;
 if(m.mode>4)return false;{if(m.mode==1){auto own=members_.find(state_.self);if(own==members_.end()||!own->second.team||own->second.team!=member->second.team)return false;}}
 if(flight_&&m.character==state_.self&&m.mode==flight_->mode&&m.text==flight_->text){state_.delivery=Delivery::echo_received;flight_.reset();deadline_=0;}
 state_.lines.push_back({++sequence_,now,m.character,m.mode,member->second.name,m.text});if(state_.lines.size()>64)state_.lines.erase(state_.lines.begin());return true;
}
void Session::failed(uint64_t serial){std::lock_guard lock(mutex_);if(state_.serial!=serial)return;queued_.reset();flight_.reset();state_.delivery=Delivery::unconfirmed;}
void Session::tick(uint64_t now){std::lock_guard lock(mutex_);expire(now);}
bool Session::receive_radio(uint32_t character,std::string text,uint64_t now){
 std::lock_guard lock(mutex_);auto member=members_.find(character);
 if(!state_.joined||member==members_.end()||text.empty()||text.size()>512)return false;
 state_.lines.push_back({++sequence_,now,character,0,member->second.name,std::move(text),true});
 if(state_.lines.size()>64)state_.lines.erase(state_.lines.begin());return true;
}
}
