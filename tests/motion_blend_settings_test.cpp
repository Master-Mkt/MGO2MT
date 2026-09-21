#include "motion_blend_settings.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace mgo2mt::motion_blend;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static std::string bytes(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
int main(){try{
 Settings settings;
 check(settings.valid()&&settings.percentPerSecond==500,"enabled default is 500 percent per second");
 check(settings.rate_per_second()==5.&&settings.completion_seconds()==.2,"percent is normalized progress per SECOND, not per frame");
 for(unsigned value=10;value<=2000;++value){
  check(settings.set(value),"all supported integer values accepted");
  check(decode(encode(settings))==settings,"every permitted value has a canonical codec roundtrip");
  check(std::abs(settings.rate_per_second()*settings.completion_seconds()-1.)<1e-12,"rate and completion duration agree");
 }
 check(settings.completion_seconds()==.05,"2000 percent per second completes in 50ms");
 check(settings.set(10)&&settings.completion_seconds()==10.,"10 percent per second completes in 10s");
 for(auto value:{0u,1u,9u,2001u,(std::numeric_limits<unsigned>::max)()})check(!settings.set(value)&&settings.percentPerSecond==10,"invalid change keeps active rate, zero/off prohibited");
 for(auto text:{"", "MGO2MT.MOTION_BLEND 1 0\n", "MGO2MT.MOTION_BLEND 1 9\n", "MGO2MT.MOTION_BLEND 1 2001\n",
  "MGO2MT.MOTION_BLEND 1 0500\n", "MGO2MT.MOTION_BLEND 1 +500\n", "MGO2MT.MOTION_BLEND 1 -500\n",
  "MGO2MT.MOTION_BLEND 1 500.0\n", "MGO2MT.MOTION_BLEND 1 500%\n", "MGO2MT.MOTION_BLEND 1 off\n",
  "MGO2MT.MOTION_BLEND 2 500\n", "MGO2MT.MOTION_BLEND 1 500", "MGO2MT.MOTION_BLEND 1 500\r\n",
  "MGO2MT.MOTION_BLEND 1  500\n", "MGO2MT.MOTION_BLEND 1 500 \n", "MGO2MT.MOTION_BLEND 1 500\nextra\n",
  "MGO2MT.MOTION_BLEND 1 42949672960\n"})check(!decode(text),"strict cfg rejects malformed/noncanonical/off values");
 check(!decode(std::string(1000,'x')),"oversized cfg rejected");
 auto invalid=Settings{};invalid.percentPerSecond=0;
 bool rejected=false;try{encode(invalid);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid direct struct state cannot encode");
 rejected=false;try{invalid.completion_seconds();}catch(const std::invalid_argument&){rejected=true;}check(rejected,"no zero division or infinite transition duration");
 const auto dir=std::filesystem::temp_directory_path()/(L"mgo2mt-blend-settings-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
 check(std::filesystem::create_directory(dir),"unique isolated settings test directory");
 const auto path=dir/L"motion_blend.cfg";bool fallback=false;
 check(load(path,&fallback)==Settings{}&&fallback&&!std::filesystem::exists(path),"missing cfg uses default without creating file");
 check(settings.set(1250)&&save(path,settings),"first save");
 check(load(path,&fallback)==settings&&!fallback&&bytes(path)==encode(settings),"saved data reads back exactly");
 check(settings.set(700)&&save(path,settings)&&load(path)==settings,"atomic replacement persists new setting");
 const auto old=bytes(path);
 check(!save(path,invalid)&&bytes(path)==old,"invalid save preserves existing setting");
 HANDLE locked=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 check(locked!=INVALID_HANDLE_VALUE,"lock destination against replacement for failure test");
 settings.set(1500);const bool saved=save(path,settings);CloseHandle(locked);
 check(!saved&&bytes(path)==old,"rename failure preserves old config");
 size_t count=0;for(const auto& ignored:std::filesystem::directory_iterator(dir)){(void)ignored;++count;}check(count==1,"failed save cleans only its own temporary file");
 {std::ofstream file(path,std::ios::binary|std::ios::trunc);file<<"MGO2MT.MOTION_BLEND 1 0\n";}
 const auto corrupt=bytes(path);check(load(path,&fallback)==Settings{}&&fallback&&bytes(path)==corrupt,"broken cfg falls back to enabled default without rewrite");
 {std::ofstream file(path,std::ios::binary|std::ios::trunc);file<<std::string(500,'x');}
 check(load(path,&fallback)==Settings{}&&fallback,"oversized file uses default");
 // Only files created in this fresh isolated fixture are removed.
 std::filesystem::remove(path);std::filesystem::remove(dir);
 std::cout<<"motion blend units/ranges/strict codec/fallback/atomic save passed\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
