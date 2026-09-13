#include "host_combat_original.h"
#include <iostream>
#include <limits>
#include <string>

using namespace mgo2win::combat::original;
namespace {
void check(bool yes,const char* what){if(!yes)throw std::runtime_error(what);}
template<class F>void rejects(F&& f,Error expected,const char* what){
 try{f();}catch(const Invalid& e){check(e.code==expected,what);return;}
 throw std::runtime_error(what);
}
}
int main(){try{
 // Hand-derived from PPC 8136D8..813750, not from an observed network capture.
 Damage d{0x1234,275,7,25,{1234,-250,0x1234},Angles{-16384,8192}};
 const std::vector<uint8_t> golden={0,0x34,0x12,0x67,0x22,25,0xD2,4,6,0xFF,0x34,0x12,0,0xC0,0,0x20};
 check(encode_damage(d)==golden&&decode_damage(golden)==d,"damage field order, LE and optional angles");
 d.direction.reset();auto body=golden;body.resize(12);
 check(encode_damage(d)==body&&decode_damage(body)==d,"damage without direction is exactly twelve bytes");
 for(size_t n=0;n<16;++n){if(n==12)continue;rejects([&]{decode_damage(std::span(golden).first(n));},Error::extent,"partial damage field rejected");}
 auto invalid=golden;invalid.push_back(0);rejects([&]{decode_damage(invalid);},Error::extent,"damage trailing bytes rejected");
 invalid=body;invalid[0]=1;rejects([&]{decode_damage(invalid);},Error::opcode,"damage opcode checked");
 Damage edge{0xFFFF,2047,31,255,{-32768,32767,-1},Angles{32767,-32768}};
 check(decode_damage(encode_damage(edge))==edge,"raw flags and complete packed range preserved; no authority implied");
 edge.amount=2048;rejects([&]{encode_damage(edge);},Error::bounds,"damage cannot wrap eleven bits");
 edge.amount=0;edge.attacker=32;rejects([&]{encode_damage(edge);},Error::bounds,"attacker cannot overwrite damage bits");
 Vitals v{250,1};check(encode_vitals(v)==std::vector<uint8_t>({3,250,1}),"vitals three-byte golden body");
 check(decode_vitals(std::array<uint8_t,3>{3,250,1})==v&&v.life_value()==1000&&v.stamina_value()==4,"vitals quantum is four");
 check(quantize_vital(0)==0&&quantize_vital(1)==1&&quantize_vital(4)==1&&quantize_vital(5)==2&&quantize_vital(997)==250&&quantize_vital(1000)==250,"retail ceil quantization and alive floor");
 check(quantize_vital(std::numeric_limits<uint32_t>::max())==250,"vital saturation cannot overflow");
 for(size_t n=0;n<3;++n)rejects([&]{decode_vitals(std::span(golden).first(n));},Error::extent,"truncated vitals rejected");
 rejects([]{decode_vitals(std::array<uint8_t,4>{3,0,0,0});},Error::extent,"vital trailing bytes rejected");
 rejects([]{decode_vitals(std::array<uint8_t,3>{0,0,0});},Error::opcode,"vital opcode checked");
 rejects([]{decode_vitals(std::array<uint8_t,3>{3,251,0});},Error::bounds,"impossible original-send HP rejected");
 rejects([]{encode_vitals({0,255});},Error::bounds,"impossible original-send stamina rejected");
 Pose p{0xA567,{-32768,32767,-10},-12345,-100,200};
 const std::vector<uint8_t> poseGolden={2,0x67,0xA5,0,0x80,0xFF,0x7F,0xF6,0xFF,0xC7,0xCF,0x9C,0xC8};
 check(encode_pose(p)==poseGolden&&decode_pose(poseGolden)==p,"pose fixed body field order and signed aim byte");
 for(size_t n=0;n<13;++n)rejects([&]{decode_pose(std::span(poseGolden).first(n));},Error::extent,"truncated pose rejected");
 invalid=poseGolden;invalid.push_back(0);rejects([&]{decode_pose(invalid);},Error::extent,"action-specific pose tail cannot masquerade as ordinary body");
 invalid=poseGolden;invalid[0]=1;rejects([&]{decode_pose(invalid);},Error::opcode,"pose opcode checked");
 check(channel(0,Lane::damage)==64&&channel(23,Lane::vitals)==298&&channel(3,Lane::pose)==99,"original per-player channel formula");
 for(uint16_t n=64;n<304;++n){auto c=decode_channel(n);check(c&&c->player==(n-64)/10&&c->lane==(n-64)%10,"every channel decodes within actor lane range");}
 check(!decode_channel(0)&&!decode_channel(63)&&!decode_channel(304)&&!decode_channel(65535),"unrelated channels rejected");
 rejects([]{channel(24,Lane::damage);},Error::bounds,"player index cannot exceed original actor table");
 rejects([]{channel(0,static_cast<Lane>(10));},Error::bounds,"lane cannot cross into another player");
 check(!reliable,"original channel flags do not request reliable delivery");
 std::cout<<"host_combat_original_test passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
