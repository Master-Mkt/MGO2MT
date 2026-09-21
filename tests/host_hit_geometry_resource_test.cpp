#include "host_hit_geometry.h"
#include <bit>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace mgo2mt::host_hit;
namespace {
void check(bool b,const char*message){if(!b)throw std::runtime_error(message);}
void word(std::vector<unsigned char>&out,uint32_t v){for(unsigned n=0;n<4;++n)out.push_back(static_cast<unsigned char>(v>>(8*n)));}
void scalar(std::vector<unsigned char>&out,float v){word(out,std::bit_cast<uint32_t>(v));}
void replace(std::vector<unsigned char>&out,size_t at,uint32_t v){for(unsigned n=0;n<4;++n)out.at(at+n)=static_cast<unsigned char>(v>>(8*n));}
std::vector<unsigned char> synthetic(){
 std::vector<unsigned char> out{'G','W','H','I','T','1',0,0};for(uint32_t v:{1u,6u,21u,0u})word(out,v);
 for(unsigned p=0;p<6;++p)for(unsigned i=0;i<21;++i){word(out,100+i);
  for(unsigned a=0;a<3;++a)for(unsigned b=0;b<3;++b)scalar(out,a==b?1.f:0.f);
  scalar(out,float(i*3));scalar(out,1000.f+float(p*10+i));scalar(out,-float(i*7));}
 return out;
}
struct Fixture{
 std::filesystem::path directory=std::filesystem::temp_directory_path()/
  ("mgo2mt-hit-resource-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 Fixture(){check(std::filesystem::create_directory(directory),"unique fixture directory");}
 ~Fixture(){std::error_code ec;std::filesystem::remove_all(directory,ec);}
 std::filesystem::path write(const std::vector<unsigned char>&bytes){auto path=directory/"synthetic.gwhit";
  std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());f.close();check(bool(f),"write fixture");return path;}
};
}
int main(){try{
 Fixture fixture;std::string error="old";
 check(configure(fixture.directory/"missing.gwhit",error)==ResourceStatus::native_fallback&&error.empty()&&!using_local_resource(),"missing selects fallback");
 const float fallbackHead=pose(0,Stance::standing)[4].origin[1];
 check(fallbackHead==1600.f&&pose(1,Stance::standing)[4].origin[1]==fallbackHead,"independent shared mannequin");
 auto bytes=synthetic();auto path=fixture.write(bytes);
 check(configure(path,error)==ResourceStatus::local_resource&&error.empty()&&using_local_resource(),"synthetic resource admission");
 for(unsigned p=0;p<6;++p){auto bones=pose(uint8_t(p/3),Stance(p%3));check(bones.size()==21,"resource bone count");
  for(unsigned i=0;i<21;++i){check(bones[i].key==100+i,"resource keys");check(bones[i].origin==Vec3{float(i*3),1000.f+float(p*10+i),-float(i*7)},"resource float identity");}}
 // A returned pose remains valid when configuration changes on another thread
 // or before this thread calls pose again.
 const auto retained=pose(0,Stance::standing);
 check(configure(fixture.directory/"missing.gwhit",error)==ResourceStatus::native_fallback,"reset to fallback");
 check(retained[4].key==104&&retained[4].origin[1]==1004.f,"borrowed pose retains immutable storage");
 auto reject=[&](std::vector<unsigned char> invalid,const char*message){
  fixture.write(bytes);check(configure(path,error)==ResourceStatus::local_resource,"prime old resource");
  fixture.write(invalid);check(configure(path,error)==ResourceStatus::invalid_resource&&!error.empty()&&!using_local_resource(),message);
  check(pose(0,Stance::standing)[4].origin[1]==fallbackHead,"invalid resets old resource");};
 auto bad=bytes;bad[0]='X';reject(bad,"magic rejection");
 bad=bytes;replace(bad,8,2);reject(bad,"version rejection");
 bad=bytes;replace(bad,12,7);reject(bad,"pose count rejection");
 bad=bytes;replace(bad,16,22);reject(bad,"bone count rejection");
 bad=bytes;replace(bad,20,1);reject(bad,"reserved rejection");
 bad=bytes;bad.pop_back();reject(bad,"truncated rejection");
 bad=bytes;bad.push_back(0);reject(bad,"trailing bytes rejection");
 bad=bytes;replace(bad,24,0);reject(bad,"zero key rejection");
 bad=bytes;replace(bad,24+52,100);reject(bad,"duplicate keys rejection");
 bad=bytes;replace(bad,24+21*52,999);reject(bad,"pose key order rejection");
 bad=bytes;replace(bad,28,std::bit_cast<uint32_t>(2.f));reject(bad,"scaled axis rejection");
 bad=bytes;replace(bad,28,std::bit_cast<uint32_t>(-1.f));reject(bad,"reflected axis rejection");
 bad=bytes;replace(bad,64,std::bit_cast<uint32_t>(6000.f));reject(bad,"unbounded position rejection");
 bad=bytes;replace(bad,64,std::bit_cast<uint32_t>(std::numeric_limits<float>::quiet_NaN()));reject(bad,"nonfinite position rejection");
 check(configure(fixture.directory,error)==ResourceStatus::invalid_resource&&!using_local_resource(),"directory is invalid");
 std::cout<<"PASS: authored fallback, exact synthetic resource identity, missing/reset, immutable pose lifetime and malformed-resource rejection\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
