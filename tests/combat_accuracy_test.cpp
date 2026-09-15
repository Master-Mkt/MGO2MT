#include "combat_initial_profile.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;using namespace mgo2win::combat;
namespace {
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
constexpr Identity shooter{0,1,101},target{1,1,102};
std::shared_ptr<const stage::Collision> wall(float depth=100000){return std::make_shared<const stage::Collision>(stage::Collision::make({{-200000,-200000,depth},{200000,-200000,depth},{200000,200000,depth},{-200000,200000,depth}},{{{0,1,2}},{{0,2,3}}}));}
std::vector<Weapon> profiles(bool enabled){Weapon ak;ak.id=25;ak.damage=2000;ak.intervalMs=100;ak.reloadMs=200;ak.magazine=30;ak.reserve=60;ak.range=200000;ak.nativeAkAccuracy=enabled;Weapon secondary;secondary.id=3;secondary.heldOnly=true;return {ak,secondary};}
void setup(Authority&a,bool enabled=true,uint64_t epoch=1){a.begin(epoch,wall(),profiles(enabled));check(a.join(shooter,1,{},1000,1000,std::array<uint16_t,2>{25,3},0),"shooter join");a.active(true);a.advance(0);}
Decision fire(Authority&a,uint32_t seq,uint64_t now,uint32_t life=1){check(a.pose(shooter,a.snapshot().epoch,seq,{},now,life)==Reject::none,"fresh pose");return a.fire(shooter,{a.snapshot().epoch,seq,25,{0,0,1},life},now);}
Vec3 impact(const Decision&d){for(auto&e:d.events)if(e.kind==EventKind::impact)return e.position;throw std::runtime_error("impact expected");}
}
int main(){try{
 check(initial_profiles(20,1,0)[0].nativeAkAccuracy&&!initial_profiles(20,1,0)[1].nativeAkAccuracy,"only initial AK opts into native accuracy");
 Authority a,b;setup(a);setup(b);check(a.accuracy(shooter,0)==3&&a.sop_view(shooter)->spreadMilliRadians==3&&!a.accuracy({0,2,101},0),"initial scoped footer baseline");
 auto first=fire(a,1,0),same=fire(b,1,0);check(bool(first)&&first.events==same.events&&a.accuracy(shooter,0)==7,"identical authoritative input gives identical ray and committed bloom");auto p=impact(first);check(std::hypot(p[0],p[1]-1550)>1&&std::hypot(p[0],p[1]-1550)<301,"actual impact follows first native cone");
 Authority tilted;setup(tilted);auto offsetShot=tilted.fire(shooter,{1,1,25,{std::sin(.02f),0,std::cos(.02f)},1},0);check(bool(offsetShot)&&offsetShot.events==first.events,"accepted HOST pose centers native cone despite legacy request-direction tolerance");
 auto reject=a.fire(shooter,{1,2,25,{0,0,1},1},1);check(reject.reject==Reject::interval&&a.accuracy(shooter,0)==7,"rejected cadence does not bloom");reject=a.fire(shooter,{1,3,25,{1,0,0},1},100);check(reject.reject==Reject::invalid_direction,"untrusted aim rejected");auto second=fire(a,4,100),secondClean=fire(b,2,100);check(bool(second)&&second.events==secondClean.events,"rejected requests do not consume random sequence");
 const auto revision=a.snapshot().revision;a.advance(101);check(a.snapshot().revision==revision,"same quantized angle no needless revision");a.advance(200);check(a.snapshot().revision>revision&&a.sop_view(shooter)->spreadMilliRadians==a.accuracy(shooter,200),"time convergence changes replicated revision");a.advance(1000);check(a.accuracy(shooter,1000)==3,"idle convergence");
 check(a.equip(shooter,1,3,1000)==Reject::none&&a.accuracy(shooter,1000)==0,"held-only equip hides cone");auto held=a.fire(shooter,{1,5,3,{0,0,1},1},1000);check(!held&&held.events.empty()&&a.accuracy(shooter,1000)==0,"held-only cannot shoot or bloom");check(a.equip(shooter,1,25,1000)==Reject::none&&a.accuracy(shooter,1000)==3,"equip reset");check(bool(fire(a,6,1000))&&a.accuracy(shooter,1000)==7,"fresh equipped shot");check(bool(a.reload(shooter,1,1001))&&a.accuracy(shooter,1001)==3,"accepted reload resets cone");check(!a.fire(shooter,{1,7,25,{0,0,1},1},1100)&&a.accuracy(shooter,1100)==3,"reloading cannot bloom");
 a.active(false);check(a.accuracy(shooter,1100)==0&&a.sop_view(shooter)->spreadMilliRadians==0,"inactive clears state/footer");a.active(true);a.advance(1201);check(a.accuracy(shooter,1201)==3,"resume baseline");
 Authority legacy;setup(legacy,false);auto straight=fire(legacy,1,0);check(bool(straight)&&impact(straight)==Vec3{0,1550,100000}&&legacy.accuracy(shooter,0)==0,"synthetic legacy weapon ray exact and spread disabled");
 Authority empty;setup(empty);for(uint32_t i=0;i<30;++i)check(bool(fire(empty,i+1,uint64_t(i)*100)),"finite magazine fires");const auto beforeEmpty=empty.accuracy(shooter,3000);check(fire(empty,31,3000).reject==Reject::no_ammo&&empty.accuracy(shooter,3000)==beforeEmpty,"empty magazine cannot change recovered state");
 // Locate a distant victim around the actual first ray. The aim-centre ray is
 // outside its capsule, so damage proves use of the sampled ray, not just VFX.
 Authority hit;setup(hit);const float z=99000;auto projected=Vec3{p[0]*z/p[2],(p[1]-1550)*z/p[2],z};const float offset=projected[0]>=0?220.f:-220.f;projected[0]+=offset;
 check(std::abs(projected[0])>260,"fixture victim excludes unspread aim");check(hit.join(target,2,{projected},1000,1000,std::array<uint16_t,1>{25},0),"sampled ray victim joins");auto damaged=fire(hit,1,0);check(bool(damaged)&&!hit.snapshot().players[target.slot]->alive,"HOST HP uses sampled cone ray");
 Authority blocked;blocked.begin(1,wall(50000),profiles(true));check(blocked.join(shooter,1,{},1000,1000,std::array<uint16_t,1>{25},0)&&blocked.join(target,2,{projected},1000,1000,std::array<uint16_t,1>{25},0),"occluded fixture");blocked.active(true);auto stopped=fire(blocked,1,0);check(bool(stopped)&&blocked.snapshot().players[target.slot]->hp==1000&&std::abs(impact(stopped)[2]-50000)<.1f,"spread ray still stops at nearer solid wall");
 check(hit.accuracy(target,0)==0,"death hides accuracy");
 const auto beforeFailedGrant=hit.snapshot();const auto sourceFooter=hit.sop_view(shooter),deadFooter=hit.sop_view(target);
 check(!hit.respawn(target,2,[&]{check(hit.join(target,2,{projected},1000,1000,std::array<uint16_t,1>{25},100),"failed grant temporary join");return false;}),"explicit grant failure");check(hit.snapshot()==beforeFailedGrant&&hit.sop_view(shooter)==sourceFooter&&hit.sop_view(target)==deadFooter,"failed grant restores snapshot and every recipient footer exactly");
 bool threw=false;try{hit.respawn(target,2,[&]()->bool{check(hit.join(target,2,{projected},1000,1000,std::array<uint16_t,1>{25},100),"throwing grant temporary join");throw std::runtime_error("intentional grant failure");});}catch(const std::runtime_error&){threw=true;}check(threw&&hit.snapshot()==beforeFailedGrant&&hit.sop_view(shooter)==sourceFooter&&hit.sop_view(target)==deadFooter,"throwing grant restores snapshot and other blooming footer exactly");check(hit.respawn(target,2,[&]{return hit.join(target,2,{projected},1000,1000,std::array<uint16_t,1>{25},0);}),"respawn grant");hit.advance(0);check(hit.accuracy(target,0)==3&&hit.sop_view(target)->life==2,"new life fresh cone");check(hit.fire(target,{1,99,25,{0,0,1},1},0).reject==Reject::generation&&hit.accuracy(target,0)==3,"old life cannot change new life cone");check(hit.leave(shooter)&&hit.join({0,2,101},1,{},1000,1000,std::array<uint16_t,1>{25},0),"new instance");hit.advance(0);check(!hit.accuracy(shooter,0)&&hit.accuracy({0,2,101},0)==3,"full identity reset");setup(a,true,2);check(a.accuracy(shooter,0)==3&&a.fire(shooter,{1,99,25,{0,0,1},1},0).reject==Reject::generation,"new epoch rejects prior requests");
 std::cout<<"Combat accuracy PASS: approved shot RNG/rays/HP, rejected/held/empty/reload, time footer revision, identity/life/epoch/equip/inactive, synthetic off\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}



