#include "host_match.h"
#include <algorithm>
namespace mgo2win::host {
namespace {
struct Delta {
 std::optional<uint8_t> phase,index,round,generation;
 std::optional<std::array<Rotation,16>> rotations;
};
Delta decode_delta(std::span<const uint8_t>b){
 if(b.size()<2||b[0]!=11||b[1]>25)throw Invalid(Error::message);
 Delta d;if(b[1]!=0)return d;
 // 1229B80's 55 descriptors -> 27E8B0/27EC48/27E9F0.
 // Group 16 spans offset 0x5A..0x5D (size 4); its count does not expand
 // the group interval. Children 17/18/19 consume 1+1+2, not 8 bytes.
 constexpr std::array<unsigned,55> lengths={1,16,48,1,2,1,1,4,2,1,1,2,2,1,1,8,4,1,1,2,1,1,2,1,1,2,1,1,34,34,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,2,1,1};
 if(b.size()<9||(b[8]&0x80))throw Invalid(Error::extent);
 size_t at=9;for(unsigned i=0;i<55;++i)if(b[2+i/8]&(1u<<(i%8))){
  auto n=lengths[i];if(n>b.size()-at)throw Invalid(Error::extent);
  switch(i){
   case 0:d.phase=b[at];break;
   case 2:{std::array<Rotation,16>r;for(size_t j=0;j<16;++j)r[j]={b[at+j*3],b[at+j*3+1],b[at+j*3+2]};d.rotations=r;break;}
   case 9:if(b[at]>=16)throw Invalid(Error::identity);d.index=b[at];break;
   case 13:d.round=b[at];break;
   case 51:d.generation=b[at];break;
  }
  at+=n;
 }
 if(at!=b.size())throw Invalid(Error::extent);return d;
}
}
std::optional<uint8_t> global_generation(std::span<const uint8_t>b){return decode_delta(b).generation;}
std::optional<uint8_t> update_match(MatchState&s,std::span<const uint8_t>b){
 auto d=decode_delta(b);auto next=s;
 if(d.phase)next.phase=d.phase;
 if(d.rotations){next.rotations=*d.rotations;next.rotations_known=true;}
 if(d.index){next.index=d.index;next.selection_dirty=true;}
 if(d.round){next.round=d.round;next.round_dirty=true;}
 // 77E710/77F7E0 distinguish map selection (0x52) and round (0x58)
 // updates when the synchronization generation (0xA1) is announced.
 if(d.generation&&d.generation!=s.generation){
  next.pending_transition=!s.generation?MatchTransition::initial:next.selection_dirty?(next.round_dirty?MatchTransition::map_and_round_change:MatchTransition::map_change):next.round_dirty?MatchTransition::next_round:MatchTransition::round_restart;
  next.generation=d.generation;next.selection_dirty=false;next.round_dirty=false;
  next.pending=true;next.request.reset();++next.sequence;
 }
 // 77BC88 passes selected rule/map and the prior latched rule/map (7500B0) to a GCL procedure. This native
 // boundary deliberately stops before claiming any stage resources are loaded.
 if(next.pending&&next.rotations_known&&next.index&&next.round&&next.generation){
  next.request=LoadRequest{next.sequence,*next.generation,*next.index,*next.round,next.rotations[*next.index],next.pending_transition};next.pending=false;next.selection_dirty=false;next.round_dirty=false;
 }
 if(next.phase!=s.phase||next.request!=s.request||next.pending!=s.pending)++next.revision;
 s=std::move(next);return d.generation;
}
}
