#pragma once
#include "controller_input.h"
#include "port_settings.h"
#include <stdexcept>
#include <string_view>
namespace mgo2mt {
// Explicit native local test policy. Slots in switches are one-based; XInput is zero-based.
struct LocalPlaytest {
 bool enabled=false,pad=false;unsigned slot=0;
 static LocalPlaytest parse(std::wstring_view option){
  if(option==L"--local-playtest-keyboard")return {true,false,0};
  constexpr std::wstring_view prefix=L"--local-playtest-pad-background-";
  if(option.size()==prefix.size()+1&&option.starts_with(prefix)&&option.back()>=L'1'&&option.back()<=L'4')return {true,true,unsigned(option.back()-L'1')};
  return {};
 }
 std::filesystem::path profile(const std::filesystem::path& base)const{return enabled?base/L"playtest"/(pad?L"gamepad":L"keyboard"):base;}
 uint16_t port()const{return pad?5731:5730;}
 bool pad_active(bool foreground,bool scripted)const{return !scripted&&(!enabled?foreground:pad);}
 bool gameplay_active(bool foreground,bool scripted,unsigned device)const{return !scripted&&(device?pad_active(foreground,false):foreground)&&(!enabled||device==unsigned(pad));}
 void enforce_input(InputConfig& cfg)const{if(enabled){cfg.device=unsigned(pad);if(pad)cfg.slot=slot;}}
 std::wstring label()const{return !enabled?L"":pad?L" [LOCAL TEST: PAD "+std::to_wstring(slot+1)+L" BACKGROUND / UDP 5731]":L" [LOCAL TEST: KEYBOARD FOREGROUND / UDP 5730]";}
};
// One process per fixed test role. No personal files are copied or login stores opened.
class LocalPlaytestSession {
 HANDLE lock_=INVALID_HANDLE_VALUE;
public:
 LocalPlaytestSession()=default;
 LocalPlaytestSession(const LocalPlaytestSession&)=delete;
 LocalPlaytestSession& operator=(const LocalPlaytestSession&)=delete;
 ~LocalPlaytestSession(){if(lock_!=INVALID_HANDLE_VALUE)CloseHandle(lock_);}
 void prepare(const LocalPlaytest& test,const std::filesystem::path& profile){
  if(!test.enabled)return;
  if(lock_!=INVALID_HANDLE_VALUE)throw std::runtime_error("Local test profile already prepared");
  std::filesystem::create_directories(profile);
  lock_=CreateFileW((profile/L"instance.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(lock_==INVALID_HANDLE_VALUE)throw std::runtime_error("This local test role is already running or its profile cannot be locked");
  InputConfig input;try{load_input(profile/L"input.cfg",input);}catch(...){input={};}
  test.enforce_input(input);save_input(profile/L"input.cfg",input);
  PortSettings ports;try{load_ports(profile/L"network.cfg",ports);}catch(...){ports={};}
  ports.automatic=false;ports.port=test.port();save_ports(profile/L"network.cfg",ports);
 }
};
}
