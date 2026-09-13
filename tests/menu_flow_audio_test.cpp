#include "character_screen.h"
#include "login_form.h"
#include "menu_audio.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class Screen> void cue(Screen& screen,unsigned expected,const char* message){check(screen.cues()==std::vector<unsigned>{expected},message);}
template<class Screen> void quiet(Screen& screen,const char* message){check(screen.cues().empty(),message);}
template<class Screen> void key(Screen& screen,WPARAM code,LPARAM flags=0){screen.message(nullptr,WM_KEYDOWN,code,flags);}
template<class Predicate> void settle(CharacterScreen& screen,Predicate predicate){
 const auto until=GetTickCount64()+3000;while(!predicate()&&GetTickCount64()<until){screen.draw();Sleep(1);}check(predicate(),"menu transition timed out");screen.cues();
}
void creation_audio(HWND window){
 CharacterCreation clean(nullptr);key(clean,VK_F1);quiet(clean,"same creation tab is silent");
 key(clean,VK_DOWN);cue(clean,menu_audio::Cursor,"creation focus uses cursor");
 key(clean,VK_RETURN);cue(clean,menu_audio::Cursor,"creation value change uses cursor");
 key(clean,VK_ESCAPE);cue(clean,menu_audio::Cancel,"leaving dirty creation uses cancel");
 key(clean,VK_RIGHT);quiet(clean,"already focused NO is silent");
 key(clean,VK_RETURN);cue(clean,menu_audio::Cancel,"discard NO uses cancel");check(!clean.closed(),"NO preserves draft");
 key(clean,VK_ESCAPE);clean.cues();key(clean,VK_LEFT);cue(clean,menu_audio::Cursor,"discard YES focus uses cursor");
 key(clean,VK_RETURN,1LL<<30);quiet(clean,"held confirm does not repeat");
 key(clean,VK_RETURN);cue(clean,menu_audio::Confirm,"explicit discard YES confirms action");check(clean.closed(),"explicit discard closes draft");
 CharacterCreation mouse(nullptr);mouse.message(window,WM_LBUTTONUP,0,MAKELPARAM(450,314));
 cue(mouse,menu_audio::Cursor,"mouse focus plus value change is one cursor event");
 mouse.message(window,WM_LBUTTONUP,0,MAKELPARAM(600,635));cue(mouse,menu_audio::Cancel,"mouse back uses cancel");
 mouse.message(window,WM_LBUTTONUP,0,MAKELPARAM(750,430));cue(mouse,menu_audio::Cancel,"mouse discard NO uses cancel");
 CharacterCreation review(nullptr);for(auto c:std::wstring(L"Sample"))review.message(nullptr,WM_CHAR,c,0);
 key(review,VK_END);review.cues();key(review,VK_UP);review.cues();key(review,VK_RETURN);
 cue(review,menu_audio::Confirm,"creation review advances with confirm");key(review,VK_RETURN);cue(review,menu_audio::Cancel,"review back uses cancel");
 CharacterCreation untouched(nullptr);key(untouched,VK_ESCAPE);cue(untouched,menu_audio::Cancel,"clean creation back uses cancel");
}
void login_audio(){
 LoginForm form;check(form.key(LoginForm::next)==menu_audio::Cursor,"login focus uses cursor");
 check(form.key(LoginForm::confirm)==menu_audio::Confirm,"login field advance uses confirm");
 check(form.key(LoginForm::confirm)==-1,"empty login does not sound successful");
 for(auto c:std::wstring(L"Sample"))form.character(c);form.focus(1);form.character(L'p');form.focus(2);
 check(form.key(LoginForm::confirm)==menu_audio::Confirm,"valid login submit uses confirm");
 form.focus(6);check(form.key(LoginForm::confirm)==menu_audio::Confirm,"save option confirmation");
 form.focus(3);check(form.key(LoginForm::confirm)==menu_audio::Cancel&&form.back(),"login back action uses cancel");
 form.clear();check(form.key(LoginForm::cancel)==menu_audio::Cancel,"login escape uses cancel");
}
void flow_audio(const char* catalog,HWND window){
 uint64_t clock=0;std::atomic_int deploymentUpdate=0;
 CharacterScreen screen([](const std::atomic_bool&){CharacterReply r;r.status=CharacterStatus::success;r.list.slots=4;r.list.entries.push_back({123,L"PC"});return r;},[&]{return clock;});
 screen.weapon_catalog(catalog);
 screen.selection([](uint32_t,const std::atomic_bool&){CharacterSelectionReply r;r.status=CharacterSelectionStatus::success;r.request_may_have_been_sent=true;r.character.id=123;r.lobbies.push_back({5,5735,1,L"Test",0,2});return r;},std::make_shared<CharacterSelectionState>());
 screen.rooms([&](uint32_t,const GameLobbyEntry&,const std::atomic_bool& stop,std::atomic_bool&,const RoomPublish& publish,RoomRequests& requests){
  RoomReply listing;listing.status=RoomStatus::ready;listing.rooms.push_back({77,L"Room",1,16});publish(listing);
  RoomReply joined;joined.status=RoomStatus::ready;joined.event=RoomEvent::join;joined.requested_room=77;joined.join_status=RoomJoinStatus::joined;
  RoomDetail detail;detail.id=77;detail.capacity=16;detail.players=1;detail.subtype=1;detail.roster={{456,L"HOST"}};joined.detail=detail;
  joined.host_match=host::MatchState{};joined.host_match->request=host::LoadRequest{1,1,0,0,{20,1,0},host::MatchTransition::initial};
  combat::Identity self{1,257,123};combat::wire::Preparation preparation;preparation.epoch=1;preparation.revision=1;preparation.self=self;preparation.runtimeReady=true;preparation.supported={25};preparation.requiredCategories=1;preparation.players[1]=combat::wire::RoundPlayer{self,1};joined.preparation=preparation;
  while(!stop){
   if(auto update=deploymentUpdate.exchange(0)){joined.preparation->players[1]->deployed=update==1;++joined.preparation->revision;publish(joined);}
   if(auto action=requests.take()){auto response=joined;response.event=action->event;if(action->event==RoomEvent::detail){response.join_status=RoomJoinStatus::none;response.preparation.reset();}publish(response);}
   if(auto command=requests.take_command()){if(command->kind==combat::wire::CommandKind::ready){joined.preparation->players[1]->ready=command->enabled;++joined.preparation->revision;publish(joined);}}
   if(requests.cancel_join.exchange(false)){auto response=joined;response.join_status=RoomJoinStatus::host_cancelled;response.preparation.reset();publish(response);}
   Sleep(1);
  }
 });
 settle(screen,[&]{return !screen.busy();});key(screen,VK_HOME);quiet(screen,"same PC focus is silent");
 key(screen,VK_BACK);clock=3000;screen.draw();cue(screen,menu_audio::Confirm,"delete hold opens confirmation");
 key(screen,VK_RIGHT);quiet(screen,"delete already focused NO is silent");key(screen,VK_RETURN);cue(screen,menu_audio::Cancel,"delete NO uses cancel");
 key(screen,VK_DOWN);cue(screen,menu_audio::Cursor,"PC slot movement uses cursor");
 screen.message(window,WM_LBUTTONUP,0,MAKELPARAM(250,638));cue(screen,menu_audio::Confirm,"mouse create is one confirm without focus click");
 key(screen,VK_ESCAPE);cue(screen,menu_audio::Cancel,"nested creation back cue survives owner close");check(!screen.creation_visible(),"creation closes");
 key(screen,VK_HOME);screen.cues();key(screen,VK_RETURN);settle(screen,[&]{return screen.lobby_visible();});
 key(screen,VK_LEFT);cue(screen,menu_audio::Cursor,"lobby category column movement");key(screen,VK_RETURN);cue(screen,menu_audio::Confirm,"lobby category advance is one confirm");
 key(screen,VK_RETURN);settle(screen,[&]{return screen.room_count()==1;});
 key(screen,VK_RETURN);settle(screen,[&]{return screen.room_detail_visible()&&!screen.room_detail_busy();});
 key(screen,VK_ESCAPE);cue(screen,menu_audio::Cancel,"room detail back uses cancel");
 key(screen,VK_RETURN);settle(screen,[&]{return screen.room_detail_visible()&&!screen.room_detail_busy();});
 key(screen,VK_RETURN);settle(screen,[&]{return screen.room_join_status()==RoomJoinStatus::joined;});
 key(screen,VK_F9);cue(screen,menu_audio::Confirm,"ready enable uses confirm");
 settle(screen,[&]{return screen.combat_preparation()->players[1]->ready;});
 key(screen,VK_F9);cue(screen,menu_audio::Cancel,"ready disable uses cancel");
 settle(screen,[&]{return !screen.combat_preparation()->players[1]->ready;});
 key(screen,VK_F4);cue(screen,menu_audio::Confirm,"weapon screen opens with confirm");
 check(screen.weapon_music_available(),"music available before deployment");
 key(screen,VK_F8);quiet(screen,"unavailable library does not confirm");check(!screen.take_weapon_music_request(),"library loading cannot open picker");
 screen.weapon_music_feedback(L"Sample title",true);key(screen,VK_F8);cue(screen,menu_audio::Confirm,"music entry opens with confirm");
 check(screen.take_weapon_music_request()&&!screen.take_weapon_music_request(),"music request consumed once");
 key(screen,VK_DOWN);cue(screen,menu_audio::Cursor,"music to OK moves cursor");key(screen,VK_RIGHT);cue(screen,menu_audio::Cursor,"OK to back moves cursor");
 key(screen,VK_RETURN);cue(screen,menu_audio::Cancel,"weapon back uses cancel");check(!screen.weapon_music_available(),"closed screen disarms music");
 key(screen,VK_F4);screen.cues();screen.message(window,WM_LBUTTONUP,0,MAKELPARAM(500,597));cue(screen,menu_audio::Confirm,"mouse MUSIC is one confirm");check(screen.take_weapon_music_request(),"mouse music request");
 deploymentUpdate=1;settle(screen,[&]{return screen.combat_preparation()->players[1]->deployed;});
 check(!screen.weapon_music_available(),"ordinary BGM editing disarmed after deployment");key(screen,VK_F8);quiet(screen,"deployed BGM request is rejected silently");check(!screen.take_weapon_music_request(),"deployed player cannot open ordinary picker");
 deploymentUpdate=2;settle(screen,[&]{return !screen.combat_preparation()->players[1]->deployed;});check(screen.weapon_music_available(),"received predeployment state restores picker");
 key(screen,VK_ESCAPE);screen.cues();key(screen,VK_ESCAPE);cue(screen,menu_audio::Cancel,"joined room leave request uses cancel");
 check(screen.briefing_panel()==briefing::Panel::quit&&!screen.briefing_confirm_yes(),"leave defaults to NO");key(screen,VK_LEFT);screen.cues();key(screen,VK_RETURN);cue(screen,menu_audio::Cancel,"explicit leave YES requests cancellation");
 settle(screen,[&]{return screen.room_join_status()==RoomJoinStatus::host_cancelled;});
 key(screen,VK_ESCAPE);cue(screen,menu_audio::Cancel,"cancelled room detail closes with cancel");
 key(screen,VK_ESCAPE);cue(screen,menu_audio::Cancel,"room list back uses cancel");
 key(screen,VK_ESCAPE);cue(screen,menu_audio::Cancel,"lobby back uses cancel");
 key(screen,VK_ESCAPE);cue(screen,menu_audio::Cancel,"PC list back uses cancel");
}
}
int main(int argc,char**argv){try{
 check(argc==2,"weapon catalog argument required");
 const auto window=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,1280,720,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 check(window!=nullptr,"hidden test window");login_audio();creation_audio(window);flow_audio(argv[1],window);DestroyWindow(window);
 std::cout<<"Menu confirm/cancel/cursor, mouse single events, nested close, readiness and MUSIC routing passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
