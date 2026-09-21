#pragma once
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
namespace mgo2mt::special_pc {
// Original root-Y compensation is optional local data, never embedded. Missing
// data supplies no compensation; the separate native physical jump still works.
namespace jump_resource {
using Samples=std::array<float,190>;
inline std::atomic<std::shared_ptr<const Samples>> current;
}
inline bool configure_gekko_jump(const std::filesystem::path& file,std::string& error) noexcept {
 try {
  error.clear();jump_resource::current.store({});
  if(!std::filesystem::exists(file))return true;
  std::ifstream in(file,std::ios::binary|std::ios::ate);
  if(!in||in.tellg()!=776)throw std::runtime_error("Invalid special/gekko_jump.gwjc length");
  in.seekg(0);std::array<unsigned char,776> bytes{};in.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
  if(!in)throw std::runtime_error("Cannot read special/gekko_jump.gwjc");
  constexpr std::array<unsigned char,8> magic{'G','W','G','J','1',0,0,0};
  for(size_t i=0;i<magic.size();++i)if(bytes[i]!=magic[i])throw std::runtime_error("Invalid Gekko curve magic");
  auto u=[&](size_t at){uint32_t value=0;for(unsigned i=0;i<4;++i)value|=uint32_t(bytes[at+i])<<(8*i);return value;};
  if(u(8)!=1||u(12)!=190)throw std::runtime_error("Invalid Gekko curve header");
  auto values=std::make_shared<jump_resource::Samples>();
  for(size_t i=0;i<values->size();++i){auto value=std::bit_cast<float>(u(16+i*4));if(!std::isfinite(value)||value<0||value>20000)throw std::runtime_error("Invalid Gekko curve sample");(*values)[i]=value;}
  if(values->front()!=0||values->back()!=0)throw std::runtime_error("Invalid Gekko curve endpoints");
  jump_resource::current.store(std::move(values));return true;
 } catch(const std::exception& e){error=e.what();return false;}catch(...){error="Cannot configure Gekko curve";return false;}
}
inline bool using_local_gekko_jump() noexcept{return bool(jump_resource::current.load());}
inline float gekko_jump_height_seconds(double seconds){
 if(!std::isfinite(seconds)||seconds<=0||seconds>=3.15)return 0;
 const auto samples=jump_resource::current.load();if(!samples)return 0;
 const double frame=seconds*60.;const auto at=uint32_t(frame);const auto alpha=float(frame-at);
 return (*samples)[at]*(1-alpha)+(*samples)[at+1]*alpha;
}
inline float gekko_jump_height_ms(uint32_t elapsed){return gekko_jump_height_seconds(double(elapsed)/1000.);}
}
