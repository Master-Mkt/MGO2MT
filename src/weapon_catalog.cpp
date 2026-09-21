#include "product_identity.h"
#include "weapon_catalog.h"
#include "gameplay_config.h"
#include <charconv>
#include <fstream>
#include <limits>
#include <set>
#include <string_view>

namespace mgo2mt::weapons {
namespace {
bool number(std::string_view text,uint32_t& out,uint32_t maximum=UINT32_MAX){
 if(text.empty())return false;
 const auto r=std::from_chars(text.data(),text.data()+text.size(),out);
 return r.ec==std::errc{}&&r.ptr==text.data()+text.size()&&out<=maximum;
}
bool utf8_name(std::string_view text){
 if(text.empty()||text.size()>128)return false;
 for(size_t i=0;i<text.size();){
  const auto c=static_cast<uint8_t>(text[i++]);
  if(c<0x20||c==0x7f)return false;
  if(c<0x80)continue;
  unsigned remaining;uint32_t value,min;
  if(c>=0xc2&&c<=0xdf){remaining=1;value=c&31;min=0x80;}
  else if(c>=0xe0&&c<=0xef){remaining=2;value=c&15;min=0x800;}
  else if(c>=0xf0&&c<=0xf4){remaining=3;value=c&7;min=0x10000;}
  else return false;
  if(i+remaining>text.size())return false;
  while(remaining--){const auto d=static_cast<uint8_t>(text[i++]);if((d&0xc0)!=0x80)return false;value=(value<<6)|(d&63);}
  if(value<min||value>0x10ffff||(value>=0xd800&&value<=0xdfff))return false;
 }
 return true;
}
std::vector<std::string_view> fields(const std::string& line){
 std::vector<std::string_view> result;size_t begin=0;
 for(;;){auto end=line.find('\t',begin);result.emplace_back(line.data()+begin,(end==line.npos?line.size():end)-begin);if(end==line.npos)break;begin=end+1;}
 return result;
}
}
const char* category_name(Category c){switch(c){case Category::primary:return "PRIMARY";case Category::secondary:return "SECONDARY";case Category::support:return "SUPPORT";}return "UNKNOWN";}
bool Catalog::load(const std::filesystem::path& path,std::string& error){
 // A present JSON is authoritative. An invalid file must not silently restore
 // the compiled/TSV loadout and hide a misspelled user configuration.
 const auto json=path.extension()==".json"?path:path.parent_path()/"gameplay.json";
 std::error_code ec;const bool hasJson=std::filesystem::exists(json,ec);
 if(ec){error="Could not inspect gameplay configuration: "+ec.message();return false;}
 if(hasJson){gameplay::Config config;if(!config.load(json,error))return false;std::map<uint16_t,std::string> names;for(const auto&d:config.definitions())names.emplace(d.weapon.id,d.name);entries_=config.entries();initial_dp_=config.initial_dp();all_names_=std::move(names);return true;}
 error.clear();std::ifstream in(path,std::ios::binary);
 if(!in){error="Weapon catalog could not be opened.";return false;}
 in.seekg(0,std::ios::end);auto size=in.tellg();
 if(size<=0||size>65536){error="Weapon catalog size is invalid.";return false;}in.seekg(0);
 std::vector<Entry> next;std::optional<uint32_t> initial;std::set<std::pair<unsigned,uint16_t>> keys;
 bool version=false,initial_seen=false;std::string line;unsigned line_number=0;
 auto fail=[&](){error="Invalid weapon catalog at line "+std::to_string(line_number)+".";return false;};
 while(std::getline(in,line)){
  ++line_number;if(!line.empty()&&line.back()=='\r')line.pop_back();
  if(line.size()>1024||line.find('\0')!=line.npos)return fail();
  if(line.empty()||line[0]=='#')continue;
  auto f=fields(line);uint32_t value=0;
  if(!version){if(f.size()!=2||f[0]!=mgo2mt::brand::Format{"MGO2MT_WEAPON_CATALOG"}||f[1]!="1")return fail();version=true;continue;}
  if(f[0]=="INITIAL_DP"){
   if(initial_seen||f.size()!=2)return fail();initial_seen=true;
   if(f[1]!="?"){if(!number(f[1],value))return fail();initial=value;}continue;
  }
  if(f.size()!=7||f[0]!="WEAPON"||next.size()>=128)return fail();Entry entry;
  if(f[1]=="PRIMARY")entry.category=Category::primary;
  else if(f[1]=="SECONDARY")entry.category=Category::secondary;
  else if(f[1]=="SUPPORT")entry.category=Category::support;
  else return fail();
  if(!number(f[2],value,511))return fail();entry.id=static_cast<uint16_t>(value);
  if(!keys.emplace(static_cast<unsigned>(entry.category),entry.id).second)return fail();
  if(!utf8_name(f[3]))return fail();entry.display_name=f[3];
  if(f[4]!="?"){if(!number(f[4],value))return fail();entry.dp_cost=value;}
  if(f[5]=="1")entry.available_without_dp=true;else if(f[5]=="0")entry.available_without_dp=false;else if(f[5]!="?")return fail();
  if(f[6]!="?"){if(!number(f[6],value,127)||value==0)return fail();entry.restriction_bit=static_cast<uint8_t>(value);}
  next.push_back(std::move(entry));
 }
 if(in.bad()||!version||!initial_seen||next.empty()){error="Incomplete weapon catalog.";return false;}
 entries_=std::move(next);initial_dp_=initial;all_names_.clear();return true;
}
const Entry* Catalog::find(Category category,uint16_t id)const{for(const auto& e:entries_)if(e.id==id&&e.category==category)return &e;return nullptr;}
std::string_view Catalog::name(uint16_t id)const{if(auto i=all_names_.find(id);i!=all_names_.end())return i->second;for(const auto&e:entries_)if(e.id==id)return e.display_name;return {};}
Access access(const Entry& e,const SelectionContext& context){
 if(context.room_restrictions[0]&1){
  if(e.restriction_bit){auto bit=*e.restriction_bit;if(bit==0||bit>=128)return Access::unverified;if(context.room_restrictions[bit/8]&(1u<<(bit%8)))return Access::restricted;}
  else if(e.id!=0)return Access::unverified;
 }
 if(!context.dp_enabled){if(context.native_operator_grant&&e.category==Category::secondary&&e.id==3)return Access::allowed;if(!e.available_without_dp)return Access::unverified;return *e.available_without_dp?Access::allowed:Access::dp_disabled;}
 if(!e.dp_cost)return Access::unverified;
 return *e.dp_cost<=context.dp_balance?Access::allowed:Access::insufficient_dp;
}
std::vector<const Entry*> Catalog::choices(Category category,const SelectionContext& context,bool include_unavailable)const{
 std::vector<const Entry*> result;for(const auto& e:entries_)if(e.category==category&&(include_unavailable||access(e,context)==Access::allowed))result.push_back(&e);return result;
}
Quote quote(std::span<const Entry* const> entries,const SelectionContext& context){
 Quote result;unsigned categories=0;
 for(const auto* e:entries){
  if(!e||static_cast<unsigned>(e->category)>2){result.access=Access::unverified;return result;}
  const auto bit=1u<<static_cast<unsigned>(e->category);if(categories&bit){result.access=Access::restricted;return result;}categories|=bit;
  const auto permission=access(*e,context);if(permission!=Access::allowed){result.access=permission;return result;}
  if(context.dp_enabled)result.cost+=*e->dp_cost;
 }
 if(result.cost>context.dp_balance&&context.dp_enabled)result.access=Access::insufficient_dp;
 return result;
}
}
