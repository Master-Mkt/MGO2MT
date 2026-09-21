#include "evade_travel_curve.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <vector>
using namespace mgo2mt::combat::evade_runtime;
namespace {
void check(bool b,const char*message){if(!b)throw std::runtime_error(message);}
void word(std::vector<unsigned char>&out,uint32_t v){for(unsigned n=0;n<4;++n)out.push_back(static_cast<unsigned char>(v>>(8*n)));}
void replace(std::vector<unsigned char>&out,size_t at,uint32_t v){for(unsigned n=0;n<4;++n)out.at(at+n)=static_cast<unsigned char>(v>>(8*n));}
std::vector<unsigned char> synthetic(){std::vector<unsigned char> out{'G','W','E','V','A','D','1',0};
 for(uint32_t v:{1u,41u,46u,0u})word(out,v);
 for(unsigned i=0;i<41;++i)word(out,std::bit_cast<uint32_t>(float(i*80)));
 for(unsigned i=0;i<46;++i)word(out,std::bit_cast<uint32_t>(4000.f+float(i*10)));
 return out;
}
struct Fixture {
 std::filesystem::path directory=std::filesystem::temp_directory_path()/("mgo2mt-evade-resource-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 Fixture(){check(std::filesystem::create_directory(directory),"unique fixture directory");}
 ~Fixture(){std::error_code ec;std::filesystem::remove_all(directory,ec);}
 std::filesystem::path write(const std::vector<unsigned char>&data){auto p=directory/"curve.gwet";std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(data.data()),data.size());f.close();check(bool(f),"write fixture");return p;}
};
void timing(float total){
 check(distance_seconds(-1)==0&&distance_seconds(std::numeric_limits<double>::infinity())==0&&distance_seconds(std::numeric_limits<double>::quiet_NaN())==0,"invalid time");
 check(distance_seconds(1.25)==total&&distance_seconds(1e300)==total,"terminal stop");
 for(unsigned fps:{30u,60u,144u,1000u}){double sum=0,previous=0;float at=0;
  for(unsigned i=1;previous<1.417;++i){const double next=std::min(1.417,double(i)/fps);float value=distance_seconds(next);
   check(value>=at&&double(value-at)<=6000*(next-previous)+.001,"monotonic speed bound");sum+=double(value)-at;previous=next;at=value;}
  check(sum==total,"partition independent travel");}
}
}
int main(){try{
 Fixture fixture;std::string error="old";auto missing=fixture.directory/"missing.gwet";
 check(configure(missing,error)==ResourceStatus::native_fallback&&!using_local_resource()&&error.empty(),"missing authored fallback");timing(3500);
 auto bytes=synthetic();auto path=fixture.write(bytes);
 check(configure(path,error)==ResourceStatus::local_resource&&using_local_resource()&&error.empty(),"local synthetic load");timing(3550);
 auto selected=source_profile();check(selected&&selected->roll[40]==3200&&selected->recover[45]==4450,"raw local values");
 check(configure(missing,error)==ResourceStatus::native_fallback&&selected->distances.back()==3550,"immutable reader lifetime");
 auto reject=[&](std::vector<unsigned char> bad,const char*message){fixture.write(bytes);check(configure(path,error)==ResourceStatus::local_resource,"prime local");fixture.write(bad);
  check(configure(path,error)==ResourceStatus::invalid_resource&&!error.empty()&&!using_local_resource(),message);timing(3500);};
 auto bad=bytes;bad[0]='X';reject(bad,"bad magic");
 for(size_t offset:{8u,12u,16u,20u}){bad=bytes;replace(bad,offset,999);reject(bad,"bad header");}
 bad=bytes;bad.pop_back();reject(bad,"truncated");bad=bytes;bad.push_back(0);reject(bad,"trailing bytes");
 bad=bytes;replace(bad,24,std::bit_cast<uint32_t>(std::numeric_limits<float>::infinity()));reject(bad,"nonfinite");
 bad=bytes;replace(bad,24,std::bit_cast<uint32_t>(100001.f));reject(bad,"unbounded");
 bad=bytes;for(size_t i=24;i<bad.size();i+=4)replace(bad,i,0);reject(bad,"empty motion");
 check(configure(fixture.directory,error)==ResourceStatus::invalid_resource,"directory rejected");
 std::cout<<"PASS: authored evade math, local source samples, timing/stop/speed, immutable lifetime and invalid resource reset\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
