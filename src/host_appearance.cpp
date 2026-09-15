#include "host_appearance.h"
#include <algorithm>
#include <set>
namespace mgo2win::host {
namespace {
void put(std::vector<uint8_t>&b,uint32_t n,unsigned width){while(width--){b.push_back(uint8_t(n));n>>=8;}}
uint32_t read(std::span<const uint8_t>b,size_t at,unsigned width){uint32_t n=0;for(unsigned i=0;i<width;++i)n|=uint32_t(b[at+i])<<(i*8);return n;}
bool valid(const std::array<uint8_t,28>&a){return a[0]<=1&&std::all_of(a.begin()+9,a.begin()+13,[](uint8_t x){return x==0;})&&a[27]==0;}
}
std::vector<uint8_t> appearance_record(std::span<const Player> players){
 if(players.size()>24)throw Invalid(Error::extent);
 std::vector<uint8_t>b{appearance_opcode,'G','W','A','V',2,uint8_t(players.size())};std::set<uint8_t> slots;std::set<uint32_t> characters;
 for(const auto&p:players){if(p.slot>=24||!p.instance||!p.character||!p.appearance||!valid(*p.appearance)||!slots.insert(p.slot).second||!characters.insert(p.character).second)throw Invalid(Error::identity);
  b.push_back(p.slot);put(b,p.instance,2);put(b,p.character,4);b.insert(b.end(),p.appearance->begin(),p.appearance->end());put(b,p.level?unsigned(*p.level):256,2);}
 return b;
}
void update_appearance(Roster&r,std::span<const uint8_t>b){
 if(b.size()<7||b[0]!=appearance_opcode||b[1]!='G'||b[2]!='W'||b[3]!='A'||b[4]!='V'||(b[5]!=1&&b[5]!=2)||b[6]>24)throw Invalid(Error::extent);
 const size_t stride=b[5]==1?35:37;if(b.size()!=7+size_t(b[6])*stride)throw Invalid(Error::extent);
 auto next=r;std::set<uint8_t> slots;bool changed=false;
 for(size_t at=7;at<b.size();at+=stride){auto slot=b[at];if(slot>=24||!slots.insert(slot).second)throw Invalid(Error::identity);
  auto&p=next.slots[slot];if(!p||p->instance!=read(b,at+1,2)||p->character!=read(b,at+3,4))throw Invalid(Error::identity);
  std::array<uint8_t,28>a;std::copy_n(b.begin()+at+7,28,a.begin());if(!valid(a))throw Invalid(Error::identity);
  std::optional<uint8_t> level;
  if(b[5]==2){auto value=read(b,at+35,2);if(value>256)throw Invalid(Error::identity);if(value<256)level=uint8_t(value);}
  if(p->appearance!=a||p->level!=level){p->appearance=a;p->level=level;changed=true;}}
 if(changed){++next.revision;r=std::move(next);}
}
}
