#include "world_inventory.h"
#include <atomic>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
using namespace mgo2mt::items;
namespace {
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
void check(const Result& r,const char* s){check(bool(r),s);}
const Actor a{1,11,101,1},b{2,12,102,1};const Scope scope{3,9};
DropPolicy allow(){DropPolicy p;p.drop=DropOverride::allow;return p;}
HeldSlot gun(){return {{25,1,17,93,0,Resource::ammunition},1};}
Request request(Actor actor,uint64_t sequence,bool permitted=true){return {scope,actor,sequence,permitted};}
std::string entry(std::string suffix=""){return "{\"id\":25,\"domain\":\"weapon\",\"name\":\"AK102\",\"source\":\"test-only\",\"originalKind\":\"unknown\",\"originalDrop\":null,\"originalEmptyDiscard\":null,\"drop\":\"allow\",\"emptyDiscard\":false"+suffix+"}";}
std::string document(std::string entries){return "{\"schema\":\"MGO2MT.item_drop_policy\",\"version\":1,\"entries\":["+entries+"]}";}
}
int main(int argc,char** argv){try{
 DropPolicies policies;std::string error;check(policies.parse(document(entry()),error),"strict policy parse");check(policies.find(25)&&policies.find(25)->policy.allows_drop()&&!policies.find(25)->policy.discards_empty(),"independent overrides");
 for(auto bad:{document(entry()+","+entry()),document(entry(",\"id\":26")),document(entry(",\"typo\":true")),document(entry())+"x",document(entry()).replace(document(entry()).find("version\":1")+9,1,"2")})check(!policies.parse(bad,error)&&policies.entries().size()==1&&policies.find(25),"atomic rejected reload");
 auto escaped=document(entry());auto pos=escaped.find("AK102");escaped.replace(pos,5,"\\u65e5\\ud83d\\ude00");check(policies.parse(escaped,error),"JSON Unicode scalar escapes");escaped=document(entry());pos=escaped.find("AK102");escaped.replace(pos,5,"\\ud800");check(!policies.parse(escaped,error),"unpaired JSON surrogate");
 if(argc>1){check(policies.load(argv[1],error),error.c_str());check(policies.entries().size()>=183&&policies.find(25)&&policies.find(113,Domain::world_item)&&policies.find(113)->name!=policies.find(113,Domain::world_item)->name,"complete catalog namespaces");for(const auto&[id,e]:policies.entries())check(!e.policy.originalDrop&&!e.policy.originalEmptyDiscard&&!e.policy.allows_drop(),"unknown original stays null and default denied");}
 WorldInventory world({1,1});check(world.reset(scope)&&world.admit(a)&&world.admit(b),"admission");
 HeldSlot held=gun();auto initial=held;DropPolicy unknown;
 check(world.drop(request(a,1),held,1,{},unknown).code==ResultCode::policy&&held==initial,"unknown deny preserves inventory");
 check(world.drop(request(a,2,false),held,1,{},allow()).code==ResultCode::unauthorized&&held==initial,"authoritative permission gate");
 check(world.drop(request(a,2),held,1,{},allow()).code==ResultCode::replay,"denied attempt no later replay");
 auto dropped=world.drop(request(a,3),held,1,{},allow());check(dropped&&dropped.entity&&held.contents==Contents{}&&held.revision==2&&dropped.entity->contents==initial.contents,"atomic all ammo drop");
 auto other=gun(),before=other;check(world.drop(request(b,1),other,1,{},allow()).code==ResultCode::capacity&&other==before,"full capacity loses nothing");
 HeldSlot occupied=gun();check(world.pickup(request(b,2),dropped.entity->key,1,occupied,1).code==ResultCode::occupied&&world.size(PlacementKind::dropped)==1,"occupied no destruction");
 HeldSlot empty;check(world.pickup(request(b,3),dropped.entity->key,2,empty,1).code==ResultCode::stale&&empty.contents==Contents{},"entity revision");
 auto picked=world.pickup(request(b,4),dropped.entity->key,1,empty,1);check(picked&&empty.contents==initial.contents&&world.size(PlacementKind::dropped)==0,"pickup conserves ammo");
 HeldSlot duplicate;check(world.pickup(request(a,4),dropped.entity->key,1,duplicate,1).code==ResultCode::not_found,"competing pickup no duplicate");
 auto again=world.drop(request(b,5),empty,2,{},allow());check(again&&again.entity->key.id>dropped.entity->key.id,"entity IDs not reused");
 auto emptyGun=gun();emptyGun.contents.magazine=emptyGun.contents.reserve=0;auto vanished=allow();vanished.emptyDiscard=true;check(world.drop(request(a,5),emptyGun,1,{},vanished).destroyed&&emptyGun.contents==Contents{},"empty discard independent of full world capacity");
 auto deniedEmpty=gun();deniedEmpty.contents.magazine=deniedEmpty.contents.reserve=0;auto deny=vanished;deny.drop=DropOverride::deny;check(world.drop(request(a,6),deniedEmpty,1,{},deny).code==ResultCode::policy&&deniedEmpty.contents.item==25,"deny defeats empty discard");
 HeldSlot durable{{69,1,0,0,0,Resource::durable},1};check(!durable.contents.empty_resource(),"durable not empty from zero ammo");
 auto installed=world.install(request(a,7),durable,1,1,{},deny);check(installed&&installed.entity->kind==PlacementKind::installed,"install permission independent of drop override");
 check(world.consume(request(a,8),again.entity->key,1,Consume::magazine,1,allow()).code==ResultCode::invalid,"dropped gun is not installed weapon");
 HeldSlot retrieve;check(world.pickup(request(a,9),installed.entity->key,1,retrieve,1)&&retrieve.contents.item==69,"retrieve installed item");
 HeldSlot charge{{66,1,0,0,2,Resource::charges},1};auto chargePlacement=world.install(request(a,10),charge,1,1,{},allow());check(chargePlacement,"install charges");
 check(world.consume(request(a,11),chargePlacement.entity->key,1,Consume::charges,3,allow()).code==ResultCode::resource,"cannot overconsume");
 auto used=world.consume(request(a,12),chargePlacement.entity->key,1,Consume::charges,1,allow());check(used&&used.entity->contents.charges==1&&used.entity->revision==2,"remaining charges");
 auto usedUp=world.consume(request(a,13),chargePlacement.entity->key,2,Consume::charges,1,vanished);check(usedUp&&usedUp.destroyed&&world.size(PlacementKind::installed)==0,"use empty discard");
 auto nan=gun();Position invalid;invalid.x=std::numeric_limits<float>::quiet_NaN();check(world.drop(request(a,14),nan,1,invalid,allow()).code==ResultCode::invalid&&nan==gun(),"NaN placement no mutation");
 auto revived=a;revived.life=2;check(world.admit(revived)&&!world.admit(a),"life monotonic");check(world.drop(request(a,15),nan,1,{},allow()).code==ResultCode::identity,"old life rejected");check(world.drop(request(revived,1),nan,1,{},allow()).code==ResultCode::capacity,"new life starts sequence 1 without bypassing capacity");
 world.remove(a);check(world.drop(request(revived,15),nan,1,{},allow()).code==ResultCode::capacity,"old removal cannot remove new life");
 check(!world.configure({0,1})&&world.size(PlacementKind::dropped)==1,"unsafe capacity shrink rejected");check(world.configure({64,64}),"runtime expansion");
 check(!world.reset({2,9})&&!world.reset({3,8})&&world.size(PlacementKind::dropped)==1,"scope rollback rejected");check(world.reset(scope)&&world.size(PlacementKind::dropped)==1,"same scope reset idempotent");
 check(world.reset({4,1})&&world.snapshot().empty(),"epoch clears placements");
 // Independent configurable pools can grow beyond legacy 12 and candidate 64.
 WorldInventory many({65,65});check(many.reset(scope)&&many.admit(a),"capacity scope");
 for(uint64_t i=1;i<=65;++i){auto slot=gun();check(many.drop(request(a,i),slot,1,{},allow()),"65 configured dropped entities");}
 for(uint64_t i=66;i<=130;++i){HeldSlot slot{{64,1,0,0,1,Resource::charges},1};check(many.install(request(a,i),slot,1,1,{},allow()),"65 independent installed entities");}
 auto full=gun();check(many.drop(request(a,131),full,1,{},allow()).code==ResultCode::capacity&&full==gun(),"66th rejected without loss");check(many.size(PlacementKind::dropped)==65&&many.size(PlacementKind::installed)==65,"independent pools");
 // Concurrent pickup transaction: only one authoritative destination wins.
 WorldInventory race({1,0});race.reset(scope);race.admit(a);race.admit(b);auto source=gun();auto entity=race.drop(request(a,1),source,1,{},allow());HeldSlot left,right;std::atomic<unsigned> winners=0;
 std::thread t1([&]{if(race.pickup(request(a,2),entity.entity->key,1,left,1))++winners;});std::thread t2([&]{if(race.pickup(request(b,1),entity.entity->key,1,right,1))++winners;});t1.join();t2.join();check(winners==1&&(left.contents.item!=0)!=(right.contents.item!=0)&&race.snapshot().empty(),"concurrent single winner");
 std::cout<<"world_inventory_test PASS: atomic policy, inventory conservation, namespaces, capacity, epoch/life/replay and competing pickup\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
