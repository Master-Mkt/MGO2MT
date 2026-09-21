#include "product_identity.h"
#include "item_settings.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <charconv>
#include <fstream>
#include <set>
#include <sstream>
namespace mgo2mt::items {
namespace {
constexpr size_t maximum_file=1024*1024;
bool number(std::string_view s,uint32_t& n){if(s.empty())return false;auto result=std::from_chars(s.data(),s.data()+s.size(),n);return result.ec==std::errc{}&&result.ptr==s.data()+s.size();}
const char* mode(DropOverride d){return d==DropOverride::allow?"allow":d==DropOverride::deny?"deny":"default";}
std::string encode(const Settings& s){std::ostringstream o;o<<"MGO2MT_ITEM_SETTINGS 1\ncapacity "<<s.capacity.dropped<<' '<<s.capacity.installed<<"\nrecover_others "<<(s.recoverOthers?1:0)<<'\n';for(const auto&[id,v]:s.weapons)o<<"weapon "<<id<<' '<<mode(v.drop)<<' '<<(v.emptyDiscard?(*v.emptyDiscard?"discard":"keep"):"default")<<'\n';return o.str();}
}
bool Settings::valid()const noexcept{if(capacity.dropped>4096||capacity.installed>4096||weapons.size()>4096)return false;for(const auto&[id,v]:weapons)if(v.drop!=DropOverride::original_default&&v.drop!=DropOverride::deny&&v.drop!=DropOverride::allow)return false;return true;}
DropPolicy Settings::policy(uint32_t id,const DropPolicy* original)const noexcept{DropPolicy p;if(original){p.originalDrop=original->originalDrop;p.originalEmptyDiscard=original->originalEmptyDiscard;}auto found=weapons.find(id);if(found!=weapons.end()){p.drop=found->second.drop;p.emptyDiscard=found->second.emptyDiscard;}return p;}
bool Settings::parse(std::string_view bytes,std::string& error){try{
 if(bytes.size()>maximum_file)throw std::runtime_error("Item settings exceed 1 MiB");Settings candidate;candidate.weapons.clear();std::istringstream input{std::string(bytes)};std::string line;bool header=false,capacitySeen=false,recoverSeen=false;
 while(std::getline(input,line)){if(line.size()>8192)throw std::runtime_error("Item settings line too long");std::istringstream words(line);std::string key;if(!(words>>key)||key[0]=='#')continue;std::string a,b,c,extra;
  if(!header){if(key!=mgo2mt::brand::Format{"MGO2MT_ITEM_SETTINGS"}||!(words>>a)||a!="1"||(words>>extra))throw std::runtime_error("Unknown item settings version");header=true;continue;}
  if(key=="capacity"){if(capacitySeen||!(words>>a>>b)||(words>>extra)||!number(a,candidate.capacity.dropped)||!number(b,candidate.capacity.installed))throw std::runtime_error("Invalid item capacities");capacitySeen=true;}
  else if(key=="recover_others"){if(recoverSeen||!(words>>a)||(words>>extra)||(a!="0"&&a!="1"))throw std::runtime_error("Invalid recovery permission");candidate.recoverOthers=a=="1";recoverSeen=true;}
  else if(key=="weapon"){uint32_t id=0;if(!(words>>a>>b>>c)||(words>>extra)||!number(a,id)||candidate.weapons.contains(id)||candidate.weapons.size()>=4096)throw std::runtime_error("Invalid or duplicate weapon override");WeaponOverride v;if(b=="allow")v.drop=DropOverride::allow;else if(b=="deny")v.drop=DropOverride::deny;else if(b!="default")throw std::runtime_error("Invalid drop override");if(c=="discard")v.emptyDiscard=true;else if(c=="keep")v.emptyDiscard=false;else if(c!="default")throw std::runtime_error("Invalid empty-item override");candidate.weapons.emplace(id,v);}
  else throw std::runtime_error("Unknown item settings field");
 }
 if(!header||!capacitySeen||!recoverSeen||!candidate.valid())throw std::runtime_error("Incomplete or out-of-range item settings");*this=std::move(candidate);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}}
bool Settings::load(const std::filesystem::path& path,std::string& error){try{std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f){error="Cannot open item settings";return false;}auto size=f.tellg();if(size<0||size>static_cast<std::streamoff>(maximum_file)){error="Invalid item settings size";return false;}std::string text(static_cast<size_t>(size),'\0');f.seekg(0);if(!f.read(text.data(),static_cast<std::streamsize>(text.size()))){error="Cannot read item settings";return false;}return parse(text,error);}catch(const std::exception& e){error=e.what();return false;}}
bool Settings::save(const std::filesystem::path& path,std::string& error)const{
 if(!valid()){error="Invalid item settings";return false;}
 static std::atomic<uint64_t> serial{0};std::filesystem::path temporary;HANDLE file=INVALID_HANDLE_VALUE;bool created=false;
 try{const auto bytes=encode(*this);if(bytes.size()>maximum_file)throw std::runtime_error("Item settings exceed size limit");
  temporary=path;temporary+=L".tmp."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(++serial);
  file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create temporary item settings");
  created=true;
  DWORD written=0;if(!WriteFile(file,bytes.data(),DWORD(bytes.size()),&written,nullptr)||written!=bytes.size()||!FlushFileBuffers(file))throw std::runtime_error("Cannot flush item settings");
  if(!CloseHandle(file)){file=INVALID_HANDLE_VALUE;throw std::runtime_error("Cannot close item settings");}file=INVALID_HANDLE_VALUE;
  if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot replace item settings; original preserved");
  error.clear();return true;
 }catch(const std::exception& e){if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);if(created)DeleteFileW(temporary.c_str());error=e.what();return false;}
}
}
