#include "combat_initial_profile.h"
#include "combat_service.h"
#include "native_loadout.h"
#include "hold_inventory_bridge.h"
#include "world_inventory_session.h"
#include "world_inventory_host.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
namespace {
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
std::shared_ptr<const stage::Collision> floor(){return std::make_shared<const stage::Collision>(stage::Collision::make({{-20000,0,-20000},{20000,0,-20000},{20000,0,20000},{-20000,0,20000}},{{{0,1,2}},{{0,2,3}}}));}
}
int main(int argc,char**argv){try{
 check(argc==2,"weapon catalog argument");auto profiles=combat::initial_profiles(20,1,0);
 auto catalog=std::make_shared<weapons::Catalog>();std::string error;check(catalog->load(argv[1],error),"real catalog");
 auto op=catalog->find(weapons::Category::secondary,3);check(op&&op->available_without_dp==false,"original OPERATOR restriction preserved");
 weapons::SelectionContext access;check(weapons::access(*op,access)==weapons::Access::dp_disabled,"ordinary catalog unchanged");access.native_operator_grant=true;
 check(weapons::access(*op,access)==weapons::Access::allowed,"explicit native grant exception");access.room_restrictions[0]=9;check(weapons::access(*op,access)==weapons::Access::restricted,"room restriction wins exception");
 combat::Identity id{1,100,1000};combat::Pose pose;pose.feet={0,2,0};combat::Service service(5);service.configure(floor(),profiles);
 service.configure_round({},catalog,[](combat::Authority&a,combat::Identity who,uint8_t team,std::span<const uint16_t> items,uint64_t now){combat::Pose p;p.feet={0,2,0};return a.join(who,team,p,1000,1000,items,now);});
 check(service.admit(id)&&service.receive(id,combat::wire::encode(combat::wire::Accept{5}),0),"admitted real Service");
 auto command=[&](combat::wire::CommandKind kind,uint32_t seq){combat::wire::Command c;c.epoch=5;c.sequence=seq;c.kind=kind;if(kind==combat::wire::CommandKind::loaded){c.enabled=true;c.generation=1;c.sceneRevision=1;}if(kind==combat::wire::CommandKind::ready)c.enabled=true;if(kind==combat::wire::CommandKind::loadout)c.weapons={25,0,0};check(service.receive(id,combat::wire::encode(c),seq),"round command accepted");service.poll(seq);};
 command(combat::wire::CommandKind::loaded,1);command(combat::wire::CommandKind::ready,2);command(combat::wire::CommandKind::loadout,3);
 auto& authority=service.authority();auto held=authority.item_held(id,777);check(held&&held->selectedSlot==0,"default AK selected");
 for(unsigned i=0;i<3;++i)check(held->slots[i].contents.item==weapons::native_loadout::initial[i]&&held->slots[i].contents.quantity==1,"all three native initial slots granted");
 check(held->slots[1].contents.resource==items::Resource::durable&&!held->slots[1].contents.magazine,"held-only does not invent live ammunition");
 check(bool(authority.fire(id,{5,1,25,{0,0,1},1},4)),"original AK actual shot");check(authority.snapshot().players[1]->ammo==29,"ammo consumed once");
 items::ClientSession client;items::HostSession host;items::ClientContext context{{5,1},{1,100,1000,1},{0,2,0,0},true};
 auto outgoing=client.pump(context,5,777,true,{});check(outgoing.size()==1&&host.receive(authority,id,outgoing[0],5),"scoped inventory handshake");
 auto deliver=[&](uint64_t now){for(unsigned i=0;i<8;++i){host.poll(authority,now);std::vector<std::vector<uint8_t>> incoming;if(auto b=host.front(id)){incoming.push_back(*b);host.pop(id);}client.pump(context,now,0,true,incoming);}};deliver(6);
 check(client.state().capabilities&32,"explicit equip advertised");
 check(!hold_selection::inventory_blocks_gameplay(items::Delivery::none)&&!hold_selection::inventory_blocks_gameplay(items::Delivery::confirmed)&&!hold_selection::inventory_blocks_gameplay(items::Delivery::rejected)&&hold_selection::inventory_blocks_gameplay(items::Delivery::pending)&&hold_selection::inventory_blocks_gameplay(items::Delivery::unconfirmed),"pending and unknown results block gameplay until a resolved scope");
 const auto ui=hold_selection::inventory_snapshot(client.state(),true);check(ui.eligible&&ui.weapons.size()==4&&ui.equipment.empty()&&ui.selectedWeapon==0,"actual HOST holdings become exactly three UI weapon candidates");
 auto unsupported=client.state();unsupported.capabilities&=~32u;check(!hold_selection::inventory_snapshot(unsupported,true).eligible,"old HOST cannot expose equip UI");unsupported=client.state();unsupported.held->header.actor.life++;check(!hold_selection::inventory_snapshot(unsupported,true).eligible,"mixed old-life holdings cannot expose UI");
 const auto captured=client.state();const auto& candidate=captured.held->slots[1];auto staleActor=context.actor;staleActor.life++;
 check(!client.submit_equip(captured.connection+1,context.scope,context.actor,1,3,candidate.revision)&&!client.submit_equip(captured.connection,{6,1},context.actor,1,3,candidate.revision)&&!client.submit_equip(captured.connection,context.scope,staleActor,1,3,candidate.revision)&&!client.submit_equip(captured.connection,context.scope,context.actor,1,52,candidate.revision)&&!client.submit_equip(captured.connection,context.scope,context.actor,1,3,candidate.revision+1),"atomic release rejects changed connection/scope/life/item/revision before queue");
 auto equip=[&](uint8_t slot,uint64_t now){const auto choices=hold_selection::inventory_snapshot(client.state(),true);auto row=std::find_if(choices.weapons.begin(),choices.weapons.end(),[&](const auto& item){return item.slot==slot;});check(row!=choices.weapons.end(),"real bridge candidate exists");check(hold_selection::submit_inventory_selection(client,{choices.scope,hold_selection::Kind::weapons,*row}),"release bridge submits exact candidate atomically");check(hold_selection::inventory_blocks_gameplay(client.state().delivery)&&!hold_selection::inventory_snapshot(client.state(),true).eligible,"post-submit state immediately blocks new gameplay and selection");auto bytes=client.pump(context,now,0,true,{});check(bytes.size()==1&&host.receive(authority,id,bytes[0],now),"HOST handles equip");deliver(now+1);check(client.state().delivery==items::Delivery::confirmed&&client.state().held->selectedSlot==slot&&!hold_selection::inventory_blocks_gameplay(client.state().delivery),"ack plus selected held revision unblocks gameplay");return bytes[0];};
 auto opRequest=equip(1,7);check(authority.snapshot().players[1]->weapon==3,"operator equipped");
 check(authority.fire(id,{5,2,3,{0,0,1},1},9).reject==combat::Reject::weapon&&authority.reload(id,5,9).reject==combat::Reject::weapon,"held-only cannot borrow AK firing or reload");
 auto revision=authority.snapshot().revision;check(host.receive(authority,id,opRequest,10),"duplicate returns explicit rejection");deliver(11);check(authority.snapshot().revision==revision,"replay cannot mutate equipment");
 auto decoded=std::get<items::wire::Command>(*items::wire::decode(opRequest));decoded.header.sequence=20;decoded.header.token++;check(!host.receive(authority,id,*items::wire::encode(decoded),12),"wrong token cannot choose");decoded.header.token--;decoded.header.actor.life++;check(!host.receive(authority,id,*items::wire::encode(decoded),12),"old life cannot choose");decoded.header.actor.life--;check(authority.item_action(id,decoded,12).code==items::ResultCode::stale,"old held revision rejected");
 // Send future fixture seq only after UI completed to avoid contaminating its
 // independent application sequence below: stale request already consumed 20.
 decoded.header.sequence=21;decoded.heldSlot=2;decoded.heldRevision=authority.item_held(id,777)->slots[2].revision;check(bool(authority.item_action(id,decoded,13)),"grenade held slot selectable");
 check(authority.snapshot().players[1]->weapon==52&&authority.fire(id,{5,3,52,{0,0,1},1},14).reject==combat::Reject::weapon,"grenade does not fire rifle bullets");
 decoded.header.sequence=22;decoded.heldSlot=0;decoded.heldRevision=authority.item_held(id,777)->slots[0].revision;check(bool(authority.item_action(id,decoded,15))&&authority.snapshot().players[1]->ammo==29&&authority.snapshot().players[1]->reserve==90,"switch back preserves exact AK ammo");
 decoded.header.sequence=23;decoded.heldSlot=1;decoded.heldRevision=authority.item_held(id,777)->slots[1].revision;check(bool(authority.item_action(id,decoded,16)),"select sidearm before old movement");
 combat::wire::Input input;input.epoch=5;input.sequence=1;input.pose=pose;input.weapon=25;input.fire=true;input.firePressed=true;
 check(service.receive(id,combat::wire::encode(input),17),"old observed AK heartbeat accepted for pose");service.poll(17);check(authority.snapshot().players[1]->weapon==3&&authority.snapshot().eventWatermark==1,"stale weapon input neither reverses choice nor fires");
 auto bad=decoded;bad.entity=1;check(!items::wire::encode(bad),"equip cannot smuggle world item");bad=decoded;bad.heldSlot=5;check(!items::wire::encode(bad),"equip slot bounded");
 input.sequence=2;input.weapon=3;check(service.receive(id,combat::wire::encode(input),18),"current selected input");service.poll(18);check(authority.snapshot().eventWatermark==1,"held-only Service produces no attack");
 decoded.header.sequence=24;decoded.heldSlot=4;decoded.heldRevision=authority.item_held(id,777)->slots[4].revision;check(bool(authority.item_action(id,decoded,19))&&authority.item_held(id,777)->selectedSlot==4&&authority.snapshot().players[1]->weapon==1,"initial knife selectable in fifth slot");
 // Death entitlement resets every slot, while rejecting the previous life.
 auto lethal=profiles;lethal[0].damage=1000;lethal[0].nativeAkPenetration=false;lethal[0].nativeAkHitRegions=false;combat::Authority lives;lives.begin(8,floor(),lethal);combat::Identity foe{2,101,1001};auto enemy=pose;enemy.feet[2]=-3000;
 check(lives.join(id,1,pose,1000,1000,weapons::native_loadout::initial,0)&&lives.join(foe,2,enemy,1000,1000,std::array<uint16_t,1>{25},0),"respawn fixture");lives.active(true);check(bool(lives.fire(foe,{8,1,25,{0,0,1},1},1))&&!lives.snapshot().players[1]->alive,"real death");
 check(lives.respawn(id,2,[&]{return lives.join(id,1,pose,1000,1000,weapons::native_loadout::initial,2);}),"new life granted");auto restored=lives.item_held(id,777);check(restored&&restored->header.actor.life==2&&restored->slots[0].contents.magazine==30&&restored->slots[1].contents.item==3&&restored->slots[2].contents.item==52,"new life replenishes fixed three-slot grant");
 decoded.header.scope={8,1};decoded.header.actor.life=1;decoded.header.sequence=999;check(lives.item_action(id,decoded,3).code==items::ResultCode::identity,"previous-life switch cannot affect regrant");
 auto invalid=profiles;invalid[1].damage=275;bool rejected=false;try{combat::Authority invalidHost;invalidHost.begin(1,floor(),invalid);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"held-only profile rejects accidental rifle values");
 std::cout<<"three-slot grant / native OPERATOR exception / scoped equip ACK / replay-stale rejection / ammo preservation / stale heartbeat / respawn / unsupported attacks fail closed PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
