#include "stage_surface_alpha.h"
#include <windows.h>
#include <bcrypt.h>
#include <cstring>
#include <fstream>
#include <stdexcept>
namespace mgo2mt::stage {
size_t apply_surface_alpha(CharacterModel&m,std::span<const char>gwm,std::span<const char>b){
 auto require=[](bool ok){if(!ok)throw std::runtime_error("Invalid reviewed stage surface alpha");};
 require(b.size()>=48&&b.size()<=48+4096*24&&!std::memcmp(b.data(),"GSA1",4)&&gwm.size()>=48&&gwm.size()<=64*1024*1024);
 size_t at=4;auto word=[&](){require(at+4<=b.size());uint32_t v=0;for(unsigned k=0;k<4;++k)v|=uint32_t(uint8_t(b[at++]))<<(8*k);return v;};
 const auto version=word(),parts=word(),count=word();require(version==1&&parts==m.parts.size()&&count&&count<=parts&&b.size()==48+size_t(count)*24);
 std::array<unsigned char,32>sha{};require(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<char*>(gwm.data())),ULONG(gwm.size()),sha.data(),32)>=0);require(!std::memcmp(sha.data(),b.data()+16,32));at=48;
 std::vector<uint32_t>checked;checked.reserve(count);
 for(unsigned i=0;i<count;++i){const auto index=word(),first=word(),size=word(),texture=word(),shader=word(),mode=word();
  require(index<parts&&(checked.empty()||index>checked.back())&&shader==0x120000&&mode==1);const auto&p=m.parts[index];
  require(p.first==first&&p.count==size&&p.texture==texture&&p.materialShader==shader&&!p.flags&&!p.surfaceAlpha&&texture<m.textures.size()&&uint64_t(first)+size<=m.indices.size());
  for(size_t j=first;j<size_t(first)+size;++j)require(m.indices[j]<m.vertices.size());checked.push_back(index);
 }
 // Atomically apply only after the original file and every selected part match.
 for(auto index:checked)m.parts[index].surfaceAlpha=1;
 return checked.size();
}
size_t load_surface_alpha(CharacterModel&m,std::span<const char>gwm,const std::filesystem::path&p){
 if(!std::filesystem::exists(p))return 0;std::ifstream in(p,std::ios::binary|std::ios::ate);const auto n=in.tellg();if(n<48||n>48+4096*24)throw std::runtime_error("Stage surface alpha extent");std::vector<char>b(static_cast<size_t>(n));in.seekg(0);if(!in.read(b.data(),n))throw std::runtime_error("Stage surface alpha read");return apply_surface_alpha(m,gwm,b);
}
}
