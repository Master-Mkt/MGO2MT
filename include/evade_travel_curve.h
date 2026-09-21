#pragma once
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
namespace mgo2mt::combat::evade_runtime {
inline constexpr float travel_speed_cap=6000.f;
inline constexpr double travel_fps=60.;
inline constexpr size_t recover_stop_frame=35;
struct Curve {
 std::array<float,41> roll{};
 std::array<float,46> recover{};
 std::array<float,86> distances{};
};
enum class ResourceStatus:uint8_t {native_fallback,local_resource,invalid_resource};
inline std::atomic<std::shared_ptr<const Curve>> local_curve{};
// Authored mathematical substitute, not sampled animation: smooth 3.5m travel
// over 1.25s, then stationary recovery. Its peak speed is below the HOST cap.
inline constexpr auto fallback_distances=[] {
 std::array<float,86> result{};
 for(size_t i=0;i<result.size();++i){float t=i>=75?1.f:float(i)/75.f;result[i]=3500.f*t*t*(3.f-2.f*t);}
 return result;
}();
inline std::shared_ptr<const Curve> source_profile() noexcept{return local_curve.load();}
inline bool using_local_resource() noexcept{return bool(local_curve.load());}
// GWEVAD1: 8-byte magic; LE u32 version/roll count/recover count/reserved
// (1/41/46/0); then 87 LE IEEE float32 root-Z samples. Not a public asset.
// Missing or invalid resources reset to the authored fallback. Invalid inputs
// return a diagnostic so callers can reject startup instead of hiding a fault.
inline ResourceStatus configure(const std::filesystem::path&file,std::string&error) noexcept {
 error.clear();
 try {
  std::error_code ec;const bool exists=std::filesystem::exists(file,ec);
  if(ec)throw std::runtime_error("Evade travel resource cannot be inspected");
  if(!exists){local_curve.store({});return ResourceStatus::native_fallback;}
  constexpr size_t size=24+87*4;
  if(!std::filesystem::is_regular_file(file,ec)||ec||std::filesystem::file_size(file,ec)!=size||ec)
   throw std::runtime_error("Invalid evade travel resource size/type");
  std::array<unsigned char,size> bytes{};std::ifstream in(file,std::ios::binary);
  if(!in.read(reinterpret_cast<char*>(bytes.data()),bytes.size())||in.peek()!=std::char_traits<char>::eof())
   throw std::runtime_error("Cannot read complete evade travel resource");
  constexpr std::array<unsigned char,8> magic{'G','W','E','V','A','D','1',0};
  for(size_t i=0;i<magic.size();++i)if(bytes[i]!=magic[i])throw std::runtime_error("Invalid evade travel resource magic");
  size_t at=8;auto word=[&]{uint32_t v=uint32_t(bytes[at])|uint32_t(bytes[at+1])<<8|uint32_t(bytes[at+2])<<16|uint32_t(bytes[at+3])<<24;at+=4;return v;};
  if(word()!=1||word()!=41||word()!=46||word()!=0)throw std::runtime_error("Unsupported evade travel resource layout");
  auto curve=std::make_shared<Curve>();
  auto read=[&](auto&target){for(float&v:target){v=std::bit_cast<float>(word());if(!std::isfinite(v)||std::abs(v)>100000)throw std::runtime_error("Invalid evade travel sample");}};
  read(curve->roll);read(curve->recover);
  size_t k=1;auto append=[&](float delta){delta=delta<0?0:delta>100?100:delta;curve->distances[k]=curve->distances[k-1]+delta;++k;};
  for(size_t i=1;i<curve->roll.size();++i)append(curve->roll[i]-curve->roll[i-1]);
  for(size_t i=1;i<curve->recover.size();++i)append(i>recover_stop_frame?0:curve->recover[i]-curve->recover[i-1]);
  if(curve->distances.back()<=0||curve->distances.back()>8500)throw std::runtime_error("Empty or unbounded evade travel curve");
  local_curve.store(std::move(curve));return ResourceStatus::local_resource;
 }catch(const std::exception&e){local_curve.store({});try{error=e.what();}catch(...){}return ResourceStatus::invalid_resource;}
 catch(...){local_curve.store({});try{error="Evade travel resource error";}catch(...){}return ResourceStatus::invalid_resource;}
}
// Pure cumulative distance: call differences at action ages. Finite huge times
// clamp; negative/NaN/infinite input returns zero. Both local and fallback
// curves respect the same frame duration, speed ceiling and terminal stop.
inline float distance_seconds(double seconds){
 if(!std::isfinite(seconds)||seconds<0)return 0;
 const auto selected=local_curve.load();const auto&distances=selected?selected->distances:fallback_distances;
 if(seconds>=85./60)return distances.back();
 const double f=seconds*travel_fps;const auto i=static_cast<size_t>(f);
 return float(double(distances[i])+(double(distances[i+1])-distances[i])*(f-double(i)));
}
}
