#pragma once
#include <array>
#include <charconv>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include "original_camera_speed.h"
namespace mgo2mt::camera {
// The reference OPTIONS screen has independent vertical/horizontal direction
// choices for normal, shoulder and subjective cameras. Original setting curves
// are mapped by the save converter; absolute rates/integration remain native.
struct Settings {
 std::array<bool,6> reversed{}; // normal Y/X, shoulder Y/X, subjective Y/X
 std::array<unsigned,3> speed{5,5,5}; // independent display values 1..10
 std::array<float,2> motion(bool firstPerson,bool aiming,float turn,float look)const{
  const auto index=firstPerson?4u:aiming?2u:0u;
  return {reversed[index+1]?-turn:turn,reversed[index]?-look:look};
 }
 std::array<float,2> rates(bool firstPerson,bool aiming)const{
  const auto value=speed[firstPerson?2u:aiming?1u:0u];
  const auto mode=firstPerson?original::camera_speed::Mode::firstPerson:
      aiming?original::camera_speed::Mode::shoulder:original::camera_speed::Mode::normal;
  const float scale=original::camera_speed::relative_to_default(mode,value).value_or(1.f);
  return {2.f*scale,1.5f*scale}; // native radians/second; not original full camera dynamics
 }
 bool operator==(const Settings&)const=default;
};
inline std::string encode(const Settings& settings){
 std::string text="MGO2MT.CAMERA 2";
 for(bool value:settings.reversed){text+=' ';text+=value?'1':'0';}
 for(unsigned value:settings.speed){
  if(!original::camera_speed::valid_display(value))throw std::invalid_argument("camera speed outside 1..10");
  text+=' ';text+=std::to_string(value);
 }
 text+='\n';return text;
}
inline std::optional<Settings> decode(std::string_view text){
 const std::string_view prefix=text.starts_with("MGO2WIN.CAMERA ")?"MGO2WIN.CAMERA ":"MGO2MT.CAMERA ";
 if(text.size()<prefix.size()+14||!text.starts_with(prefix)||text.back()!='\n')return std::nullopt;
 const char version=text[prefix.size()];if(version!='1'&&version!='2')return std::nullopt;
 const auto base=prefix.size()+1;
 Settings settings;
 for(unsigned i=0;i<6;++i){const auto at=base+2*i;
  if(text[at]!=' '||(text[at+1]!='0'&&text[at+1]!='1'))return std::nullopt;
  settings.reversed[i]=text[at+1]=='1';
 }
 auto tail=text.substr(base+12); // includes final newline
 if(version=='1'){if(tail!="\n")return std::nullopt;return settings;}
 for(auto&value:settings.speed){
  if(tail.empty()||tail.front()!=' ')return std::nullopt;tail.remove_prefix(1);
  const auto end=tail.find_first_of(" \n");if(end==std::string_view::npos||end==0)return std::nullopt;
  const auto token=tail.substr(0,end);if(token.front()=='0')return std::nullopt;
  const auto parsed=std::from_chars(token.data(),token.data()+token.size(),value);
  if(parsed.ec!=std::errc{}||parsed.ptr!=token.data()+token.size()||!original::camera_speed::valid_display(value))return std::nullopt;
  tail.remove_prefix(end);
 }
 if(tail!="\n")return std::nullopt;
 return settings;
}
}
