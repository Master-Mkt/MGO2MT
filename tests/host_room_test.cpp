#include "host_room.h"
#include "character_client.h"
#include <algorithm>
#include <iostream>
using namespace mgo2win;
using namespace mgo2win::host;
namespace {
void check(bool value,const char*message){if(!value)throw std::runtime_error(message);}
template<class F>void invalid(F f){bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid settings accepted");}
LobbyPacket reply(uint16_t request,uint32_t result=0,uint32_t id=0){LobbyPacket p;p.command=request+1;p.payload={uint8_t(result>>24),uint8_t(result>>16),uint8_t(result>>8),uint8_t(result)};if(id){p.payload.insert(p.payload.end(),{uint8_t(id>>24),uint8_t(id>>16),uint8_t(id>>8),uint8_t(id)});}return p;}
void common_settings_checks(){
 Settings defaults;
 auto original=settings_payload(defaults);auto original_env=room_environment(defaults);
 auto number=[&](size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v=(v<<8)|original_env[at+i];return v;};
 check(round_duration_ms(defaults,0)==number(0x88)*60000&&round_duration_ms(defaults,1)==number(0x7c)*60000&&round_duration_ms(defaults,1)==300000,"native minute clock reads advertised DM/TDM value");
 check(!round_duration_ms(defaults,255)&&!round_duration_ms(defaults,2),"unreviewed rule identity has no fabricated clock");
 check(original[322]==0x25&&original[323]==0xcb&&original[324]==0x20,"retained default common bytes");
 check(original_env[177]==0x25&&original_env[178]==0xcb&&original_env[179]==0x20,"retained default environment common bytes");
 // Each control changes its one observed original bit in both independently
 // laid-out messages, preserving all neighboring settings and reserved bytes.
 struct Toggle{bool Settings::*field;size_t setting_at,environment_at;uint8_t mask;};
 const Toggle toggles[]={
  {&Settings::friendly_fire,322,177,0x08},{&Settings::ghosts,322,177,0x10},
  {&Settings::auto_aim,322,177,0x20},{&Settings::uniques,322,177,0x80},
  {&Settings::teams_switch,323,178,0x01},{&Settings::auto_assign,323,178,0x02},
  {&Settings::silent,323,178,0x04},{&Settings::enemy_nametags,323,178,0x08},
  {&Settings::level_limit,323,178,0x10},{&Settings::voice_chat,323,178,0x40}};
 for(const auto&t:toggles){
  auto changed=defaults;changed.*(t.field)=!(changed.*(t.field));
  auto wanted=original;wanted[t.setting_at]^=t.mask;
  auto wanted_env=original_env;wanted_env[t.environment_at]^=t.mask;
  check(settings_payload(changed)==wanted,"common setting toggle damaged another setting");
  check(room_environment(changed)==wanted_env,"environment toggle disagrees with setting");
 }
 auto quiet=defaults;quiet.idle_kick_minutes=0;quiet.team_kill_kick=0;
 auto no_kicks=settings_payload(quiet);auto no_kicks_env=room_environment(quiet);
 check(no_kicks[322]==0x24&&no_kicks[323]==0x4b&&no_kicks[325]==0&&no_kicks[326]==0&&no_kicks[327]==0&&no_kicks[328]==0,"disabled kick flags and counts");
 check(no_kicks_env[177]==0x24&&no_kicks_env[178]==0x4b&&no_kicks_env[180]==0&&no_kicks_env[181]==0&&no_kicks_env[182]==0&&no_kicks_env[183]==0,"disabled environment kick flags and counts");
 auto custom=defaults;custom.capacity=9;custom.briefing_minutes=11;custom.level_limit=true;
 custom.level_limit_base=42;custom.level_limit_tolerance=7;custom.idle_kick_minutes=99;custom.team_kill_kick=99;
 auto configured=settings_payload(custom);auto configured_env=room_environment(custom);
 check(configured[161]==1&&configured[229]==9&&configured[230]==0&&configured[231]==0&&configured[232]==0&&configured[233]==11,"dedicated capacity and briefing settings");
 check(configured[246]==0&&configured[247]==7&&configured[248]==0&&configured[249]==0&&configured[250]==0&&configured[251]==42,"LEVEL setting layout");
 check(configured_env[94]==0&&configured_env[95]==7&&configured_env[96]==0&&configured_env[97]==0&&configured_env[98]==0&&configured_env[99]==42,"LEVEL environment layout");
 check(configured[325]==0&&configured[326]==99&&configured[327]==0&&configured[328]==99,"kick big-endian shorts");
 check(configured_env[180]==0&&configured_env[181]==99&&configured_env[182]==0&&configured_env[183]==99,"environment kick big-endian shorts");
 check(configured[323]==0xdb&&configured_env[178]==0xdb,"LEVEL enabled bit");
 auto nonstat=defaults;nonstat.non_stat=true;
 auto ns=settings_payload(nonstat);auto nsenv=room_environment(nonstat);
 check(ns[340]==2&&ns[341]==0x20&&nsenv[199]==2,"nonstat create and environment flags");
 // Retail kick editors clamp 0..99. LEVEL bounds here are the native exposed
 // range within the original 64-entry threshold table, not a restored table.
 auto boundary=defaults;boundary.level_limit_base=64;boundary.level_limit_tolerance=63;
 settings_payload(boundary);room_environment(boundary);
 const auto reject_both=[](const Settings&s){invalid([&]{settings_payload(s);});invalid([&]{room_environment(s);});};
 auto bad=defaults;bad.idle_kick_minutes=100;reject_both(bad);
 bad=defaults;bad.team_kill_kick=100;reject_both(bad);
 bad=defaults;bad.level_limit_base=65;reject_both(bad);
 bad=defaults;bad.level_limit_tolerance=64;reject_both(bad);
}
}
int main(){try{
 common_settings_checks();
 Settings settings;settings.name=L"試験ルーム";settings.password=L"abc123";settings.rotations={{20,1,0x81},{7,2,0x20}};settings.friendly_fire=true;settings.uniques=true;settings.non_stat=true;settings.weapon_restrictions[3]=0x42;
 auto b=settings_payload(settings);
 // Offsets are independently taken from the retail F128D0 writer and the
 // candidate Hosts.checkSettings reader, including its skipped 16th rotation.
 check(b.size()==345&&b[144]==1&&b[161]==1&&b[162]==2,"settings dedicated header");
 check(b[163]==1&&b[164]==20&&b[165]==0x81&&b[166]==2&&b[167]==7&&b[168]==0x20,"rule/map/flags ordering");
 check(b[229]==17&&b[233]==2&&b[216]==0x42&&b[322]==0xad&&b[340]==2,"settings common offsets");
 check(std::all_of(b.begin()+208,b.begin()+213,[](uint8_t v){return !v;}),"unused rotation/skins");
 auto env=room_environment(settings);check(env[66]==17&&env[67]==0&&env[71]==2&&env[177]==0xad&&env[199]==2,"environment offsets");
 check(env[0]==1&&env[1]==20&&env[2]==0x81&&env[106]==0&&env[107]==2,"environment rule defaults");
 auto bad=settings;bad.name=L"a";invalid([&]{settings_payload(bad);});bad=settings;bad.name=L"日本語の長過ぎる部屋名";invalid([&]{settings_payload(bad);});bad=settings;bad.password.assign(16,L'a');invalid([&]{settings_payload(bad);});bad=settings;bad.subtype=0;invalid([&]{settings_payload(bad);});bad=settings;bad.capacity=18;invalid([&]{settings_payload(bad);});bad=settings;bad.rotations.resize(16,settings.rotations[0]);invalid([&]{settings_payload(bad);});
 auto dm=settings;dm.rotations={{2,0,uint8_t(RotationMode::drebin_points)}};check(settings_payload(dm)[165]==2,"DM rule zero and DP mode supported");
 std::atomic_bool cancel=false;Lifecycle lifecycle;unsigned calls=0;std::vector<uint16_t> commands;
 auto exchange=[&](uint16_t command,std::span<const uint8_t>payload){++calls;commands.push_back(command);switch(command){case 0x4310:check(payload.size()==b.size()&&std::equal(payload.begin(),payload.end(),b.begin()),"create settings");return reply(command);case 0x4316:check(payload.size()==1&&payload[0]==1,"create confirmation is not subtype");return reply(command,0,1234);case 0x4394:check(payload.size()==203&&payload[66]==17&&payload[70]==2,"heartbeat omits numPlayers byte");return reply(command);case 0x4340:case 0x4342:case 0x4344:check(payload.size()==(command==0x4344?5:4)&&payload[3]==9,"peer ID request");return reply(command,0,9);case 0x4392:case 0x43ca:check(payload.size()==1,"single-byte match control");return reply(command);case 0x4380:check(payload.empty(),"close payload");return reply(command);default:throw std::runtime_error("unexpected command");}};
 auto result=lifecycle.create(settings,exchange,cancel);check(result.status==RoomControlStatus::success&&result.room==1234&&lifecycle.room_may_exist(),"create lifecycle");
 check(lifecycle.create(settings,exchange,cancel).status==RoomControlStatus::invalid_state&&calls==2,"duplicate create blocked");
 check(lifecycle.heartbeat(exchange).status==RoomControlStatus::success,"lease response");check(lifecycle.player_connected(9,exchange).status==RoomControlStatus::success,"reserved peer registered");check(lifecycle.player_team(9,1,exchange).status==RoomControlStatus::success,"peer team");check(lifecycle.player_disconnected(9,exchange).status==RoomControlStatus::success,"peer removal");check(lifecycle.rotation(1,exchange).status==RoomControlStatus::success&&lifecycle.rotation(2,exchange).status==RoomControlStatus::invalid_input,"rotation bound");check(lifecycle.round_started(3,exchange).status==RoomControlStatus::success,"round control");check(lifecycle.close(exchange).status==RoomControlStatus::success&&!lifecycle.room_may_exist()&&!lifecycle.room(),"close lifecycle");
 Lifecycle cancelled;cancel=true;check(cancelled.create(settings,exchange,cancel).status==RoomControlStatus::cancelled,"cancel before network");cancel=false;
 Lifecycle rejected;auto rejection=rejected.create(settings,[](uint16_t c,std::span<const uint8_t>){return reply(c,0xc0ffee02);},cancel);check(rejection.status==RoomControlStatus::rejected&&!rejected.room_may_exist(),"settings rejected does not create");
 Lifecycle unknown;auto uncertain=unknown.create(settings,[](uint16_t c,std::span<const uint8_t>){if(c==0x4316)throw std::runtime_error("connection lost after send");return reply(c);},cancel);check(uncertain.status==RoomControlStatus::outcome_unknown&&unknown.room_may_exist(),"ambiguous create retained");check(unknown.create(settings,exchange,cancel).status==RoomControlStatus::invalid_state,"no create retry after ambiguous send");check(unknown.close(exchange).status==RoomControlStatus::success,"unknown create can close on same channel");
 Lifecycle wrongIdentity;check(wrongIdentity.create(settings,exchange,cancel).status==RoomControlStatus::success,"identity fixture create");check(wrongIdentity.player_connected(9,[](uint16_t c,std::span<const uint8_t>){return reply(c,0,10);}).status==RoomControlStatus::outcome_unknown,"peer response ID must match");
 NetworkKeys keys;auto wire=host_room_wire_payload(keys,0x4310,b);check(wire.size()==352,"settings cipher padding");network_block(wire,keys.packet,false);check(std::equal(b.begin(),b.end(),wire.begin())&&std::all_of(wire.begin()+345,wire.end(),[](uint8_t v){return !v;}),"settings encryption round trip");invalid([&]{host_room_wire_payload(keys,0x4310,std::span(b).first(344));});invalid([&]{host_room_wire_payload(keys,0x4700,{});});
 std::cout<<"dedicated room settings, creation, lease, peer registration, closure and ambiguity checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
