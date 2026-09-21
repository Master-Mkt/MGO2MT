#include "item_drop_policy.h"
#include <cstdlib>
#include <iostream>
using namespace mgo2mt::items;
static void check(bool v,const char* text){if(!v){std::cerr<<text<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 check(argc==2,"catalog path required");
 DropPolicies p;std::string error;check(p.load(argv[1],error),error.c_str());
 check(p.entries().size()==282,"complete separate namespaces");
 size_t weapons=0,equipment=0,world=0;
 for(const auto&[key,e]:p.entries()){
  if(e.domain==Domain::weapon)++weapons;
  else if(e.domain==Domain::equipment)++equipment;
  else ++world;
  check(!e.policy.originalDrop.has_value()&&!e.policy.originalEmptyDiscard.has_value(),"unproved flags stay null");
  check(!e.policy.allows_drop()&&!e.policy.discards_empty(),"unknown default is conservative");
  check(e.policy.drop_decision().basis==PolicyBasis::unresolved&&e.policy.empty_decision().basis==PolicyBasis::unresolved,"unknown must not mean original prohibition");
 }
 check(weapons==181&&equipment==99&&world==2,"namespace row counts");
 for(uint32_t i=0;i<181;++i)check(p.find(i,Domain::weapon),"weapon index coverage");
 for(uint32_t i=0;i<99;++i)check(p.find(i,Domain::equipment),"equipment index coverage");
 check(!p.find(181)&&!p.find(99,Domain::equipment)&&!p.find(22,Domain::world_item),"no cross-namespace fallback");
 check(p.find(22,Domain::equipment)->name=="ENVG"&&p.find(140)->name=="ENVG", "separate ENVG identities");
 check(p.find(140,Domain::world_item)->name=="ibox_item_small", "world 140 is not ENVG");
 check(p.find(25)->name!=p.find(25,Domain::equipment)->name,"weapon25 not equipment25");
 check(p.find(98,Domain::equipment)->name=="IPOD MUSIC","last equipment record retained without granting usability");
 DropPolicy d;d.originalDrop=false;d.originalEmptyDiscard=false;
 check(!d.drop_decision().value&&d.drop_decision().basis==PolicyBasis::original_fact,"verified false is distinct from null");
 check(!d.empty_decision().value&&d.empty_decision().basis==PolicyBasis::original_fact,"verified empty false is distinct from null");
 d.drop=DropOverride::allow;d.emptyDiscard=true;
 check(d.allows_drop()&&d.discards_empty()&&d.drop_decision().basis==PolicyBasis::local_override&&d.empty_decision().basis==PolicyBasis::local_override,"overrides preserve source");
 d.originalDrop=true;d.drop=DropOverride::deny;d.emptyDiscard=false;
 check(!d.allows_drop()&&!d.discards_empty()&&d.empty_decision().basis==PolicyBasis::local_override,"explicit false override is not absent");
 const auto before=p.entries().size();const auto name=p.find(22,Domain::equipment)->name;
 check(!p.parse(R"({"schema":"MGO2MT.item_drop_policy","version":2,"entries":[]})",error),"unsupported version rejected");
 check(p.entries().size()==before&&p.find(22,Domain::equipment)->name==name,"failed reload retains full namespaces");
 std::cout<<"item drop policy namespaces/provenance PASS\n";
}
