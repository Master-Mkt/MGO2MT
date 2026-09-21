#include "render_reflections.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace mgo2mt::render_reflections;
namespace {
unsigned checks=0;
void check(bool condition,const char* why){if(!condition)throw std::runtime_error(why);++checks;}
template<class F>void rejected(F&& action,const char* why){bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed,why);}
Profile parse(const std::string& text){std::istringstream input(text);return read(input);}
std::string row(std::string shader="1245187",std::string reflectivity="0.18",std::string roughness="0.65"){
 return "{\"shader\":"+shader+",\"reflectivity\":"+reflectivity+",\"roughness\":"+roughness+"}";
}
std::string document(const std::string& rows){return "{\"version\":1,\"materials\":["+rows+"]}";}
void write(const std::filesystem::path& path,const std::string& text){std::ofstream output(path,std::ios::binary);output<<text;if(!output)throw std::runtime_error("Test fixture write failed");}
}
int main(int argc,char** argv){try{
 const auto baseline=defaults();const auto text=document(row("16","0.35","0.35")+','+row("97","0.35","0.35")+','+row("1048576","0.35","0.35")+','+row());
 check(valid(baseline)&&baseline.count==4,"Default profile has four valid native rules");
 check(baseline.rules[3]==Rule{0x130003,.18f,.65f},"Native floor default is exact and separate from source assets");
 for(uint32_t shader:{0x10u,0x61u,0x100000u})check(material(baseline,shader)==std::array<float,2>{.35f,.35f},"Known reflection-operation key has an explicitly native coefficient");
 check(material(baseline,0x130003)==std::array<float,2>{.18f,.65f},"Known native material returns configured values");
 check(material(baseline,0x120000)==std::array<float,2>{0,1},"Unmatched shaders have no reflection");
 check(parse(text)==baseline,"JSON seed equals compiled fallback");
 check(parse("{\"materials\":[{\"roughness\":0.65,\"shader\":1245187,\"reflectivity\":0.18}],\"version\":1}")==parse(document(row())),"Field order is irrelevant");
 check(parse(" \r\n"+text+"\t\n")==baseline,"Ordinary JSON whitespace is accepted");
 const auto empty=parse(document(""));check(empty.count==0&&valid(empty)&&material(empty,0x130003)==std::array<float,2>{0,1},"Empty materials disables every native rule without restoring defaults");
 const auto disabled=parse(document(row("1245187","0","0")));check(material(disabled,0x130003)==std::array<float,2>{0,0},"Explicit all-zero values stay zero");
 check(parse(document(row("0","1","1"))).rules[0]==Rule{0,1,1},"Unsigned shader zero and upper fraction boundaries are valid");
 check(parse(document(row("4294967295"))).rules[0].shader==UINT32_MAX,"Full uint32 shader range is exact");
 auto changed=baseline;changed.rules[0].reflectivity=.2f;check(changed!=baseline,"Value equality detects live setting changes");
 changed=baseline;changed.count=33;check(!valid(changed)&&material(changed,0x130003)==std::array<float,2>{0,1},"Invalid count is rejected and cannot overrun lookup");
 changed=baseline;changed.count=2;changed.rules[1]=changed.rules[0];check(!valid(changed),"Duplicate shader in a constructed profile is invalid");
 for(float bad:{-1.f,1.01f,std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
  changed=baseline;changed.rules[0].reflectivity=bad;check(!valid(changed)&&material(changed,changed.rules[0].shader)==std::array<float,2>{0,1},"Invalid constructed reflectivity fails closed");
  changed=baseline;changed.rules[0].roughness=bad;check(!valid(changed)&&material(changed,changed.rules[0].shader)==std::array<float,2>{0,1},"Invalid constructed roughness fails closed");
 }
 std::string maximum;for(unsigned i=0;i<32;++i){if(i)maximum+=',';maximum+=row(std::to_string(i));}
 auto full=parse(document(maximum));check(full.count==32&&valid(full)&&material(full,31)==std::array<float,2>{.18f,.65f},"All 32 independent rules are accepted");
 rejected([&]{parse(document(maximum+','+row("32")));},"Rule 33 exceeds the profile budget");
 rejected([&]{parse(document(row()+','+row()));},"Duplicate shader rows are rejected");
 for(const char* bad:{"-1","4294967296","1.5","true","\"1245187\"","null"})rejected([&]{parse(document(row(bad)));},"Shader must be an unsigned integer number");
 for(const char* bad:{"-0.001","1.00000000001","1e309","NaN","Infinity","true","null","\"0.5\""}){
  rejected([&]{parse(document(row("1245187",bad)));},"Reflectivity must be a finite unit-range JSON number");
  rejected([&]{parse(document(row("1245187","0.18",bad)));},"Roughness must be a finite unit-range JSON number");
 }
 for(const char* bad:{"{}","[]","{\"version\":1}","{\"materials\":[]}","{\"version\":2,\"materials\":[]}","{\"version\":true,\"materials\":[]}","{\"version\":1.1,\"materials\":[]}","{\"version\":1,\"materials\":{}}","{\"version\":1,\"materials\":null}","{\"version\":1,\"materials\":[],\"unknown\":0}","{\"version\":1,\"version\":1,\"materials\":[]}"})rejected([&]{parse(bad);},"Strict profile header and version");
 for(const char* bad:{"{}","null","[]","{\"shader\":1,\"reflectivity\":0}","{\"shader\":1,\"roughness\":0}","{\"reflectivity\":0,\"roughness\":0}","{\"shader\":1,\"reflectivity\":0,\"roughness\":0,\"unknown\":1}","{\"shader\":1,\"shader\":2,\"reflectivity\":0,\"roughness\":0}"})rejected([&]{parse(document(bad));},"Strict row schema rejects missing/unknown/duplicate fields");
 rejected([&]{parse(text+" true");},"Trailing JSON data is rejected");
 rejected([&]{parse(text+std::string(1,'\0'));},"Embedded NUL is rejected");
 rejected([&]{parse(std::string("\xef\xbb\xbf")+text);},"BOM is not accepted as JSON whitespace");
 rejected([&]{parse("");},"Empty file is invalid rather than a default request");
 auto limit=text+std::string(65536-text.size(),' ');check(parse(limit)==baseline,"Exactly 64 KiB of valid JSON is accepted");
 rejected([&]{parse(limit+' ');},"64 KiB plus one byte is rejected");
 std::istringstream broken(text);broken.setstate(std::ios::failbit);rejected([&]{read(broken);},"Stream I/O failure is not treated as a missing optional profile");
 std::istringstream throwing(text);throwing.exceptions(std::ios::badbit|std::ios::failbit);check(read(throwing)==baseline,"Valid short input supports exception-enabled streams");
 auto folder=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/"MGO2MT-reflection-profile-tests"/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
 std::filesystem::create_directories(folder);
 check(load(folder/"missing.json")==baseline,"Only a missing optional file selects native defaults");
 const auto fixture=folder/"render_materials.json";write(fixture,text);check(load(fixture)==baseline,"Existing profile is loaded from disk");
 write(fixture,document(""));check(load(fixture).count==0,"Existing empty rule set preserves full disable on disk");
 write(fixture,"broken");rejected([&]{load(fixture);},"Malformed existing file never silently restores reflections");
 write(fixture,limit+' ');rejected([&]{load(fixture);},"Oversized disk profile is rejected");
 rejected([&]{load(folder);},"A directory is not an optional missing profile");
 if(argc>2)check(load(argv[2])==baseline,"Delivered sample matches compiled native defaults");
 std::cout<<"Native reflection material profile PASS: "<<checks<<" checks\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
