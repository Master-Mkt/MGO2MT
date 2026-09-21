#include "combat_burning.h"
#include "combat_authority.h"
#include <iostream>
#include <limits>
using namespace mgo2mt;
namespace {
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
using namespace combat;
burning::Key key(uint8_t slot=0){return {7,slot,1,uint32_t(100+slot),1};}
burning::Source source(){return {key(),53,0,1};}
std::shared_ptr<const stage::Collision> empty(){return std::make_shared<const stage::Collision>(stage::Collision::make({},{}));}
std::shared_ptr<const stage::Collision> wall(){return std::make_shared<const stage::Collision>(stage::Collision::make({{500,-1000,-2000},{500,4000,-2000},{500,4000,2000},{500,-1000,2000}},{{{0,1,2}},{{0,2,3}}}));}
Weapon gun(){Weapon w;w.id=25;w.damage=50;w.intervalMs=100;w.reloadMs=1000;w.magazine=30;w.reserve=90;w.range=10000;return w;}
void admitted(Authority& a,Identity id,uint8_t team,float x,uint64_t now=0){Pose p;p.feet={x,0,0};std::array<uint16_t,1> inventory{25};check(a.join(id,team,p,1000,1000,inventory,now),"join actual authority fixture");}
burning::Blast blast(uint64_t serial=1){return {source(),serial,{0,850,0},3500,0,true};}
void core(){
 burning::State state;state.bind(key(1));auto objectOnly=source();objectOnly.weapon=0;objectOnly.object=123;
 check(!state.ignite(key(1),objectOnly,1,0)&&!burning::valid(burning::Blast{objectOnly,1,{0,0,0},1000,1,true}),"missing source weapon rejected before wire events; no fabricated weapon fallback");
 check(state.ignite(key(1),source(),1,0),"ignite scoped player");
 check(!state.ignite(key(1),source(),1,0),"same event never refreshes");
 auto step=state.advance(key(1),1000,1000,true,true,false);check(step.damage==50&&step.burning&&step.remainingMs==5000,"native five percent per second");
 check(state.advance(key(1),999,1000,true,true,false).damage==0,"backward clock does not damage");
 check(state.advance(key(1),1000,1000,true,true,false).damage==0,"same clock does not double damage");
 check(!state.ignite(key(1),source(),2,1100),"refresh must settle previous elapsed first");
 check(state.ignite(key(1),source(),2,1000),"fresh event refreshes nonstacking duration");
 step=state.advance(key(1),7000,1000,true,true,false);check(step.damage==300&&!step.burning,"refreshed lifetime expires exactly");
 check(state.advance(key(1),9000,1000,true,true,false).damage==0,"expired no residual damage");
 for(uint64_t cadence:{1ull,7ull,16ull,1000ull,6000ull}){burning::State s;s.bind(key(1));s.ignite(key(1),source(),1,0);uint32_t damage=0;for(uint64_t t=cadence;t<6000;t+=cadence)damage+=s.advance(key(1),t,1001,true,true,false).damage;damage+=s.advance(key(1),6000,1001,true,true,false).damage;check(damage==300,"fraction carry independent of frame cadence");}
 for(unsigned reason=0;reason<3;++reason){burning::State s;s.bind(key(1));s.ignite(key(1),source(),1,0);auto r=s.advance(key(1),1000,1000,reason!=0,reason!=1,reason==2);check(!r.burning&&!r.damage,"dead inactive or submerged clears without retrospective damage");}
 burning::State s;s.bind(key(1));check(!s.ignite(key(2),source(),1,0),"different full identity rejected");check(!s.ignite(key(1),source(),1,UINT64_MAX-1),"duration overflow rejected");
 check(!s.ignite(key(1),source(),1,0,{0,50})&&!s.ignite(key(1),source(),1,0,{60001,50}),"invalid policy rejected");s.ignite(key(1),source(),1,0);auto newLife=key(1);++newLife.life;s.bind(newLife);check(!s.active(),"new life clears state");
 burning::Replay replay;auto src=source();check(replay.accept(src,10)&&!replay.accept(src,10)&&!replay.accept(src,9),"per producer replay highwater");src.object=999;check(replay.accept(src,10),"same shot distinct drum is not a replay");src.weapon=50;check(replay.accept(src,10),"different source type channel");auto old=src;++src.actor.life;check(replay.accept(src,1)&&!replay.accept(old,10),"life preserves old replay tombstone");
 auto b=blast();check(burning::exposed(b,{1000,0,0},{260,1700,2},nullptr),"near capsule exposed");check(!burning::exposed(b,{1000,0,0},{260,1700,2},wall().get()),"real wall blocks blast");check(!burning::exposed(b,{3760,0,0},{260,1700,2},nullptr),"strict radius boundary");b.radius=std::numeric_limits<float>::quiet_NaN();check(!burning::valid(b),"NaN blast rejected");
 check(burning::flames(key(),false,{0,0,0},{260,1700,2},0).empty(),"no burning means no visual");const auto lines=burning::flames(key(),true,{0,0,0},{260,1700,2},300);check(lines.size()==32,"bounded visible procedural flames");
}
void authority(){
 const std::array<Weapon,1> weapons{gun()};const Identity shooter{0,1,100},enemy{1,1,101},friendId{2,1,102};Authority a;a.begin(7,empty(),weapons);admitted(a,shooter,1,0);admitted(a,enemy,2,1000);admitted(a,friendId,1,-1000);a.active(true);
 auto b=blast();auto first=a.explode(b,0);check(bool(first)&&a.snapshot().players[0]->burning&&a.snapshot().players[1]->burning&&!a.snapshot().players[2]->burning,"self and enemy burn, FF-off teammate unaffected");
 check(a.explode(b,0).reject==Reject::sequence,"blast duplicate rejected");auto dot=a.advance_burning(1000);check(dot.events.size()==2&&a.snapshot().players[1]->hp==950,"host gradual damage events");
 check(a.advance_burning(1000).events.empty()&&a.advance_burning(999).events.empty(),"host equal/backward clock idempotent");
 check(a.leave(shooter),"old source may leave");dot=a.advance_burning(2000);check(dot.events.size()==1&&dot.events[0].source==shooter&&dot.events[0].sourceLife==1,"owner identity retained after departure");
 a.active(false);a.advance_burning(2000);check(!a.snapshot().players[1]->burning,"pause clears visual and damage state");
 Authority blocked;blocked.begin(7,wall(),weapons);admitted(blocked,shooter,1,0);admitted(blocked,enemy,2,1000);blocked.active(true);blocked.explode(blast(),0);check(!blocked.snapshot().players[1]->burning,"world occlusion integrated at authority");
 Authority death;death.begin(7,empty(),weapons);admitted(death,shooter,1,0);admitted(death,enemy,2,1000);death.active(true);b=blast();b.damage=1000;auto events=death.explode(b,0);check(bool(events)&&!death.snapshot().players[1]->alive&&!death.snapshot().players[1]->burning&&!death.snapshot().players[1]->stunned,"instant death never starts persistent burn");check(death.scores()[1]->deaths==1&&death.scores()[0]->kills==1,"exact owner credit and victim death once");check(death.explode(b,0).reject==Reject::sequence&&death.scores()[1]->deaths==1,"replay cannot duplicate death");
}
}
int main(){try{core();authority();std::cout<<"burn timer/fixedpoint/replay/exposure/flames and Authority damage/FF/death PASS\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
