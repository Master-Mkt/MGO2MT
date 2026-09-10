#include "controller_input.h"
#include "controller_panel.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
void require(bool b){if(!b)throw std::runtime_error("Controller contract failed");}
int main(){
 auto dir=std::filesystem::current_path()/(L"controller-test-"+std::to_wstring(GetCurrentProcessId()));require(std::filesystem::create_directory(dir));auto p=dir/L"input.cfg";
 try{
  InputConfig c,loaded;require(valid_input_config(c)&&!load_input(p,loaded));assign_input(c,4,'Z');require(c.keyboard[4]=='Z'&&c.keyboard[6]==VK_RETURN);
  c.device=1;assign_input(c,4,5);require(c.gamepad[4]==5&&c.gamepad[5]==4);c.slot=3;save_input(p,c);require(load_input(p,loaded)&&loaded.keyboard==c.keyboard&&loaded.gamepad==c.gamepad&&loaded.device==1&&loaded.slot==3);
  {std::ofstream f(p);f<<"MGO2WIN.INPUT 1 2 0";}bool bad=false;try{load_input(p,loaded);}catch(...){bad=true;}require(bad&&loaded.device==1);
  auto invalid=c;invalid.keyboard[4]=VK_ESCAPE;require(!valid_input_config(invalid));invalid=c;invalid.gamepad[0]=invalid.gamepad[1];require(!valid_input_config(invalid));invalid=c;invalid.slot=4;require(!valid_input_config(invalid));
  save_input(p,c);auto runtime=std::make_shared<ControllerInput>(p);XINPUT_STATE state{};bool connected=true;unsigned queried=0;
  runtime->reader=[&](DWORD slot,XINPUT_STATE* out){queried=slot;*out=state;return connected?DWORD(ERROR_SUCCESS):DWORD(ERROR_DEVICE_NOT_CONNECTED);};
  state.Gamepad.wButtons=XINPUT_GAMEPAD_B;require(!runtime->poll(true).pressed&&queried==3);state={};runtime->poll(true);state.Gamepad.wButtons=XINPUT_GAMEPAD_B;auto press=runtime->poll(true);require(runtime->actions(press)[4]&&menu_key(4)==VK_RETURN);require(!runtime->poll(true).pressed);
  runtime->poll(false);require(!runtime->poll(true).pressed);state={};runtime->poll(true);state.Gamepad.wButtons=XINPUT_GAMEPAD_B;require(runtime->poll(true).pressed);connected=false;require(!runtime->poll(true).connected);connected=true;require(!runtime->poll(true).pressed);state={};runtime->poll(true,1);require(queried==1);
  XINPUT_GAMEPAD pad{};pad.sThumbLX=-32768;pad.sThumbRY=32767;pad.bLeftTrigger=31;auto bits=pad_buttons(pad);require((bits&(1u<<18))&&(bits&(1u<<20))&&(bits&(1u<<14)));pad={};pad.sThumbLX=XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;pad.bLeftTrigger=XINPUT_GAMEPAD_TRIGGER_THRESHOLD;require(!pad_buttons(pad));
  runtime->config={};ControllerPanel panel(p,runtime);auto key=[&](WPARAM k){panel.message(nullptr,WM_KEYDOWN,k,0);};
  for(int i=0;i<6;++i)key(VK_DOWN);key(VK_RETURN);require(panel.capturing());key('F');require(!panel.capturing());for(int i=0;i<6;++i)key(VK_DOWN);key(VK_RETURN);require(runtime->config.keyboard[4]=='F'&&runtime->keyboard_menu('F')==VK_RETURN);require(load_input(p,loaded)&&loaded.keyboard[4]=='F');
  // Switch draft to XInput, then bind B to confirm only after neutral input.
  for(int i=0;i<3;++i)key(VK_DOWN);key(VK_RIGHT);for(int i=0;i<6;++i)key(VK_DOWN);key(VK_RETURN);require(panel.capturing());
  require(panel.sample({true,1u<<4,1u<<4})&&panel.capturing());panel.sample({true,0,0});panel.sample({true,1u<<5,1u<<5});require(!panel.capturing());for(int i=0;i<6;++i)key(VK_DOWN);key(VK_RETURN);require(runtime->config.device==1&&runtime->config.gamepad[4]==5);
  for(int i=0;i<6;++i)key(VK_UP);key(VK_RETURN);key(VK_ESCAPE);require(!panel.capturing()&&!panel.back());
  std::filesystem::remove(p);std::filesystem::remove(dir);
 }catch(...){std::error_code ec;std::filesystem::remove(p,ec);std::filesystem::remove(dir,ec);throw;}
 std::cout<<"Controller config persistence, key/pad swap, invalid mapping, deadzones, focus/reconnect neutral gating, menu dispatch, keyboard/pad capture and cancellation passed with synthetic XInput.\n";
}
