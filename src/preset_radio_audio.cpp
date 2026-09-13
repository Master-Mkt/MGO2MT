#include "preset_radio_audio.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
namespace mgo2win::radio_audio {
namespace {
constexpr std::array<uint32_t,16> bases{30639,30789,30939,31089,36780,36930,37080,37230,38410,38560,38710,38860,39010,39160,39310,39460};
constexpr bool preset(unsigned id) noexcept {return id<=7||(id>=9&&id<=16);}
constexpr std::optional<unsigned> voice_index(unsigned type) noexcept {
 if(type>=7&&type<=14)return type-7;
 if(type>=16&&type<=23)return type-8;
 return {};
}
}
std::optional<CuePair> resolve(unsigned type,unsigned id,bool specialType23) noexcept {
 const auto index=voice_index(type);
 if(!index||!preset(id)||(type==23&&specialType23))return {};
 return CuePair{bases[*index]+id,bases[*index]+81+id};
}
std::optional<AppearanceVoice> appearance_voice(std::span<const uint8_t> a) noexcept {
 if((a.size()!=27&&a.size()!=28)||a[0]>1||a[8]>30)return {};
 const unsigned first=a[0]?16u:7u;
 if(a[7]<first||a[7]>first+7)return {};
 return AppearanceVoice{a[7],a[8]};
}
std::optional<float> pitch_ratio(unsigned type,unsigned pitch) noexcept {
 if(type>25||pitch>30)return {};
 if(type<=6||type==25)return 1.0f;
 // Raw big-endian IEEE32 values at 11C68D0/11C68D8/11C68E0.
 constexpr float upper=std::bit_cast<float>(0x3f8c28f6u);
 constexpr float scale=std::bit_cast<float>(0x3d888889u);
 constexpr float lower=std::bit_cast<float>(0x3f69ca3au);
 const float exponent=static_cast<float>(static_cast<int>(pitch)-15)*scale;
 return std::clamp(std::pow(upper,exponent),lower,upper);
}
std::filesystem::path asset_path(const std::filesystem::path& root,uint32_t cue) {
 bool valid=false;
 for(auto base:bases) {
  if(cue>=base&&preset(cue-base))valid=true;
  if(cue>=base+81&&preset(cue-base-81))valid=true;
 }
 if(!valid)return {};
 return root/(std::to_string(cue)+".gwa");
}
}
