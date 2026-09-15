#include "motion_blend_settings.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
#include <charconv>
#include <fstream>
#include <stdexcept>

namespace mgo2win::motion_blend {
bool Settings::valid()const noexcept{return percentPerSecond>=minimum&&percentPerSecond<=maximum;}
bool Settings::set(unsigned value)noexcept{if(value<minimum||value>maximum)return false;percentPerSecond=value;return true;}
double Settings::rate_per_second()const{if(!valid())throw std::invalid_argument("Invalid motion blend rate");return double(percentPerSecond)/100.;}
double Settings::completion_seconds()const{if(!valid())throw std::invalid_argument("Invalid motion blend rate");return 100./double(percentPerSecond);}
std::string encode(const Settings& settings){
 if(!settings.valid())throw std::invalid_argument("Invalid motion blend rate");
 return "MGO2WIN.MOTION_BLEND 1 "+std::to_string(settings.percentPerSecond)+'\n';
}
std::optional<Settings> decode(std::string_view text)noexcept{
 constexpr std::string_view prefix="MGO2WIN.MOTION_BLEND 1 ";
 if(text.size()>128||!text.starts_with(prefix)||text.size()<=prefix.size()+1||text.back()!='\n')return std::nullopt;
 text.remove_prefix(prefix.size());text.remove_suffix(1);
 if(text.empty()||text.front()=='0')return std::nullopt;
 unsigned value=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
 Settings result;if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||!result.set(value))return std::nullopt;
 return result;
}
Settings load(const std::filesystem::path& path,bool* usedFallback)noexcept{
 if(usedFallback)*usedFallback=true;
 try{
  std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)return {};
  const auto size=file.tellg();if(size<=0||size>128)return {};
  std::string text(static_cast<size_t>(size),'\0');file.seekg(0);
  if(!file.read(text.data(),static_cast<std::streamsize>(text.size())))return {};
  // A file that grew during reading must not be accepted as a truncated config.
  if(file.peek()!=std::char_traits<char>::eof())return {};
  if(auto result=decode(text)){if(usedFallback)*usedFallback=false;return *result;}
 }catch(...){}
 return {};
}
bool save(const std::filesystem::path& path,const Settings& settings)noexcept{
 if(!settings.valid())return false;
 static std::atomic<unsigned long long> serial{0};
 std::filesystem::path temporary;HANDLE file=INVALID_HANDLE_VALUE;bool created=false;
 try{
  const auto text=encode(settings);
  temporary=path;temporary+=L".tmp."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64())+L"."+std::to_wstring(++serial);
  file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE)return false;created=true;
  DWORD written=0;bool okay=WriteFile(file,text.data(),DWORD(text.size()),&written,nullptr)&&written==text.size()&&FlushFileBuffers(file);
  if(!CloseHandle(file))okay=false;file=INVALID_HANDLE_VALUE;
  if(okay&&MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;
 }catch(...){}
 if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
 if(created)DeleteFileW(temporary.c_str());
 return false;
}
}
