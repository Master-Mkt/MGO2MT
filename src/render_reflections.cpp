#include "render_reflections.h"
#include "multi_ui_json.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace mgo2mt::render_reflections {
namespace {
using J=multi_ui::json::Value;
constexpr size_t maximumBytes=64*1024;
void require(bool condition,const char* why){if(!condition)throw std::runtime_error(why);}
bool unit(float value) noexcept {return std::isfinite(value)&&value>=0&&value<=1;}
void keys(const J& value,std::initializer_list<std::string_view> allowed){
 require(value.type==J::object,"Reflection profile requires a JSON object");
 for(const auto& [key,unused]:value.o){(void)unused;require(std::find(allowed.begin(),allowed.end(),key)!=allowed.end(),"Unknown reflection profile field");}
}
const J& field(const J& value,const char* name){
 const auto found=value.o.find(name);require(found!=value.o.end(),"Missing reflection profile field");return found->second;
}
uint32_t integer(const J& value){
 require(value.type==J::number&&std::isfinite(value.n)&&value.n>=0&&value.n<=std::numeric_limits<uint32_t>::max()&&std::floor(value.n)==value.n,"Reflection shader/version requires an unsigned integer");
 return static_cast<uint32_t>(value.n);
}
float fraction(const J& value){
 require(value.type==J::number&&std::isfinite(value.n)&&value.n>=0&&value.n<=1,"Reflection values must be finite numbers from 0 to 1");return static_cast<float>(value.n);
}
}
Profile defaults() noexcept {
 Profile result;result.count=4;
 result.rules[0]={0x10,.35f,.35f};result.rules[1]={0x61,.35f,.35f};
 result.rules[2]={0x100000,.35f,.35f};result.rules[3]={0x130003,.18f,.65f};return result;
}
bool valid(const Profile& profile) noexcept {
 if(profile.count>profile.rules.size())return false;
 for(unsigned i=0;i<profile.count;++i){
  const auto& rule=profile.rules[i];if(!unit(rule.reflectivity)||!unit(rule.roughness))return false;
  for(unsigned j=0;j<i;++j)if(rule.shader==profile.rules[j].shader)return false;
 }
 return true;
}
Profile read(std::istream& input){
 // Read one extra byte to reject oversize streams without allocating by file size.
 std::array<char,maximumBytes+1> bytes;
 try{input.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));}
 catch(const std::ios_base::failure&){if(input.bad()||!input.eof())throw;}
 const auto size=input.gcount();
 require(!input.bad()&&(input.eof()||!input.fail()),"Reflection profile stream read failed");
 require(size>0&&size<=static_cast<std::streamsize>(maximumBytes),"Reflection profile must be 1 byte to 64 KiB");
 const auto root=multi_ui::json::parse(std::string_view(bytes.data(),static_cast<size_t>(size)));
 keys(root,{"version","materials"});require(integer(field(root,"version"))==1,"Unsupported reflection profile version");
 const auto& rows=field(root,"materials");require(rows.type==J::array&&rows.a.size()<=32,"Reflection profile allows at most 32 materials");
 Profile result;
 for(const auto& row:rows.a){
  keys(row,{"shader","reflectivity","roughness"});
  result.rules[result.count++]={integer(field(row,"shader")),fraction(field(row,"reflectivity")),fraction(field(row,"roughness"))};
 }
 require(valid(result),"Duplicate or invalid reflection material");return result;
}
Profile load(const std::filesystem::path& path){
 if(!std::filesystem::exists(path))return defaults();
 require(std::filesystem::is_regular_file(path),"Reflection profile must be a regular file");
 std::ifstream input(path,std::ios::binary);require(bool(input),"Reflection profile cannot be opened");return read(input);
}
std::array<float,2> material(const Profile& profile,uint32_t shader) noexcept {
 if(profile.count>profile.rules.size())return {0,1};
 for(unsigned i=0;i<profile.count;++i){const auto& rule=profile.rules[i];if(rule.shader==shader)return unit(rule.reflectivity)&&unit(rule.roughness)?std::array<float,2>{rule.reflectivity,rule.roughness}:std::array<float,2>{0,1};}
 return {0,1};
}
}
