#include "combat_authority.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2win;using namespace mgo2win::combat;
namespace {
void check(bool p,const char*m){if(!p)throw std::runtime_error(m);}
constexpr Identity a{0,10,100},b{1,11,101},c{2,12,102},d{3,13,103};
const uint16_t gear[]={23,7};
std::array<Weapon,2> fixture_weapons(){return {{{23,1000,0,10,300,10,20,20000},{7,0,1000,10,300,10,20,20000}}};}
auto floor(){std::vector<Vec3> v{{-100000,0,-100000},{100000,0,-100000},{100000,0,100000},{-100000,0,100000}};return std::make_shared<const stage::Collision>(stage::Collision::make(v,{{{0,1,2}},{{0,2,3}}}));}
Pose at(float x,float z,float yaw=0){Pose p;p.feet={x,2,z};p.yaw=yaw;return p;}
void add(Authority&h,Identity id,uint8_t team,Pose p){check(h.join(id,team,p,100,100,gear,0),"join");}
void begin(Authority&h){auto w=fixture_weapons();h.begin(1,floor(),w);check(h.configure_sop(100,50),"configure");h.active(true);}
void input(Authority&h,Identity id,uint32_t seq,bool pressed,bool held,uint64_t now){auto p=*h.snapshot().players[id.slot];check(h.pose(id,1,seq,p.pose,now,p.life)==Reject::none,"pose input");check(h.special(id,1,seq,pressed,held,now,p.life)==Reject::none,"special input");}
uint32_t mask(Authority&h,Identity id){return h.sop_view(id)->visibleMask;}
void pair(Authority&h){begin(h);add(h,a,1,at(0,0));add(h,b,1,at(0,3000));input(h,a,1,true,true,100);h.advance(200);check(mask(h,a)==2&&mask(h,b)==1,"pair links");}
}
int main(){try{
 Authority h;begin(h);add(h,a,1,at(0,0));add(h,b,1,at(0,3000));add(h,c,1,at(1500,3000));add(h,d,2,at(-1500,3000));
 check(!h.configure_sop(60001,50),"bounded host timing");
 check(h.special(a,1,0,true,true,0)==Reject::sequence,"requires accepted pose");
 input(h,a,1,false,true,100);check(h.snapshot().players[0]->specialPhase==SpecialPhase::none,"held alone cannot start");
 input(h,a,2,true,true,110);check(h.snapshot().players[0]->specialPhase==SpecialPhase::start,"start");
 check(h.special(a,1,2,true,true,110)==Reject::sequence,"duplicate input");
 h.advance(209);check(mask(h,a)==0,"no premature hold link");h.advance(210);check(mask(h,a)==2,"only first eligible target per tick");
 const auto activation=h.sop_view(a)->activation;h.advance(211);check(mask(h,a)==6&&mask(h,b)==5&&mask(h,c)==3,"union propagation");
 check(mask(h,d)==0&&h.sop_view(a)->activation==activation+1,"enemy excluded and expanded set activation");
 check(h.sop_view(a)->origin==h.snapshot().players[0]->pose.feet&&h.sop_view(b)->origin==h.snapshot().players[1]->pose.feet&&h.sop_view(c)->origin==h.snapshot().players[2]->pose.feet,"native scan origin is each recipient");
 const auto stable=h.sop_view(a)->activation;h.advance(212);check(h.sop_view(a)->activation==stable,"no replay each hold tick");
 check(h.equip(a,1,23,212)==Reject::none&&h.equip(a,1,7,212)==Reject::unavailable,"same equip only");
 check(h.reload(a,1,212).reject==Reject::unavailable&&h.fire(a,{1,1,23,{0,0,1}},212).reject==Reject::unavailable,"action gate");
 items::wire::Command item;item.header={{1,1},1,{a.slot,a.instance,a.character,1},1};item.action=items::wire::Action::drop;item.heldSlot=0;item.heldRevision=h.item_held(a,1)->slots[0].revision;
 check(h.item_action(a,item,212).code==items::ResultCode::unauthorized,"special blocks item action");
 auto moving=h.snapshot().players[0]->pose;moving.feet[0]+=1;check(h.pose(a,1,3,moving,212)==Reject::unavailable,"movement frozen");
 check(h.sop_view(a)->inputSequence==2,"failed pose not acknowledged");
 input(h,a,3,false,false,220);check(h.snapshot().players[0]->specialPhase==SpecialPhase::end,"release end");h.advance(269);check(h.snapshot().players[0]->specialPhase==SpecialPhase::end,"end duration");h.advance(270);
 input(h,a,4,false,true,280);check(h.snapshot().players[0]->specialPhase==SpecialPhase::none,"held without fresh press no restart");
 // Move linked peer away using valid speed-limited poses: link does not expire with range.
 for(uint32_t i=1;i<=8;++i){auto p=h.snapshot().players[1]->pose;p.feet[2]+=1000;check(h.pose(b,1,i,p,280+250*i)==Reject::none,"move linked peer");}
 h.advance(2280);check(mask(h,a)==6,"distance does not break established group");
 check(h.sop_jam(b,true,2280)&&mask(h,a)==4&&mask(h,b)==0&&h.sop_view(b)->jammed,"host jam immediate visibility loss");
 check(h.sop_view(a)->activation==stable,"loss alone no activation");check(h.sop_jam(b,false,2280)&&mask(h,b)==0,"unjam requires fresh acquisition");
 auto before=h.snapshot();check(!h.leave({1,99,101})&&h.snapshot()==before,"old incarnation no clear");
 check(h.leave(c)&&mask(h,a)==0,"leave and singleton dissolve");
 auto revision=h.snapshot().revision;h.active(false);check(mask(h,a)==0&&h.sop_view(a)->activation==0&&h.sop_view(a)->inputSequence==4&&h.snapshot().revision>revision,"inactive visible reset with accepted ack advances revision");
 auto stopped=h.snapshot();h.active(false);check(h.snapshot()==stopped,"repeated inactive reset is idempotent");
 h.active(true);check(mask(h,a)==0,"active alone no relink");
 Authority tap;begin(tap);add(tap,a,1,at(0,0));add(tap,b,1,at(0,3000));input(tap,a,1,true,false,10);tap.advance(110);check(mask(tap,a)==2&&tap.snapshot().players[0]->specialPhase==SpecialPhase::end,"short tap first hold opportunity then end");
 Authority stale;begin(stale);add(stale,a,1,at(0,0));add(stale,b,1,at(0,3000));input(stale,a,1,true,true,10);check(stale.release_special(a,20),"stale release");stale.advance(200);check(mask(stale,a)==0&&stale.snapshot().players[0]->specialPhase==SpecialPhase::none,"stale held cannot later link");
 Authority dm(Policy{false,6000,15000,500,true});begin(dm);add(dm,a,0,at(0,0));add(dm,b,0,at(0,3000));input(dm,a,1,true,true,10);dm.advance(110);check(dm.snapshot().players[0]->specialPhase==SpecialPhase::hold&&mask(dm,a)==0,"DM gesture but no SOP link");
 Authority disabled;auto profile=fixture_weapons();disabled.begin(1,floor(),profile);disabled.active(true);add(disabled,a,1,at(0,0));check(disabled.pose(a,1,1,at(0,0),10)==Reject::none,"disabled ack");check(disabled.special(a,1,1,true,true,10)==Reject::unavailable&&disabled.sop_view(a)->inputSequence==1,"disabled SOP retains pose acknowledgment");
 Authority crouched;begin(crouched);auto crouch=at(0,0);crouch.capsule.height=1100;add(crouched,a,1,crouch);check(crouched.pose(a,1,1,crouch,10)==Reject::none&&crouched.special(a,1,1,true,true,10)==Reject::invalid_pose,"standing required before special");
 Authority dead;pair(dead);add(dead,c,2,at(-3000,0,1.57079632679f));check(dead.fire(c,{1,1,23,{1,0,0}},200).reject==Reject::none,"lethal shot");check(!dead.snapshot().players[0]->alive&&mask(dead,b)==0&&dead.sop_view(a)->activation==0,"death clears identity state immediately");
 const auto snap=dead.snapshot();const auto viewA=dead.sop_view(a),viewB=dead.sop_view(b);check(!dead.respawn(a,2,[&]{check(dead.join(a,1,at(0,0),100,100,gear,300),"temporary respawn");return false;}),"failed respawn");check(dead.snapshot()==snap&&dead.sop_view(a)==viewA&&dead.sop_view(b)==viewB,"exact failed respawn SOP rollback");
 check(dead.respawn(a,2,[&]{return dead.join(a,1,at(0,0),100,100,gear,300);}),"successful respawn");check(dead.sop_view(a)->life==2&&mask(dead,a)==0,"new life no inherited link");check(dead.special(a,1,1,true,true,300,1)==Reject::generation,"old life special rejected");
 Authority stun;pair(stun);add(stun,c,2,at(-3000,0,1.57079632679f));check(stun.equip(c,1,7,200)==Reject::none,"stun weapon");check(stun.fire(c,{1,1,7,{1,0,0}},200).reject==Reject::none,"stun shot");check(stun.snapshot().players[0]->stunned&&stun.snapshot().players[0]->specialPhase==SpecialPhase::none&&mask(stun,a)==2,"stun cancels phase but preserves link");
 check(stun.pose(a,1,2,stun.snapshot().players[0]->pose,201)==Reject::none,"stun stationary ack");check(stun.special(a,1,2,true,true,201)==Reject::dead,"stun start prohibited");
 auto w=fixture_weapons();stun.begin(2,floor(),w);check(!stun.sop_view(a)&&stun.special(a,1,3,true,true,300)==Reject::generation,"epoch clears SOP");
 std::cout<<"SOP Authority phase/input/union/reset/jam/death/stun/rollback PASS\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

