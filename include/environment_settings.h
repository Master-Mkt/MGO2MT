#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <istream>
#include <mutex>
#include <optional>
namespace mgo2mt::environment {
enum class Time:uint8_t {original,dawn,morning,day,evening,night};
enum class Sound:uint8_t {original,rain,blizzard,sandstorm,forest,silent};
// Native HOST presentation policy. Defaults preserve each stage's current
// original/native environment. Manual hemisphere takes precedence over Time.
struct Config {
 bool weatherOverride=false,fog=false,rain=false,snow=false,sandstorm=false;
 Time time=Time::original;Sound sound=Sound::original;
 bool manualHemisphere=false;
 std::array<uint8_t,3> upper{180,190,210},lower{55,55,50};
 uint16_t gainMilli=1000; // exact 0..4, in 0.001 steps; no wire NaN/Inf
 bool operator==(const Config&)const=default;
};
inline bool valid(const Config&c){return unsigned(c.time)<=unsigned(Time::night)&&unsigned(c.sound)<=unsigned(Sound::silent)&&c.gainMilli<=4000;}
struct View {uint64_t revision=1;Config config;bool operator==(const View&)const=default;};
class Control {
 mutable std::mutex mutex_;View view_;
public:
 View state()const{std::lock_guard lock(mutex_);return view_;}
 bool configure(const Config&c){if(!valid(c))return false;std::lock_guard lock(mutex_);if(view_.config==c)return true;if(view_.revision==UINT64_MAX)return false;view_.config=c;++view_.revision;return true;}
};
Config read(std::istream&);
Config load(const std::filesystem::path&); // missing file -> original settings
void save(const std::filesystem::path&,const Config&);
}
