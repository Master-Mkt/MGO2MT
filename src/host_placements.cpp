#include "host_placements.h"
#include <algorithm>
#include <bit>
namespace mgo2win::host {
static uint16_t le(std::span<const uint8_t>b,size_t at){return uint16_t(b[at]|unsigned(b[at+1])<<8);}
std::array<float,3> ItemPlacement::position()const{return {coordinates[0]*10.f,coordinates[1]*1.f,coordinates[2]*10.f};}
std::array<float,3> ItemPlacement::degrees()const{return {angles[0]*(360.f/256),angles[1]*(360.f/256),angles[2]*(360.f/256)};}
void PlacementReceiver::clear(){state_={};assembling_={};next_=0;}
void PlacementReceiver::begin(uint8_t generation){if(state_.generation==generation)return;auto revision=state_.revision+1;clear();state_.generation=generation;state_.revision=revision;}
void PlacementReceiver::receive(uint8_t generation,std::span<const uint8_t>b){
 if(state_.generation!=generation)return;
 constexpr size_t lengths[]={6,6,10,9};
 auto invalid=[&](){next_=0;assembling_={};state_.partial=false;throw Invalid(Error::message);};
 if(b.empty()||b[0]>3||b.size()!=lengths[b[0]])invalid();
 if(b[0]==0){
  assembling_={};assembling_.operation=b[1]&31;assembling_.state=b[1]>>5;assembling_.id=le(b,2);assembling_.packedOwner=le(b,4);
  // 72F618 also replays unused slots (zero ID, operation and state).
  if((!assembling_.id&&(assembling_.operation||assembling_.state))||(assembling_.operation&15)>5)invalid();
  next_=1;state_.partial=true;return;
 }
 if(b[0]!=next_)invalid();
 if(next_==1){assembling_.type=b[1];assembling_.variant=b[2];assembling_.quantity=le(b,3);assembling_.parameter=b[5];
  if((assembling_.operation&15)>3){next_=2;return;}
 }else if(next_==2){for(unsigned i=0;i<3;++i){assembling_.coordinates[i]=std::bit_cast<int16_t>(le(b,1+2*i));assembling_.angles[i]=std::bit_cast<int8_t>(b[7+i]);}assembling_.transform=true;next_=3;return;}
 else std::copy(b.begin()+1,b.end(),assembling_.auxiliary.begin());
 auto item=assembling_;next_=0;assembling_={};state_.partial=false;
 if(!item.id)return;
 auto found=state_.items.find(item.id);const unsigned op=item.operation&15;
 // 72E078: operation 2/state 5 remove; operation 1 marks an existing item
 // consumed. This map retains scene placements, not player inventory state.
 if(op==1||op==2||item.state==5||item.state==0){if(found!=state_.items.end()){state_.items.erase(found);++state_.revision;}return;}
 if(found==state_.items.end()&&(op==1||!item.transform))return;
 if(!item.transform){item.coordinates=found->second.coordinates;item.angles=found->second.angles;item.auxiliary=found->second.auxiliary;item.transform=true;}
 if(found!=state_.items.end()&&found->second==item)return;
 if(found==state_.items.end()&&state_.items.size()>=12)throw Invalid(Error::extent);
 state_.items.insert_or_assign(item.id,item);++state_.revision;
}
ObjectStates::ObjectStates(uint8_t localSlot,std::vector<uint8_t> widths,std::vector<Update> updates):widths_(std::move(widths)),values_(widths_.size()),pending_(widths_.size()),updates_(std::move(updates)),slot_(localSlot){
 if(slot_>=24||widths_.size()>224||std::any_of(widths_.begin(),widths_.end(),[](uint8_t n){return n==0||n>8;}))throw Invalid(Error::message);
 if(!updates_.empty()&&updates_.size()!=widths_.size())throw Invalid(Error::extent);
 if(std::any_of(updates_.begin(),updates_.end(),[](Update u){return u!=Update::bits&&u!=Update::maximum;}))throw Invalid(Error::message);
}
bool ObjectStates::receive(std::span<const uint8_t>b){
 if(b.empty())throw Invalid(Error::extent);
 if(b[0]==0xe0){
  if(b.size()<2)throw Invalid(Error::extent);if(b[1]!=slot_||complete_)return false;
  size_t bits=0;for(auto n:widths_)bits+=n;
  if(b.size()!=2+(bits+7)/8)throw Invalid(Error::extent);
  auto next=values_;size_t at=0;
  for(size_t i=0;i<next.size();++i){unsigned v=0;for(unsigned j=0;j<widths_[i];++j,++at)v|=((b[2+at/8]>>(at%8))&1)<<j;next[i]=uint8_t(v);}
  // Publish only after the complete addressed snapshot is decoded. Initial
  // application is assignment, unlike the later per-object OR callback.
  initial_=next;
  // The snapshot and ordinary deltas use distinct registered channels.
  // A monotone OR/max update received first must not be erased by an older
  // E0 image. Keep those updates private until the full baseline is valid.
  for(size_t i=0;i<next.size();++i){if(!updates_.empty()&&updates_[i]==Update::maximum)next[i]=std::max(next[i],pending_[i]);else next[i]|=pending_[i];}
  values_=std::move(next);pending_.clear();complete_=true;return true;
 }
 if(b[0]>=widths_.size())throw Invalid(Error::message);
 auto width=widths_[b[0]];if(b.size()!=(width==1?1:2))throw Invalid(Error::extent);
 // 737860 OR and 737888 maximum callbacks both exist. Require the reviewed
 // concrete actor policy for multibit deltas instead of generalizing OR.
 if(width>1&&updates_.empty())throw Invalid(Error::message);
 auto&v=(complete_?values_:pending_)[b[0]];auto old=v;auto incoming=uint8_t((width==1?1:b[1])&((1u<<width)-1));
 if(!updates_.empty()&&updates_[b[0]]==Update::maximum)v=std::max(v,incoming);else v|=incoming;return complete_&&old!=v;
}
}
