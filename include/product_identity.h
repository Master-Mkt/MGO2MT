#pragma once
#include <string>
#include <string_view>

namespace mgo2mt::brand {
// Only serialized format identifiers accept the former product prefix. This
// preserves local converted resources without changing original asset bytes.
struct Format {
 std::string_view current;
 friend constexpr bool operator==(std::string_view value,Format expected){
  if(value==expected.current)return true;
  constexpr std::string_view previous="MGO2WIN",now="MGO2MT";
  return value.starts_with(previous)&&expected.current.starts_with(now)&&
   value.substr(previous.size())==expected.current.substr(now.size());
 }
};
inline std::string normalize_format(std::string_view value){
 if(value.starts_with("MGO2WIN."))return "MGO2MT"+std::string(value.substr(7));
 if(value.starts_with("MGO2WIN_"))return "MGO2MT"+std::string(value.substr(7));
 return std::string(value);
}
}
