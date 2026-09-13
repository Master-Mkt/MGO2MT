#include "world_inventory_wire.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2win::items;
namespace w=mgo2win::items::wire;
namespace {void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}const w::Header header{{7,9},123,{1,0x101,200,2},0};}
int main(){try{
 auto probe=w::encode(w::Probe{header});check(probe&&probe->size()==56&&w::decode(*probe),"probe56");
 auto offer=w::encode(w::Offer{header,5,{64,64}});check(offer&&offer->size()==72&&w::decode(*offer),"offer72 capabilities");
 w::Held held;held.header=header;held.selectedSlot=0;held.slots[0].contents={25,1,17,93,0,Resource::ammunition};held.slots[0].revision=19;
 auto holdings=w::encode(held);check(holdings&&holdings->size()==156&&holdings->at(6)==7&&w::decode(*holdings),"held156 kind7");auto heldDecoded=std::get<w::Held>(*w::decode(*holdings));check(heldDecoded.slots[0]==held.slots[0]&&heldDecoded.slots[1].revision==1,"held contents/revision");held.slots[0].contents={};held.selectedSlot=255;check(w::encode(held).has_value(),"unarmed holdings");held.selectedSlot=0;check(!w::encode(held),"selected empty holding rejected");
 w::Command cmd;cmd.header=header;cmd.header.sequence=1;cmd.action=w::Action::drop;cmd.heldSlot=0;cmd.heldRevision=3;
 auto encoded=w::encode(cmd);check(encoded&&encoded->size()==88,"command88");
 std::vector<uint8_t> golden{0xec,'G','W','I','V',1,3,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,9,0,0,0,0,0,0,0,123,1,0,1,1,0,0,0,200,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0};
 check(*encoded==golden,"literal BE command golden");
 for(size_t i=0;i<encoded->size();++i)check(!w::decode(std::span(*encoded).first(i)),"all truncations");auto trailing=*encoded;trailing.push_back(0);check(!w::decode(trailing),"trailing bytes");
 for(size_t offset:{size_t(0),size_t(1),size_t(2),size_t(3),size_t(4),size_t(5),size_t(7),size_t(33),size_t(44),size_t(45),size_t(46),size_t(47),size_t(59)}){auto bad=*encoded;bad[offset]^=0x80;check(!w::decode(bad),"strict header/reserved");}
 for(auto action:{w::Action::install,w::Action::pickup,w::Action::recover,w::Action::use}){auto c=cmd;c.action=action;if(action==w::Action::install)c.amount=1;else{c.entity=1;c.entityRevision=2;}if(action==w::Action::use){c.heldSlot=255;c.heldRevision=0;c.amount=1;c.resource=Consume::charges;}auto body=w::encode(c);check(body&&w::decode(*body),"all operation shapes");}
 auto wrong=cmd;wrong.entity=1;check(!w::encode(wrong),"drop cannot identify invented entity");wrong=cmd;wrong.action=w::Action::pickup;check(!w::encode(wrong),"pickup requires entity revision");wrong=cmd;wrong.header.actor.life=0;check(!w::encode(wrong),"operation requires actor life");
 w::Reply reply{cmd.header,w::Action::drop,0,ResultCode::ok,false,1,4,10,1};auto replyBody=w::encode(reply);check(replyBody&&replyBody->size()==96&&w::decode(*replyBody),"reply96 correlation");
 SnapshotState state{header.scope,10,{64,64},{}};for(uint64_t i=1;i<=65;++i){Entity e{{header.scope,i},1,i<=64?PlacementKind::dropped:PlacementKind::installed,header.actor,{25,1,17,93,0,Resource::ammunition},{float(i),2,3,0}};state.entities.push_back(e);}
 auto pages=w::pages(state,header);check(pages&&pages->size()==5&&w::encode(pages->front())->size()==1172,"16 row page shape");
 w::Receiver receiver;receiver.bind(header,{64,64});for(size_t i=pages->size();i>1;--i){check(receiver.receive(*w::encode((*pages)[i-1]))&&!receiver.state(),"out of order partial unpublished");}check(receiver.receive(*w::encode((*pages)[0]))&&receiver.state()&&receiver.state()->entities.size()==65,"atomic complete snapshot");
 check(receiver.receive(*w::encode((*pages)[0]))&&receiver.state()->revision==10,"identical duplicate idempotent");auto badPage=pages->front();badPage.entities[0].contents.magazine++;check(!receiver.receive(*w::encode(badPage))&&receiver.state()->revision==10,"conflicting duplicate does not overwrite");
 auto newState=state;newState.revision=11;newState.entities.pop_back();auto fresh=w::pages(newState,header);check(fresh&&receiver.receive(*w::encode((*fresh)[0]))&&receiver.state()->revision==10,"new snapshot retains old until complete");check(!receiver.receive(*w::encode((*pages)[1])),"older revision cannot mix");for(size_t i=1;i<fresh->size();++i)check(receiver.receive(*w::encode((*fresh)[i])),"new remaining pages");check(receiver.state()->revision==11&&receiver.state()->entities.size()==64,"new revision committed");
 w::Receiver duplicateIds;duplicateIds.bind(header,{64,64});auto duplicate=*pages;duplicate[1].entities[0].key.id=duplicate[0].entities[0].key.id;bool last=false;for(const auto& p:duplicate)last=duplicateIds.receive(*w::encode(p));check(!last&&!duplicateIds.state(),"cross-page duplicate entity fails transaction");
 w::Receiver budget;budget.bind(header,{12,12});check(!budget.receive(*w::encode(pages->front()))&&!budget.state(),"runtime capacity budget");
 for(unsigned variant=0;variant<4;++variant){auto changed=pages->front();if(variant==0)changed.header.token++;if(variant==1)changed.header.scope.epoch++;if(variant==2)changed.header.actor.life++;if(variant==3)changed.header.actor.instance++;for(auto& e:changed.entities)e.key.scope=changed.header.scope;check(!receiver.receive(*w::encode(changed)),"scope identity nonce lifetime binding");}
 auto emptyState=SnapshotState{header.scope,12,{64,64},{}};auto emptyPages=w::pages(emptyState,header);check(emptyPages&&emptyPages->size()==1&&w::encode(emptyPages->front())->size()==84&&receiver.receive(*w::encode(emptyPages->front()))&&receiver.state()->entities.empty(),"empty snapshot removes all atomically");
 std::cout<<"world_inventory_wire_test PASS: exact wire, operation scope, bounded pages, revision mixing and atomic snapshot\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
