#include "world_inventory_session.h"
#include "world_inventory_host.h"
#include "combat_initial_profile.h"
#include "combat_service.h"
#include <iostream>
using namespace mgo2mt;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(){try{
 auto floor=std::make_shared<const stage::Collision>(stage::Collision::make({{-20000,0,-20000},{20000,0,-20000},{20000,0,20000},{-20000,0,20000}},{{{0,1,2}},{{0,2,3}}}));
 combat::Identity id{1,100,1000};combat::Authority authority;auto profiles=combat::initial_profiles(20,1,0);authority.begin(5,floor,profiles);check(authority.configure_items(3,{64,64}),"items config");combat::Pose p;p.feet={0,2,0};check(authority.join(id,1,p,1000,1000,std::array<uint16_t,1>{25},0),"spawn");authority.active(true);
 items::ClientSession client;items::HostSession host;items::ClientContext context{{5,3},{1,100,1000,1},{0,2,0,0},true};
 auto send=client.pump(context,0,777,true,{});check(send.size()==1&&host.receive(authority,id,send.front(),0),"optional probe");
 auto deliver=[&](uint64_t now){for(unsigned i=0;i<5;++i){host.poll(authority,now);std::vector<std::vector<uint8_t>> messages;if(auto b=host.front(id)){messages.push_back(*b);host.pop(id);}client.pump(context,now,0,true,messages);}};
 deliver(1);check(client.state().status==items::ClientStatus::ready&&client.state().held&&client.state().world&&client.state().held->slots[0].contents.magazine==30,"offer actual held and paged world");
 check(client.submit(items::wire::Action::drop,0),"UI drop accepted");send=client.pump(context,2,0,true,{});check(send.size()==1&&host.receive(authority,id,send[0],2),"authoritative drop");deliver(3);check(client.state().delivery==items::Delivery::confirmed&&!authority.snapshot().players[1]->weapon&&authority.item_state().entities.size()==1,"drop ACK / unarmed / one item");
 const auto entity=authority.item_state().entities.front().key.id;check(client.state().held->slots[0].contents.item==0&&client.state().world->entities.size()==1,"new revision published");
 check(client.submit(items::wire::Action::pickup,0,entity),"pick exact entity");send=client.pump(context,4,0,true,{});check(send.size()==1&&host.receive(authority,id,send[0],4),"pickup host");deliver(5);check(authority.snapshot().players[1]->weapon==25&&authority.snapshot().players[1]->ammo==30&&authority.item_state().entities.empty(),"same weapon/ammo restored");
 auto old=*client.state().held;old.slots[0].contents.magazine=29;std::vector<std::vector<uint8_t>> bad{*items::wire::encode(old)};client.pump(context,6,0,true,bad);check(client.state().held->slots[0].contents.magazine==30,"same revision cannot rewrite holdings");
 check(client.submit(items::wire::Action::install,0),"place UI");send=client.pump(context,7,0,true,{});check(send.size()==1&&host.receive(authority,id,send[0],7),"place host");deliver(8);auto installed=authority.item_state().entities.front().key.id;
 check(client.submit(items::wire::Action::use,0,installed),"equip for use UI");send=client.pump(context,9,0,true,{});check(host.receive(authority,id,send[0],9),"equip for use host");deliver(10);check(authority.snapshot().players[1]->weapon==25&&authority.item_state().entities.empty(),"placed weapon usable through ordinary fire path");
 check(client.submit(items::wire::Action::drop,0),"pending request");check(client.pump(context,11,0,true,{}).size()==1,"sent once");check(client.pump(context,5011,0,true,{}).empty()&&client.state().delivery==items::Delivery::unconfirmed&&!client.submit(items::wire::Action::drop,0),"unknown result never retransacted");
 context.actor.life=2;client.pump(context,5012,888,true,bad);check(client.state().status==items::ClientStatus::probing&&!client.state().held&&!client.state().world,"new life clears replica and rejects old holdings");
 {combat::Authority h;auto lethal=profiles;lethal[0].damage=1000;lethal[0].nativeAkPenetration=false;lethal[0].nativeAkHitRegions=false;h.begin(10,floor,lethal);h.configure_items(1,{64,64});combat::Pose enemy=p;enemy.feet[2]=-3000;combat::Identity enemyId{2,101,1001};check(h.join(id,1,p,1000,1000,std::array<uint16_t,1>{25},0)&&h.join(enemyId,2,enemy,1000,1000,std::array<uint16_t,1>{25},0),"life fixture");h.active(true);
  items::wire::Command c;c.header={{10,1},9,{id.slot,id.instance,id.character,1},50};c.action=items::wire::Action::drop;c.heldSlot=0;c.heldRevision=1;check(bool(h.item_action(id,c,1)),"old life sequence50");
  check(bool(h.fire(enemyId,{10,1,25,{0,0,1},1},2))&&!h.snapshot().players[id.slot]->alive,"old life dies");
  check(h.respawn(id,2,[&]{return h.join(id,1,p,1000,1000,std::array<uint16_t,1>{25},3);}),"real respawn");c.header.actor.life=2;c.header.sequence=1;c.heldRevision=h.item_held(id,9)->slots[0].revision;check(bool(h.item_action(id,c,4)),"new life starts sequence1");c.header.actor.life=1;c.header.sequence=999;check(h.item_action(id,c,5).code==items::ResultCode::identity,"old life cannot reuse high sequence");
 }
 {combat::Service svc(11);svc.configure(floor,profiles);check(svc.admit(id,0)&&svc.receive(id,combat::wire::encode(combat::wire::Accept{11}),0),"service fixture");check(svc.authority().join(id,1,p,1000,1000,std::array<uint16_t,1>{25},0),"equipped authoritative grant");svc.authority().active(true);combat::wire::Input input;input.epoch=11;input.sequence=1;input.pose=p;input.weapon=0;check(svc.receive(id,combat::wire::encode(input),1),"delayed unarmed pose received");svc.poll(2);check(svc.authority().snapshot().players[1]->weapon==25,"delayed unarmed pose does not undo pickup/equip");}
 std::cout<<"GWIV session / actual authority / optional handshake / drop-pick-place-use / stale revision / no uncertain retry / real respawn and delayed pose PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
