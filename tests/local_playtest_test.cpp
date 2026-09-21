#include "local_playtest.h"
#include "port_screen.h"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace mgo2mt;
static void require(bool value){if(!value)throw std::runtime_error("Local playtest isolation failed");}
int main(){
 const auto keyboard=LocalPlaytest::parse(L"--local-playtest-keyboard"),pad=LocalPlaytest::parse(L"--local-playtest-pad-background-3");
 require(keyboard.enabled&&!keyboard.pad&&pad.enabled&&pad.pad&&pad.slot==2);
 for(auto bad:{L"",L"--local-playtest-pad-background-0",L"--local-playtest-pad-background-5",L"--local-playtest-pad-background-11",L"--local-playtest-pad-background-1 --probe-auth",L"--local-playtest-keyboard/../"})require(!LocalPlaytest::parse(bad).enabled);
 for(bool foreground:{false,true})for(bool scripted:{false,true}){
  LocalPlaytest normal;require(normal.pad_active(foreground,scripted)==(foreground&&!scripted));
  require(!keyboard.pad_active(foreground,scripted));require(pad.pad_active(foreground,scripted)==!scripted);
  require(keyboard.gameplay_active(foreground,scripted,0)==(foreground&&!scripted));require(!keyboard.gameplay_active(foreground,scripted,1));
  require(pad.gameplay_active(foreground,scripted,1)==!scripted);require(!pad.gameplay_active(foreground,scripted,0));
 }
 const auto dir=std::filesystem::current_path()/(L"local-playtest-test-"+std::to_wstring(GetCurrentProcessId()));
 require(std::filesystem::create_directory(dir));
 try{
  {std::ofstream f(dir/L"login.dat",std::ios::binary);f<<"synthetic personal sentinel";}
  {std::ofstream f(dir/L"input.cfg",std::ios::binary);f<<"synthetic personal input sentinel";}
  const auto kp=keyboard.profile(dir),pp=pad.profile(dir);require(kp!=pp&&kp!=dir&&pp!=dir&&keyboard.port()==5730&&pad.port()==5731);
  {
   LocalPlaytestSession k,p;k.prepare(keyboard,kp);p.prepare(pad,pp);
   InputConfig ki,pi;PortSettings kn,pn;
   require(load_input(kp/L"input.cfg",ki)&&ki.device==0);require(load_input(pp/L"input.cfg",pi)&&pi.device==1&&pi.slot==2);
   require(load_ports(kp/L"network.cfg",kn)&&!kn.automatic&&kn.port==5730);require(load_ports(pp/L"network.cfg",pn)&&!pn.automatic&&pn.port==5731);
   require(!std::filesystem::exists(kp/L"login.dat")&&!std::filesystem::exists(pp/L"login.dat"));
   auto port_report=[](PortScreen& screen){std::ostringstream out;auto previous=std::cout.rdbuf(out.rdbuf());screen.report();std::cout.rdbuf(previous);return out.str();};
   // Editing, automatic selection and Reset cannot change the role's actual port contract.
   {PortScreen screen(pp/L"network.cfg",false,check_stun,{},{},pad.port());
    auto key=[&](WPARAM k){screen.message(nullptr,WM_KEYDOWN,k,0);};
    key(VK_UP);key(VK_UP); // check -> bandwidth -> port number
    key(VK_DELETE);screen.message(nullptr,WM_CHAR,L'9',0);key(VK_UP);key(VK_RIGHT);key(VK_RETURN);
    for(int i=0;i<5;++i)key(VK_DOWN);key(VK_RETURN); // Reset
    const auto report=port_report(screen);require(report.find("\"automatic\":false")!=std::string::npos&&report.find("\"port_number\":\"5731\"")!=std::string::npos);
   }
   {PortScreen screen(kp/L"network.cfg",false);
    auto key=[&](WPARAM k){screen.message(nullptr,WM_KEYDOWN,k,0);};
    key(VK_UP);key(VK_UP);key(VK_DELETE);screen.message(nullptr,WM_CHAR,L'9',0);key(VK_UP);key(VK_RIGHT);
    const auto report=port_report(screen);require(report.find("\"automatic\":true")!=std::string::npos&&report.find("\"port_number\":\"9\"")!=std::string::npos);
   }
   bool blocked=false;try{LocalPlaytestSession duplicate;duplicate.prepare(pad,pp);}catch(const std::exception&){blocked=true;}require(blocked);
   ControllerInput runtime(pp/L"input.cfg");XINPUT_STATE state{};bool connected=true;unsigned queried=4;
   runtime.reader=[&](DWORD slot,XINPUT_STATE* out){queried=slot;*out=state;return connected?DWORD(ERROR_SUCCESS):DWORD(ERROR_DEVICE_NOT_CONNECTED);};
   auto poll=[&]{return runtime.poll(pad.pad_active(false,false),pad.slot);};
   state.Gamepad.wButtons=XINPUT_GAMEPAD_B;require(!poll().armed&&queried==2);state={};poll();state.Gamepad.wButtons=XINPUT_GAMEPAD_B;require(poll().pressed&&poll().armed);
   connected=false;require(!poll().connected);connected=true;require(!poll().armed);state={};poll();state.Gamepad.sThumbLX=32767;require(poll().left_x==1);
   runtime.poll(false);require(!poll().armed); // Explicit suspension still requires neutral before rearming.
   pi.left_deadzone=33;save_input(pp/L"input.cfg",pi);pn.bandwidth_kbps=1024;pn.port=5730;save_ports(pp/L"network.cfg",pn);
  }
  {LocalPlaytestSession reopened;reopened.prepare(pad,pp);InputConfig input;PortSettings ports;require(load_input(pp/L"input.cfg",input)&&input.left_deadzone==33&&input.slot==2);require(load_ports(pp/L"network.cfg",ports)&&ports.port==5731&&ports.bandwidth_kbps==1024);}
  {std::ifstream f(dir/L"login.dat",std::ios::binary);std::string s((std::istreambuf_iterator<char>(f)),{});require(s=="synthetic personal sentinel");}
  {std::ifstream f(dir/L"input.cfg",std::ios::binary);std::string s((std::istreambuf_iterator<char>(f)),{});require(s=="synthetic personal input sentinel");}
  std::filesystem::remove_all(dir);
 }catch(...){std::error_code ec;std::filesystem::remove_all(dir,ec);throw;}
 std::cout<<"Local test exact options, keyboard/normal foreground gate, explicit background pad, slot/neutral/disconnect, isolated profiles/ports, duplicate exclusion and personal sentinels passed; synthetic XInput only, no sockets or authentication.\n";
}
