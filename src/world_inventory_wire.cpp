#include "world_inventory_wire.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
namespace mgo2win::items::wire {
namespace {
struct Invalid{};
void require(bool b){if(!b)throw Invalid{};}
struct IO {
 std::span<const uint8_t> in;std::vector<uint8_t> out;size_t at=0;bool writing=false;
 uint64_t integer(uint64_t v,unsigned n){if(writing){for(unsigned i=n;i;--i)out.push_back(uint8_t(v>>(8*(i-1))));return v;}require(at<=in.size()&&n<=in.size()-at);v=0;while(n--)v=(v<<8)|in[at++];return v;}
 template<class T>void value(T& v,unsigned n){v=static_cast<T>(integer(static_cast<uint64_t>(v),n));}
 void zero(unsigned n){require(integer(0,n)==0);}
 void number(float& v){auto bits=std::bit_cast<uint32_t>(v);value(bits,4);v=std::bit_cast<float>(bits);require(std::isfinite(v));}
 void actor(Actor& a,bool neutral=false){value(a.slot,1);zero(1);value(a.instance,2);value(a.character,4);value(a.life,4);require(neutral?a==Actor{}:(a.slot<24&&a.instance&&a.character));}
 void header(Header& h){value(h.scope.epoch,8);value(h.scope.generation,8);value(h.token,8);actor(h.actor);zero(4);value(h.sequence,8);require(h.scope.epoch&&h.scope.generation&&h.token);}
 void entity(Entity& e,Scope scope){e.key.scope=scope;value(e.key.id,8);value(e.revision,8);value(e.kind,1);value(e.contents.domain,1);value(e.contents.resource,1);zero(1);value(e.contents.item,4);value(e.contents.quantity,4);value(e.contents.magazine,4);value(e.contents.reserve,4);value(e.contents.charges,4);actor(e.owner,e.kind==PlacementKind::round);number(e.position.x);number(e.position.y);number(e.position.z);number(e.position.yaw);
  require(e.key.id&&e.revision&&e.kind<=PlacementKind::round&&e.contents.domain<=Domain::world_item&&e.contents.resource<=Resource::charges&&e.contents.item&&e.contents.quantity&&(e.kind==PlacementKind::round||e.owner.life));
  if(e.contents.resource==Resource::durable)require(!e.contents.magazine&&!e.contents.reserve&&!e.contents.charges);
  if(e.contents.resource==Resource::ammunition)require(e.contents.quantity==1&&!e.contents.charges);
  if(e.contents.resource==Resource::charges)require(!e.contents.magazine&&!e.contents.reserve);
 }
};
bool action(Action a){return a>=Action::drop&&a<=Action::equip;}
void payload(IO& io,Record& record){std::visit([&](auto& r){using T=std::decay_t<decltype(r)>;io.header(r.header);
 if constexpr(std::is_same_v<T,Probe>){require(!r.header.sequence);}
 else if constexpr(std::is_same_v<T,Offer>){require(!r.header.sequence);io.value(r.capabilities,4);io.zero(4);io.value(r.capacity.dropped,4);io.value(r.capacity.installed,4);require(!(r.capabilities&~all_capabilities));}
 else if constexpr(std::is_same_v<T,Command>){require(r.header.sequence&&r.header.actor.life);io.value(r.action,1);io.value(r.heldSlot,1);io.value(r.resource,1);io.zero(1);io.value(r.amount,4);io.value(r.entity,8);io.value(r.heldRevision,8);io.value(r.entityRevision,8);require(action(r.action)&&r.resource<=Consume::charges);
  if(r.action==Action::use)require(r.heldSlot==255&&r.entity&&r.entityRevision&&r.amount&&!r.heldRevision);
  else{require(r.heldSlot<held_slot_count&&r.heldRevision&&r.resource==Consume::magazine);if(r.action==Action::drop||r.action==Action::equip)require(!r.entity&&!r.entityRevision&&!r.amount);if(r.action==Action::install)require(!r.entity&&!r.entityRevision&&r.amount);if(r.action==Action::pickup||r.action==Action::recover)require(r.entity&&r.entityRevision&&!r.amount);}
 }else if constexpr(std::is_same_v<T,Reply>){require(r.header.sequence&&r.header.actor.life);io.value(r.action,1);io.value(r.heldSlot,1);io.value(r.result,1);uint8_t destroyed=r.destroyed?1:0;io.value(destroyed,1);require(destroyed<=1);r.destroyed=destroyed!=0;io.zero(4);io.value(r.entity,8);io.value(r.heldRevision,8);io.value(r.worldRevision,8);io.value(r.entityRevision,8);require(action(r.action)&&r.result<=ResultCode::resource&&(!r.destroyed||r.result==ResultCode::ok));if(r.result==ResultCode::ok)require(r.worldRevision);
 }else if constexpr(std::is_same_v<T,Page>){require(!r.header.sequence);io.value(r.revision,8);io.value(r.total,4);io.value(r.index,2);io.value(r.pages,2);uint16_t n=static_cast<uint16_t>(r.entities.size());if(io.writing)require(r.entities.size()<=rows_per_page);io.value(n,2);io.zero(2);io.value(r.capacity.dropped,4);io.value(r.capacity.installed,4);
  const uint64_t pageCount=std::max<uint64_t>(1,(uint64_t(r.total)+rows_per_page-1)/rows_per_page);
  require(r.revision&&pageCount<=UINT16_MAX&&r.pages==pageCount&&r.index<r.pages&&n<=rows_per_page&&uint64_t(r.total)<=uint64_t(r.capacity.dropped)+r.capacity.installed);
  require(n==(r.total?std::min<uint32_t>(rows_per_page,r.total-uint32_t(r.index)*rows_per_page):0));r.entities.resize(n);std::set<uint64_t> ids;for(auto& e:r.entities){if(io.writing)require(e.key.scope==r.header.scope);io.entity(e,r.header.scope);require(ids.insert(e.key.id).second);}
 }else if constexpr(std::is_same_v<T,Held>){
  require(!r.header.sequence&&r.header.actor.life);io.value(r.selectedSlot,1);io.value(r.selectedEquipment,1);io.zero(2);require(r.selectedEquipment==3||r.selectedEquipment==255);require(weapon_slot(r.selectedSlot)||r.selectedSlot==255);
  for(auto& slot:r.slots){auto& c=slot.contents;io.value(c.domain,1);io.value(c.resource,1);io.zero(2);io.value(c.item,4);io.value(c.quantity,4);io.value(c.magazine,4);io.value(c.reserve,4);io.value(c.charges,4);io.value(slot.revision,8);require(slot.revision&&c.domain<=Domain::world_item&&c.resource<=Resource::charges);
   if(!c.item)require(c==Contents{});else{require(c.quantity);if(c.resource==Resource::durable)require(!c.magazine&&!c.reserve&&!c.charges);if(c.resource==Resource::ammunition)require(c.quantity==1&&!c.charges);if(c.resource==Resource::charges)require(!c.magazine&&!c.reserve);}
  }
  if(weapon_slot(r.selectedSlot))require(r.slots[r.selectedSlot].contents.item&&r.slots[r.selectedSlot].contents.domain==Domain::weapon);if(r.selectedEquipment==3)require(r.slots[3].contents.item&&r.slots[3].contents.domain==Domain::equipment);
  if(r.slots[knife_slot].contents.item)require(r.slots[knife_slot].contents.item==1||r.slots[knife_slot].contents.item==131);
  for(size_t i=0;i<r.slots.size();++i)if(r.slots[i].contents.item)require(r.slots[i].contents.domain==(i==3?Domain::equipment:Domain::weapon));
 }
},record);}
bool compatible(const Page& a,const Page& b){return a.header==b.header&&a.revision==b.revision&&a.total==b.total&&a.pages==b.pages&&a.capacity.dropped==b.capacity.dropped&&a.capacity.installed==b.capacity.installed;}
}
bool recognized(std::span<const uint8_t> b)noexcept{return !b.empty()&&b[0]==marker;}
std::optional<std::vector<uint8_t>> encode(const Record& source){try{auto record=source;IO io;io.writing=true;const auto kind=std::holds_alternative<Held>(record)?uint8_t(7):uint8_t(record.index()+1);io.out={marker,'G','W','I','V',version,kind,0};payload(io,record);require(io.out.size()<=maximum_body);return io.out;}catch(const Invalid&){return {};}catch(const std::bad_alloc&){return {};}}
std::optional<Record> decode(std::span<const uint8_t> b){try{require(b.size()>=56&&b.size()<=maximum_body&&b[0]==marker&&b[1]=='G'&&b[2]=='W'&&b[3]=='I'&&b[4]=='V'&&b[5]==version&&b[7]==0);Record r;switch(b[6]){case 1:r=Probe{};break;case 2:r=Offer{};break;case 3:r=Command{};break;case 4:r=Reply{};break;case 5:r=Page{};break;case 7:r=Held{};break;default:throw Invalid{};}IO io;io.in=b;io.at=8;payload(io,r);require(io.at==b.size());return r;}catch(const Invalid&){return {};}catch(const std::bad_alloc&){return {};}}
std::optional<std::vector<Page>> pages(const SnapshotState& state,Header recipient){try{
 require(state.scope==recipient.scope&&state.revision&&state.entities.size()<=uint64_t(rows_per_page)*UINT16_MAX&&!recipient.sequence);
 const auto total=uint32_t(state.entities.size());const auto count=uint16_t(std::max<uint32_t>(1,(total+rows_per_page-1)/rows_per_page));std::vector<Page> out;out.reserve(count);std::set<uint64_t> ids;uint64_t dropped=0,installed=0;
 for(const auto& e:state.entities){require(e.key.scope==state.scope&&ids.insert(e.key.id).second);if(e.kind!=PlacementKind::installed)++dropped;else ++installed;}require(dropped<=state.capacity.dropped&&installed<=state.capacity.installed);
 for(uint16_t index=0;index<count;++index){Page p{recipient,state.revision,total,index,count,state.capacity,{}};auto first=std::min<size_t>(size_t(index)*rows_per_page,state.entities.size()),last=std::min(first+rows_per_page,state.entities.size());p.entities.assign(state.entities.begin()+first,state.entities.begin()+last);require(encode(p).has_value());out.push_back(std::move(p));}return out;
 }catch(const Invalid&){return {};}catch(const std::bad_alloc&){return {};}}
void Receiver::bind(Header h,Capacity maximum){expected_=h;maximum_=maximum;floor_=0;pending_.reset();pages_.clear();state_.reset();}
bool Receiver::receive(std::span<const uint8_t> body){auto decoded=decode(body);if(!decoded)return false;auto page=std::get_if<Page>(&*decoded);if(!page||page->header!=expected_||page->capacity.dropped>maximum_.dropped||page->capacity.installed>maximum_.installed||page->revision<floor_)return false;
 if(page->revision>floor_){floor_=page->revision;pending_=*page;pages_.clear();}
 if(!pending_||!compatible(*pending_,*page))return false;
 if(auto old=pages_.find(page->index);old!=pages_.end())return encode(old->second)==encode(*page);
 // Page metadata is validated before retaining rows; no unbounded ID allocation.
 try{pages_.emplace(page->index,*page);if(pages_.size()!=page->pages)return true;SnapshotState candidate{page->header.scope,page->revision,page->capacity,{}};candidate.entities.reserve(page->total);std::set<uint64_t> ids;uint64_t dropped=0,installed=0;
 for(const auto&[index,p]:pages_)for(const auto& e:p.entities){if(!ids.insert(e.key.id).second)return false;if(e.kind!=PlacementKind::installed)++dropped;else ++installed;candidate.entities.push_back(e);}
 if(candidate.entities.size()!=page->total||dropped>page->capacity.dropped||installed>page->capacity.installed)return false;
 state_=std::move(candidate);return true;
 }catch(const std::bad_alloc&){return false;}
}
}
