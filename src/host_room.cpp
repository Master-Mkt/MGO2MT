#include <windows.h>
#include "host_room.h"
#include "character_client.h"
#include <algorithm>
#include <stdexcept>
namespace mgo2mt::host {
namespace {
void put(std::span<uint8_t>b,size_t at,uint32_t v,unsigned n=4){if(at+n>b.size())throw std::logic_error("host room extent");while(n){b[at+--n]=uint8_t(v);v>>=8;}}
uint32_t number(std::span<const uint8_t>b,size_t at){if(at+4>b.size())throw std::runtime_error("host room response extent");uint32_t v=0;for(unsigned i=0;i<4;++i)v=v*256+b[at+i];return v;}
void text(std::span<uint8_t>b,std::wstring_view value,size_t minimum,bool multiline=false){
 for(auto c:value)if((c<32&&!(multiline&&(c==10||c==13||c==9)))||c==127)throw std::invalid_argument("host room text control");
 int n=value.empty()?0:WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);
 if((!value.empty()&&!n)||size_t(n)<minimum||size_t(n)>b.size())throw std::invalid_argument("host room text size/encoding");
 if(n)WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),reinterpret_cast<char*>(b.data()),n,nullptr,nullptr);
}
void validate(const Settings&s){
 if(s.subtype<1||s.subtype>9||s.capacity<1||s.capacity>17||s.briefing_minutes>60||s.rotations.empty()||s.rotations.size()>15||s.unique_red>=128||s.unique_blue>=128)throw std::invalid_argument("host room settings");
 if(s.idle_kick_minutes>99||s.team_kill_kick>99||s.level_limit_tolerance>63||s.level_limit_base>64)throw std::invalid_argument("host room common setting range");
 for(auto&r:s.rotations)if(!r.map)throw std::invalid_argument("host room empty rotation"); // DM rule 0 is valid.
}
constexpr std::array<uint32_t,17> rule_defaults={7,2,4,2,7,2,5,2,51,5,30,5,2,7,2,10,2};
uint8_t common_a(const Settings&s){return uint8_t(0x04|(s.idle_kick_minutes?0x01:0)|(s.friendly_fire?0x08:0)|(s.ghosts?0x10:0)|(s.auto_aim?0x20:0)|(s.uniques?0x80:0));}
uint8_t common_b(const Settings&s){return uint8_t((s.teams_switch?0x01:0)|(s.auto_assign?0x02:0)|(s.silent?0x04:0)|(s.enemy_nametags?0x08:0)|(s.level_limit?0x10:0)|(s.voice_chat?0x40:0)|(s.team_kill_kick?0x80:0));}
struct Rejected{uint32_t error;};
void response(const LobbyPacket&p,uint16_t command,size_t size,uint32_t identity=0){
 if(p.command!=command||p.payload.size()<4)throw std::runtime_error("host room response command");
 auto error=number(p.payload,0);
 if(identity){if(p.payload.size()!=8||number(p.payload,4)!=identity)throw std::runtime_error("host room response identity");}
 else if(p.payload.size()!=(error?4:size))throw std::runtime_error("host room response extent");
 if(error)throw Rejected{error};
}
std::array<uint8_t,4> character_payload(uint32_t character){if(!character)throw std::invalid_argument("host room character");std::array<uint8_t,4>b{};put(b,0,character);return b;}
}
std::vector<uint8_t> settings_payload(const Settings&s){
 validate(s);std::vector<uint8_t>b(room_settings_size);auto span=std::span(b);
 text(span.subspan(0,16),s.name,3);text(span.subspan(16,128),s.comment,0,true);
 if(!s.password.empty()){b[144]=1;text(span.subspan(145,15),s.password,3);} // Candidate reads 15 password bytes; final byte remains NUL.
 b[161]=1;b[162]=s.subtype;
 for(size_t i=0;i<s.rotations.size();++i){b[163+i*3]=s.rotations[i].rule;b[164+i*3]=s.rotations[i].map;b[165+i*3]=s.rotations[i].flags;}
 std::copy(s.weapon_restrictions.begin(),s.weapon_restrictions.end(),b.begin()+213);
 b[229]=s.capacity;put(b,230,s.briefing_minutes);b[247]=s.level_limit_tolerance;put(b,248,s.level_limit_base);
 for(size_t i=0;i<rule_defaults.size();++i)put(b,252+i*4,rule_defaults[i]);
 b[320]=s.unique_red;b[321]=s.unique_blue;b[322]=common_a(s);b[323]=common_b(s);b[324]=0x20;
 put(b,325,s.idle_kick_minutes,2);put(b,327,s.team_kill_kick,2);b[330]=3;b[331]=3;b[332]=2;b[333]=20;b[334]=1;b[335]=5;b[336]=2;b[337]=5;b[338]=2;
 b[340]=s.non_stat?2:0;b[341]=s.non_stat?0x20:0;
 return b;
}
std::array<uint8_t,room_environment_size> room_environment(const Settings&s){
 validate(s);std::array<uint8_t,room_environment_size>b{};
 for(size_t i=0;i<s.rotations.size();++i){b[i*3]=s.rotations[i].rule;b[1+i*3]=s.rotations[i].map;b[2+i*3]=s.rotations[i].flags;}
 std::copy(s.weapon_restrictions.begin(),s.weapon_restrictions.end(),b.begin()+50);b[66]=s.capacity;
 put(b,68,s.briefing_minutes);b[95]=s.level_limit_tolerance;put(b,96,s.level_limit_base);
 for(size_t i=0;i<rule_defaults.size();++i)put(b,100+i*4,rule_defaults[i]);
 b[168]=s.unique_red;b[169]=s.unique_blue;b[177]=common_a(s);b[178]=common_b(s);b[179]=0x20;
 put(b,180,s.idle_kick_minutes,2);put(b,182,s.team_kill_kick,2);put(b,184,0x2e);b[189]=3;b[190]=3;b[191]=2;b[192]=20;b[193]=1;b[194]=5;b[195]=2;b[196]=5;b[197]=2;b[199]=s.non_stat?2:0;
 return b;
}
uint32_t round_duration_ms(const Settings&s,uint8_t rule){
 // These are the rule IDs already used by the native DM/TDM room/profile.
 // Read the exact advertised field so its native clock cannot drift from a
 // separately hardcoded five-minute value. Other modes remain unknown.
 size_t offset=0;if(rule==0)offset=0x88;else if(rule==1)offset=0x7c;else return 0;
 auto environment=room_environment(s);const auto minutes=number(environment,offset);
 if(!minutes||minutes>24*60)return 0;return minutes*60000;
}
std::vector<uint8_t> host_room_wire_payload(const NetworkKeys&keys,uint16_t command,std::span<const uint8_t>plain){
 size_t size=0;
 switch(command){case 0x4310:size=room_settings_size;break;case 0x4316:case 0x4392:case 0x43ca:size=1;break;case 0x4340:case 0x4342:size=4;break;case 0x4344:size=5;break;case 0x4394:size=room_environment_size-1;break;case 0x4380:size=0;break;default:throw std::invalid_argument("host room wire command");}
 if(plain.size()!=size)throw std::invalid_argument("host room wire extent");
 std::vector<uint8_t>b(plain.begin(),plain.end());if(command==0x4310){b.resize((b.size()+7)&~size_t(7));network_block(b,keys.packet,true);}return b;
}
CreateReply Lifecycle::create(const Settings&s,const RoomExchange&exchange,const std::atomic_bool&cancel){
 if(attempted_||may_exist_)return {RoomControlStatus::invalid_state,room_,0,may_exist_};
 if(cancel)return {RoomControlStatus::cancelled};
 std::vector<uint8_t> payload;
 try{payload=settings_payload(s);}catch(const std::invalid_argument&){return {RoomControlStatus::invalid_input};}
 struct Wipe{std::vector<uint8_t>&p;~Wipe(){SecureZeroMemory(p.data(),p.size());}}wipe{payload};
 attempted_=true;
 try{
  response(exchange(0x4310,payload),0x4311,4);settings_=s;
  if(cancel)return {RoomControlStatus::cancelled};
  constexpr uint8_t confirm=1;may_exist_=true;
  auto p=exchange(0x4316,{&confirm,1});response(p,0x4317,8);room_=number(p.payload,4);
  if(!room_)throw std::runtime_error("host room zero identity");
  return {RoomControlStatus::success,room_,0,true};
 }catch(const Rejected&r){may_exist_=false;return {RoomControlStatus::rejected,0,r.error,false};}
 catch(...){return {may_exist_?RoomControlStatus::outcome_unknown:RoomControlStatus::protocol_error,room_,0,may_exist_};}
}
RoomControlReply Lifecycle::control(uint16_t command,std::span<const uint8_t>payload,const RoomExchange&exchange,uint32_t identity){
 if(!may_exist_||(!room_&&command!=0x4380))return {RoomControlStatus::invalid_state};
 try{response(exchange(command,payload),uint16_t(command+1),identity?8:4,identity);return {RoomControlStatus::success};}
 catch(const Rejected&r){return {RoomControlStatus::rejected,r.error};}
 catch(...){return {RoomControlStatus::outcome_unknown};}
}
RoomControlReply Lifecycle::heartbeat(const RoomExchange&exchange){auto env=room_environment(settings_);std::vector<uint8_t>payload(env.begin(),env.end());payload.erase(payload.begin()+67);return control(0x4394,payload,exchange);}
RoomControlReply Lifecycle::player_connected(uint32_t id,const RoomExchange&e){if(!id)return {RoomControlStatus::invalid_input};return control(0x4340,character_payload(id),e,id);}
RoomControlReply Lifecycle::player_disconnected(uint32_t id,const RoomExchange&e){if(!id)return {RoomControlStatus::invalid_input};return control(0x4342,character_payload(id),e,id);}
RoomControlReply Lifecycle::player_team(uint32_t id,uint8_t team,const RoomExchange&e){if(!id)return {RoomControlStatus::invalid_input};auto idBytes=character_payload(id);std::vector<uint8_t>b(idBytes.begin(),idBytes.end());b.push_back(team);return control(0x4344,b,e,id);}
RoomControlReply Lifecycle::round_started(uint8_t marker,const RoomExchange&e){return control(0x43ca,{&marker,1},e);}
RoomControlReply Lifecycle::rotation(uint8_t index,const RoomExchange&e){if(index>=settings_.rotations.size())return {RoomControlStatus::invalid_input};return control(0x4392,{&index,1},e);}
RoomControlReply Lifecycle::close(const RoomExchange&e){auto r=control(0x4380,{},e);if(r.status==RoomControlStatus::success){may_exist_=false;room_=0;}return r;}
}
