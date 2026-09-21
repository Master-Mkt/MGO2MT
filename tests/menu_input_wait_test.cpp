#include "menu_input_wait.h"
#include "character_screen.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){
 using Kind=MenuInputWait::Kind;
 MenuInputWait wait;wait.context(1,0);
 require(wait.accept(Kind::decision,0),"initial confirm");
 require(!wait.accept(Kind::decision,1),"simultaneous second binding");
 require(!wait.accept(Kind::decision,299),"rapid release and repress");
 require(!wait.accept(Kind::decision,1000,true),"held keyboard cannot reactivate after timeout");
 require(wait.accept(Kind::decision,1001),"fresh press after release");
 wait.context(2,3000);
 require(!wait.accept(Kind::decision,3000),"async new screen gets its own wait");
 require(!wait.accept(Kind::move,3299),"navigation cannot leak across screen");
 wait.context(2,3300);require(wait.accept(Kind::decision,3300),"stable screen does not extend wait");
 require(wait.accept(Kind::none,3301),"text editing remains immediate");
 require(wait.accept(Kind::move,3600),"first cursor step");
 require(!wait.accept(Kind::move,3699,true)&&wait.accept(Kind::move,3700,true),"100 ms cursor repeat");
 require(MenuInputWait::key(VK_BACK,true)==Kind::none&&MenuInputWait::key(VK_LEFT,true)==Kind::none,"text erase and caret");
 require(MenuInputWait::key(VK_RETURN,true)==Kind::decision,"text submit still guarded");
 require(MenuInputWait::key(VK_F4)==Kind::decision,"shortcut guarded");
 // Real controller edges: held B, bounce during lockout, release/repress,
 // disconnect and reconnect held. Gameplay values are not modified by a menu.
 ControllerInput controller({});controller.config.device=1;XINPUT_STATE state{};bool connected=true;
 controller.reader=[&](DWORD,XINPUT_STATE* result){*result=state;return connected?ERROR_SUCCESS:ERROR_DEVICE_NOT_CONNECTED;};
 controller.poll(true);state.Gamepad.wButtons=XINPUT_GAMEPAD_B;
 auto sample=controller.poll(true);require(controller.actions(sample)[4],"B confirm edge");
 MenuInputWait padWait;require(padWait.accept(Kind::decision,0),"pad first confirm");
 require(!controller.actions(controller.poll(true))[4],"held B has no second edge");
 state.Gamepad.wButtons=0;controller.poll(true);state.Gamepad.wButtons=XINPUT_GAMEPAD_B;
 sample=controller.poll(true);require(controller.actions(sample)[4]&&!padWait.accept(Kind::decision,100),"pad bounce discarded");
 sample=controller.poll(true);require(!controller.actions(sample)[4]&&controller.action_values(sample)[4]==1,"no delayed replay; gameplay level retained");
 connected=false;controller.poll(true);connected=true;require(!controller.actions(controller.poll(true))[4],"reconnect held is unarmed");
 state.Gamepad.wButtons=0;controller.poll(true);state.Gamepad.wButtons=XINPUT_GAMEPAD_B;
 require(controller.actions(controller.poll(true))[4]&&padWait.accept(Kind::decision,400),"neutral then fresh confirm");
 // Real lobby UI: a category confirm must not immediately enter its lobby.
 CharacterScreen screen([](const std::atomic_bool&){CharacterReply r;r.status=CharacterStatus::success;r.list.slots=1;r.list.entries.push_back({123,L"PC"});return r;});
 screen.selection([](uint32_t,const std::atomic_bool&){CharacterSelectionReply r;r.status=CharacterSelectionStatus::success;r.request_may_have_been_sent=true;r.character.id=123;r.lobbies.push_back({5,5735,1,L"Test",0,2});return r;},std::make_shared<CharacterSelectionState>());
 screen.rooms([](uint32_t,const GameLobbyEntry&,const std::atomic_bool& stop,std::atomic_bool&,const RoomPublish& publish,RoomRequests&){RoomReply r;r.status=RoomStatus::ready;publish(r);while(!stop)Sleep(1);});
 auto settle=[&](auto ready){auto until=GetTickCount64()+3000;while(!ready()&&GetTickCount64()<until){screen.draw();Sleep(1);}require(ready(),"local fixture timeout");};
 settle([&]{return !screen.busy();});screen.message(nullptr,WM_KEYDOWN,VK_RETURN,0);
 const auto until=GetTickCount64()+3000;
 while(!screen.lobby_visible()&&GetTickCount64()<until){screen.message(nullptr,WM_KEYDOWN,VK_RETURN,0);Sleep(1);}
 require(screen.lobby_visible()&&!screen.room_visible(),"worker completion cannot carry old confirm into new lobby");
 screen.message(nullptr,WM_KEYDOWN,VK_LEFT,0);
 MenuInputWait lobbyWait;lobbyWait.context(screen.input_context(),0);
 auto press=[&](uint64_t now,bool repeat=false){lobbyWait.context(screen.input_context(),now);if(lobbyWait.accept(Kind::decision,now,repeat))screen.message(nullptr,WM_KEYDOWN,VK_RETURN,repeat?1LL<<30:0);lobbyWait.context(screen.input_context(),now);};
 const auto category=screen.input_context();press(0);require(screen.input_context()!=category&&!screen.room_visible(),"one confirm exits category only");
 press(1);press(299);press(1000,true);require(!screen.room_visible(),"rapid and held confirmation stay in lobby");
 press(1001);require(screen.room_visible(),"fresh deliberate confirmation enters lobby");
 std::cout<<"menu input wait: PASS\n";
}
