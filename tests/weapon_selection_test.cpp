#include "weapon_selection.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2win::weapons;
static void check(bool ok,const char*name){if(!ok)throw std::runtime_error(name);}
int main(){
 auto path=std::filesystem::temp_directory_path()/("mgo2win-weapon-selection-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tsv");
 struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove(path,ec);}}cleanup{path};
 try{
  {std::ofstream f(path);f<<"MGO2WIN_WEAPON_CATALOG\t1\nINITIAL_DP\t1000\n"
   "WEAPON\tPRIMARY\t1\tMain A\t600\t1\t1\n"
   "WEAPON\tPRIMARY\t2\tMain B\t800\t0\t2\n"
   "WEAPON\tSECONDARY\t3\tSecondary A\t300\t1\t3\n"
   "WEAPON\tSUPPORT\t4\tSupport A\t200\t1\t4\n"
   "WEAPON\tSUPPORT\t5\tSupport B\t100\t1\t5\n";}
  auto catalog=std::make_shared<Catalog>();std::string error;check(catalog->load(path,error),"fixture loads");Selection selection(catalog);
  check(selection.choices(Category::primary).size()==2&&selection.choose(Category::primary,1)==Access::unverified,"catalog alone cannot assume received settings or balance");
  SelectionContext context;selection.context(context);
  check(selection.choose(Category::primary,2)==Access::dp_disabled&&!selection.selected(Category::primary),"paid-only weapon cannot enter DP-off draft");
  check(selection.choose(Category::primary,1)==Access::allowed&&selection.choose(Category::secondary,3)==Access::allowed&&selection.choose(Category::support,4)==Access::allowed&&selection.complete()&&selection.quote().cost==0,"DP-off basic loadout does not charge catalog prices");
  context.dp_enabled=true;context.dp_balance=1000;selection.context(context);
  check(!selection.complete()&&!selection.selected(Category::primary),"enabling DP revalidates aggregate cost");
  check(selection.choose(Category::primary,1)==Access::allowed&&selection.choose(Category::secondary,3)==Access::allowed,"affordable partial choices");
  check(selection.choose(Category::support,4)==Access::insufficient_dp&&!selection.selected(Category::support)&&selection.quote().cost==900,"rejected combined overspend keeps previous choices");
  check(selection.choose(Category::support,5)==Access::allowed&&selection.complete()&&selection.quote().cost==1000,"exact total balance allows complete loadout");
  check(selection.choose(Category::primary,2)==Access::insufficient_dp&&selection.selected(Category::primary)->id==1,"failed replacement preserves original selected weapon");
  context.room_restrictions[0]=1|(1<<3);selection.context(context);check(!selection.selected(Category::secondary)&&!selection.complete(),"new host restriction invalidates prohibited selected category");
  selection.context(std::nullopt);check(!selection.selected(Category::primary)&&selection.quote().access==Access::unverified,"disconnect discards authority and draft");
  check(selection.choose(Category(99),1)==Access::unverified&&selection.choices(Category(99)).empty(),"invalid category bounded");
  std::cout<<"weapon selection, budget, changing restrictions, rollback and missing authority passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
