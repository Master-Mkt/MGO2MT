#include "host_combat_original.h"
#include <algorithm>
#include <bit>

namespace mgo2mt::combat::original {
namespace {
void put16(std::vector<uint8_t>& b,uint16_t v){b.push_back(uint8_t(v));b.push_back(uint8_t(v>>8));}
uint16_t get16(std::span<const uint8_t> b,size_t at){return uint16_t(b[at])|uint16_t(uint16_t(b[at+1])<<8);}
void position(std::vector<uint8_t>& b,const Position& p){put16(b,uint16_t(p.x));put16(b,uint16_t(p.y));put16(b,uint16_t(p.z));}
Position position(std::span<const uint8_t> b,size_t at){return {std::bit_cast<int16_t>(get16(b,at)),std::bit_cast<int16_t>(get16(b,at+2)),std::bit_cast<int16_t>(get16(b,at+4))};}
void opcode(std::span<const uint8_t> b,uint8_t expected){if(b[0]!=expected)throw Invalid(Error::opcode);}
void check_vitals(const Vitals& v){if(v.life>250||v.stamina>250)throw Invalid(Error::bounds);}
}
uint16_t channel(uint8_t player,Lane lane){
 const auto n=uint8_t(lane);
 if(player>=player_limit||n>=channel_count)throw Invalid(Error::bounds);
 return uint16_t(channel_base+player*channel_count+n);
}
std::optional<Channel> decode_channel(uint16_t value){
 if(value<channel_base||value>=channel_base+player_limit*channel_count)return std::nullopt;
 const uint16_t n=value-channel_base;
 return Channel{uint8_t(n/channel_count),uint8_t(n%channel_count)};
}
std::vector<uint8_t> encode_damage(const Damage& d){
 if(d.amount>2047||d.attacker>31)throw Invalid(Error::bounds);
 std::vector<uint8_t> b;b.reserve(d.direction?16:12);b.push_back(0);
 put16(b,d.flags);put16(b,uint16_t((d.amount<<5)|d.attacker));b.push_back(d.weapon);position(b,d.position);
 if(d.direction){put16(b,uint16_t(d.direction->pitch));put16(b,uint16_t(d.direction->yaw));}
 return b;
}
Damage decode_damage(std::span<const uint8_t> b){
 if(b.size()!=12&&b.size()!=16)throw Invalid(Error::extent);
 opcode(b,0);Damage d;d.flags=get16(b,1);
 const auto packed=get16(b,3);d.amount=packed>>5;d.attacker=uint8_t(packed&31);d.weapon=b[5];d.position=position(b,6);
 if(b.size()==16)d.direction=Angles{std::bit_cast<int16_t>(get16(b,12)),std::bit_cast<int16_t>(get16(b,14))};
 return d;
}
uint8_t quantize_vital(uint32_t value){return uint8_t(std::min<uint32_t>(250,value/4+(value%4!=0)));}
std::vector<uint8_t> encode_vitals(const Vitals& v){check_vitals(v);return {3,v.life,v.stamina};}
Vitals decode_vitals(std::span<const uint8_t> b){
 if(b.size()!=3)throw Invalid(Error::extent);
 opcode(b,3);Vitals v{b[1],b[2]};check_vitals(v);return v;
}
std::vector<uint8_t> encode_pose(const Pose& p){
 std::vector<uint8_t> b;b.reserve(13);b.push_back(2);put16(b,p.action);position(b,p.position);
 put16(b,uint16_t(p.yaw));b.push_back(std::bit_cast<uint8_t>(p.look_lr));b.push_back(p.look_u);return b;
}
Pose decode_pose(std::span<const uint8_t> b){
 if(b.size()!=13)throw Invalid(Error::extent);
 opcode(b,2);return {get16(b,1),position(b,3),std::bit_cast<int16_t>(get16(b,9)),std::bit_cast<int8_t>(b[11]),b[12]};
}
}
