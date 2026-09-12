#include "stun.h"
#include "port_screen.h"
#include <stdexcept>
#include <iostream>
#include <sstream>
using namespace mgo2win;
void require(bool b){if(!b)throw std::runtime_error("STUN contract failed");}
std::vector<uint8_t> reply(const StunId& id){auto a=stun_request(id);std::vector<uint8_t>b(a.begin(),a.end());b[0]=1;b[3]=12;b.insert(b.end(),{0,0x20,0,8,0,1,0x37,0x70,0xea,0x12,0xd5,0x45});return b;}
int main(){
 StunId id={1,2,3,4,5,6,7,8,9,10,11,12};auto req=stun_request(id);require(req[0]==0&&req[1]==1&&req[3]==0&&req[4]==0x21&&req[19]==12);
 auto b=reply(id);auto r=parse_stun(b,id);require(r.status==StunStatus::success&&r.mapped_port==5730&&r.address==std::array<uint8_t,4>{203,0,113,7});
 auto mapped=b;mapped[21]=1;mapped[26]=0x16;mapped[27]=0x62;mapped[28]=203;mapped[29]=0;mapped[30]=113;mapped[31]=7;require(parse_stun(mapped,id).status==StunStatus::success);
 for(size_t length=0;length<b.size();++length)require(parse_stun(std::span(b).first(length),id).status==StunStatus::protocol_error);
 for(size_t index:{size_t(0),size_t(2),size_t(4),size_t(8),size_t(20),size_t(22),size_t(24),size_t(25)}){auto bad=b;bad[index]^=0x40;require(parse_stun(bad,id).status==StunStatus::protocol_error);}
 auto duplicate=b;duplicate.insert(duplicate.end(),b.begin()+20,b.end());duplicate[3]=24;require(parse_stun(duplicate,id).status==StunStatus::protocol_error);
 auto optional=b;optional[3]=20;optional.insert(optional.end(),{0x80,0x22,0,1,'x',0,0,0});require(parse_stun(optional,id).status==StunStatus::success);
 auto fingerprint=b;fingerprint[3]=20;fingerprint.insert(fingerprint.end(),{0x80,0x28,0,4,0,0,0,0});require(parse_stun(fingerprint,id).status==StunStatus::protocol_error);
 auto error=req;error[0]=1;error[1]=0x11;error[3]=8;std::vector<uint8_t> e(error.begin(),error.end());e.insert(e.end(),{0,9,0,4,0,0,4,1});require(parse_stun(e,id).status==StunStatus::server_error&&parse_stun(e,id).error==401);
 std::atomic_bool cancel=true;require(check_stun(~uintptr_t(0),cancel).status==StunStatus::cancelled);
 auto dir=std::filesystem::current_path()/(L"stun-ui-test-"+std::to_wstring(GetCurrentProcessId()));require(std::filesystem::create_directory(dir));
 try{
  std::atomic_uint calls=0;
  auto pump=[](PortScreen&s,unsigned ms){auto until=GetTickCount64()+ms;do{s.draw();Sleep(5);}while(GetTickCount64()<until);};
  // These UI cases test probes and speed persistence, not ownership of the
  // user's gameplay port. Automatic selection keeps a live client untouched.
  auto automatic=[](PortScreen&s){for(int i=0;i<3;++i)s.message(nullptr,WM_KEYDOWN,VK_UP,0);s.message(nullptr,WM_KEYDOWN,VK_RIGHT,0);for(int i=0;i<3;++i)s.message(nullptr,WM_KEYDOWN,VK_DOWN,0);};
  {PortScreen s(dir/L"network.cfg",true,[&](uintptr_t,const std::atomic_bool& cancelled){++calls;while(!cancelled)Sleep(5);StunResult r;r.status=StunStatus::cancelled;return r;});
   automatic(s);
   s.message(nullptr,WM_KEYDOWN,VK_RETURN,0);pump(s,30);for(int i=0;i<10;++i)s.message(nullptr,WM_KEYDOWN,VK_RETURN,0);require(calls==1);
   s.message(nullptr,WM_KEYDOWN,VK_ESCAPE,0);require(!s.back());s.message(nullptr,WM_KEYDOWN,VK_ESCAPE,0);require(s.back());}
  for(auto status:{StunStatus::success,StunStatus::timeout,StunStatus::protocol_error}){
   unsigned probes=0;PortScreen s(dir/L"network.cfg",true,[&](uintptr_t,const std::atomic_bool&){++probes;StunResult r;r.status=status;r.mapped_port=status==StunStatus::success?5730:0;return r;});
   automatic(s);s.message(nullptr,WM_KEYDOWN,VK_RETURN,0);pump(s,30);s.message(nullptr,WM_KEYDOWN,VK_ESCAPE,0);require(s.back()&&probes==1);
  }
  require(!std::filesystem::exists(dir/L"network.cfg"));
  {PortScreen s(dir/L"network.cfg",false);auto key=[&](WPARAM k){s.message(nullptr,WM_KEYDOWN,k,0);};
   automatic(s);
   key(VK_RETURN); // Reserve port once; speed edits must retain this check.
   key(VK_UP);key(VK_RETURN);key(VK_HOME);key(VK_RETURN);key(VK_DOWN);key(VK_DOWN);key(VK_RETURN);
   PortSettings cfg;require(load_ports(dir/L"network.cfg",cfg)&&cfg.bandwidth_kbps==256);
   key(VK_UP);key(VK_UP);key(VK_RETURN);key(VK_END);key(VK_ESCAPE);require(!s.back());
   key(VK_DOWN);key(VK_DOWN);key(VK_RETURN);require(load_ports(dir/L"network.cfg",cfg)&&cfg.bandwidth_kbps==256);
   key(VK_UP);key(VK_UP);key(VK_RETURN);key(VK_END);key(VK_RETURN);key(VK_DOWN);key(VK_DOWN);key(VK_RETURN);
   require(load_ports(dir/L"network.cfg",cfg)&&cfg.bandwidth_kbps==2048);
  }
  std::filesystem::remove(dir/L"network.cfg");std::filesystem::remove(dir);
 }catch(...){std::error_code ec;std::filesystem::remove(dir/L"network.cfg",ec);std::filesystem::remove(dir,ec);throw;}
 std::cout<<"STUN request/response boundaries, transactions, XOR/mapped addresses, errors, cancellation and UI duplicate suppression passed offline.\n";
}
