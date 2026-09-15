#include "stage_object_sync.h"
#include "stage_profiles.h"
#include <set>
#include <utility>
#include <fstream>
#include <string>
#include <algorithm>

namespace mgo2win::stage {
ObjectRegistry load_object_registry(const std::filesystem::path&path){
 std::ifstream f(path);std::string magic,profile;unsigned version=0,map=0,rule=0,count=0;
 if(!(f>>magic>>version>>profile>>map>>rule>>count)||magic!="MGO2WIN.STAGE_OBJECTS"||version!=1||map>255)throw host::Invalid(host::Error::message);
 const auto* candidate=runtime_profile(uint8_t(map));
 if(map==7&&profile=="n007a_native_lights_v1"){
  if(rule!=255||count!=15)throw host::Invalid(host::Error::message);
  ObjectRegistry out;out.map=7;out.nativeLights=true;
  for(unsigned i=0;i<count;++i){unsigned index=0,width=0;uint32_t id=0;std::string policy;
   if(!(f>>index>>id>>width>>policy)||index!=i||id!=0xb0070001u+i||width!=1||policy!="bits")throw host::Invalid(host::Error::message);
   out.entries.push_back({id,1,host::ObjectStates::Update::bits});
  }
  std::string tail;if(f>>tail)throw host::Invalid(host::Error::extent);
  out.complete=true;return out;
 }
 if(candidate&&map!=20&&map!=7&&profile==std::string(candidate->stage)+"_native_static_v1"){
  std::string tail;if(rule!=255||count!=0||(f>>tail))throw host::Invalid(host::Error::message);
  ObjectRegistry out;out.map=uint8_t(map);out.complete=true;out.nativeStatic=true;return out;
 }
 if(profile!="n022a_success32"||map!=20||rule!=1||count!=32)throw host::Invalid(host::Error::message);
 ObjectRegistry out;out.map=uint8_t(map);out.rule=uint8_t(rule);
 constexpr uint32_t ids[]={5565312,5565360,5565408,5565472,5565536,5565584,5565648,5565696,5565744,5565776,5565120,5565152,5566224,5566528,5566880,5567184,5567568};
 for(unsigned i=0;i<count;++i){unsigned index=0,width=0;uint32_t id=0;std::string policy;
  if(!(f>>index>>id>>width>>policy)||index!=i||id!=(i<17?ids[i]:0xc0000000u+i-17)||width!=(i<12?1u:i<17?8u:2u)||policy!=(i<17?"bits":"maximum"))throw host::Invalid(host::Error::message);
  out.entries.push_back({id,uint8_t(width),i<17?host::ObjectStates::Update::bits:host::ObjectStates::Update::maximum});
 }
 std::string extra;if(f>>extra)throw host::Invalid(host::Error::extent);
 out.complete=true;return out;
}
ObjectRegistry load_combat_object_registry(const std::filesystem::path&path){
 auto out=load_object_registry(path);out.rule.reset();out.combatRulesOnly=true;return out;
}
SceneAuthority::SceneAuthority(ObjectRegistry registry):registry_(std::move(registry)){
 SceneReceiver validate(registry_,0);
}
void SceneAuthority::begin(std::optional<host::LoadRequest> request){
 if(request_==request)return;
 request_.reset();values_.clear();
 if(!request||!registry_.complete||request->rotation.map!=registry_.map||(registry_.rule&&request->rotation.rule!=*registry_.rule)||((registry_.nativeStatic||registry_.nativeLights||registry_.combatRulesOnly)&&request->rotation.rule>1))return;
 request_=std::move(request);values_.resize(registry_.entries.size()); // 739300 initializes current/initial to zero.
}
std::optional<std::vector<uint8_t>> SceneAuthority::snapshot(uint8_t slot)const{
 if(slot>=24)throw host::Invalid(host::Error::identity);if(!request_)return std::nullopt;
 size_t bits=0;for(const auto&e:registry_.entries)bits+=e.width;
 std::vector<uint8_t>b(2+(bits+7)/8);b[0]=0xe0;b[1]=slot;size_t at=0;
 for(size_t i=0;i<values_.size();++i)for(unsigned j=0;j<registry_.entries[i].width;++j,++at)b[2+at/8]|=uint8_t(((values_[i]>>j)&1)<<(at%8));
 return b;
}
std::optional<std::vector<uint8_t>> SceneAuthority::update(uint32_t bindingId,uint8_t incoming){
 if(!request_)return std::nullopt;
 auto it=std::find_if(registry_.entries.begin(),registry_.entries.end(),[&](const auto&e){return e.bindingId==bindingId;});
 if(it==registry_.entries.end())throw host::Invalid(host::Error::identity);
 const size_t index=it-registry_.entries.begin();incoming&=uint8_t((1u<<it->width)-1);
 auto&v=values_[index];const auto next=it->update==host::ObjectStates::Update::maximum?std::max(v,incoming):uint8_t(v|incoming);
 if(v==next)return std::nullopt;v=next;
 std::vector<uint8_t>record{uint8_t(index)};if(it->width>1)record.push_back(v);return record;
}
SceneReceiver::SceneReceiver(ObjectRegistry registry,uint8_t localSlot):registry_(std::move(registry)),slot_(localSlot){
 if(slot_>=24||!registry_.map||registry_.entries.size()>224)throw host::Invalid(host::Error::message);
 std::set<uint32_t> bindings;
 for(const auto&e:registry_.entries){
  if(!e.bindingId||!bindings.insert(e.bindingId).second||!e.width||e.width>8||
     (e.update!=host::ObjectStates::Update::bits&&e.update!=host::ObjectStates::Update::maximum))throw host::Invalid(host::Error::message);
 }
}
void SceneReceiver::begin(std::optional<host::LoadRequest> request){
 if(request_==request)return;
 request_=std::move(request);states_.reset();snapshot_.reset();++revision_;
 if(!request_||!registry_.complete||request_->rotation.map!=registry_.map||(registry_.rule&&request_->rotation.rule!=*registry_.rule)||((registry_.nativeStatic||registry_.nativeLights||registry_.combatRulesOnly)&&request_->rotation.rule>1))return;
 std::vector<uint8_t> widths;std::vector<host::ObjectStates::Update> updates;
 for(const auto&e:registry_.entries){widths.push_back(e.width);updates.push_back(e.update);}
 states_.emplace(slot_,std::move(widths),std::move(updates));
}
bool SceneReceiver::receive(const host::LoadRequest&request,std::span<const uint8_t>record){
 if(!request_||request!=*request_||!states_)return false;
 const bool changed=states_->receive(record);
 if(!states_->complete()||(!changed&&snapshot_))return false;
 SceneSnapshot next{*request_,revision_+1,{}};next.objects.reserve(registry_.entries.size());
 const auto&current=states_->values();
 for(size_t i=0;i<registry_.entries.size();++i){
  const auto initial=snapshot_?snapshot_->objects[i].initial:current[i];
  next.objects.push_back({registry_.entries[i].bindingId,current[i],initial});
 }
 snapshot_=std::move(next);++revision_;return true;
}
std::optional<std::array<uint8_t,2>> SceneReceiver::snapshot_request()const{
 return states_?states_->snapshot_request():std::nullopt;
}
SceneSyncStatus SceneReceiver::status()const{
 if(!request_)return SceneSyncStatus::idle;
 if(!states_)return SceneSyncStatus::unverified_registry;
 return snapshot_?SceneSyncStatus::ready:SceneSyncStatus::waiting_snapshot;
}
}
