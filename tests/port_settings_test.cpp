#include "port_settings.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
using namespace mgo2win;
void require(bool b){if(!b)throw std::runtime_error("Port setting contract failed");}
int main(){
 uint16_t n=0;require(parse_port(L"1024",n)&&n==1024);require(parse_port(L"65535",n)&&n==65535);require(parse_port(L"05730",n)&&n==5730);
 for(auto s:{L"",L"0",L"1023",L"65536",L"999999999999",L"+5730",L"5730x",L" 5730",L"５７３０"})require(!parse_port(s,n));
 auto dir=std::filesystem::current_path()/("port-settings-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 require(std::filesystem::create_directory(dir));auto path=dir/"network.cfg";
 try{
  PortSettings cfg,loaded;require(!load_ports(path,loaded));cfg={true,5730};save_ports(path,cfg);require(load_ports(path,loaded)&&loaded.automatic&&loaded.port==5730);
  cfg={false,65535};save_ports(path,cfg);require(load_ports(path,loaded)&&!loaded.automatic&&loaded.port==65535);
  for(unsigned speed=256;speed<=2048;speed+=256){cfg.bandwidth_kbps=static_cast<uint16_t>(speed);save_ports(path,cfg);require(load_ports(path,loaded)&&loaded.bandwidth_kbps==speed);}
  {std::ofstream f(path);f<<"MGO2WIN.NETWORK 1\n1 5730\n";}
  require(load_ports(path,loaded)&&loaded.automatic&&loaded.port==5730&&loaded.bandwidth_kbps==512);
  for(auto body:{"MGO2WIN.NETWORK 2 0 5730 0","MGO2WIN.NETWORK 2 0 5730 255","MGO2WIN.NETWORK 2 0 5730 257","MGO2WIN.NETWORK 2 0 5730 2049","MGO2WIN.NETWORK 2 0 5730 -256","MGO2WIN.NETWORK 2 0 5730 2048 extra","MGO2WIN.NETWORK 3 0 5730 512"}){
   {std::ofstream f(path);f<<body;}bool bad=false;try{load_ports(path,loaded);}catch(...){bad=true;}require(bad);
  }
  for(auto body:{"MGO2WIN.NETWORK 2 0 5730","MGO2WIN.NETWORK 1 2 5730","MGO2WIN.NETWORK 1 0 80","MGO2WIN.NETWORK 1 0 5730 extra"}){
   {std::ofstream f(path);f<<body;}bool bad=false;try{load_ports(path,loaded);}catch(...){bad=true;}require(bad);
  }
  PortReservation first,second;auto a=first.check({true,5730});require(a.status==PortStatus::available&&a.port>=1024);
  auto busy=second.check({false,a.port});require(busy.status==PortStatus::in_use||busy.status==PortStatus::denied);
  auto fallback=second.check({true,a.port});require(fallback.status==PortStatus::available&&fallback.port!=a.port);
  second.reset();first.reset();auto released=second.check({false,a.port});require(released.status==PortStatus::available&&released.port==a.port);
  auto invalid=second.check({false,0});require(invalid.status==PortStatus::failed);
  std::filesystem::remove(path);std::filesystem::remove(dir);
  std::cout<<"Port boundaries, saved settings, corruption rejection, exclusive UDP bind, automatic fallback and release passed; no external packets sent.\n";
 }catch(...){std::error_code ec;std::filesystem::remove(path,ec);std::filesystem::remove(dir,ec);throw;}
}
