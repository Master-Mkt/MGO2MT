#include "weapon_connection_points.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
namespace mgo2mt::weapon_hand::connections {
bool Table::decode(std::span<const char> bytes,std::string& error){
 rows_.clear();try{
  if(bytes.size()<12||bytes.size()>12+4096*64||std::memcmp(bytes.data(),"GCP1",4))throw std::runtime_error("CNP resource magic or size");
  size_t at=4;auto word=[&](){if(at+4>bytes.size())throw std::runtime_error("CNP resource truncated");uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(uint8_t(bytes[at++]))<<(i*8);return v;};
  if(word()!=1)throw std::runtime_error("CNP resource version");const auto count=word();if(!count||count>4096||bytes.size()!=12+size_t(count)*64)throw std::runtime_error("CNP resource row extent");
  std::vector<Point> next;next.reserve(count);std::set<std::pair<uint32_t,uint32_t>> keys;std::map<uint32_t,std::string> hashes;
  auto scalar=[&](){float v=std::bit_cast<float>(word());if(!std::isfinite(v)||std::abs(v)>1000000)throw std::runtime_error("CNP resource coordinate");return v;};
  constexpr char digits[]="0123456789abcdef";
  for(uint32_t i=0;i<count;++i){Point p;p.weapon=word();p.key=word();if(!p.weapon||p.weapon>511||p.key>0xffffff||!keys.emplace(p.weapon,p.key).second)throw std::runtime_error("CNP resource duplicate or invalid identity");unsigned nonzero=0;
   for(unsigned j=0;j<32;++j){auto b=uint8_t(bytes[at++]);nonzero|=b;p.mdnSha256+=digits[b>>4];p.mdnSha256+=digits[b&15];}if(!nonzero)throw std::runtime_error("CNP resource missing model identity");
   auto[hash,inserted]=hashes.emplace(p.weapon,p.mdnSha256);if(!inserted&&hash->second!=p.mdnSha256)throw std::runtime_error("CNP resource inconsistent model identity");
   for(auto&v:p.axis.rear)v=scalar();for(auto&v:p.axis.front)v=scalar();double length=0;for(unsigned j=0;j<3;++j){double d=double(p.axis.front[j])-p.axis.rear[j];length+=d*d;}if(length<1e-8)throw std::runtime_error("CNP resource degenerate axis");next.push_back(std::move(p));
  }
  rows_=std::move(next);error.clear();return true;
 }catch(const std::exception&e){error=e.what();return false;}
}
bool Table::load(const std::filesystem::path&path,std::string&error){
 rows_.clear();try{std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in||in.tellg()<12||in.tellg()>12+4096*64)throw std::runtime_error("CNP resource missing or size");std::vector<char> bytes(size_t(in.tellg()));in.seekg(0);if(!in.read(bytes.data(),std::streamsize(bytes.size())))throw std::runtime_error("CNP resource read");return decode(bytes,error);}catch(const std::exception&e){error=e.what();return false;}
}
const Point*Table::find(uint32_t weapon,uint32_t key)const{auto it=std::find_if(rows_.begin(),rows_.end(),[&](const auto&p){return p.weapon==weapon&&p.key==key;});return it==rows_.end()?nullptr:&*it;}
}
