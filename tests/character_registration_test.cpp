#include "character_client.h"
#include <stdexcept>
#include <iostream>
#include <tuple>
using namespace mgo2mt;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(){try{
 std::array<uint8_t,28> native{};native[2]=11;native[3]=22;native[15]=46;native[17]=57;
 auto mapped=character_create_request(L"試作兵士",native,0);
 const std::array<uint8_t,27> male={0,0,11,22,0,0,0,7,15,0,0,0,0,28,68,46,86,57,102,102,0,0,0,0,0,0,0};
 check(mapped.wire_appearance==male,"original default IDs, unequipped sentinels and male voice/pitch");
 native[1]=6;native[22]=3;check(character_create_request(L"試作兵士",native,0).wire_appearance[22]==0,"synthetic bare-hand skin palette never leaks onto wire");
 native[0]=1;native[7]=7;native[13]=29;native[20]=2;native[2]=12;native[5]=5;
 mapped=character_create_request(L"試作兵士",native,-7);check(mapped.wire_appearance[7]==23&&mapped.wire_appearance[8]==8&&mapped.wire_appearance[13]==29&&mapped.wire_appearance[20]==2&&mapped.wire_appearance[2]==12&&mapped.wire_appearance[5]==5,"female +16 and catalog IDs/colors preserved");
 check(character_create_request(L"試作兵士",native,7).wire_appearance[8]==22,"pitch +7 maps to 22");
 native[7]=8;bool invalidVoice=false;try{character_create_request(L"試作兵士",native,0);}catch(...){invalidVoice=true;}check(invalidVoice,"out-of-range voice rejected");
 CharacterCreateRequest request;request.name=L"試作兵士";request.wire_appearance={1,2,3,4,5,6,7,8,15,0,0,0,0,9,10,11,12,13,14,15,16,17,18,19,20,21,22};
 auto bytes=character_create_payload(request);
 // Fixed expected vector: reviewed F03D24 PPC stores, not a reserialization oracle.
 const std::vector<uint8_t> expected={0xe8,0xa9,0xa6,0xe4,0xbd,0x9c,0xe5,0x85,0xb5,0xe5,0xa3,0xab,0,0,0,0,1,2,3,4,5,6,7,8,15,0,0,0,0,9,10,11,12,13,14,15,16,17,18,19,20,21,22};
 check(bytes==expected,"43-byte original create layout");
 auto invalid=request;invalid.name=L"日本語名前六";bool rejected=false;try{character_create_payload(invalid);}catch(...){rejected=true;}check(rejected,"oversized name cannot truncate");
 invalid=request;invalid.wire_appearance[9]=1;rejected=false;try{character_create_payload(invalid);}catch(...){rejected=true;}check(rejected,"reserved bytes rejected");
 std::atomic_bool cancel=false;unsigned createCalls=0,listCalls=0;int mode=0;
 CharacterExchange fake=[&](uint16_t cmd,std::span<const uint8_t>body){
  if(cmd==0x3048){++listCalls;check(body.empty(),"list has no payload");LobbyPacket p{0x3049,1,std::vector<uint8_t>(471)};p.payload[4]=2;if(mode==6)cancel=true;if(mode==7)p.command=0x3102;if(mode==8)p.payload[4]=255;return p;}
  check(cmd==0x3101,"only create opcode");++createCalls;check(std::vector<uint8_t>(body.begin(),body.end())==expected,"create payload sent once");
  if(mode==1)throw std::runtime_error("simulated lost reply");
  if(mode==2)return LobbyPacket{0x3102,2,{0,0,0,5}};
  if(mode==3)return LobbyPacket{0x3102,2,{0,0,0,0}};
  if(mode==4)return LobbyPacket{0x3102,2,{0,0,0,0,0,0,0,0}};
  if(mode==5)return LobbyPacket{0x3049,2,{0,0,0,0,0,0,0,99}};
  return LobbyPacket{0x3102,2,{0,0,0,0,0,0,0,99}};
 };
 auto r=exchange_character_create(request,fake,cancel);check(r.status==CharacterCreateStatus::success&&r.created_id==99&&r.request_may_have_been_sent&&createCalls==1&&listCalls==1,"confirmed created ID");
 for(mode=1;mode<=5;++mode){createCalls=0;r=exchange_character_create(request,fake,cancel);check(createCalls==1&&r.request_may_have_been_sent,"never retries after a send");check(r.status==(mode==2?CharacterCreateStatus::rejected:CharacterCreateStatus::outcome_unknown),"rejection versus unknown result");}
 for(mode=6;mode<=8;++mode){cancel=false;createCalls=0;r=exchange_character_create(request,fake,cancel);check(!createCalls&&!r.request_may_have_been_sent,"cancel/bad capacity preflight cannot mutate");}
 cancel=true;listCalls=0;r=exchange_character_create(request,fake,cancel);check(r.status==CharacterCreateStatus::cancelled&&!listCalls,"cancel before connection");
 cancel=false;
 // Four included slots apply to legacy 3-slot accounts. Additional slots must
 // come from the server. Exercise the boundary through the actual preflight.
 for(auto [serverSlots,characters,allowed]:{std::tuple{3,3,true},{3,4,false},{4,4,false},{5,4,true},{5,5,false},{8,8,false}}){
  unsigned sent=0;CharacterExchange capacity=[&](uint16_t cmd,std::span<const uint8_t>){
   if(cmd==0x3101){++sent;return LobbyPacket{0x3102,2,{0,0,0,0,0,0,0,99}};}
   check(cmd==0x3048,"capacity preflight opcode");LobbyPacket p{0x3049,1,std::vector<uint8_t>(471)};p.payload[4]=uint8_t(serverSlots);p.payload[5]=uint8_t(characters);
   for(int i=0;i<characters;++i){size_t at=i?72+52*(i-1):7;if(i)p.payload[at+3]=uint8_t(i);at+=i?4:17;p.payload[at+3]=uint8_t(i+1);p.payload[at+4]='N';p.payload[at+5]=uint8_t('0'+i);}return p;
  };
  r=exchange_character_create(request,capacity,cancel);check(sent==unsigned(allowed)&&r.status==(allowed?CharacterCreateStatus::success:CharacterCreateStatus::full),"four free, fifth paid, eight wire maximum");
 }
 CharacterExchange duplicate=[&](uint16_t cmd,std::span<const uint8_t>){check(cmd==0x3048,"existing same name never sends create");LobbyPacket p{0x3049,1,std::vector<uint8_t>(471)};p.payload[4]=2;p.payload[5]=1;p.payload[27]=1;for(unsigned i=0;i<16;++i)p.payload[28+i]=expected[i];return p;};
 r=exchange_character_create(request,duplicate,cancel);check(r.status==CharacterCreateStatus::rejected&&!r.request_may_have_been_sent,"preflight refuses already-listed exact name");
 request.name=std::wstring(16,L'名');request.unicode_name=true;unsigned nativeCalls=0;mode=0;
 CharacterExchange nativeExchange=[&](uint16_t command,std::span<const uint8_t> body){
  if(command==0x3048){LobbyPacket p{0x3049,1,std::vector<uint8_t>(471)};p.payload[4]=4;return p;}
  check(command==0x30e2,"native never falls back to original create");++nativeCalls;
  check(body.size()==95&&body[0]=='G'&&body[3]=='M'&&body[5]==3&&body[16]==0&&body[17]==48&&body[19]==27,"native Japanese 16 is length-prefixed 48 bytes, not 32");
  for(size_t at=20;at<68;at+=3)check(body[at]==0xe5&&body[at+1]==0x90&&body[at+2]==0x8d,"full name preserved");
  check(std::equal(request.wire_appearance.begin(),request.wire_appearance.end(),body.begin()+68),"native appearance offset follows variable name");
  if(mode==9)throw std::runtime_error("lost response after create");
  std::vector<uint8_t> p{'G','W','N','M',1,0,3,1};p.insert(p.end(),body.begin()+8,body.begin()+16);p.insert(p.end(),{0,0,16,64,0,0,0,7,0,0,0,0,0,0,0,99});
  if(mode>=1&&mode<=4){p[5]=uint8_t(mode);p[7]=p[18]=p[19]=p[23]=p[31]=0;if(mode==4)p[24]=p[25]=p[26]=0xff,p[27]=0xfc;}
  if(mode==5){p[5]=1;p[7]=p[18]=p[19]=p[23]=0;p.resize(24);}
  if(mode==6)p[8]^=1;if(mode==8)p[31]=0;
  return LobbyPacket{uint16_t(mode==7?0x3102:0x30e3),2,std::move(p)};
 };
 r=exchange_character_create(request,nativeExchange,cancel,[]{return false;});check(!nativeCalls&&!r.request_may_have_been_sent&&r.status==CharacterCreateStatus::rejected,"same-connection capability required before native mutation");
 for(mode=0;mode<=9;++mode){nativeCalls=0;r=exchange_character_create(request,nativeExchange,cancel,[]{return true;});
  const auto expectedStatus=mode==0?CharacterCreateStatus::success:(mode==1||mode==2||mode==4)?CharacterCreateStatus::rejected:CharacterCreateStatus::outcome_unknown;
  check(nativeCalls==1&&r.request_may_have_been_sent&&r.status==expectedStatus,"native success, guaranteed rejection and unknown outcome stay distinct without retry");
  if(!mode)check(r.created_id==99,"native stable ID confirms result");
 }
 request.name+=L'名';nativeCalls=0;r=exchange_character_create(request,nativeExchange,cancel,[]{return true;});check(!nativeCalls&&!r.request_may_have_been_sent,"17 Unicode scalars never sent");
 std::cout<<"Creation payload, preflight, response and no-retry cases passed (fake transport only)\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
