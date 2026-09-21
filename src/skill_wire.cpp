#include "skill_wire.h"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace mgo2mt::skills {
namespace {void put(std::vector<uint8_t>&b,uint32_t v,unsigned n){while(n)b.push_back(uint8_t(v>>(8*--n)));}uint32_t get(std::span<const uint8_t>b,size_t i,unsigned n){if(i+n>b.size())throw std::runtime_error("skill reply extent");uint32_t v=0;while(n--)v=(v<<8)|b[i++];return v;}}
std::vector<uint8_t> remote_payload(const RemoteRequest&r){
 if(!r.token||r.set>3||r.loadout.entries.size()>8)throw std::runtime_error("skill request");
 std::vector<uint8_t>b;put(b,0x4757534b,4);put(b,1,1);put(b,r.set,1);put(b,0,2);put(b,r.token,4);
 if(r.write){put(b,r.revision,4);put(b,uint32_t(r.loadout.entries.size()),1);put(b,0,3);std::set<uint16_t>ids;for(auto&e:r.loadout.entries){if(!e.id||e.id>255||!e.level||!ids.insert(e.id).second)throw std::runtime_error("skill selection");put(b,e.id,2);put(b,e.level,1);put(b,0,1);}}
 return b;
}
RemoteProfile remote_reply(std::span<const uint8_t>b){
 if(b.size()<24||get(b,0,4)!=0x4757534b||b[4]!=1||b[5]>6||b[6]>3||b[17]>8||get(b,18,2)||b.size()!=24+size_t(b[17])*4)throw std::runtime_error("skill reply");
 RemoteProfile p;p.status=b[5];p.set=b[6];p.capacity=b[7];p.token=get(b,8,4);p.revision=get(b,12,4);p.used=b[16];p.character=get(b,20,4);
 if(!p.status&&(!p.character||p.capacity<4||p.capacity>8||p.used>p.capacity))throw std::runtime_error("skill authority");
 std::set<uint16_t> ids;for(size_t i=24;i<b.size();i+=4){Choice c{uint16_t(get(b,i,2)),b[i+2]};if(!c.id||c.id>255||!c.level||b[i+3]||!ids.insert(c.id).second)throw std::runtime_error("skill reply entry");p.loadout.entries.push_back(c);}return p;
}
void Remote::character(uint32_t id){std::lock_guard lock(mutex_);if(state_.character==id)return;auto serial=state_.serial+1;state_={};state_.character=id;state_.serial=serial;queued_.reset();flight_.reset();}
void Remote::connected(){std::lock_guard lock(mutex_);flight_.reset();queued_.reset();state_.profile.reset();if(!state_.character)return;queued_=RemoteRequest{++token_};state_.status=RemoteStatus::reading;++state_.serial;}
void Remote::disconnected(){std::lock_guard lock(mutex_);queued_.reset();flight_.reset();state_.status=RemoteStatus::offline;++state_.serial;}
bool Remote::fetch(){std::lock_guard lock(mutex_);if(queued_||flight_||state_.status==RemoteStatus::offline)return false;queued_=RemoteRequest{++token_};state_.status=RemoteStatus::reading;++state_.serial;return true;}
bool Remote::save(const Loadout&l){std::lock_guard lock(mutex_);if(queued_||flight_||!state_.profile||state_.status!=RemoteStatus::ready||l.entries.size()>8)return false;queued_=RemoteRequest{++token_,state_.profile->revision,0,true,l};state_.status=RemoteStatus::saving;++state_.serial;return true;}
RemoteState Remote::state()const{std::lock_guard lock(mutex_);return state_;}
std::optional<RemoteRequest> Remote::take(uint64_t now){std::lock_guard lock(mutex_);if(flight_||!queued_)return {};flight_=std::move(queued_);queued_.reset();deadline_=now+3000;return flight_;}
void Remote::receive(const RemoteProfile&p,bool write){std::lock_guard lock(mutex_);if(!flight_||p.token!=flight_->token||p.set!=flight_->set||write!=flight_->write)return;
 if(p.character&&p.character!=state_.character)return;flight_.reset();state_.error=p.status;
 if(!p.status){state_.profile=p;state_.status=RemoteStatus::ready;}else{state_.status=p.status==4?RemoteStatus::conflict:RemoteStatus::rejected;}++state_.serial;
}
void Remote::tick(uint64_t now){std::lock_guard lock(mutex_);if(flight_&&now>=deadline_){state_.status=flight_->write?RemoteStatus::outcome_unknown:RemoteStatus::unsupported;flight_.reset();++state_.serial;}}
bool Remote::in_flight()const{std::lock_guard lock(mutex_);return bool(flight_||queued_);}
}
