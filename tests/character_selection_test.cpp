#include "character_screen.h"
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <sstream>
using namespace mgo2win;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
void put(std::vector<uint8_t>&b,size_t at,unsigned v,unsigned bytes=4){while(bytes){b[at+--bytes]=uint8_t(v);v>>=8;}}
LobbyPacket list(std::initializer_list<unsigned> ids){
 LobbyPacket p{0x3049,1,std::vector<uint8_t>(471)};p.payload[4]=8;p.payload[5]=uint8_t(ids.size());size_t at=7;unsigned i=0;
 for(auto id:ids){if(i)put(p.payload,at,i);at+=i?4:17;put(p.payload,at,id);p.payload[at+4]='P';p.payload[at+5]='C';p.payload[at+6]=uint8_t('0'+i++);at+=48;}return p;
}
std::vector<uint8_t> record(unsigned index,unsigned type,unsigned id,const char*name,const char*ip="49.212.132.180"){
 std::vector<uint8_t>b(46);put(b,0,index);put(b,4,type);std::copy_n(name,strlen(name),b.begin()+8);std::copy_n(ip,strlen(ip),b.begin()+24);put(b,39,type==1?5732:5740,2);put(b,41,257,2);put(b,43,id,2);return b;
}
void settle(CharacterScreen&ui){auto deadline=GetTickCount64()+2000;while(ui.busy()&&GetTickCount64()<deadline){ui.draw();Sleep(1);}check(!ui.busy(),"UI completion deadline");ui.draw();}
int main(){try{
 auto roomPacket=[](uint32_t id){LobbyPacket p{0x4302,2,std::vector<uint8_t>(55)};put(p.payload,0,id);p.payload[4]='R';p.payload[20]=1;p.payload[22]=4;p.payload[23]=9;p.payload[25]=16;p.payload[29]=7;return p;};
 std::vector<LobbyPacket> roomPackets{{0x4301,1,{0,0,0,0}},roomPacket(77),{0x4303,3,{0,0,0,0}}};
 auto parseRooms=[&](const auto& ps){size_t n=0;return read_room_directory([&]{check(n<ps.size(),"room termination required");return ps[n++];});};
 auto rooms=parseRooms(roomPackets);check(rooms.size()==1&&rooms[0].id==77&&rooms[0].players==7&&rooms[0].capacity==16&&rooms[0].password&&rooms[0].rule==4&&rooms[0].map==9,"55-byte room record offsets");
 auto emptyRooms=roomPackets;emptyRooms.erase(emptyRooms.begin()+1);check(parseRooms(emptyRooms).empty(),"empty room list has exact successful termination");
 for(int mode=0;mode<7;++mode){auto ps=roomPackets;if(mode==0)ps[1].payload.pop_back();if(mode==1)ps[1].payload[4]=255;if(mode==2)ps.pop_back();if(mode==3)ps.insert(ps.begin()+2,ps[1]);if(mode==4)ps[2].payload[3]=7;if(mode==5)ps[1].command=0x4902;if(mode==6)put(ps[1].payload,0,0);bool bad=false;try{parseRooms(ps);}catch(...){bad=true;}check(bad,"malformed or partial room list refused");}
 NetworkKeys zeroKeys;AuthReply gameAuth;gameAuth.status=AuthStatus::success;gameAuth.user=17;
 auto accountPayload=session_payload(zeroKeys,gameAuth),gamePayload=game_session_payload(zeroKeys,gameAuth,77);network_block(accountPayload,zeroKeys.packet,false);network_block(gamePayload,zeroKeys.packet,false);
 check(accountPayload[3]==17&&gamePayload[3]==77&&gameAuth.user==17,"game session uses selected PC, preserves account identity");
 auto membership=[](const std::string&s){std::istringstream in(s);return read_lobby_membership(in);};
 auto rows=membership("MGO2WIN.LOBBIES 1\n49.212.132.180\n7\n3 5733 8\n4 5734 10\n5 5735 1\n6 5736 4\n7 5737 2\n8 5738 3\n9 5739 7\n");
 std::vector<GameLobbyEntry> groups;for(auto row:rows)groups.push_back({row.id,row.port,1,L"Renamed",0});
 apply_lobby_membership(groups,rows);const unsigned expected[]={2,5,1,3,0,4,2};
 for(size_t i=0;i<groups.size();++i)check(unsigned(lobby_group(groups[i].subtype))==expected[i],"reviewed server IDs mapped independently of name");
 check(lobby_group_count(groups)==6&&lobby_group_rows(groups,2)==std::vector<size_t>({0,6}),"both training kinds combined without reordering or loss");
 groups[0].port=5999;groups.push_back({250,5999,0,L"Otacon",0,1});apply_lobby_membership(groups,rows);
 check(groups[0].subtype==0&&groups.back().subtype==0&&lobby_group_count(groups)==7,"changed endpoint or familiar name cannot claim membership");
 for(const auto&s:{"MGO2WIN.LOBBIES 2 49.212.132.180 0","MGO2WIN.LOBBIES 1 203.0.113.1 0","MGO2WIN.LOBBIES 1 49.212.132.180 257","MGO2WIN.LOBBIES 1 49.212.132.180 1 1 5733","MGO2WIN.LOBBIES 1 49.212.132.180 1 1 65536 1","MGO2WIN.LOBBIES 1 49.212.132.180 2 1 5733 1 1 5734 2","MGO2WIN.LOBBIES 1 49.212.132.180 0 junk"}){
  bool bad=false;try{membership(s);}catch(...){bad=true;}check(bad,"malformed membership rejected before selection");}
 std::atomic_bool cancel=false;unsigned reads=0,sends=0;int mode=0;
 CharacterExchange fake=[&](uint16_t cmd,std::span<const uint8_t>b){
  if(cmd==0x3048){++reads;check(b.empty(),"preflight payload");if(mode==6)cancel=true;if(mode==7)return list({92});if(mode==8)return list({91,91});if(mode==9)return LobbyPacket{0x3049,1,{0,0,0,7}};return list({92,91});}
  check(cmd==0x3103&&b.size()==1&&b[0]==1,"stable ID resolved to fresh list index, not old index 0");++sends;
  if(mode==1)throw std::runtime_error("reply lost");if(mode==2)return LobbyPacket{0x3104,2,{0,0,0,9}};
  if(mode==3)return LobbyPacket{0x3102,2,{0,0,0,0}};if(mode==4)return LobbyPacket{0x3104,2,{0,0,0}};
  if(mode==5)return LobbyPacket{0x3104,2,{0,0,0,0,0,0,0,91}};return LobbyPacket{0x3104,2,{0,0,0,0}};
 };
 const auto verified=CharacterSelectionContract::channel_snapshot_v1;
 auto r=exchange_character_selection(91,fake,cancel,CharacterSelectionContract::unverified);check(r.status==CharacterSelectionStatus::unavailable&&!reads&&!sends,"unverified server cannot read or select");
 AuthReply auth;r=select_character(L"nonexistent.gnk",auth,91,cancel);check(r.status==CharacterSelectionStatus::unavailable,"production default blocks before keys/network");
 r=exchange_character_selection(91,fake,cancel,verified);check(r.status==CharacterSelectionStatus::success&&r.character.id==91&&reads==1&&sends==1,"same exchange selects requested ID");
 for(mode=1;mode<=5;++mode){sends=0;r=exchange_character_selection(91,fake,cancel,verified);check(sends==1&&r.request_may_have_been_sent,"one send without retry");check(r.status==(mode==2?CharacterSelectionStatus::rejected:CharacterSelectionStatus::outcome_unknown),"only exact result32 can confirm");}
 for(mode=6;mode<=9;++mode){cancel=false;sends=0;r=exchange_character_selection(91,fake,cancel,verified);check(!sends&&!r.request_may_have_been_sent,"cancel/missing/duplicate/error preflight cannot select");check(mode!=7||r.status==CharacterSelectionStatus::missing,"missing PC distinguished");}
 reads=0;cancel=true;r=exchange_character_selection(91,fake,cancel,verified);check(!reads&&r.status==CharacterSelectionStatus::cancelled,"early cancellation");cancel=false;
 r=exchange_character_selection(0,fake,cancel,verified);check(!reads&&r.status==CharacterSelectionStatus::protocol_error,"zero ID never maps to slot zero");
 mode=0;CharacterExchange eighth=[&](uint16_t cmd,std::span<const uint8_t>b){if(cmd==0x3048)return list({1,2,3,4,5,6,7,91});check(b.size()==1&&b[0]==7,"eighth slot wire boundary");return LobbyPacket{0x3104,2,{0,0,0,0}};};check(exchange_character_selection(91,eighth,cancel,verified).status==CharacterSelectionStatus::success,"eight-slot maximum");
 std::vector<LobbyPacket> packets{{0x2002,1,{0,0,0,0}},{0x2003,2,record(0,1,1,"Account")},{0x2003,3,record(1,2,2,"OpenMGO2")},{0x2004,4,{0,0,0,0}}};
 auto directory=[&](const auto& ps){size_t at=0;return read_lobby_directory([&]{check(at<ps.size(),"directory terminated");return ps[at++];});};
 auto d=directory(packets);check(d.account_port==5732&&d.games.size()==1&&d.games[0].id==2&&d.games[0].players==257&&d.games[0].port==5740&&d.games[0].name==L"OpenMGO2","46-byte BE candidate directory layout");
 for(int i=0;i<8;++i){auto p=packets;if(i==0)p[2].payload.pop_back();if(i==1)put(p[2].payload,43,1,2);if(i==2)p[2].payload[24]='8';if(i==3)p[3].payload[3]=1;if(i==4)p.pop_back();if(i==5)p[2].payload[8]=0xff;if(i==6)put(p[2].payload,0,0);if(i==7)p[2].command=0x3104;bool failed=false;try{directory(p);}catch(...){failed=true;}check(failed,"bad directory refused");}
 auto noGames=packets;noGames.erase(noGames.begin()+2);check(directory(noGames).games.empty(),"empty game directory is valid");
 unsigned bounded=0;bool failed=false;try{read_lobby_directory([&]{if(!bounded++)return packets[0];return LobbyPacket{0x2003,2,record(bounded-2,2,bounded,"Lobby")};});}catch(...){failed=true;}check(failed&&bounded<=17,"bounded unterminated stream");
 // UI: delayed selection locks repeats and hides preview; only confirmed ID
 // opens the directory. Pending cancellation remains uncertain across screens.
 auto transport=[](const std::atomic_bool&){CharacterReply r;r.status=CharacterStatus::success;r.list={4,{{91,L"Test"}}};return r;};
 for(int uiMode=0;uiMode<4;++uiMode){auto state=std::make_shared<CharacterSelectionState>();CharacterScreen ui(transport);settle(ui);std::atomic_bool release=false;unsigned calls=0;
  ui.selection([&](uint32_t id,const std::atomic_bool& c){++calls;while(!release&&!c)Sleep(1);CharacterSelectionReply r;r.status=uiMode==2?CharacterSelectionStatus::outcome_unknown:uiMode==3?CharacterSelectionStatus::rejected:CharacterSelectionStatus::success;r.request_may_have_been_sent=true;r.character={uiMode==1?92u:id,L"Test"};for(unsigned i=0;i<11;++i)r.lobbies.push_back({uint16_t(i+1),5740,17,L"Preview",0,2});return r;},state);
  ui.message(nullptr,WM_KEYDOWN,VK_RETURN,0);for(int i=0;i<8;++i)ui.message(nullptr,WM_KEYDOWN,VK_RETURN,1LL<<30);check(ui.busy()&&!ui.preview_visible(),"pending selection hides model");release=true;settle(ui);check(calls==1,"rapid confirmation selects once");
  if(!uiMode){check(ui.lobby_visible()&&ui.selected_character_id()==91&&!ui.preview_visible(),"confirmed selection opens directory");ui.message(nullptr,WM_KEYDOWN,VK_NEXT,0);ui.draw();ui.message(nullptr,WM_KEYDOWN,VK_RETURN,0);check(calls==1&&ui.focused_lobby_id()==7,"second page retains filtered ID; cannot connect yet");
   ui.message(nullptr,WM_KEYDOWN,VK_LEFT,0);ui.message(nullptr,WM_KEYDOWN,VK_DOWN,0);check(ui.current_lobby_group()==1&&ui.focused_lobby_id()==0&&ui.lobby_visible(),"empty group remains visible");ui.draw();
   ui.message(nullptr,WM_KEYDOWN,VK_UP,0);check(ui.focused_lobby_id()==1,"switching group resets row focus");
   ui.message(nullptr,WM_KEYDOWN,VK_UP,0);check(ui.current_lobby_group()==5,"category up wraps to registration");
   ui.message(nullptr,WM_KEYDOWN,VK_DOWN,0);check(ui.current_lobby_group()==0,"category down wraps to automatching");ui.message(nullptr,WM_KEYDOWN,VK_RIGHT,0);ui.message(nullptr,WM_KEYDOWN,VK_DOWN,0);check(ui.current_lobby_group()==0&&ui.focused_lobby_id()==2,"right column navigates lobbies without changing category");ui.message(nullptr,WM_KEYDOWN,VK_ESCAPE,0);check(!ui.lobby_visible()&&ui.preview_character_id()==91,"back restores PC preview");}
  else{check(!ui.lobby_visible()&&ui.preview_character_id()==91,"bad reply cannot transition");check(state->unresolved==(uiMode!=3),"reject versus uncertain");if(state->unresolved){ui.message(nullptr,WM_KEYDOWN,VK_RETURN,0);check(!ui.busy()&&calls==1,"uncertain selection cannot repeat");}}
 }
 // A configured salute preserves the model only for its presentation duration.
 // If transport outlasts the salute, return to the ordinary pending display.
 {
  uint64_t now=100;CharacterScreen ui(transport,[&]{return now;});settle(ui);std::atomic_bool release=false;unsigned calls=0;
  ui.selection_presentation(2000,false,false);
  ui.selection([&](uint32_t id,const std::atomic_bool& c){++calls;while(!release&&!c)Sleep(1);CharacterSelectionReply r;r.status=CharacterSelectionStatus::success;r.character.id=id;r.request_may_have_been_sent=true;return r;},std::make_shared<CharacterSelectionState>());
  ui.message(nullptr,WM_KEYDOWN,VK_RETURN,0);ui.draw();check(ui.busy()&&ui.preview_visible(),"configured salute keeps pending preview");
  now=2099;ui.draw();check(ui.preview_visible(),"salute remains visible before its final millisecond");
  now=2100;ui.draw();check(ui.busy()&&!ui.preview_visible()&&!ui.lobby_visible(),"finished salute restores pending display until server reply");
  ui.message(nullptr,WM_KEYDOWN,VK_RETURN,0);release=true;settle(ui);check(calls==1&&ui.lobby_visible(),"expired salute never resends selection and confirmed reply advances");
 }
 auto state=std::make_shared<CharacterSelectionState>();{
  CharacterScreen ui(transport);settle(ui);ui.selection([](uint32_t,const std::atomic_bool&c){while(!c)Sleep(1);CharacterSelectionReply r;r.status=CharacterSelectionStatus::outcome_unknown;r.request_may_have_been_sent=true;return r;},state);ui.message(nullptr,WM_KEYDOWN,VK_RETURN,0);ui.message(nullptr,WM_KEYDOWN,VK_ESCAPE,0);check(ui.back()&&state->unresolved,"leaving pending selection retains uncertainty");
 }{CharacterScreen ui(transport);settle(ui);unsigned invoked=0;ui.selection([&](uint32_t,const std::atomic_bool&){++invoked;return CharacterSelectionReply{};},state);ui.message(nullptr,WM_KEYDOWN,VK_RETURN,0);check(!invoked&&!ui.busy(),"uncertainty survives screen recreation");}
 {CharacterScreen ui(transport);settle(ui);ui.selection([](uint32_t id,const std::atomic_bool&){CharacterSelectionReply r;r.status=CharacterSelectionStatus::success;r.character.id=id;r.request_may_have_been_sent=true;r.lobbies={{7,5737,0,L"Test",0,2}};return r;},std::make_shared<CharacterSelectionState>());
  unsigned starts=0;std::atomic_bool ended=false;
  ui.rooms([&](uint32_t id,const GameLobbyEntry&l,const std::atomic_bool&c,std::atomic_bool&refresh,const RoomPublish&publish,RoomRequests& requests){++starts;check(id==91&&l.id==7,"mouse arrow preserves selected PC and filtered lobby ID");publish({RoomStatus::ready,{{77,L"Room",1,16}},0});while(!c){if(refresh.exchange(false))publish({RoomStatus::ready,{},0});Sleep(1);}ended=true;});
  ui.message(nullptr,WM_KEYDOWN,VK_RETURN,0);settle(ui);
  auto window=CreateWindowW(L"STATIC",L"offline lobby click",WS_POPUP,0,0,1280,720,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);check(window!=nullptr,"offline click surface");
  ui.message(window,WM_LBUTTONUP,0,MAKELPARAM(1125,235));DestroyWindow(window);
  auto waitFor=[&](auto predicate){auto until=GetTickCount64()+2000;while(!predicate()&&GetTickCount64()<until){ui.draw();Sleep(1);}check(predicate(),"room UI deadline");};
  waitFor([&]{return ui.room_status()==RoomStatus::ready;});check(ui.room_visible()&&ui.room_count()==1,"acknowledged game session opens room list");
  ui.message(nullptr,WM_KEYDOWN,VK_F5,0);waitFor([&]{return ui.room_count()==0;});check(starts==1,"refresh uses same live session");
  ui.message(nullptr,WM_KEYDOWN,VK_ESCAPE,0);check(ended&&!ui.room_visible()&&ui.lobby_visible(),"back cancels and joins session before lobby list");
 }
 std::cout<<"Selection/room protocol, persistent session UI, identity/no-retry and directory validation passed; synthetic transport only.\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
