#include "round_items.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
#include <stdexcept>
namespace mgo2mt::items {
bool RoundItems::save(const std::filesystem::path& path,std::string& error)const{
 if(!valid()){error="Invalid item settings";return false;}
 static std::atomic<uint64_t> serial{0};std::filesystem::path temporary;HANDLE file=INVALID_HANDLE_VALUE;bool created=false;
 try{const auto bytes=serialize();if(bytes.size()>1048576)throw std::runtime_error("Item settings exceed size limit");
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
