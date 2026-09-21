#include "gcx_round_items.h"
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
using namespace mgo2mt;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F> void rejects(F f,const char* message){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,message);}
items::GcxItemLayout read(const std::string& text){std::istringstream in(text);return items::GcxItemLayout::read(in);}
const std::string fixture=
 "MGO2MT.GCX_ROUND_ITEMS 1 20 verified\n"
 "groups 2\n"
 "group 0 100 140 equipment 22 3\n"
 "anchor 1000 10 0 250 0 0.25\n"
 "anchor 1032 10 0 250 0 0.25\n"
 "anchor 1064 11 700 250 0 1.25\n"
 "group 1 100 113 equipment 10 1\n"
 "anchor 1000 10 0 250 0 0.25\n";
stage::Collision floor(){return stage::Collision::make({{-5000,0,-5000},{5000,0,-5000},{5000,0,5000},{-5000,0,5000}},{{{0,1,2}},{{0,2,3}}});}
std::optional<items::Contents> lookup(items::Domain domain,uint32_t item){
 if(item==65535)return {};
 items::Contents c;c.domain=domain;c.item=item;c.quantity=1;return c;
}
}
int main(int argc,char** argv){try{
 auto layout=read(fixture);check(layout.verified&&layout.map==20&&layout.groups.size()==2,"verified source parser");
 check(layout.groups[0].anchors.size()==3&&layout.groups[0].anchors[0].position==layout.groups[0].anchors[1].position,"equal positions retain separate weighted candidates");
 check(!read("MGO2MT.GCX_ROUND_ITEMS 1 4 unavailable\ngroups 0\n").verified,"unavailable is not empty verified layout");
 check(read("MGO2MT.GCX_ROUND_ITEMS 1 4 verified\ngroups 0\n").verified,"explicit verified empty retained");
 for(const auto& bad:std::vector<std::string>{
  "MGO2MT.GCX_ROUND_ITEMS 2 20 verified\ngroups 0\n",
  "MGO2MT.GCX_ROUND_ITEMS 1 255 verified\ngroups 0\n",
  "MGO2MT.GCX_ROUND_ITEMS 1 20 guessed\ngroups 0\n",
  "MGO2MT.GCX_ROUND_ITEMS 1 20 verified\ngroups 513\n",
  "MGO2MT.GCX_ROUND_ITEMS 1 20 verified\ngroups 0\nextra\n",
  "MGO2MT.GCX_ROUND_ITEMS 1 20 verified\ngroups 1\ngroup 0 1 140 equipment 22 0\n",
  "MGO2MT.GCX_ROUND_ITEMS 1 20 verified\ngroups 1\ngroup 0 1 140 weapon 140 1\nanchor 1 2 0 0 0 0\n",
  "MGO2MT.GCX_ROUND_ITEMS 1 20 verified\ngroups 1\ngroup 0 1 113 equipment 22 1\nanchor 1 2 0 0 0 0\n",
  "MGO2MT.GCX_ROUND_ITEMS 1 20 unavailable\ngroups 1\ngroup 0 1 140 equipment 22 1\nanchor 1 2 0 0 0 0\n",
  "MGO2MT.GCX_ROUND_ITEMS 1 20 verified\ngroups 1\ngroup 0 1 140 equipment 22 1\nanchor -1 2 0 0 0 0\n",
  "MGO2MT.GCX_ROUND_ITEMS 1 20 verified\ngroups 1\ngroup 0 1 140 equipment 22 1\nanchor 1 2 nan 0 0 0\n",
  "MGO2MT.GCX_ROUND_ITEMS 1 20 verified\ngroups 1\ngroup 0 1 140 equipment 22 1\nanchor 1 2 0 0 0\n",
  "MGO2MT.GCX_ROUND_ITEMS 1 20 verified\ngroups 1\ngroup 0 1 140 equipment 22 2\nanchor 1 2 0 0 0 0\nanchor 1 3 3 0 0 0\n"
 })rejects([&]{read(bad);},"malformed/unresolved source rejected");
 rejects([&]{read(fixture+std::string(1025,' '));},"oversized source row rejected");
 stage::CboxLayout cbox;cbox.count=2;cbox.anchors={
  {2000,1,{1000,0,1000}},{2032,1,{2000,0,1000}},{2064,2,{3000,0,1000}}};
 items::RoundItems config;
 auto plan=items::plan_gcx_round_items(config,&layout,cbox,20,0,9);
 auto again=items::plan_gcx_round_items(config,&layout,cbox,20,0,9);
 check(plan.verified&&plan.items.size()==1&&plan.items[0].domain==items::Domain::equipment&&plan.items[0].item==22&&plan.removeCboxBindings.empty(),"default DM selects ENVG only and keeps CBOX");
 check(plan.items[0].sourceOffset==again.items[0].sourceOffset&&plan.items[0].position==again.items[0].position,"same round deterministic");
 auto tdm=items::plan_gcx_round_items(config,&layout,cbox,20,1,9);
 check(tdm.items.size()==1&&tdm.items[0].item==10,"rule-specific group selected");
 check(!items::plan_gcx_round_items(config,nullptr,cbox,20,0,9).verified,"missing source unavailable");
 auto unavailable=read("MGO2MT.GCX_ROUND_ITEMS 1 20 unavailable\ngroups 0\n");
 check(!items::plan_gcx_round_items(config,&unavailable,cbox,20,0,9).verified,"unverified source unavailable");
 auto off=config;off.useGcx=false;auto disabled=items::plan_gcx_round_items(off,nullptr,cbox,20,0,9);
 check(disabled.verified&&disabled.items.empty(),"explicit GCX off needs no sidecar");
 auto wrongMap=layout;wrongMap.map=4;rejects([&]{items::plan_gcx_round_items(config,&wrongMap,cbox,20,0,9);},"route identity mismatch rejected");
 config.replacements={{20,items::GcxSource::pickup,0,items::Domain::weapon,1},
  {20,items::GcxSource::pickup,plan.items[0].sourceOffset,items::Domain::weapon,22},
  {20,items::GcxSource::cbox,0,items::Domain::equipment,16}};
 auto selected=cbox.select(9);
 config.replacements.push_back({20,items::GcxSource::cbox,selected[1].anchor.sourceOffset,items::Domain::weapon,25});
 auto replaced=items::plan_gcx_round_items(config,&layout,cbox,20,0,9);
 check(replaced.items.size()==3&&replaced.items[0].item==22,"exact pickup override beats wildcard");
 check(replaced.items[1].item==16&&replaced.items[2].item==25,"exact CBOX override beats wildcard");
 check(replaced.removeCboxBindings==std::vector<uint32_t>{0xc0000000u,0xc0000001u},"bindings use selected ordinal");
 for(uint32_t i=0;i<2;++i){const auto& p=replaced.items[i+1];check(p.cboxOrdinal==i&&p.sourceOffset==selected[i].anchor.sourceOffset&&p.position.x==selected[i].anchor.position[0]&&p.position.z==selected[i].anchor.position[2]&&p.position.yaw==selected[i].rotationRadians,"CBOX selected position/yaw retained");}
 auto noWildcard=items::RoundItems{};uint32_t unselected=0;
 for(const auto& a:cbox.anchors)if(a.sourceOffset!=selected[0].anchor.sourceOffset&&a.sourceOffset!=selected[1].anchor.sourceOffset)unselected=a.sourceOffset;
 noWildcard.replacements={{20,items::GcxSource::cbox,unselected,items::Domain::weapon,1}};
 check(items::plan_gcx_round_items(noWildcard,&layout,cbox,20,0,9).removeCboxBindings.empty(),"nonselected exact source does not remove another CBOX");
 bool selectedLater=false;for(unsigned gen=0;gen<256;++gen){auto future=items::plan_gcx_round_items(noWildcard,&layout,cbox,20,0,uint8_t(gen));if(!future.removeCboxBindings.empty()){check(future.removeCboxBindings.size()==1&&future.items.back().sourceOffset==unselected,"next generation applies only selected original offset");selectedLater=true;break;}}
 check(selectedLater,"exact replacement remains available in later generations");
 noWildcard.replacements[0].sourceOffset=999999;rejects([&]{items::plan_gcx_round_items(noWildcard,&layout,cbox,20,0,9);},"unknown source offset fails closed");
 auto world=floor();auto seeds=items::resolve_gcx_round_items(replaced,world,lookup);
 check(seeds.size()==3,"all selected pickup/replacements resolved");
 for(size_t i=0;i<seeds.size();++i)check(seeds[i].position.x==replaced.items[i].position.x&&seeds[i].position.z==replaced.items[i].position.z&&seeds[i].position.yaw==replaced.items[i].position.yaw&&std::abs(seeds[i].position.y-12)<.001f,"floor resolution preserves exact XZ and yaw");
 auto failed=replaced;failed.items.back().position.x=7000;
 rejects([&]{items::resolve_gcx_round_items(failed,world,lookup);},"last placement missing floor rejects whole batch without scatter");
 auto empty=stage::Collision::make({},{});rejects([&]{items::resolve_gcx_round_items(replaced,empty,lookup);},"empty collision fails closed");
 // Actual n022a CBOX GEOM sources contain Y=0 and Y=50 on a Y=125 floor.
 // The old 60-unit upward allowance started below this recovered surface.
 auto raised=stage::Collision::make({{-5000,125,-5000},{5000,125,-5000},{5000,125,5000},{-5000,125,5000}},{{{0,1,2}},{{0,2,3}}});
 auto lowSource=plan;lowSource.items[0].position.y=0;
 check(std::abs(items::resolve_gcx_round_items(lowSource,raised,lookup)[0].position.y-137)<.001f,"authored Y0 accepts exact-XZ recovered floor125");
 lowSource.items[0].position.y=50;
 check(std::abs(items::resolve_gcx_round_items(lowSource,raised,lookup)[0].position.y-137)<.001f,"authored Y50 accepts exact-XZ recovered floor125");
 failed=replaced;failed.items.back().item=65535;rejects([&]{items::resolve_gcx_round_items(failed,world,lookup);},"unknown final template rejects whole batch");
 rejects([&]{items::resolve_gcx_round_items(replaced,world,[](items::Domain,uint32_t item){return lookup(items::Domain::world_item,item);});},"template namespace substitution rejected");
 failed=replaced;failed.items.back().position=failed.items[0].position;rejects([&]{items::resolve_gcx_round_items(failed,world,lookup);},"overlapping authored points fail rather than scatter");
 failed=replaced;failed.verified=false;rejects([&]{items::resolve_gcx_round_items(failed,world,lookup);},"unverified plan cannot seed");
 // An overhead roof must not replace the source's floor: the downward query
 // starts near authored Y, not 1200 units above it.
 auto roof=stage::Collision::make({{-5000,1000,-5000},{5000,1000,-5000},{5000,1000,5000},{-5000,1000,5000}},{{{0,1,2}},{{0,2,3}}});
 std::array<stage::CollisionInstance,1> overhead{{{1,std::make_shared<const stage::Collision>(roof),{},{}}}};
 auto roofWorld=stage::Collision::combine(world,overhead);
 check(std::abs(items::resolve_gcx_round_items(plan,roofWorld,lookup)[0].position.y-12)<.001f,"roof above source not selected");
 // Solid geometry crossing the small placement capsule is an obstruction.
 const auto source=plan.items[0].position;
 auto wall=stage::Collision::make({{source.x,0,source.z-100},{source.x,200,source.z-100},{source.x,200,source.z+100},{source.x,0,source.z+100}},{{{0,1,2}},{{0,2,3}}});
 std::array<stage::CollisionInstance,1> obstacle{{{2,std::make_shared<const stage::Collision>(wall),{},{}}}};
 auto blocked=stage::Collision::combine(world,obstacle);rejects([&]{items::resolve_gcx_round_items(plan,blocked,lookup);},"obstruction does not move original source");
 if(argc>1){std::ifstream in(argv[1]);check(bool(in),"actual GCX sidecar exists");auto actual=items::GcxItemLayout::read(in);check(actual.map==20&&actual.verified&&actual.groups.size()==4,"actual source group contract");
  const auto directory=std::filesystem::path(argv[1]).parent_path();auto loaded=items::load_gcx_item_layout(directory,20);check(loaded&&loaded->map==20&&loaded->groups.size()==4,"stage route loader reads actual source");
  check(!items::load_gcx_item_layout(directory/"gcx-test-missing-directory",20),"missing source file remains unavailable");
  rejects([&]{items::load_gcx_item_layout(directory,255);},"unknown stage loader route rejected");
  for(uint8_t rule:{0,1}){auto p=items::plan_gcx_round_items(items::RoundItems{},&actual,{},20,rule,9);check(p.items.size()==2&&p.items[0].domain==items::Domain::equipment&&p.items[0].item==22&&p.items[1].item==10,"actual GCX ENVG/DRUM namespace contract");}
 }
 std::cout<<"GCX strict parser / authored selection / exact override / ordinal bindings / all-or-none source-floor resolution PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
