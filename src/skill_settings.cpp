#include "skill_settings.h"
#include <windows.h>
#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace mgo2win::skills {
namespace {
template<class T>T number(std::string_view value){
 unsigned long long n=0;auto [end,ec]=std::from_chars(value.data(),value.data()+value.size(),n);
 if(value.empty()||ec!=std::errc{}||end!=value.data()+value.size()||n>std::numeric_limits<T>::max())throw std::runtime_error("Skill number");
 return static_cast<T>(n);
}
std::vector<std::string> fields(const std::string&line){std::vector<std::string> out;size_t start=0;
 do{auto at=line.find('\t',start);out.push_back(line.substr(start,at-start));if(at==line.npos)break;start=at+1;}while(out.size()<10);return out;
}
std::string read_file(const std::filesystem::path&path,size_t limit){
 std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in||in.tellg()<=0||uint64_t(in.tellg())>limit)throw std::runtime_error("Skill file extent");
 std::string value(size_t(in.tellg()),'\0');in.seekg(0);in.read(value.data(),std::streamsize(value.size()));if(!in)throw std::runtime_error("Skill file read");return value;
}
bool valid_name(const std::string&s){return !s.empty()&&s.size()<=192&&s.find_first_of("\r\n\t\0",0,4)==s.npos&&
 MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0)>0&&std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32||c==127;});}
