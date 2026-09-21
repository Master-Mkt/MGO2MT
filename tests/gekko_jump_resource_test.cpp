#include "gekko_jump_curve.h"
#include <chrono>
#include <iostream>
#include <limits>
#include <vector>
using namespace mgo2mt::special_pc;
int main(){
 const auto directory=std::filesystem::temp_directory_path()/("mgo2mt-jump-resource-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 std::filesystem::create_directory(directory);const auto file=directory/"curve.gwjc";std::string error;
 auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
 try{
  check(configure_gekko_jump(file,error)&&error.empty()&&!using_local_gekko_jump()&&gekko_jump_height_seconds(1)==0,"missing uses no compensation");
  std::vector<unsigned char> bytes(776);const unsigned char magic[]{'G','W','G','J','1',0,0,0};std::copy(std::begin(magic),std::end(magic),bytes.begin());
  auto u=[&](size_t at,uint32_t value){for(unsigned i=0;i<4;++i)bytes.at(at+i)=uint8_t(value>>(8*i));};
  u(8,1);u(12,190);u(16+60*4,std::bit_cast<uint32_t>(600.f));u(16+61*4,std::bit_cast<uint32_t>(1200.f));
  const auto valid=bytes;
  auto write=[&](){std::ofstream out(file,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());check(bool(out),"write fixture");};
  auto prime=[&](){bytes=valid;write();check(configure_gekko_jump(file,error)&&using_local_gekko_jump(),"valid resource");};
  prime();check(gekko_jump_height_seconds(1)==600&&std::abs(gekko_jump_height_seconds(60.5/60)-900)<.01,"60fps interpolation");
  check(gekko_jump_height_seconds(0)==0&&gekko_jump_height_seconds(3.15)==0&&gekko_jump_height_seconds(std::numeric_limits<double>::quiet_NaN())==0,"invalid and end times");
  auto reject=[&](){write();check(!configure_gekko_jump(file,error)&&!error.empty()&&!using_local_gekko_jump()&&gekko_jump_height_seconds(1)==0,"invalid resets previous table");};
  for(auto at:{0u,8u,12u}){prime();bytes[at]^=1;reject();}
  prime();bytes.pop_back();reject();prime();bytes.push_back(0);reject();
  for(float value:{-1.f,20001.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){prime();u(16+60*4,std::bit_cast<uint32_t>(value));reject();}
  prime();u(16,std::bit_cast<uint32_t>(1.f));reject();prime();u(16+189*4,std::bit_cast<uint32_t>(1.f));reject();
  prime();std::filesystem::remove(file);check(configure_gekko_jump(file,error)&&error.empty()&&!using_local_gekko_jump(),"missing resets previous table");
  std::filesystem::remove(directory);std::cout<<"Gekko local compensation resource validated\n";return 0;
 }catch(const std::exception& e){std::filesystem::remove(file);std::filesystem::remove(directory);std::cerr<<e.what()<<'\n';return 1;}
}
