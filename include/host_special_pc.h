#pragma once
#include "combat_authority.h"
#include <mutex>
#include <algorithm>
#include <vector>
namespace mgo2win::special_pc {
struct Config {bool allow=false,showNames=true,random=true;bool operator==(const Config&)const=default;};
struct Assignment {combat::Identity id;uint32_t life=0;Kind desired=Kind::human,current=Kind::human;combat::Reject result=combat::Reject::none;bool operator==(const Assignment&)const=default;};
struct View {uint64_t epoch=0,revision=0;Config config;std::vector<Assignment> players;};
// In-process HOST operator channel only. No lobby/database/original unique-PC
// flags are changed and there is no client command for configuring these values.
class Control {
 mutable std::mutex mutex_;Config config_;uint64_t revision_=1,epoch_=0;
 std::vector<Assignment> players_;std::map<uint32_t,std::pair<combat::Identity,Kind>> direct_;
 struct Attempt {uint64_t epoch=0,revision=0;combat::Identity id;uint32_t life=0;Kind desired=Kind::human;bool names=true;combat::Reject result=combat::Reject::none;};
 std::map<uint32_t,Attempt> attempted_;
 std::vector<combat::Identity> roster_;std::optional<combat::Identity> randomWinner_;
public:
 View state()const{std::lock_guard lock(mutex_);return {epoch_,revision_,config_,players_};}
 void configure(Config config){std::lock_guard lock(mutex_);if(config_!=config){config_=config;++revision_;}}
 void reset(){std::lock_guard lock(mutex_);epoch_=0;players_.clear();direct_.clear();attempted_.clear();roster_.clear();randomWinner_.reset();++revision_;}
 void roster(std::span<const combat::Identity> ids){std::lock_guard lock(mutex_);roster_.assign(ids.begin(),ids.end());std::erase_if(direct_,[&](const auto& p){return std::find(roster_.begin(),roster_.end(),p.second.first)==roster_.end();});if(randomWinner_&&std::find(roster_.begin(),roster_.end(),*randomWinner_)==roster_.end())randomWinner_.reset();}
 bool assign(uint64_t epoch,combat::Identity id,Kind kind){std::lock_guard lock(mutex_);if(!epoch||epoch!=epoch_||!valid(kind)||(!config_.allow&&kind!=Kind::human))return false;auto found=std::find_if(players_.begin(),players_.end(),[&](const auto&p){return p.id==id;});if(found==players_.end())return false;config_.random=false;direct_[id.character]={id,kind};++revision_;return true;}
 // Called only by the dedicated simulation owner. Failed transformations are
 // not retried until an operator revision/new life; space clearing is not an ACK.
 bool apply(combat::Authority&,uint64_t now);
};
}
