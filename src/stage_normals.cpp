#include "stage_normals.h"
#include <windows.h>
#include <bcrypt.h>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
namespace mgo2mt::stage {
std::array<float,3> rsx_cmp_normal(uint32_t p){
 auto sign=[](uint32_t v,unsigned bits){return int(v)-((v&(1u<<(bits-1)))?int(1u<<bits):0);};
 std::array<float,3> n{float(sign(p&2047,11)*32),float(sign((p>>11)&2047,11)*32),float(sign((p>>22)&1023,10)*64)};
 float length=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);if(length==0)return {0,1,0};for(auto&v:n)v/=length;return n;
}
size_t apply_original_normals(CharacterModel& model,std::span<const char> gwm,std::span<const char> b){
 auto require=[](bool ok){if(!ok)throw std::runtime_error("Invalid original stage normals");};
 require(b.size()>=44&&b.size()<=8000044&&!std::memcmp(b.data(),"GWN1",4)&&gwm.size()<=64*1024*1024);
 auto word=[&](size_t at){uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(uint8_t(b[at+i]))<<(8*i);return v;};
 const auto nv=word(4),count=word(8);require(nv==model.vertices.size()&&count>0&&count<=nv&&b.size()==44+uint64_t(count)*8);
 std::array<unsigned char,32> digest{};
 require(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<char*>(gwm.data())),ULONG(gwm.size()),digest.data(),ULONG(digest.size()))>=0);
 require(!std::memcmp(digest.data(),b.data()+12,32));
 // Validate all indices before touching the model, including duplicate/order checks.
 uint32_t previous=0;for(uint32_t i=0;i<count;++i){auto index=word(44+size_t(i)*8);require(index<nv&&(!i||index>previous));previous=index;}
 for(uint32_t i=0;i<count;++i){auto&v=model.vertices[word(44+size_t(i)*8)];auto n=rsx_cmp_normal(word(48+size_t(i)*8));v.nx=n[0];v.ny=n[1];v.nz=n[2];}
 return count;
}
size_t load_original_normals(CharacterModel&model,std::span<const char>gwm,const std::filesystem::path&path){
 if(!std::filesystem::exists(path))return 0;
 std::ifstream in(path,std::ios::binary|std::ios::ate);auto size=in.tellg();if(size<44||size>8000044)throw std::runtime_error("Original stage normals extent");
 std::vector<char>b(static_cast<size_t>(size));in.seekg(0);if(!in.read(b.data(),size))throw std::runtime_error("Original stage normals read");
 return apply_original_normals(model,gwm,b);
}
}
