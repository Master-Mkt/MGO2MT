#pragma once
#include "native_character_names.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <limits>
namespace mgo2win::names {
// Display metadata only. Never feed these strings into PS3/HOST wire records.
// A directory belongs to one authenticated connection; the current roster
// controls interest, while a fresh nonce binds each bounded batch response.
class Directory {
 mutable std::mutex mutex_;
 uint32_t self_=0;uint64_t nonce_=0,deadline_=0;
 bool unsupported_=false;
 std::set<uint32_t> wanted_,attempted_,pending_;
 std::map<uint32_t,std::string> names_;
 void expire(uint64_t now){if(nonce_&&now>=deadline_){nonce_=deadline_=0;pending_.clear();unsupported_=true;}}
public:
 void connect(uint32_t self){std::lock_guard lock(mutex_);self_=self;nonce_=deadline_=0;unsupported_=false;wanted_.clear();attempted_.clear();pending_.clear();names_.clear();if(self)wanted_.insert(self);}
 void want(std::span<const uint32_t> ids){std::lock_guard lock(mutex_);if(!self_||ids.size()>24)return;std::set<uint32_t> next{self_};for(auto id:ids)if(id)next.insert(id);wanted_=std::move(next);
  std::erase_if(names_,[&](const auto&p){return !wanted_.contains(p.first);});std::erase_if(attempted_,[&](auto id){return !wanted_.contains(id);});}
 bool needs_request(uint64_t now){std::lock_guard lock(mutex_);expire(now);if(!self_||unsupported_||nonce_)return false;return std::any_of(wanted_.begin(),wanted_.end(),[&](auto id){return !names_.contains(id)&&!attempted_.contains(id);});}
 std::optional<std::vector<uint8_t>> take(uint64_t nonce,uint64_t now){std::lock_guard lock(mutex_);expire(now);if(!self_||unsupported_||nonce_||!nonce||now>std::numeric_limits<uint64_t>::max()-3000)return {};
  std::vector<uint32_t> ids;for(auto id:wanted_)if(!names_.contains(id)&&!attempted_.contains(id)){ids.push_back(id);if(ids.size()==8)break;}if(ids.empty())return {};
  auto payload=query(Operation::ids,nonce,ids);nonce_=nonce;deadline_=now+3000;pending_={ids.begin(),ids.end()};attempted_.insert(ids.begin(),ids.end());return payload;}
 bool receive(std::span<const uint8_t> payload,uint64_t now){std::lock_guard lock(mutex_);expire(now);if(!nonce_)return false;
  try{auto reply=parse(payload);if(reply.nonce!=nonce_||reply.op!=Operation::ids||(reply.status&&payload.size()!=24))return false;
   if(reply.status){nonce_=deadline_=0;pending_.clear();unsupported_=true;return false;}
   if(!(reply.capabilities&1))return false;for(const auto&r:reply.records)if(!pending_.contains(r.id))return false;
   for(const auto&r:reply.records)if(wanted_.contains(r.id))names_[r.id]=r.name;
   nonce_=deadline_=0;pending_.clear();return true;
  }catch(const std::exception&){return false;}}
 std::string display(uint32_t id,std::string_view fallback)const{std::lock_guard lock(mutex_);auto found=names_.find(id);return found==names_.end()?std::string(fallback):found->second;}
 std::map<uint32_t,std::string> resolved()const{std::lock_guard lock(mutex_);return names_;}
};
}
