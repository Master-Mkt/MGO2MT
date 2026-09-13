#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "item_settings.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2win::items;
namespace {
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
std::string read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
constexpr auto base="MGO2WIN_ITEM_SETTINGS 1\ncapacity 64 64\nrecover_others 1\n";
bool same(const Settings&a,const Settings&b){return a.capacity.dropped==b.capacity.dropped&&a.capacity.installed==b.capacity.installed&&a.recoverOthers==b.recoverOthers&&a.weapons==b.weapons;}
}
int main(){std::filesystem::path directory;try{
 std::string error;Settings s;check(s.policy(25).allows_drop()&&!s.policy(24).allows_drop(),"native initial AK-only policy");check(!s.policy(25).discards_empty(),"unknown empty keeps");
 DropPolicy facts;facts.originalDrop=true;facts.originalEmptyDiscard=true;facts.drop=DropOverride::deny;facts.emptyDiscard=false;
 check(s.policy(24,&facts).allows_drop()&&s.policy(24,&facts).discards_empty(),"merge facts, ignore artifact native controls");
 check(s.parse(std::string(base)+"weapon 25 default default\nweapon 4294967295 allow discard\n",error),"parse maximum weapon ID");check(!s.policy(25).allows_drop()&&s.policy(UINT32_MAX).allows_drop()&&s.policy(UINT32_MAX).discards_empty(),"explicit default removes native initial allow");
 const auto old=s;
 for(const auto& bad:std::vector<std::string>{"", "MGO2WIN_ITEM_SETTINGS 2\ncapacity 1 1\nrecover_others 1\n", "MGO2WIN_ITEM_SETTINGS 1\ncapacity 4097 0\nrecover_others 1\n", "MGO2WIN_ITEM_SETTINGS 1\ncapacity -1 2\nrecover_others 1\n", "MGO2WIN_ITEM_SETTINGS 1\ncapacity 2 2\n",std::string(base)+"capacity 1 1\n",std::string(base)+"recover_others 0\n",std::string(base)+"weapon 25 allow default\nweapon 25 deny keep\n",std::string(base)+"weapon 4294967296 allow keep\n",std::string(base)+"weapon 1 allow unknown\n",std::string(base)+"weapon 1 unknown keep\n",std::string(base)+"weapon 1 allow keep extra\n",std::string(base)+"unknown 1\n",std::string(1024*1024+1,' ')}){
  check(!s.parse(bad,error)&&!error.empty()&&same(s,old),"failed parse changes nothing");
 }
 check(s.parse("# local overrides\nMGO2WIN_ITEM_SETTINGS 1\r\ncapacity 0 4096\r\nrecover_others 0\nweapon 25 deny keep\n",error),"boundary capacities");check(!s.recoverOthers&&s.capacity.dropped==0&&s.capacity.installed==4096&&!s.policy(25,&facts).allows_drop()&&!s.policy(25,&facts).discards_empty(),"independent override values");
 Settings invalid=s;invalid.weapons[25].drop=static_cast<DropOverride>(-1);check(!invalid.valid(),"reject negative enum");invalid=s;invalid.capacity.installed=4097;check(!invalid.valid(),"reject oversized capacity");
 Settings many;many.weapons.clear();for(uint32_t i=0;i<4096;++i)many.weapons.emplace(i,WeaponOverride{});check(many.valid(),"4096 overrides");many.weapons.emplace(4096,WeaponOverride{});check(!many.valid(),"4097 overrides rejected");
 directory=std::filesystem::temp_directory_path()/(L"mgo2win-item-settings-test-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));check(std::filesystem::create_directory(directory),"create isolated directory");const auto path=directory/L"設定.cfg";
 // An existing temporary path must never be deleted when CREATE_NEW fails.
 auto collision=path;collision+=L".tmp."+std::to_wstring(GetCurrentProcessId())+L".1";{std::ofstream f(collision);f<<"owned by another writer";}
 check(!s.save(path,error)&&read(collision)=="owned by another writer"&&!std::filesystem::exists(path),"temporary collision preserves existing file");std::filesystem::remove(collision);
 check(s.save(path,error)&&error.empty(),"save initial");Settings loaded;check(loaded.load(path,error)&&same(loaded,s),"roundtrip exact settings");const auto original=read(path);
 s.capacity={4096,0};s.recoverOthers=true;s.weapons[25]={DropOverride::allow,true};
 HANDLE locked=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);check(locked!=INVALID_HANDLE_VALUE,"lock target");const bool lockSave=s.save(path,error);CloseHandle(locked);check(!lockSave&&read(path)==original,"failed replace preserves target bytes");
 size_t files=0;for(const auto& entry:std::filesystem::directory_iterator(directory)){(void)entry;++files;}check(files==1,"failed replace cleans only owned temp");
 check(s.save(path,error)&&loaded.load(path,error)&&same(loaded,s),"atomic replacement");const auto saved=read(path);check(!invalid.save(path,error)&&read(path)==saved,"invalid save preserves old");
 const auto before=loaded;{std::ofstream f(path);f<<"corrupt";}check(!loaded.load(path,error)&&same(loaded,before),"failed file reload preserves settings");check(!loaded.load(directory/L"missing.cfg",error)&&same(loaded,before),"missing file preserves settings");
 std::filesystem::remove_all(directory);std::cout<<"item_settings_test PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<"item_settings_test FAIL: "<<e.what()<<'\n';return 1;}}
