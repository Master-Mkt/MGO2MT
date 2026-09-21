#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mgo2mt::unicode_character_name {
// DRAFT native extension. No production opcode or capability negotiation is
// assigned here. Never pass these bytes to an original fixed-width name field.
inline constexpr std::string_view capability="MGO2WIN.unicode-character-name.v1";
inline constexpr size_t minimum_codepoints=4,maximum_codepoints=16,maximum_bytes=64,header_bytes=8;
inline constexpr std::array<uint8_t,4> magic{'G','W','U','N'};
inline constexpr uint8_t version=1;
enum class Error {none,empty,byte_limit,malformed_utf8,codepoint_limit,edge_space,forbidden,reserved};
struct Validation {
 Error error=Error::none;
 size_t codepoints=0,bytes=0;
 explicit operator bool()const{return error==Error::none;}
};
// Counts Unicode scalar values, not UTF-16 units or grapheme clusters. No
// normalization, case folding, trimming, replacement or truncation is performed.
inline Validation validate(std::string_view name){
 Validation result;result.bytes=name.size();
 auto fail=[&](Error error){result.error=error;return result;};
 if(name.empty())return fail(Error::empty);
 if(name.size()>maximum_bytes)return fail(Error::byte_limit);
 std::array<uint32_t,maximum_bytes> points{};
 for(size_t at=0;at<name.size();){
  uint32_t c=static_cast<uint8_t>(name[at++]);unsigned extra=0;uint32_t minimum=0;
  if(c<0x80){}
  else if(c>=0xc2&&c<=0xdf){c&=0x1f;extra=1;minimum=0x80;}
  else if(c>=0xe0&&c<=0xef){c&=0x0f;extra=2;minimum=0x800;}
  else if(c>=0xf0&&c<=0xf4){c&=7;extra=3;minimum=0x10000;}
  else return fail(Error::malformed_utf8);
  if(extra>name.size()-at)return fail(Error::malformed_utf8);
  for(unsigned n=0;n<extra;++n){uint8_t next=static_cast<uint8_t>(name[at++]);if((next&0xc0)!=0x80)return fail(Error::malformed_utf8);c=(c<<6)|(next&0x3f);}
  if(c<minimum||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return fail(Error::malformed_utf8);
  points[result.codepoints++]=c;
 }
 if(result.codepoints<minimum_codepoints||result.codepoints>maximum_codepoints)return fail(Error::codepoint_limit);
 auto space=[](uint32_t c){return c==0x20||c==0x3000;};
 if(space(points[0])||space(points[result.codepoints-1]))return fail(Error::edge_space);
 // Matches CharacterCreation::name_error's existing character policy. In
 // particular this is not a claim that all Unicode format controls are excluded.
 for(size_t n=0;n<result.codepoints;++n){auto c=points[n];
  if(c<32||(c>=127&&c<=159)||c=='\\'||(c>=0x200b&&c<=0x200d)||(c>=0x2028&&c<=0x202e)||
     (c>=0x2066&&c<=0x2069)||c==0xfeff)return fail(Error::forbidden);
 }
 std::string lower(name);for(auto&c:lower)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
 if(lower=="openmgo2"||name.starts_with(":#")||name.starts_with("GM_")||name.starts_with("GM-")||
    name.starts_with("GM.")||name.starts_with("GM,"))return fail(Error::reserved);
 return result;
}
// GWUN | u8 version=1 | u8 reserved=0 | u16 BE byte length | exact UTF-8 bytes.
// No trailing NUL, main-character marker, name padding, or following fields.
inline std::optional<std::vector<uint8_t>> encode(std::string_view name){
 if(!validate(name))return {};
 std::vector<uint8_t> frame;frame.reserve(header_bytes+name.size());frame.insert(frame.end(),magic.begin(),magic.end());
 frame.insert(frame.end(),{version,0,uint8_t(name.size()>>8),uint8_t(name.size())});
 for(unsigned char c:name)frame.push_back(c);
 return frame;
}
inline std::optional<std::string> decode(std::span<const uint8_t> frame){
 if(frame.size()<header_bytes||frame.size()>header_bytes+maximum_bytes)return {};
 for(size_t n=0;n<magic.size();++n)if(frame[n]!=magic[n])return {};
 if(frame[4]!=version||frame[5]!=0)return {};
 size_t length=(size_t(frame[6])<<8)|frame[7];
 if(length>maximum_bytes||frame.size()!=header_bytes+length)return {};
 std::string_view name(reinterpret_cast<const char*>(frame.data()+header_bytes),length);
 if(!validate(name))return {};
 return std::string(name);
}
}
