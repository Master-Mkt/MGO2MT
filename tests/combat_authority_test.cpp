#include "combat_authority.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
using namespace mgo2win;
using namespace mgo2win::combat;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::shared_ptr<const stage::Collision> floor(bool wall=false){
 std::vector<Vec3> v{{-20000,0,-20000},{20000,0,-20000},{20000,0,20000},{-20000,0,20000}};
 std::vector<stage::CollisionTriangle> t{{{0,1,2}},{{0,2,3}}};if(wall){v.insert(v.end(),{{-1000,0,1500},{1000,0,1500},{1000,3000,1500},{-1000,3000,1500}});t.push_back({{4,5,6},0,0,~0u,700});t.push_back({{4,6,7},0,0,~0u,700});}
 return std::make_shared<const stage::Collision>(stage::Collision::make(v,t));
}
// Deliberately synthetic independent fixtures, never shipped weapon defaults.
std::array<Weapon,2> fixture_weapons(){return {{{23,35,0,100,300,3,4,10000,9001,9002,true},{7,0,60,200,500,2,3,10000,9003,9004,false}}};}
constexpr Identity a{1,10,101},b{2,11,202};
Pose standing(float z){Pose p;p.feet={0,2,z};return p;}
void join(Authority&h,uint8_t team=2){const uint16_t gear[]={23,7};check(h.join(a,1,standing(0),100,100,gear,0)&&h.join(b,team,standing(3000),100,100,gear,0),"host grants identified players and inventory");h.active(true);}
FireRequest fire(uint32_t seq=1,uint16_t weapon=23,uint64_t epoch=1){return {epoch,seq,weapon,{0,0,1}};}
size_t count(const Decision&d,EventKind k){return std::count_if(d.events.begin(),d.events.end(),[&](auto&e){return e.kind==k;});}
}
// These independent fixtures coexist through main; keep their bounded HOST
// replay/state arrays off the default Windows 1 MiB thread stack.
int main(){try{
 auto w=fixture_weapons();auto hOwner=std::make_unique<Authority>();auto& h=*hOwner;h.begin(1,floor(),w);join(h);const auto initial=h.snapshot();
 check(!h.join(a,1,standing(0),100,100,std::array<uint16_t,1>{23},0),"duplicate admission cannot refill health/ammo");
 Replica first,second;check(first.snapshot(initial)&&second.snapshot(initial),"both participants accept complete host state");
 auto shot=h.fire(a,fire(),0);check(bool(shot)&&count(shot,EventKind::shot)==1&&count(shot,EventKind::damage)==1,"host resolves one shot and one damage");
 check(h.snapshot().players[2]->hp==65&&h.snapshot().players[1]->ammo==2,"damage and ammunition belong to host");
 check(shot.events.front().cue==9001&&shot.events[1].cue==9002&&shot.events[2].hpDamage==35,"host chooses cues and damage from weapon definition");
 check(first.events(shot.events)==second.events(shot.events)&&first.events(shot.events).empty(),"two players share exact events; retransmit does not replay sound");
 check(h.fire(a,fire(),1).reject==Reject::sequence&&h.snapshot().players[2]->hp==65,"duplicate request cannot damage twice");
 check(h.fire(a,fire(2),50).reject==Reject::interval&&h.snapshot().players[1]->ammo==2,"rate check precedes ammo or sound mutation");
 auto invalid=fire(3);invalid.direction={0,0,std::numeric_limits<float>::quiet_NaN()};check(h.fire(a,invalid,100).reject==Reject::invalid_direction,"NaN direction rejected");
 invalid=fire(4);invalid.direction={0,0,-1};check(h.fire(a,invalid,100).reject==Reject::invalid_direction,"direction must agree with registered aim");
 auto secondShot=h.fire(a,fire(5),100);check(bool(secondShot)&&h.snapshot().players[2]->hp==30,"next eligible shot");
 auto lethal=h.fire(a,fire(6),200);check(bool(lethal)&&count(lethal,EventKind::death)==1&&h.snapshot().players[2]->hp==0&&!h.snapshot().players[2]->alive,"lethal shot clamps HP and emits one death");
 check(h.fire(b,fire(),200).reject==Reject::dead,"dead player cannot shoot");
 check(h.fire(a,fire(7),300).reject==Reject::no_ammo,"empty magazine cannot emit a shot");
 auto reload=h.reload(a,1,300);check(bool(reload)&&count(reload,EventKind::reload)==1,"host starts timed reload");
 check(h.fire(a,fire(8),400).reject==Reject::reloading&&h.reload(a,1,400).reject==Reject::reloading,"reload time cannot be skipped by requests");
 h.advance(599);check(h.snapshot().players[1]->ammo==0,"reload not early");h.advance(600);check(h.snapshot().players[1]->ammo==3&&h.snapshot().players[1]->reserve==1,"reload conserves finite reserve");
 check(h.pose(a,1,1,standing(0),600)==Reject::none,"refresh server-checked pose");
 check(bool(h.fire(a,fire(9),600))&&h.snapshot().players[1]->ammo==2,"shoot after reload");
 check(h.equip(a,1,7,600)==Reject::none&&h.equip(a,1,23,600)==Reject::none&&h.snapshot().players[1]->ammo==2,"switching granted weapons does not refill magazine");
 check(h.equip(a,1,999,600)==Reject::weapon,"ungranted weapon rejected");
 Replica late;check(late.snapshot(h.snapshot())&&late.events(lethal.events).empty(),"late join restores HP but never replays old death/SE");
 check(h.leave(b)&&!h.leave(b),"identity-specific leave");auto recycled=b;recycled.instance=12;recycled.character=303;check(h.join(recycled,2,standing(3000),100,100,std::array<uint16_t,1>{23},600),"slot reuse obtains new incarnation");check(h.fire(b,fire(10),600).reject==Reject::identity,"old occupant cannot act on reused slot");
 h.begin(2,floor(),w);check(h.snapshot().players[1]==std::nullopt&&h.fire(a,fire(11),700).reject==Reject::generation,"round generation clears players and old commands");

 auto staged=w;staged[0].reloadRefillMs=100;auto reloadStagesOwner=std::make_unique<Authority>();auto& reloadStages=*reloadStagesOwner;reloadStages.begin(1,floor(),staged);join(reloadStages);
 check(bool(reloadStages.fire(a,fire(),0))&&bool(reloadStages.reload(a,1,100)),"two-stage reload starts");
 reloadStages.advance(199);check(reloadStages.snapshot().players[1]->ammo==2,"no refill before its event");
 reloadStages.advance(200);auto refilled=reloadStages.snapshot();check(refilled.players[1]->ammo==3&&refilled.players[1]->reserve==3&&refilled.players[1]->reloadUntil==400,"refill occurs before motion completion and conserves total");
 Replica refillPeer;check(refillPeer.snapshot(refilled)&&refillPeer.state()->players[1]->ammo==3,"intermediate ammo snapshot reaches receiver");
 check(reloadStages.fire(a,fire(2),200).reject==Reject::reloading&&reloadStages.equip(a,1,7,200)==Reject::reloading&&reloadStages.reload(a,1,200).reject==Reject::reloading,"filled magazine does not bypass motion completion");
 reloadStages.advance(350);check(reloadStages.snapshot().players[1]->reserve==3,"refill event applies only once");reloadStages.advance(400);check(bool(reloadStages.fire(a,fire(3),400)),"shoot after completion");
 check(bool(reloadStages.reload(a,1,400)),"second reload");reloadStages.advance(800);check(reloadStages.snapshot().players[1]->ammo==3&&reloadStages.snapshot().players[1]->reserve==2&&!reloadStages.snapshot().players[1]->reloadUntil,"skipped ticks process refill then completion once");
 auto bad=staged;bad[0].reloadRefillMs=bad[0].reloadMs+1;bool rejected=false;try{auto invalidOwner=std::make_unique<Authority>();auto& invalid=*invalidOwner;invalid.begin(1,floor(),bad);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"refill cannot follow completion");

 auto blockedOwner=std::make_unique<Authority>();auto& blocked=*blockedOwner;blocked.begin(1,floor(true),w);join(blocked);auto wall=blocked.fire(a,fire(),0);check(bool(wall)&&count(wall,EventKind::damage)==0&&wall.events[1].object==700&&blocked.snapshot().players[2]->hp==100,"world wall occludes player target");
 auto friendsOwner=std::make_unique<Authority>();auto& friends=*friendsOwner;friends.begin(1,floor(),w);join(friends,1);check(bool(friends.fire(a,fire(),0))&&friends.snapshot().players[2]->hp==100,"friendly-fire-off retains teammate health");auto friendlyOwner=std::make_unique<Authority>(Policy{true});auto& friendly=*friendlyOwner;friendly.begin(1,floor(),w);join(friendly,1);check(bool(friendly.fire(a,fire(),0))&&friendly.snapshot().players[2]->hp==65,"host room policy enables friendly fire");
 auto stunOwner=std::make_unique<Authority>();auto& stun=*stunOwner;stun.begin(1,floor(),w);join(stun);check(stun.equip(a,1,7,0)==Reject::none,"host selects granted stun gun");check(bool(stun.fire(a,fire(1,7),0))&&bool(stun.fire(a,fire(2,7),200)),"stamina shots");check(stun.snapshot().players[2]->hp==100&&stun.snapshot().players[2]->stamina==0&&stun.snapshot().players[2]->stunned,"stamina damage is distinct from lethal damage");check(stun.fire(b,fire(),200).reject==Reject::dead,"stunned player cannot fire");

 auto movementOwner=std::make_unique<Authority>();auto& movement=*movementOwner;movement.begin(1,floor(true),w);join(movement);auto p=standing(1000);
 check(movement.pose(a,1,1,p,1)==Reject::too_fast,"client teleport rejected using host elapsed time");p=standing(3000);check(movement.pose(a,1,2,p,1000)==Reject::too_fast,"stalled connection grants at most250ms movement");p=standing(1450);check(movement.pose(a,1,3,p,250)==Reject::obstructed,"player cannot move into wall");p=standing(100);check(movement.pose(a,1,4,p,100)==Reject::none,"bounded unobstructed movement accepted");check(movement.pose(a,1,4,p,101)==Reject::sequence,"pose duplicate rejected");check(movement.pose(a,1,5,p,99)==Reject::clock,"backward host timestamp rejected");p.feet[0]=std::numeric_limits<float>::infinity();check(movement.pose(a,1,5,p,200)==Reject::invalid_pose,"nonfinite position rejected");check(movement.fire(a,fire(),1000).reject==Reject::invalid_pose,"stale shooter position cannot authorize fire");movement.active(false);check(movement.fire(a,fire(2),1001).reject==Reject::not_active,"room admission is not combat activation");
 auto contactOwner=std::make_unique<Authority>();auto& contact=*contactOwner;contact.begin(1,floor(),w);join(contact);check(contact.pose(a,1,1,standing(1400),250)==Reject::none,"movement toward player accepted before contact");check(contact.pose(a,1,2,standing(2800),500)==Reject::obstructed,"host player capsule blocks crossing into another player");check(!contact.join({3,13,303},1,standing(3000),100,100,std::array<uint16_t,1>{23},500),"host cannot grant overlapping live spawns");
 auto bodyWeapons=w;bodyWeapons[0].bodyCue=1369;auto bodyOwner=std::make_unique<Authority>();auto& body=*bodyOwner;body.begin(1,floor(),bodyWeapons);join(body);auto bodyHit=body.fire(a,fire(),0);check(bodyHit.events[1].cue==1369,"body cue is distinct from material impact cue");auto wallSoundOwner=std::make_unique<Authority>();auto& wallSound=*wallSoundOwner;wallSound.begin(1,floor(true),bodyWeapons);join(wallSound);check(wallSound.fire(a,fire(),0).events[1].cue==9002,"wall does not borrow a body impact sound");
 std::cout<<"host-authoritative occlusion/damage/ammo/reload, player contact, body/material SE, two-receiver SE, latejoin, generation and invalid-request checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
