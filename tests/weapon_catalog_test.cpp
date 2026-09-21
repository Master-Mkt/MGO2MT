#include "weapon_catalog.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt::weapons;
namespace {
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct Fixture {
 std::filesystem::path path=std::filesystem::temp_directory_path()/("mgo2mt-weapon-catalog-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tsv");
 void write(const std::string& s){std::ofstream f(path,std::ios::binary);f<<s;}
 ~Fixture(){std::error_code ec;std::filesystem::remove(path,ec);}
};
}
int main(){try{
 Catalog catalog;std::string error;
 auto data=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/weapon_catalog.tsv";
 SelectionContext context;
 if(std::filesystem::exists(data)){
 check(catalog.load(data,error),error.c_str());
 check(catalog.entries().size()==41&&catalog.initial_dp()==1000,"reviewed ordinary TDM catalog and initial DP");
 auto ak=catalog.find(Category::primary,25),m4=catalog.find(Category::primary,24),rpg=catalog.find(Category::primary,50);
 auto mk2=catalog.find(Category::secondary,2),c4=catalog.find(Category::support,66);
 check(ak&&ak->display_name=="AK102"&&ak->dp_cost==1000&&m4&&m4->dp_cost==2000&&rpg&&rpg->dp_cost==20000,"GCX prices use original 250 DP multiplier");
 check(mk2&&mk2->dp_cost==0&&c4&&c4->dp_cost==1000,"secondary and support original prices");
 check(catalog.choices(Category::primary,context,false).size()==8&&catalog.choices(Category::secondary,context,false).size()==2&&catalog.choices(Category::support,context,false).size()==6,"DP-off ordinary weapon subsets");
 for(auto id:{9,27,71,40})check(!catalog.find(Category::primary,static_cast<uint16_t>(id)),"raw special/variant weapon rows are not granted to ordinary PCs");
 check(!catalog.find(Category::primary,22),"PATRIOT needs original entitlement verification beyond DP affordability");
 for(auto id:{10,5})check(!catalog.find(Category::secondary,static_cast<uint16_t>(id)),"secondary eligibility not yet proved is withheld");
 check(access(*m4,context)==Access::allowed&&access(*rpg,context)==Access::dp_disabled,"DP-off availability differs from zero-price weapons");
 context.dp_enabled=true;context.dp_balance=1000;
 check(access(*ak,context)==Access::allowed&&access(*m4,context)==Access::insufficient_dp,"DP exact affordability boundary");
 auto combined=quote(std::array{ak,mk2,c4},context);
 check(combined.access==Access::insufficient_dp&&combined.cost==2000,"combined choices cannot overspend");
 context.dp_balance=2000;check(quote(std::array{ak,mk2,c4},context).access==Access::allowed,"combined exact budget");
 check(quote(std::array{ak,m4},context).access==Access::restricted,"cannot select two primary weapons");
 context.room_restrictions[3]=2;check(access(*ak,context)==Access::allowed,"deny bits ignored when room restriction master flag is off");
 context.room_restrictions[0]=1;check(access(*ak,context)==Access::restricted&&access(*m4,context)==Access::allowed,"original room restriction byte3 bit1 denies AK only");
 Entry variant{9,Category::primary,"D EAGLE L",1000,true,std::nullopt};check(access(variant,context)==Access::unverified,"unverified variant mapping cannot bypass enabled room restrictions");
 }else std::cout<<"Private recovered catalog absent; synthetic contract checks remain enabled\n";
 Fixture fixture;std::string header="MGO2MT_WEAPON_CATALOG\t1\nINITIAL_DP\t?\n";
 fixture.write(header+"WEAPON\tPRIMARY\t1\tST KNIFE\t?\t1\t1\n");
 check(catalog.load(fixture.path,error)&&!catalog.initial_dp(),"explicit unknown DP metadata accepted without fabricated zeros");
 auto unknown=catalog.find(Category::primary,1);context={};
 check(unknown&&access(*unknown,context)==Access::allowed,"known DP-off eligibility does not require a price");
 context.dp_enabled=true;context.dp_balance=UINT32_MAX;
 check(access(*unknown,context)==Access::unverified,"unknown price remains unselectable even with maximum budget");
 const std::array malformed={
  header+"WEAPON\tPRIMARY\t1\tName\t-1\t1\t1\n",
  header+"WEAPON\tPRIMARY\t1\tName\t4294967296\t1\t1\n",
  header+"WEAPON\tPRIMARY\t512\tName\t0\t1\t1\n",
  header+"WEAPON\tPRIMARY\t1\tName\t0\t1\t0\n",
  header+"WEAPON\tPRIMARY\t1\tName\t0\t1\t128\n",
  header+"WEAPON\tPRIMARY\t1\tName\t0\t1\t1\nWEAPON\tPRIMARY\t1\tDuplicate\t0\t1\t1\n",
  header+"WEAPON\tPRIMARY\t1\t\xc0\xaf\t0\t1\t1\n",
  header+"INITIAL_DP\t1000\nWEAPON\tPRIMARY\t1\tName\t0\t1\t1\n",
  std::string(65537,'x')};
 for(const auto& s:malformed){fixture.write(s);check(!catalog.load(fixture.path,error)&&!error.empty()&&catalog.entries().size()==1&&catalog.find(Category::primary,1)->display_name=="ST KNIFE","invalid reload rejected and previous data preserved");}
 std::cout<<"weapon catalog contract and all available reference checks passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
