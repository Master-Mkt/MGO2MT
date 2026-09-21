#include <windows.h>
#include "host_roster.h"
#include <algorithm>
namespace mgo2mt::host {
namespace {
uint32_t read(std::span<const uint8_t>b,size_t at,unsigned n){if(at>b.size()||n>b.size()-at)throw Invalid(Error::extent);uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(b[at+i])<<(8*i);return v;}
std::string label(std::span<const uint8_t>b,bool required){
 if(b.size()>23||(required&&b.empty()))throw Invalid(Error::identity);
 if(!b.empty()&&!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,reinterpret_cast<const char*>(b.data()),int(b.size()),nullptr,0))throw Invalid(Error::message);
 for(auto c:b)if(c<32||c==127)throw Invalid(Error::message);
 return {b.begin(),b.end()};
}
}
size_t Roster::count()const{return std::count_if(slots.begin(),slots.end(),[](const auto&p){return p.has_value();});}
bool update_roster(Roster&r,std::span<const uint8_t>b){
 // ELF 281AF8 dispatch -> table 1191B60: writer 275A90, reader 275718,
 // removal 275310 -> 26FC80. Header: length16, class8, instance16, flag8.
 if(b.size()<7||b[0]!=7||b[3]>31||b[6]>3)throw Invalid(Error::message);
 auto length=read(b,1,2);auto instance=uint16_t(read(b,4,2));auto flag=b[6];
 if(flag==0){if(length!=b.size()-7)throw Invalid(Error::extent);}
 else if(flag==3){if(length||b.size()!=7)throw Invalid(Error::extent);}
 else if(b.size()!=9)throw Invalid(Error::extent);
 if(b[3]!=0)return false;
 if(flag==3){if(!r.complete){r.complete=true;++r.revision;}return true;}
 if(flag==1||flag==2){
  // Original callback resolves the full instance, not its low-byte slot.
  for(auto&p:r.slots)if(p&&p->instance==instance){p.reset();++r.revision;break;}
  return true;
 }
 auto body=b.subspan(7);if(body.size()<18)throw Invalid(Error::extent);
 Player p;p.slot=body[0];p.instance=instance;p.character=read(body,1,4);
 if(p.slot>=24||!p.character)throw Invalid(Error::identity);
 // flags32, peer version16, endpoint count8, endpoints [IPv4,portLE16].
 // Keep routing data out of UI snapshots and do not infer readiness from flags.
 auto endpoints=body[11];if(endpoints>2)throw Invalid(Error::extent);
 size_t at=12+size_t(endpoints)*6;
 p.clanId=read(body,at,4);p.emblem=uint8_t(read(body,at+4,1));at+=5;
 if(at>=body.size()||body.size()-at>47)throw Invalid(Error::extent);
 auto tail=body.subspan(at);auto end=std::find(tail.begin(),tail.end(),0);
 if(end==tail.end())throw Invalid(Error::extent);
 auto n=size_t(end-tail.begin());p.name=label(tail.first(n),true);p.clan=label(tail.subspan(n+1),false);
 for(size_t i=0;i<r.slots.size();++i)if(i!=p.slot&&r.slots[i]&&(r.slots[i]->instance==instance||r.slots[i]->character==p.character))throw Invalid(Error::identity);
 if(r.slots[p.slot]&&r.slots[p.slot]->instance==p.instance&&r.slots[p.slot]->character==p.character){p.appearance=r.slots[p.slot]->appearance;p.level=r.slots[p.slot]->level;}
 if(!r.slots[p.slot]||*r.slots[p.slot]!=p){r.slots[p.slot]=std::move(p);++r.revision;}
 return true;
}
}
