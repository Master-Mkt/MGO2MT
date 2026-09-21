#include "unicode_character_name.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2mt::unicode_character_name;
static void check(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
static std::string scalar(uint32_t c){
 std::string out;if(c<=0x7f)out+=char(c);
 else if(c<=0x7ff){out+=char(0xc0|(c>>6));out+=char(0x80|(c&63));}
 else if(c<=0xffff){out+=char(0xe0|(c>>12));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}
 else{out+=char(0xf0|(c>>18));out+=char(0x80|((c>>12)&63));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}return out;
}
static std::string repeated(uint32_t c,unsigned count){std::string out;while(count--)out+=scalar(c);return out;}
static void roundtrip(const std::string&name,size_t count){auto valid=validate(name);check(bool(valid)&&valid.codepoints==count&&valid.bytes==name.size(),"valid scalar count and byte count");auto frame=encode(name);check(frame&&frame->size()==header_bytes+name.size(),"encoded exact extent");auto restored=decode(*frame);check(restored&&*restored==name,"lossless roundtrip");}
int main(){try{
 check(validate("").error==Error::empty,"empty name");check(!validate("abc"),"minimum four scalars");roundtrip("ABCD",4);roundtrip("abcdefghijklmnop",16);check(!validate("abcdefghijklmnopq"),"17 ASCII scalars");
 roundtrip(repeated(0x65e5,4),4);roundtrip(repeated(0x65e5,16),16);check(!validate(repeated(0x65e5,17)),"17 Japanese scalars");
 roundtrip(repeated(0xd55c,16),16);roundtrip(repeated(0x1f600,16),16);check(repeated(0x1f600,16).size()==64,"16 supplementary scalars require 64 bytes");
 roundtrip(repeated(0x65e5,15)+scalar(0x1f600),16);check((repeated(0x65e5,15)+scalar(0x1f600)).size()==49,"49 bytes are valid, not a 48-byte cap");
 check(!validate(repeated(0x1f600,17)),"17 supplementary scalars rejected");
 roundtrip(std::string("e")+scalar(0x301)+"e"+scalar(0x301),4); // Two graphemes, four scalars.
 check(!validate(repeated(0xe9,2)),"two precomposed scalars remain below minimum");
 auto composed=repeated(0xe9,4),decomposed=std::string("e")+scalar(0x301)+"e"+scalar(0x301)+"e"+scalar(0x301)+"e"+scalar(0x301);
 roundtrip(composed,4);roundtrip(decomposed,8);check(*decode(*encode(composed))!=*decode(*encode(decomposed)),"no implicit normalization or collision policy");
 for(uint32_t c=0;c<32;++c)check(validate("AB"+scalar(c)+"C").error==Error::forbidden,"C0 and embedded NUL rejected");
 for(uint32_t c=127;c<=159;++c)check(validate("AB"+scalar(c)+"C").error==Error::forbidden,"DEL and C1 rejected");
 for(auto c:{0x5cu,0x200bu,0x200cu,0x200du,0x2028u,0x2029u,0x202au,0x202bu,0x202cu,0x202du,0x202eu,0x2066u,0x2067u,0x2068u,0x2069u,0xfeffu})
  check(validate("AB"+scalar(c)+"C").error==Error::forbidden,"existing invisible and bidi exclusions retained");
 for(auto c:{0x20u,0x3000u}){check(!validate(scalar(c)+"ABC"),"leading whitespace");check(!validate("ABC"+scalar(c)),"trailing whitespace");roundtrip("AB"+scalar(c)+"C",4);}
 // Existing policy does not reject these formatting marks; do not silently
 // invent a different policy on the new wire. Review before activation.
 for(auto c:{0x61cu,0x200eu,0x200fu})roundtrip("AB"+scalar(c)+"C",4);
 for(const auto&name:{"OpenMGO2","oPeNmGo2",":#ab","GM_ab","GM-ab","GM.ab","GM,ab"})check(validate(name).error==Error::reserved,"existing reserved names");
 roundtrip("gm_ab",5);roundtrip("*ABCD",5); // Marker must be a separate future metadata field.
 for(const auto&bad:{std::string("\x80",1),std::string("\xc0\xaf",2),std::string("\xc1\xbf",2),std::string("\xe0\x80\xaf",3),
     std::string("\xf0\x80\x80\xaf",4),std::string("\xed\xa0\x80",3),std::string("\xed\xbf\xbf",3),std::string("\xf4\x90\x80\x80",4),
     std::string("\xf5\x80\x80\x80",4),std::string("\xff",1),std::string("\xc2",1),std::string("\xe6\x97",2),std::string("\xf0\x9f\x98",3),std::string("\xe6" "A\x80",3)}){
  auto name="ABCD"+bad;check(validate(name).error==Error::malformed_utf8&&!encode(name),"malformed, overlong, surrogate, range or truncated UTF-8 rejected");
 }
 auto four=scalar(0x1f600);for(size_t n=1;n<four.size();++n)check(!validate("ABCD"+four.substr(0,n)),"all supplementary truncation positions");
 auto frame=*encode("ABCD");check(frame==std::vector<uint8_t>{'G','W','U','N',1,0,0,4,'A','B','C','D'},"draft schema exact bytes and BE16 prefix");
 for(size_t n=0;n<frame.size();++n)check(!decode(std::span(frame).first(n)),"every truncated frame rejected");
 auto mutated=frame;mutated.push_back(0);check(!decode(mutated),"trailing NUL rejected");mutated=frame;mutated.insert(mutated.end(),frame.begin(),frame.end());check(!decode(mutated),"concatenated frames rejected");
 for(unsigned n=0;n<4;++n){mutated=frame;mutated[n]^=1;check(!decode(mutated),"wrong magic rejected");}
 for(auto versionByte:{0u,2u,255u}){mutated=frame;mutated[4]=uint8_t(versionByte);check(!decode(mutated),"unknown version rejected");}
 mutated=frame;mutated[5]=1;check(!decode(mutated),"reserved bits rejected");
 for(unsigned n:{0u,3u,5u,65u,256u,65535u}){mutated=frame;mutated[6]=uint8_t(n>>8);mutated[7]=uint8_t(n);check(!decode(mutated),"length mismatch or oversized prefix rejected");}
 mutated=frame;mutated[8]=0;check(!decode(mutated),"embedded NUL on decode rejected");mutated=frame;mutated[11]=0xc2;check(!decode(mutated),"exact frame cannot hide truncated UTF-8");
 auto maximum=*encode(repeated(0x1f600,16));check(maximum.size()==72&&maximum[6]==0&&maximum[7]==64,"maximum draft frame 72 bytes");
 maximum.push_back(0);check(!decode(maximum),"over maximum frame fails before copying");
 check(!decode(std::vector<uint8_t>(16,'A')),"legacy fixed 16-byte field is not extension");check(!encode(std::string(1000000,'A')),"large input bounded before parsing");
 std::cout<<"Unicode name draft: 4..16 scalars, 64 UTF-8 bytes, legacy policy, exact versioned length frame passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
