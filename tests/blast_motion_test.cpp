#include "combat_service.h"
#include "gekko_test_profiles.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt;
using namespace mgo2mt::combat;
namespace {
unsigned checks=0;
constexpr Identity self{0,1,100},enemy{1,2,200},ally{2,3,300};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
Pose at(float x=0,float z=0,float y=2){Pose p;p.feet={x,y,z};return p;}
std::shared_ptr<const stage::Collision> world(std::optional<float> wall={},float ceiling=0){
 std::vector<Vec3> vertices{{-100000,0,-100000},{100000,0,-100000},{100000,0,100000},{-100000,0,100000}};
 std::vector<stage::CollisionTriangle> triangles{{{0,2,1},stage::attribute::native_solid},{{0,3,2},stage::attribute::native_solid}};
 if(wall){unsigned i=unsigned(vertices.size());vertices.insert(vertices.end(),{{*wall,0,-100000},{*wall,10000,-100000},{*wall,10000,100000},{*wall,0,100000}});triangles.push_back({{i,i+1,i+2},stage::attribute::native_solid});triangles.push_back({{i,i+2,i+3},stage::attribute::native_solid});}
 if(ceiling){unsigned i=unsigned(vertices.size());vertices.insert(vertices.end(),{{-100000,ceiling,-100000},{100000,ceiling,-100000},{100000,ceiling,100000},{-100000,ceiling,100000}});triangles.push_back({{i,i+1,i+2},stage::attribute::native_solid});triangles.push_back({{i,i+2,i+3},stage::attribute::native_solid});}
 return std::make_shared<const stage::Collision>(stage::Collision::make(std::move(vertices),std::move(triangles)));
}
std::vector<Weapon> weapons(){
 Weapon rifle;rifle.id=25;rifle.damage=100;rifle.intervalMs=100;rifle.reloadMs=1000;rifle.magazine=10;rifle.reserve=20;rifle.range=50000;
 auto grenade=rifle;grenade.id=52;grenade.magazine=2;grenade.reserve=6;grenade.nativeProjectile=true;grenade.range=100000;
 auto gun=rifle;gun.id=500;gun.mountedOnly=true;
 return {rifle,grenade,gun};
}
std::unique_ptr<Authority> host(std::shared_ptr<const stage::Collision> geometry=world(),Policy policy={},Pose initial=at(),bool peers=false,std::optional<blast_motion::Policy> settings={}){
 auto h=std::make_unique<Authority>(policy);auto profiles=weapons();profiles[1].blastMotion=settings;h->begin(1,std::move(geometry),profiles);const uint16_t gear[]{25,52};
 check(h->join(self,policy.freeForAll?0:1,initial,10000,10000,gear,0),"join blast victim");
 if(peers){check(h->join(enemy,policy.freeForAll?0:2,at(2500),10000,10000,gear,0),"join enemy");check(h->join(ally,policy.freeForAll?0:1,at(-2500),10000,10000,gear,0),"join teammate");}
 h->active(true);return h;
}
burning::Blast grenade(uint32_t damage=100,uint64_t serial=1){return {{{1,self.slot,self.instance,self.character,1},52,0,1},serial,{-600,700,0},6000,damage,false};}
Player player(Authority& h,Identity id=self){return *h.snapshot().players[id.slot];}
const Event* impulse(const Decision& d,Identity target=self){auto i=std::find_if(d.events.begin(),d.events.end(),[&](const auto&e){return e.kind==EventKind::knockback&&e.target==target;});return i==d.events.end()?nullptr:&*i;}
void finish(Authority&h,const stage::Collision& geometry,uint64_t first=20){
 for(uint64_t now=first;now<=first+2000&&player(h).flightId;now+=20){h.advance(now);const auto p=player(h);check(geometry.clear(p.pose.feet,p.pose.capsule,stage::query::player),"every HOST flight endpoint remains outside solid geometry");}
 check(!player(h).flightId&&!player(h).blastFlight&&player(h).pose.feet[1]>=1.9f&&player(h).pose.feet[1]<3,"checked landing clears blast state");
}
void self_flight(){
 auto geometry=world();auto h=host(geometry);auto before=player(*h);const auto inventory=h->item_held(self,1)->slots;
 auto result=h->explode(grenade(),0);const auto* push=impulse(result);
 check(bool(result)&&push&&valid_knockback_event(*push)&&push->source==self&&push->sourceLife==1&&push->targetLife==1,"own grenade emits targeted HOST velocity");
 check(push->normal[0]>0&&push->normal[1]>0&&push->position[1]==852,"native outward/upward impulse at body center");
 auto p=player(*h);check(p.hp==9900&&p.blastFlight&&p.flightId&&valid_flight(p),"self damage also starts living blast flight with FF off");
 check(h->item_held(self,1)->slots==inventory,"blast leaves inventory intact");
 check(h->equip(self,1,52,0)==Reject::unavailable&&h->reload(self,1,0).reject==Reject::unavailable&&h->fire(self,{1,1,25,{0,0,1}},0).reject==Reject::unavailable,"flight gates equipment reload and fire");
 auto fake=at(50000,60000);fake.capsule.height=1100;fake.yaw=2;
 check(h->pose(self,1,1,fake,100)==Reject::none&&player(*h).pose==before.pose,"wire pose cannot control HOST blast trajectory");
 check(h->pose(self,1,1,fake,100)==Reject::sequence&&h->pose(self,2,2,fake,100)==Reject::generation&&h->pose(self,1,2,fake,100,2)==Reject::generation,"flight sequence epoch and life checks retained");
 h->advance(100);p=player(*h);check(p.pose.feet[0]>100&&p.pose.feet[1]>100,"HOST moves living victim through the air");
 const auto snapshot=h->snapshot();h->advance(99);check(h->snapshot()==snapshot,"backward HOST clock cannot rewind flight");
 check(h->explode(grenade(),100).reject==Reject::sequence&&h->snapshot()==snapshot,"duplicate explosion cannot stack impulse or damage");
 finish(*h,*geometry,120);p=player(*h);check(p.pose.feet[0]>500,"blast travels before landing");
 auto step=p.pose;step.feet[2]+=100;check(h->pose(self,1,2,step,2300)==Reject::none,"normal movement resumes after landing");
 check(bool(h->fire(self,{1,2,25,{0,0,1}},2300)),"normal firing resumes after landing");
}
void friendly_and_cover(){
 auto h=host(world(),{},at(),true);auto hit=h->explode(grenade(),0);
 check(impulse(hit,self)&&impulse(hit,enemy)&&!impulse(hit,ally),"self and enemy receive blast; FF-off teammate does not");
 check(player(*h,ally).hp==10000&&!player(*h,ally).flightId,"FF-off teammate has neither HP damage nor forced movement");
 Policy friendly;friendly.friendlyFire=true;auto enabled=host(world(),friendly,at(),true);check(impulse(enabled->explode(grenade(),0),ally),"FF on permits teammate knockback");
 Policy dm;dm.freeForAll=true;auto free=host(world(),dm,at(),true);auto area=grenade();area.source.team=0;check(impulse(free->explode(area,0),ally),"FFA does not apply team immunity");
 auto occluded=host(world(-300.f));check(!impulse(occluded->explode(grenade(),0))&&!player(*occluded).flightId&&player(*occluded).hp==10000,"real wall blocks both blast damage and knockback");
}
void collision(){
 auto barrier=world(1000.f);auto h=host(barrier);h->explode(grenade(),0);finish(*h,*barrier);check(player(*h).pose.feet[0]<740,"swept blast capsule cannot cross a wall");
 auto roof=world({},1900.f);auto low=host(roof);low->explode(grenade(),0);float peak=0;
 for(uint64_t now=20;now<=1500&&player(*low).flightId;now+=20){low->advance(now);auto p=player(*low);peak=(std::max)(peak,p.pose.feet[1]);check(roof->clear(p.pose.feet,p.pose.capsule,stage::query::player),"ceiling flight remains clear");}
 check(peak<201&&!player(*low).flightId,"ceiling removes upward speed and permits landing");
 auto peer=host();const uint16_t gear[]{25};check(peer->join(enemy,2,at(1000),10000,10000,gear,0),"peer obstacle admitted");auto area=grenade();area.radius=900;peer->explode(area,0);finish(*peer,*world());check(player(*peer).pose.feet[0]<480,"blast cannot sweep through another player");
}
void states(){
 auto crouch=at();crouch.capsule.height=1100;auto h=host(world({},1200.f),{},crouch);h->explode(grenade(),0);check(player(*h).blastFlight&&player(*h).pose.capsule.height==1100,"crouched blast keeps a collision-safe capsule instead of standing into ceiling");finish(*h,*world({},1200.f));
 auto prone=at();prone.capsule.height=560;prone.capsule.radius=260;auto p=host(world({},650.f),{},prone);auto proneBlast=grenade();proneBlast.position[1]=300;p->explode(proneBlast,0);check(player(*p).blastFlight&&valid_flight(player(*p)),"prone capsule also supports HOST blast motion");finish(*p,*world({},650.f));
 auto stunned=host();auto area=grenade();area.staminaDamage=10000;stunned->explode(area,0);check(player(*stunned).stunned&&player(*stunned).blastFlight&&valid_flight(player(*stunned)),"stunned living victim remains physically in flight");finish(*stunned,*world());check(player(*stunned).stunned,"landing does not erase legitimate stamina stun");
 auto reload=host();check(bool(reload->fire(self,{1,1,25,{0,0,1}},0))&&bool(reload->reload(self,1,0)),"reload setup");reload->explode(grenade(),10);check(player(*reload).blastFlight&&!player(*reload).reloadUntil&&!player(*reload).reloadElapsedMs&&player(*reload).ammo==9,"blast cancels reload without granting ammunition");
 auto mounted=std::make_unique<Authority>();mounted->begin(1,world(),weapons());mounted::Registry registry;mounted::Type t;t.id="fixture";t.name="Fixture";t.model="mounted/fixture.gwm";t.weapon=500;registry.types.push_back(t);registry.placements.push_back({20,1,t.id,{0,2,0},0});check(mounted->configure_mounted(registry,20),"mounted blast scene");const uint16_t gear[]{25,52};check(mounted->join(self,1,at(),10000,10000,gear,0),"mounted victim");mounted->active(true);check(mounted->pose(self,1,1,at(),0)==Reject::none&&mounted->mount(self,1,1,{mounted::Action::mount,1,1},0)==Reject::none,"occupy mounted weapon");mounted->explode(grenade(),0);check(player(*mounted).blastFlight&&!player(*mounted).mountedId&&player(*mounted).weapon==25,"blast releases mounted weapon and restores carried selection");
 auto seated=std::make_unique<Authority>();seated->begin(1,world(),weapons());registry.types[0].kind=mounted::Kind::catapult;registry.types[0].weapon=0;registry.types[0].operatorOffset={0,1000,-600};check(seated->configure_mounted(registry,20),"catapult blast scene");check(seated->join(self,1,at(0,-600),10000,10000,gear,0),"catapult blast victim");seated->active(true);check(seated->pose(self,1,1,player(*seated).pose,0)==Reject::none&&seated->mount(self,1,1,{mounted::Action::mount,1,1},0)==Reject::none,"occupy elevated seat");auto seat=player(*seated).pose.feet;auto stunBlast=grenade();stunBlast.position=seat;stunBlast.position[1]+=700;stunBlast.staminaDamage=10000;seated->explode(stunBlast,0);check(player(*seated).blastFlight&&player(*seated).stunned&&!player(*seated).mountedId&&player(*seated).pose.feet==seat,"stun blast starts at elevated seat without ground recovery teleport");
 auto mechanical=std::make_unique<Authority>();auto gekkoProfiles=gekko_test_profiles(weapons()[0]);mechanical->begin(1,world(),gekkoProfiles);check(mechanical->join(self,1,at(),10000,10000,std::array<uint16_t,1>{25},0),"Gekko blast fixture");mechanical->active(true);check(mechanical->assign_special(self,special_pc::Kind::gekko,true,0)==Reject::none,"Gekko conversion");auto previousHp=player(*mechanical).hp;auto mechanicalHit=mechanical->explode(grenade(),0);check(player(*mechanical).hp==previousHp-100&&!player(*mechanical).flightId&&!impulse(mechanicalHit),"Gekko takes blast damage without human flight capsule");
 auto ladderHost=host(world(),{},at(-400,0,4));ladder::Anchor anchor{1,{0,4,0},{0,3004,0},{-400,4,0},{1100,3004,0},1.57079633f};ladderHost->configure_ladders({anchor});check(ladderHost->pose(self,1,1,player(*ladderHost).pose,0)==Reject::none&&bool(ladderHost->ladder_action(self,1,1,{ladder::Action::enter,1,0},0)),"ladder blast setup");ladderHost->explode(grenade(),0);check(player(*ladderHost).blastFlight&&!player(*ladderHost).ladderAnchor,"blast releases ladder into HOST flight");
}
void policy_lifecycle(){
 blast_motion::Policy disabled;disabled.enabled=false;auto off=host(world(),{},at(),false,disabled);check(!impulse(off->explode(grenade(),0))&&!player(*off).flightId&&player(*off).hp==9900,"explicit disabled policy preserves explosion damage without motion");
 blast_motion::Policy configured;configured.horizontalSpeed=1000;configured.upwardSpeed=500;configured.minimumScale=1;auto tuned=host(world(),{},at(),false,configured);auto event=tuned->explode(grenade(),0);check(impulse(event)->normal==Vec3{1000,500,0},"configured HOST impulse values are effective");
 auto obstructed=host();check(obstructed->object_world(world(0.f)),"changed solid world fixture");auto blockedBlast=grenade();blockedBlast.position={0,700,0};auto blockedResult=obstructed->explode(blockedBlast,0);check(player(*obstructed).hp==9900&&!player(*obstructed).flightId&&!impulse(blockedResult),"invalid current capsule cannot emit living motion that HOST did not start");
 auto capped=host();for(unsigned i=1;i<=10;++i){auto result=capped->explode(grenade(1,i),0);check(impulse(result)&&valid_knockback_event(*impulse(result)),"genuine successive blasts remain velocity-bounded");}
 auto timeout=host();timeout->explode(grenade(),0);timeout->advance(100);const auto airborne=player(*timeout).pose.feet;timeout->advance(2001);check(player(*timeout).pose.feet==airborne&&player(*timeout).blastFlight,"large clock gap retains current position instead of teleporting to blast origin");finish(*timeout,*world(),2021);
 auto death=host();death->explode(grenade(),0);death->advance(100);auto fatal=grenade(100000,2);fatal.position=player(*death).pose.feet;fatal.position[1]+=500;auto result=death->explode(fatal,100);check(!player(*death).alive&&!player(*death).flightId&&!player(*death).blastFlight&&impulse(result),"death clears living motion while preserving corpse velocity event");auto killed=std::find_if(result.events.begin(),result.events.end(),[](const auto&e){return e.kind==EventKind::death;});auto launched=std::find_if(result.events.begin(),result.events.end(),[](const auto&e){return e.kind==EventKind::knockback;});check(killed<launched&&valid_knockback_event(*launched),"death precedes usable ragdoll launch event");
 const uint16_t gear[]{25,52};check(death->respawn(self,2,[&]{return death->join(self,1,at(),10000,10000,gear,100);}),"new life respawn");check(!player(*death).flightId&&!player(*death).blastFlight,"respawn inherits no old impulse");auto delayed=grenade(100,3);check(bool(death->explode(delayed,100))&&!player(*death).flightId&&player(*death).hp==10000,"previous-life self blast is not the new life's self exception");
 auto stop=host();stop->explode(grenade(),0);stop->advance(100);stop->active(false);check(!player(*stop).flightId&&!player(*stop).blastFlight,"round end clears blast flight");
 auto reset=host();reset->explode(grenade(),0);check(reset->world(world())&&!player(*reset).flightId&&!player(*reset).blastFlight,"world replacement clears old flight");reset->begin(2,world(),weapons());check(reset->explode(grenade(),0).reject==Reject::not_active,"new inactive epoch cannot replay explosion");reset->active(true);check(reset->explode(grenade(),0).reject==Reject::generation,"old epoch explosion rejected");
 auto leave=host();leave->explode(grenade(),0);check(leave->leave(self)&&!leave->snapshot().players[0],"disconnect removes the flight owner");leave->advance(500);
}
void actual_grenade(){
 auto h=host();auto p=at();p.pitch=-1.4f;check(h->pose(self,1,1,p,0)==Reject::none&&h->equip(self,1,52,0)==Reject::none,"aim actual grenade down");
 const Vec3 direction{0,std::sin(p.pitch),std::cos(p.pitch)};auto fired=h->fire(self,{1,1,52,direction},0);check(bool(fired)&&h->debug_projectile_count()==1,"actual HOST grenade launch");
 bool ownBlast=false;for(uint64_t now=100;now<=2400;now+=100){auto result=h->advance_projectiles(now);ownBlast|=impulse(result,self)!=nullptr;h->advance(now);}
 check(ownBlast&&player(*h).hp==9900&&!h->debug_projectile_count(),"real thrown HE fuse damages and launches its own thrower");
}
void wire_and_service(){
 auto h=host();const auto baseline=h->snapshot();auto result=h->explode(grenade(),0);h->advance(250);auto snapshot=h->snapshot();
 wire::Frame frame{snapshot,wire::Status::active,result.events,h->sop_view(self).value()};auto bytes=wire::encode(frame);check(wire::version==27&&std::get<wire::Frame>(wire::decode(bytes))==frame,"blast state and targeted velocity roundtrip on GWCB27");
 Replica replica;check(replica.snapshot(baseline)&&replica.snapshot(snapshot),"replica accepts blast snapshot");check(replica.events(result.events).size()==result.events.size()&&replica.events(result.events).empty(),"velocity event consumed exactly once");
 auto invalid=snapshot;invalid.players[0]->flightId=0;check(!replica.snapshot(invalid),"blast flag without flight ID rejected");invalid=snapshot;invalid.players[0]->flightElapsedMs=32000;check(!replica.snapshot(invalid),"nonrepresentable elapsed age rejected");
 auto impulseEvent=*impulse(result);auto malformed=impulseEvent;malformed.normal={15001,0,0};check(!valid_knockback_event(malformed),"overlimit impulse rejected");malformed=impulseEvent;malformed.targetLife=0;check(!valid_knockback_event(malformed),"unscoped impulse target rejected");malformed=impulseEvent;malformed.hpDamage=1;check(!valid_knockback_event(malformed),"impulse cannot hide separate HP damage");malformed=impulseEvent;malformed.normal={std::numeric_limits<float>::quiet_NaN(),0,0};check(!valid_knockback_event(malformed),"nonfinite velocity rejected");
 for(unsigned i=0;i<24;++i){auto p=*snapshot.players[0];p.identity={uint8_t(i),uint16_t(i+1),100+i};p.flightId=uint16_t(i+1);snapshot.players[i]=p;}
 impulseEvent.source=snapshot.players[0]->identity;impulseEvent.target=snapshot.players[1]->identity;auto sop=h->sop_view(self).value();sop.recipient=snapshot.players[0]->identity;frame={snapshot,wire::Status::active,{impulseEvent},sop};bytes=wire::encode(frame);check(bytes.size()<=2000&&std::get<wire::Frame>(wire::decode(bytes))==frame,"24 blast flights plus full SOP and impulse remain within 2000 bytes");std::cout<<"24 blast flights + SOP + impulse bytes="<<bytes.size()<<'\n';
 Service service(1);service.configure(world(),weapons());const uint16_t gear[]{25};check(service.authority().join(self,1,at(),10000,10000,gear,0),"service victim");service.authority().active(true);check(service.admit(self)&&service.receive(self,wire::encode(wire::Accept{1}),0),"service connection");wire::Input input;input.epoch=1;input.sequence=1;input.pose=at();input.weapon=25;check(service.receive(self,wire::encode(input),0),"service initial input");service.poll(0);service.deliveries();service.authority().explode(grenade(),1);service.poll(1);service.deliveries();service.poll(51);bool correction=false;for(const auto& packet:service.deliveries()){check(packet.payload.size()<=2000,"service packet budget");auto record=wire::decode(packet.payload);if(auto f=std::get_if<wire::Frame>(&record))correction|=f->snapshot.players[0]->blastFlight&&f->snapshot.players[0]->pose.feet!=at().feet;}check(correction,"living blast position broadcast within 50ms");
 input.sequence=2;input.suspended=true;check(service.receive(self,wire::encode(input),60),"menu input admitted");service.poll(60);check(service.authority().snapshot().players[0]->blastFlight,"menu pause cannot cancel authoritative impulse");
}
}
int main(){try{self_flight();friendly_and_cover();collision();states();policy_lifecycle();actual_grenade();wire_and_service();std::cout<<"PASS "<<checks<<" HOST grenade knockback checks\n";return 0;}catch(const std::exception&e){std::cerr<<"after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
