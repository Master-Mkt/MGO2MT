#include "character_screen.h"
#include <algorithm>
#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <vector>

using namespace mgo2win;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Window {
 HWND handle=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,1280,720,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 Window(){check(handle!=nullptr,"hidden test window");}
 ~Window(){DestroyWindow(handle);}
};
struct Counts {
 size_t actions=0,commands=0,inputs=0,cancels=0;
 bool operator==(const Counts&)const=default;
};
}

// All responses below are local in-memory fixtures. No socket, authentication,
// real room, vote, READY grant or server mutation is performed by this test.
// Optional argv: weapon_catalog output_directory briefing_icons map_icons.
int main(int argc,char** argv){try{
 check(argc>=2,"weapon catalog argument required");
 const bool unicode=argc>5&&std::string_view(argv[5])=="unicode";
 Window window;std::mutex mutex;Counts counts;std::vector<combat::wire::Command> commands;
 std::optional<RoomReply> outgoing;
 const combat::Identity self{1,257,123};
 host::LoadRequest request{1,1,0,0,{20,1,0},host::MatchTransition::initial};
 RoomReply state;state.event=RoomEvent::join;state.requested_room=77;
 state.status=RoomStatus::ready;state.join_status=RoomJoinStatus::joined;
 RoomDetail detail;detail.id=77;detail.name=L"LOCAL BRIEFING";detail.comment=L"オフライン画面確認";
 detail.subtype=1;detail.capacity=17;detail.players=3;detail.dedicated=true;
 detail.environment_known=true;detail.briefing_minutes=2;
 detail.roster={{456,L"HOST"},{123,L"DOLL-01"},{789,L"DOLL-02"}};state.detail=detail;
 state.host_roster=host::Roster{};state.host_roster->complete=true;state.host_roster->revision=1;
 state.host_roster->slots[0]=host::Player{0,256,456,"HOST",{}};
 state.host_roster->slots[1]=host::Player{1,257,123,"DOLL-01","LOCAL"};
 state.host_roster->slots[2]=host::Player{2,258,789,"DOLL-02","LOCAL"};
 state.host_match=host::MatchState{};state.host_match->generation=1;state.host_match->request=request;
 state.host_match->rotations[0]=request.rotation;state.host_match->rotations_known=true;
 state.combat_offer=combat::wire::Offer{1,self};
 state.host_scene=stage::SceneSnapshot{request,1,{}};state.scene_status=stage::SceneSyncStatus::ready;
 combat::wire::Preparation preparation;preparation.epoch=1;preparation.revision=1;
 preparation.self=self;preparation.generation=1;preparation.runtimeReady=true;
 preparation.autoAssign=false;preparation.supported={25};preparation.requiredCategories=1;
 preparation.players[1]=combat::wire::RoundPlayer{self,1};
 preparation.players[2]=combat::wire::RoundPlayer{{2,258,789},2};state.preparation=preparation;
 CharacterScreen ui([](const std::atomic_bool&){CharacterReply reply;reply.status=CharacterStatus::success;reply.list.slots=4;reply.list.entries.push_back({123,L"DOLL-01"});return reply;});
 ui.weapon_catalog(argv[1]);ui.skill_catalog(std::filesystem::path(argv[1]).parent_path()/"skill_catalog.tsv");
 if(argc>3)ui.briefing_icons(argv[3]);
 if(argc>4)ui.briefing_map(argv[4]);
 ui.selection([](uint32_t,const std::atomic_bool&){CharacterSelectionReply reply;reply.status=CharacterSelectionStatus::success;reply.request_may_have_been_sent=true;reply.character.id=123;reply.character.name=L"DOLL-01";reply.lobbies.push_back({5,5735,3,L"Offline",0,2});return reply;},std::make_shared<CharacterSelectionState>());
 ui.rooms([&](uint32_t,const GameLobbyEntry&,const std::atomic_bool& stop,std::atomic_bool&,const RoomPublish& publish,RoomRequests& requests){
  if(unicode){requests.nameDirectory->connect(123);const std::array<uint32_t,3> ids{123,456,789};requests.nameDirectory->want(ids);requests.nameDirectory->take(10,0);
   std::vector<uint8_t> p{'G','W','N','M',1,0,1,1,0,0,0,0,0,0,0,10,0,3,16,64,0,0,0,1};
   for(auto id:ids){std::string name;for(int i=0;i<16;++i)name+=id==123?"名":id==456?"主":"敵";names::detail::put(p,id,4);p.push_back(0);p.push_back(0);names::detail::put(p,name.size(),2);p.insert(p.end(),name.begin(),name.end());}
   check(requests.nameDirectory->receive(p,1),"native full names injected only through checked codec and matching ID batch");
  }
  RoomReply listing;listing.status=RoomStatus::ready;listing.rooms.push_back({77,L"LOCAL BRIEFING",3,17,1,20});publish(listing);
  while(!stop){
   if(auto action=requests.take()){
    RoomReply reply;{std::lock_guard lock(mutex);++counts.actions;reply=state;}
    reply.event=action->event;
    if(action->event==RoomEvent::detail){reply.join_status=RoomJoinStatus::none;reply.preparation.reset();reply.combat_offer.reset();}
    publish(std::move(reply));
   }
   if(auto command=requests.take_command()){std::lock_guard lock(mutex);++counts.commands;commands.push_back(*command);}
   if(requests.take_combat()){std::lock_guard lock(mutex);++counts.inputs;}
   std::optional<RoomReply> next;{std::lock_guard lock(mutex);next=std::move(outgoing);outgoing.reset();}if(next)publish(std::move(*next));
   if(requests.cancel_join.exchange(false)){
    RoomReply reply;{std::lock_guard lock(mutex);++counts.cancels;reply=state;}
    reply.join_status=RoomJoinStatus::host_cancelled;reply.preparation.reset();reply.combat_offer.reset();publish(std::move(reply));
   }
   Sleep(1);
  }
 });
 auto key=[&](WPARAM code,LPARAM flags=0){ui.message(window.handle,WM_KEYDOWN,code,flags);};
 auto wait=[&](auto predicate,const char* message){auto deadline=GetTickCount64()+3000;while(!predicate()&&GetTickCount64()<deadline){ui.draw();Sleep(1);}check(predicate(),message);};
 auto settle=[&]{for(unsigned i=0;i<30;++i){ui.draw();Sleep(1);}};
 auto currentCounts=[&]{std::lock_guard lock(mutex);return counts;};
 auto readyCount=[&]{std::lock_guard lock(mutex);return std::count_if(commands.begin(),commands.end(),[](const auto& command){return command.kind==combat::wire::CommandKind::ready;});};
 auto lastCommand=[&]{std::lock_guard lock(mutex);check(!commands.empty(),"command was recorded");return commands.back();};
 auto noEffects=[&](Counts before,const char* message){settle();check(currentCounts()==before,message);};
 auto capture=[&](const char* name){
  if(argc<3)return;std::filesystem::path directory=argv[2];std::filesystem::create_directories(directory);
  BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=1280;info.biHeight=-720;
  info.biPlanes=1;info.biBitCount=32;info.biSizeImage=1280*720*4;file.bfType=0x4d42;
  file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+info.biSizeImage;
  const auto* pixels=ui.draw();std::ofstream output(directory/name,std::ios::binary);
  output.write(reinterpret_cast<const char*>(&file),sizeof(file));output.write(reinterpret_cast<const char*>(&info),sizeof(info));
  output.write(static_cast<const char*>(pixels),info.biSizeImage);check(bool(output),"capture file write");
 };
 auto publishState=[&]{
  const auto revision=++state.preparation->revision;{std::lock_guard lock(mutex);outgoing=state;}
  wait([&]{return ui.combat_preparation()&&ui.combat_preparation()->revision==revision;},"synthetic preparation update");
 };
 wait([&]{return !ui.busy();},"character list");key(VK_RETURN);wait([&]{return ui.lobby_visible();},"lobby selection");
 key(VK_RETURN);wait([&]{return ui.room_count()==1;},"room list");key(VK_RETURN);
 wait([&]{return ui.room_detail_visible()&&!ui.room_detail_busy();},"room detail");capture("00-room-detail.bmp");
 key(VK_RETURN);wait([&]{return ui.room_join_status()==RoomJoinStatus::joined;},"synthetic room admission");settle();
 if(unicode){check(ui.player_display_name(123,L"alias")==std::wstring(16,L'名'),"full Japanese name by stable ID");auto display=ui.room_host_roster();check(display&&display->slots[1]->name.size()==48&&state.host_roster->slots[1]->name=="DOLL-01","display copy cannot overwrite legacy HOST wire record");}
 // A visual loading status revokes local loaded state but cannot mark READY.
 const auto beforeLoading=currentCounts();const auto readyBeforeLoading=readyCount();
 ui.stage_feedback(stage::Status::loading);
 wait([&]{return currentCounts().commands==beforeLoading.commands+1;},"loading status publishes unloaded state");
 check(lastCommand().kind==combat::wire::CommandKind::loaded&&!lastCommand().enabled,"loading reports only unloaded state");
 capture("22-loading-rules.bmp");
 check(readyCount()==readyBeforeLoading&&!ui.combat_preparation()->players[1]->ready&&!ui.room_match_visible(),"data wait does not imply READY or deployment");
 // A ready-looking status alone is still not an applied scene acknowledgement.
 const auto afterLoading=currentCounts();ui.stage_feedback(stage::Status::preview_ready);
 noEffects(afterLoading,"status-only preview cannot grant scene readiness");
 check(ui.briefing_focus()==0,"briefing initially focuses START");capture("01-start-selected.bmp");
 auto before=currentCounts();
 for(unsigned i=1;i<7;++i){key(VK_RIGHT);check(ui.briefing_focus()==i,"seven toolbar items in order");check(ui.briefing_panel()==briefing::Panel::none,"focus alone does not open a panel");
  const char* names[]={"unused","02-map-selected.bmp","03-rules-selected.bmp","04-skills-selected.bmp","05-host-selected.bmp","06-options-selected.bmp","07-quit-selected.bmp"};capture(names[i]);}
 key(VK_RIGHT);check(ui.briefing_focus()==0,"right wraps to START");key(VK_LEFT);check(ui.briefing_focus()==6,"left wraps to QUIT");key(VK_RIGHT);
 noEffects(before,"toolbar focus never sends an action");

 key(VK_RETURN);check(ui.briefing_panel()!=briefing::Panel::none&&!ui.briefing_confirm_yes(),"START opens default-NO confirmation");capture("08-start-confirm-no.bmp");
 key(VK_F9);key(VK_F7);noEffects(before,"START confirmation consumes gameplay shortcuts");
 key(VK_RETURN);check(ui.briefing_panel()==briefing::Panel::none,"default NO closes START confirmation");noEffects(before,"default NO cannot send READY");
 key(VK_RETURN);key(VK_LEFT);check(ui.briefing_confirm_yes(),"explicit START YES focus");capture("09-start-confirm-yes.bmp");
 key(VK_RETURN,1LL<<30);noEffects(before,"held Enter cannot confirm START");
 key(VK_RETURN);wait([&]{return currentCounts().commands==before.commands+1;},"explicit READY command");
 check(lastCommand().kind==combat::wire::CommandKind::ready&&lastCommand().enabled,"START YES queues READY only");
 check(!ui.combat_preparation()->players[1]->ready&&!ui.room_match_visible(),"local confirmation cannot acknowledge READY or deploy");
 state.preparation->players[1]->ready=true;state.preparation->lastCommand=lastCommand().sequence;publishState();capture("10-ready-acknowledged.bmp");
 key(VK_F9);wait([&]{return currentCounts().commands==before.commands+2;},"F9 compatibility ready cancellation");
 check(lastCommand().kind==combat::wire::CommandKind::ready&&!lastCommand().enabled,"F9 queues cancellation");
 state.preparation->players[1]->ready=false;state.preparation->lastCommand=lastCommand().sequence;publishState();
 before=currentCounts();

 // Delayed same-epoch phase updates must not resurrect an old YES selection
 // when an automatically opened weapon menu is closed back to the briefing.
 for(const auto nextPhase:{combat::wire::RoundPhase::selecting,combat::wire::RoundPhase::active}){
  check(ui.briefing_focus()==0,"START refocused before phase invalidation");
  key(VK_RETURN);key(VK_LEFT);check(ui.briefing_panel()==briefing::Panel::ready&&ui.briefing_confirm_yes(),"pending START YES before phase update");
  state.preparation->phase=nextPhase;publishState();
  check(ui.briefing_panel()==briefing::Panel::none&&!ui.briefing_confirm_yes(),"same-epoch phase change discards old START confirmation");
  check(ui.weapon_visible(),"synthetic phase update opens existing weapon selection");
  key(VK_ESCAPE);check(!ui.weapon_visible()&&ui.briefing_panel()==briefing::Panel::none,"weapon back cannot resurrect old START dialog");
  noEffects(before,"phase invalidation cannot send READY or a grant");
  // Reset the independent fixture; this is not a HOST transition request.
  state.preparation->phase=combat::wire::RoundPhase::waiting;publishState();
 }
 key(VK_ESCAPE);key(VK_LEFT);check(ui.briefing_panel()==briefing::Panel::quit&&ui.briefing_confirm_yes(),"pending QUIT YES before phase update");
 state.preparation->phase=combat::wire::RoundPhase::selecting;publishState();
 check(ui.briefing_panel()==briefing::Panel::none&&!ui.briefing_confirm_yes(),"same-epoch phase change discards old QUIT confirmation");
 key(VK_ESCAPE);check(!ui.weapon_visible()&&ui.room_join_status()==RoomJoinStatus::joined,"weapon back after old QUIT remains joined");
 noEffects(before,"phase update cannot apply stale QUIT YES");
 state.preparation->phase=combat::wire::RoundPhase::waiting;publishState();
 for(unsigned i=0;i<7&&ui.briefing_focus()!=0;++i)key(VK_RIGHT);
 check(ui.briefing_focus()==0,"START refocus after invalidation cases");

 key(VK_RIGHT);key(VK_RETURN);check(ui.briefing_panel()!=briefing::Panel::none,"MAP opens only after Enter");capture("11-map-detail.bmp");
 key(VK_F9);key(VK_F7);key(VK_ESCAPE);check(ui.briefing_panel()==briefing::Panel::none&&ui.room_join_status()==RoomJoinStatus::joined,"MAP Esc closes without leaving");
 key(VK_RIGHT);key(VK_RETURN);check(ui.briefing_panel()!=briefing::Panel::none,"RULES detail opens");capture("12-rules-detail.bmp");key(VK_ESCAPE);
 key(VK_RIGHT);key(VK_RETURN);check(ui.skill_visible(),"SKILLS routes to existing editor");capture("13-skills-editor.bmp");key(VK_ESCAPE);check(!ui.skill_visible(),"SKILLS Esc returns to briefing");
 key(VK_RIGHT);key(VK_RETURN);check(ui.briefing_panel()!=briefing::Panel::none&&ui.briefing_host_choice()==0,"HOST opens first petition row");capture("14-host-stage-vote.bmp");
 key(VK_F9);key(VK_F7);key(VK_F4);ui.message(window.handle,WM_LBUTTONUP,0,MAKELPARAM(150,490));
 noEffects(before,"HOST panel consumes gameplay and background inputs");
 key(VK_RETURN);capture("15-host-stage-unavailable.bmp");
 check(ui.briefing_panel()!=briefing::Panel::none,"unavailable VOTE retains panel");key(VK_DOWN);check(ui.briefing_host_choice()==1,"kick petition selection");
 capture("16-host-kick-vote.bmp");key(VK_RETURN);capture("17-host-kick-unavailable.bmp");
 noEffects(before,"neither VOTE produces room/combat/input/cancel requests");
 key(VK_ESCAPE);check(ui.briefing_panel()==briefing::Panel::none&&ui.room_join_status()==RoomJoinStatus::joined,"HOST Esc does not leave");
 key(VK_RIGHT);key(VK_RETURN);check(ui.take_gameplay_options_request()&&!ui.take_gameplay_options_request(),"OPTIONS routes one local request");
 noEffects(before,"MAP RULES SKILLS HOST OPTIONS produce no game commands");
 key(VK_RIGHT);key(VK_RETURN);check(ui.briefing_panel()!=briefing::Panel::none&&!ui.briefing_confirm_yes(),"QUIT opens default-NO confirmation");capture("18-quit-confirm-no.bmp");
 key(VK_RETURN);check(ui.briefing_panel()==briefing::Panel::none,"QUIT NO returns to briefing");noEffects(before,"QUIT NO cannot disconnect");
 key(VK_ESCAPE);check(ui.briefing_panel()!=briefing::Panel::none&&!ui.briefing_confirm_yes(),"root Esc opens default-NO quit confirmation");key(VK_ESCAPE);
 noEffects(before,"Esc cancels a nested quit confirmation");

 // Either a changed stage request or a changed offer invalidates local drafts.
 for(unsigned i=0;i<7&&ui.briefing_focus()!=4;++i)key(VK_LEFT);
 check(ui.briefing_focus()==4,"bounded HOST refocus");key(VK_RETURN);check(ui.briefing_panel()!=briefing::Panel::none,"HOST reopened before stage change");
 request.sequence=2;request.generation=2;request.round=1;request.transition=host::MatchTransition::next_round;
 state.host_match->request=request;state.host_match->generation=2;state.host_scene=stage::SceneSnapshot{request,2,{}};
 publishState();check(ui.briefing_panel()==briefing::Panel::none,"stage change clears HOST draft before new offer");
 for(unsigned i=0;i<7&&ui.briefing_focus()!=4;++i)key(VK_LEFT);
 check(ui.briefing_focus()==4,"bounded HOST refocus before offer");key(VK_RETURN);
 state.combat_offer=combat::wire::Offer{2,self};state.preparation->epoch=2;state.preparation->generation=2;publishState();
 check(ui.briefing_panel()==briefing::Panel::none,"new epoch clears stale HOST draft");check(ui.stage_load_request()==request,"new stage remains admitted");capture("19-new-round-briefing.bmp");
 noEffects(before,"new round UI reset does not submit a vote or READY");
 key(VK_ESCAPE);key(VK_LEFT);check(ui.briefing_confirm_yes(),"explicit QUIT YES focus");capture("20-quit-confirm-yes.bmp");
 key(VK_RETURN,1LL<<30);noEffects(before,"held Enter cannot confirm QUIT");key(VK_RETURN);
 wait([&]{return ui.room_join_status()==RoomJoinStatus::host_cancelled;},"explicit QUIT cancellation reaches fake transport");
 const auto after=currentCounts();check(after.cancels==before.cancels+1&&after.actions==before.actions&&after.commands==before.commands&&after.inputs==before.inputs,"QUIT sends only one local cancellation");
 check(!ui.stage_load_request()&&ui.briefing_panel()==briefing::Panel::none,"cancelled room clears stage and child panels");capture("21-left-room.bmp");
 std::cout<<"Offline briefing: seven-item focus, START/QUIT confirmation, MAP/RULES, existing SKILLS/OPTIONS, no-send VOTE, input guards and epoch invalidation passed\n";
 return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}