std::string hash(const std::string&s){uint64_t h=14695981039346656037ull;for(unsigned char c:s){h^=c;h*=1099511628211ull;}char out[17]{};for(int i=15;i>=0;--i){out[i]="0123456789abcdef"[h&15];h>>=4;}return out;}
std::filesystem::path record_path(const std::filesystem::path&directory,uint64_t id){if(!id)throw std::runtime_error("Skill character identity");return directory/(std::to_string(id)+".gsk");}
}
bool Catalog::load(const std::filesystem::path&path,std::string&error){try{
 std::istringstream in(read_file(path,65536));std::string line;auto get=[&]{if(!std::getline(in,line))return false;if(!line.empty()&&line.back()=='\r')line.pop_back();return true;};
 if(!get()||line!="MGO2WIN_SKILLS\t1")throw std::runtime_error("Skill catalog version");
 std::vector<Entry> draft;std::set<std::pair<uint16_t,uint8_t>> keys;std::set<uint16_t> ids;
 while(get()){
  auto f=fields(line);if(f.size()!=6||f[0]!="SKILL")throw std::runtime_error("Skill catalog row");
  Entry e{number<uint16_t>(f[1]),number<uint8_t>(f[2]),number<uint8_t>(f[3]),f[4],f[5]};
  if(!e.level||!e.cost||e.cost>maximum_capacity||!valid_name(e.name)||!valid_name(e.display_name)||!keys.emplace(e.id,e.level).second)throw std::runtime_error("Skill catalog value or duplicate");
  for(const auto&prior:draft)if(prior.id==e.id&&(prior.name!=e.name||prior.display_name!=e.display_name))throw std::runtime_error("Skill catalog identity mismatch");
  draft.push_back(std::move(e));ids.insert(draft.back().id);if(draft.size()>512||ids.size()>128)throw std::runtime_error("Skill catalog count");
 }
 if(draft.empty())throw std::runtime_error("Skill catalog empty");
 std::stable_sort(draft.begin(),draft.end(),[](const Entry&a,const Entry&b){return a.id<b.id||(a.id==b.id&&a.level<b.level);});
 entries_=std::move(draft);error.clear();return true;
}catch(const std::exception&e){entries_.clear();error=e.what();return false;}}
const Entry* Catalog::find(uint16_t id,uint8_t level)const{auto it=std::find_if(entries_.begin(),entries_.end(),[&](const Entry&e){return e.id==id&&e.level==level;});return it==entries_.end()?nullptr:&*it;}
std::vector<uint16_t> Catalog::ids()const{std::vector<uint16_t> out;for(const auto&e:entries_)if(out.empty()||out.back()!=e.id)out.push_back(e.id);return out;}
std::vector<const Entry*> Catalog::levels(uint16_t id)const{std::vector<const Entry*> out;for(const auto&e:entries_)if(e.id==id)out.push_back(&e);return out;}
Check validate(const Catalog&catalog,const Loadout&loadout,unsigned capacity){
 if(capacity<base_capacity||capacity>maximum_capacity)return {Validation::invalid_capacity,0};
 if(loadout.entries.size()>maximum_capacity)return {Validation::too_many,0};
 std::set<uint16_t> ids;unsigned used=0;
 for(auto choice:loadout.entries){
  if(!ids.insert(choice.id).second)return {Validation::duplicate,used};
  auto entry=catalog.find(choice.id,choice.level);if(!entry)return {catalog.levels(choice.id).empty()?Validation::unknown_skill:Validation::unknown_level,used};
  used+=entry->cost;
 }
 return {used>capacity?Validation::over_budget:Validation::valid,used};
}
Editor::Editor(std::shared_ptr<const Catalog>catalog,Loadout loadout,unsigned capacity):catalog_(std::move(catalog)),original_(std::move(loadout)),draft_(original_),capacity_(capacity){if(!catalog_)throw std::invalid_argument("Missing skill catalog");}
Check Editor::status()const{return validate(*catalog_,draft_,capacity_);}
Check Editor::set(uint16_t id,uint8_t level){auto next=draft_;auto it=std::find_if(next.entries.begin(),next.entries.end(),[&](Choice e){return e.id==id;});
 if(!level){if(it!=next.entries.end())next.entries.erase(it);}
 else if(it!=next.entries.end())it->level=level;else next.entries.push_back({id,level});
 auto result=validate(*catalog_,next,capacity_);if(result)draft_=std::move(next);return result;
}
bool save(const std::filesystem::path&directory,uint64_t characterId,const Catalog&catalog,const Loadout&loadout,unsigned capacity,std::string&error){std::filesystem::path temporary;try{
 if(!validate(catalog,loadout,capacity))throw std::runtime_error("Invalid skill loadout");
 auto path=record_path(directory,characterId);std::string body="MGO2WIN_SKILL_LOADOUT\t1\nCHARACTER\t"+std::to_string(characterId)+"\n";
 for(auto entry:loadout.entries)body+="SKILL\t"+std::to_string(entry.id)+"\t"+std::to_string(entry.level)+"\n";
 body+="CHECKSUM\t"+hash(body)+"\n";std::filesystem::create_directories(directory);
 temporary=path;temporary+=L".tmp."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetCurrentThreadId());
 {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out.write(body.data(),std::streamsize(body.size()));out.flush();if(!out)throw std::runtime_error("Skill loadout write");}
 if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Skill loadout replace");
 error.clear();return true;
}catch(const std::exception&e){if(!temporary.empty()){std::error_code ignored;std::filesystem::remove(temporary,ignored);}error=e.what();return false;}}
std::optional<Loadout> load(const std::filesystem::path&directory,uint64_t characterId,const Catalog&catalog,unsigned capacity,std::string&error){try{
 auto path=record_path(directory,characterId);if(!std::filesystem::exists(path)){error.clear();return std::nullopt;}
 auto contents=read_file(path,4096);auto checksum=contents.rfind("CHECKSUM\t");
 if(checksum==contents.npos||contents.substr(checksum)!="CHECKSUM\t"+hash(contents.substr(0,checksum))+"\n")throw std::runtime_error("Skill loadout checksum");
 std::istringstream in(contents.substr(0,checksum));std::string line;
 if(!std::getline(in,line)||line!="MGO2WIN_SKILL_LOADOUT\t1"||!std::getline(in,line))throw std::runtime_error("Skill loadout version");
 auto identity=fields(line);if(identity.size()!=2||identity[0]!="CHARACTER"||number<uint64_t>(identity[1])!=characterId)throw std::runtime_error("Skill loadout character mismatch");
 Loadout result;while(std::getline(in,line)){auto f=fields(line);if(f.size()!=3||f[0]!="SKILL")throw std::runtime_error("Skill loadout row");result.entries.push_back({number<uint16_t>(f[1]),number<uint8_t>(f[2])});if(result.entries.size()>maximum_capacity)throw std::runtime_error("Skill loadout count");}
 if(!validate(catalog,result,capacity))throw std::runtime_error("Skill loadout exceeds current entitlement or catalog");error.clear();return result;
}catch(const std::exception&e){error=e.what();return std::nullopt;}}
}
