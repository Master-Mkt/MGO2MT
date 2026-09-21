#include "weapon_restrictions.h"
#include "weapon_catalog.h"
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace mgo2mt::restrictions;
namespace {
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
const Entry& entry(std::string_view key){const auto* e=find(key);if(!e)throw std::runtime_error("missing expected restriction entry");return *e;}
}
int main(){try{
 check(catalog().size()==52,"all five original host restriction groups are present");
 std::array<size_t,5> counts{};std::set<std::string_view> keys;
 for(const auto& e:catalog()){
  check(!e.key.empty()&&!e.display_name.empty()&&keys.insert(e.key).second,"unique stable keys and names");
  check(unsigned(e.category)<counts.size()&&e.mask!=Bits{}&&(e.mask[0]&1)==0,"valid category and non-master mask");++counts[unsigned(e.category)];
  if(e.weapon_id)check(*e.weapon_id!=0,"NONE is not a host restriction");
 }
 check(counts==std::array<size_t,5>{20,6,14,10,2},"category counts match candidate source");
 // Independent expected bytes transcribed from candidate factory/wire layout.
 const Bits primary{0x02,0,0xd4,0xc7,0xe8,0x1e,0x04,0,0,0x02,0,0,0,0,0,0};
 const Bits secondary{0x9c,0x81,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
 const Bits support{0,0,0,0,0,0,0xf0,0x8f,0x2f,0,0,0,0,0,0,0};
 const Bits custom{0,0,0,0,0,0,0,0,0,0x3c,0x0e,0xb0,0x07,0,0,0};
 const Bits items{0,0,0,0,0,0,0,0,0,0,0,0,0,0x04,0x40,0};
 check(category_mask(Category::primary)==primary&&category_mask(Category::secondary)==secondary&&category_mask(Category::support)==support&&category_mask(Category::custom)==custom&&category_mask(Category::items)==items,"exact reviewed category masks");
 Bits b{};set_locked(b,entry("ak"),true);
 check(b[3]==2&&locked(b,entry("ak"))&&!enabled(b)&&!effective_locked(b,entry("ak")),"stored AK prohibition is separate from master enable");
 set_enabled(b,true);check(effective_locked(b,entry("ak"))&&!effective_locked(b,entry("m4")),"original master switch and weapon mask semantics");
 set_enabled(b,false);check(b[3]==2&&!effective_locked(b,entry("ak")),"master disable preserves configured weapon locks");
 b={};b[0]=0x61;b[15]=0xa5;set_locked(b,entry("grenade"),true);const auto before=b;
 set_category_locked(b,Category::primary,true);
 for(size_t i=0;i<b.size();++i)check(b[i]==uint8_t(before[i]|primary[i]),"category All Lock preserves other and unknown bits");
 set_category_locked(b,Category::primary,false);check(b==before,"category All Unlock preserves master, support and unknown bits");
 b={};b[9]=0x20;
 check(state(b,entry("suppressor"))==LockState::mixed&&locked(b,entry("suppressor")),"partial composite suppressor restriction is visible");
 set_locked(b,entry("suppressor"),true);check(b[9]==0x20&&b[10]==0x0e&&state(b,entry("suppressor"))==LockState::locked,"suppressor locks all four reviewed variants");
 b[10]|=0x80;set_locked(b,entry("suppressor"),false);check(b[9]==0&&b[10]==0x80,"suppressor unlock preserves an unrelated unknown bit");
 b.fill(0xff);set_all_locked(b,false);
 const Bits remaining{0x61,0x7e,0x2b,0x38,0x17,0xe1,0x0b,0x70,0xd0,0xc1,0xf1,0x4f,0xf8,0xfb,0xbf,0xff};
 check(b==remaining&&enabled(b),"All Unlock is bounded to reviewed controls, not a destructive memset");
 const auto restored=b;set_all_locked(b,true);check(b==Bits{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},"All Lock restores all known masks");
 b=restored;set_category_locked(b,static_cast<Category>(255),true);check(b==restored&&category_mask(static_cast<Category>(255))==Bits{}&&!find("not-known"),"unknown categories and keys cannot modify settings");
 mgo2mt::weapons::Catalog selectable;std::string error;
 auto privateCatalog=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/weapon_catalog.tsv";
 if(std::filesystem::exists(privateCatalog)){
 check(selectable.load(privateCatalog,error),"ordinary catalog fixture");
 check(!selectable.find(mgo2mt::weapons::Category::primary,22)&&entry("patriot").weapon_id==22&&entry("patriot").dp_only==true,"PATRIOT host restriction exists independently of equipment entitlement");
 check(!selectable.find(mgo2mt::weapons::Category::primary,1)&&entry("knife").weapon_id==1,"fixed knife can be restricted without being a selectable primary row");
 for(const auto& e:catalog())if(e.weapon_id&&unsigned(e.category)<3){
  if(const auto* weapon=selectable.find(static_cast<mgo2mt::weapons::Category>(e.category),*e.weapon_id)){
   check(e.dp_only&&weapon->available_without_dp&&*e.dp_only==!*weapon->available_without_dp,"ordinary TDM DP-only hints match original availability branch");
  }
 }
 }else std::cout<<"Private catalog cross-check omitted; restriction mask tests remain enabled\n";
 check(entry("m4").dp_only==false&&entry("mp5").dp_only==true&&!entry("suppressor").dp_only,"DP-only marker is not simply price greater than zero");
 std::cout<<"host weapon restriction masks, categories, composite controls and equipment-policy separation passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
