#include "controller_input.h"
#include "controller_panel.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <limits>
using namespace mgo2win;
void require(bool b){if(!b)throw std::runtime_error("Controller contract failed");}
void render_panel(const std::filesystem::path& output,const std::shared_ptr<ControllerInput>& input,const std::filesystem::path& config){
 std::filesystem::create_directories(output);auto dc=CreateCompatibleDC(nullptr);require(dc!=nullptr);
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=1280;info.bmiHeader.biHeight=-720;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
 void* pixels=nullptr;auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);require(bitmap&&pixels);auto previous=SelectObject(dc,bitmap);
 std::vector<HFONT> fonts;for(int size:{30,23,20,17})fonts.push_back(CreateFontW(-size,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FIXED_PITCH,L"MS Gothic"));
 SelectObject(dc,fonts[2]);for(unsigned action=0;action<input_actions;++action){SIZE size{};auto label=action_name(action);require(GetTextExtentPoint32W(dc,label,int(wcslen(label)),&size));require(size.cx<=590&&size.cy<=28);}
 ControllerPanel panel(config,input);PadSample sample;sample.connected=true;sample.raw_left_magnitude=.62f;sample.raw_right_magnitude=.40f;panel.sample(sample);
 const wchar_t* names[]={L"controller-buttons-1.bmp",L"controller-buttons-2.bmp",L"controller-movement.bmp",L"controller-analog.bmp"};
 for(int page=0;page<4;++page){
  if(page)panel.message(nullptr,WM_KEYDOWN,VK_NEXT,0);std::fill_n(static_cast<unsigned char*>(pixels),1280*720*4,0);auto point=panel.draw(dc,fonts);GdiFlush();require(point.x>=0&&point.x<1280&&point.y>=0&&point.y<720);
  BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=header.bfOffBits+1280*720*4;
  std::ofstream file(output/names[page],std::ios::binary);file.write(reinterpret_cast<const char*>(&header),sizeof(header));file.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(info.bmiHeader));file.write(static_cast<const char*>(pixels),1280*720*4);require(bool(file));
 }
 SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);for(auto font:fonts)DeleteObject(font);
}
int main(int argc,char** argv){
 auto dir=std::filesystem::current_path()/(L"controller-test-"+std::to_wstring(GetCurrentProcessId()));require(std::filesystem::create_directory(dir));auto p=dir/L"input.cfg";
 try{
  InputConfig c,loaded;require(valid_input_config(c)&&!load_input(p,loaded));assign_input(c,4,'Z');require(c.keyboard[4]=='Z'&&c.keyboard[6]==VK_RETURN);
  require(c.gamepad[4]==5&&c.gamepad[5]==4);c.device=1;assign_input(c,4,4);require(c.gamepad[4]==4&&c.gamepad[5]==5);assign_input(c,4,5);c.slot=3;
  c.left_deadzone=20;c.right_deadzone=30;c.run_threshold=70;c.run_hysteresis=12;
  save_input(p,c);require(load_input(p,loaded)&&loaded.keyboard==c.keyboard&&loaded.gamepad==c.gamepad&&loaded.device==1&&loaded.slot==3&&loaded.left_deadzone==20&&loaded.right_deadzone==30&&loaded.run_threshold==70&&loaded.run_hysteresis==12);
  {std::ifstream f(p);std::string tag;unsigned version=0;f>>tag>>version;require(tag=="MGO2WIN.INPUT"&&version==2);}
  auto legacy=InputConfig{};std::swap(legacy.gamepad[4],legacy.gamepad[5]);
  auto write_legacy=[&](const InputConfig& value){std::ofstream f(p);f<<"MGO2WIN.INPUT 1\n"<<value.device<<' '<<value.slot<<'\n';for(auto key:value.keyboard)f<<key<<' ';f<<'\n';for(auto code:value.gamepad)f<<code<<' ';f<<'\n';};
  legacy.keyboard=c.keyboard;write_legacy(legacy);require(load_input(p,loaded)&&loaded.gamepad==InputConfig{}.gamepad&&loaded.keyboard==c.keyboard&&loaded.run_threshold==65);
  legacy.device=1;assign_input(legacy,8,9);write_legacy(legacy);require(load_input(p,loaded)&&loaded.gamepad==legacy.gamepad&&loaded.gamepad[4]==4&&loaded.gamepad[5]==5); // A custom legacy preset is preserved intact.
  {std::ofstream f(p);f<<"MGO2WIN.INPUT 1 2 0";}bool bad=false;try{load_input(p,loaded);}catch(...){bad=true;}require(bad&&loaded.device==1);
  auto invalid=c;invalid.keyboard[4]=VK_ESCAPE;require(!valid_input_config(invalid));invalid=c;invalid.gamepad[0]=invalid.gamepad[1];require(!valid_input_config(invalid));invalid=c;invalid.slot=4;require(!valid_input_config(invalid));
  invalid=c;invalid.left_deadzone=91;require(!valid_input_config(invalid));invalid=c;invalid.right_deadzone=91;require(!valid_input_config(invalid));invalid=c;invalid.run_threshold=9;require(!valid_input_config(invalid));invalid=c;invalid.run_threshold=101;require(!valid_input_config(invalid));invalid=c;invalid.run_hysteresis=31;require(!valid_input_config(invalid));invalid=c;invalid.run_threshold=10;invalid.run_hysteresis=10;require(!valid_input_config(invalid));
  {std::ofstream f(p);f<<"MGO2WIN.INPUT 2\n1 0\n";for(auto key:c.keyboard)f<<key<<' ';f<<'\n';for(auto code:c.gamepad)f<<code<<' ';f<<"\n20 30 65";}bad=false;try{load_input(p,loaded);}catch(...){bad=true;}require(bad);
  save_input(p,c);auto runtime=std::make_shared<ControllerInput>(p);XINPUT_STATE state{};bool connected=true;unsigned queried=0;
  runtime->reader=[&](DWORD slot,XINPUT_STATE* out){queried=slot;*out=state;return connected?DWORD(ERROR_SUCCESS):DWORD(ERROR_DEVICE_NOT_CONNECTED);};
  state.Gamepad.wButtons=XINPUT_GAMEPAD_B;require(!runtime->poll(true).pressed&&queried==3);state={};runtime->poll(true);state.Gamepad.wButtons=XINPUT_GAMEPAD_B;auto press=runtime->poll(true);require(runtime->actions(press)[4]&&menu_key(4)==VK_RETURN);require(!runtime->poll(true).pressed);
  runtime->poll(false);require(!runtime->poll(true).pressed);state={};runtime->poll(true);state.Gamepad.wButtons=XINPUT_GAMEPAD_B;require(runtime->poll(true).pressed);connected=false;require(!runtime->poll(true).connected);connected=true;require(!runtime->poll(true).pressed);state={};runtime->poll(true,1);require(queried==1);
  XINPUT_GAMEPAD pad{};pad.sThumbLX=-32768;pad.sThumbRY=32767;pad.bLeftTrigger=31;auto bits=pad_buttons(pad);require((bits&(1u<<18))&&(bits&(1u<<20))&&(bits&(1u<<14)));pad={};pad.sThumbLX=XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;pad.bLeftTrigger=XINPUT_GAMEPAD_TRIGGER_THRESHOLD;require(!pad_buttons(pad));
  runtime->config=c;runtime->reset();state={};runtime->poll(true);auto neutral=runtime->poll(true);require(neutral.armed&&!neutral.held);
  state.Gamepad.sThumbLX=4096;state.Gamepad.sThumbRY=8192;auto dead=runtime->poll(true);require(dead.armed&&dead.left_x==0&&dead.right_y==0&&!dead.held&&dead.raw_left_magnitude>0);
  state.Gamepad.sThumbLX=16384;state.Gamepad.sThumbRY=32767;state.Gamepad.bLeftTrigger=255;auto analog=runtime->poll(true);require(analog.armed&&analog.left_x>.374f&&analog.left_x<.376f&&analog.right_y==1&&analog.left_trigger==1);
  auto values=runtime->action_values(analog);require(values[19]>.374f&&values[19]<.376f&&values[18]==0&&values[20]==1&&values[10]==1);
  assign_input(runtime->config,16,14);values=runtime->action_values(analog);require(values[16]==1&&values[10]==0);runtime->config=c;
  state.Gamepad.sThumbLX=-32768;state.Gamepad.sThumbLY=32767;state.Gamepad.sThumbRY=0;state.Gamepad.bLeftTrigger=0;auto diagonal=runtime->poll(true);require(std::abs(std::hypot(diagonal.left_x,diagonal.left_y)-1.f)<.00001f&&diagonal.left_x<-.707f&&diagonal.left_y>.707f);
  auto inactive=runtime->poll(false);require(!inactive.armed&&!inactive.held&&!inactive.pressed&&inactive.left_x==0&&runtime->action_values(inactive)[18]==0&&inactive.raw_held);
  auto retained=runtime->poll(true);require(!retained.armed&&!retained.held&&retained.left_x==0);state={};runtime->poll(true);state.Gamepad.wButtons=XINPUT_GAMEPAD_A;auto held=runtime->poll(true);require(held.armed&&(held.pressed&(1u<<4))&&runtime->action_values(held)[5]==1);
  state={};auto release=runtime->poll(true);require((release.released&(1u<<4))&&!release.held&&!release.pressed);require(!runtime->poll(true).released);
  state.Gamepad.sThumbLX=32767;runtime->poll(true);connected=false;auto lost=runtime->poll(true);require(!lost.connected&&!lost.armed&&!lost.held&&lost.left_x==0);connected=true;require(!runtime->poll(true).armed);state={};runtime->poll(true);state.Gamepad.sThumbLX=32767;auto changed_slot=runtime->poll(true,2);require(!changed_slot.armed&&!changed_slot.held&&changed_slot.left_x==0);
  state={};runtime->poll(true,2);state.Gamepad.bLeftTrigger=XINPUT_GAMEPAD_TRIGGER_THRESHOLD;require(runtime->poll(true,2).left_trigger==0);state.Gamepad.bLeftTrigger=31;require(runtime->poll(true,2).left_trigger>0&&runtime->poll(true,2).left_trigger<.005f);
  InputConfig tune;require(!input_running(.64f,false,tune)&&input_running(.65f,false,tune)&&input_running(.60f,true,tune)&&!input_running(.56f,true,tune)&&!input_running(0,true,tune)&&!input_running(std::numeric_limits<float>::quiet_NaN(),true,tune));
  runtime->config={};ControllerPanel panel(p,runtime);auto key=[&](WPARAM k){panel.message(nullptr,WM_KEYDOWN,k,0);};
  for(int i=0;i<6;++i)key(VK_DOWN);key(VK_RETURN);require(panel.capturing());key('F');require(!panel.capturing());for(int i=0;i<6;++i)key(VK_DOWN);key(VK_RETURN);require(runtime->config.keyboard[4]=='F'&&runtime->keyboard_menu('F')==VK_RETURN);require(load_input(p,loaded)&&loaded.keyboard[4]=='F');
  // Switch draft to XInput, then bind B to confirm only after neutral input.
  for(int i=0;i<3;++i)key(VK_DOWN);key(VK_RIGHT);for(int i=0;i<6;++i)key(VK_DOWN);key(VK_RETURN);require(panel.capturing());
  require(panel.sample({true,1u<<4,1u<<4})&&panel.capturing());panel.sample({true,0,0});panel.sample({true,1u<<5,1u<<5});require(!panel.capturing());for(int i=0;i<6;++i)key(VK_DOWN);key(VK_RETURN);require(runtime->config.device==1&&runtime->config.gamepad[4]==5);
  for(int i=0;i<6;++i)key(VK_UP);key(VK_RETURN);key(VK_ESCAPE);require(!panel.capturing()&&!panel.back());
  // Analog page edits stay a draft until Apply, and Cancel leaves the file intact.
  ControllerPanel analog_panel(p,runtime);auto analog_key=[&](WPARAM k){analog_panel.message(nullptr,WM_KEYDOWN,k,0);};
  analog_key(VK_PRIOR);analog_key(VK_DOWN);analog_key(VK_DOWN);analog_key(VK_RIGHT);require(runtime->config.left_deadzone==24);
  for(int i=0;i<6;++i)analog_key(VK_DOWN);analog_key(VK_RETURN);require(runtime->config.left_deadzone==25&&load_input(p,loaded)&&loaded.left_deadzone==25);
  ControllerPanel cancel_panel(p,runtime);cancel_panel.message(nullptr,WM_KEYDOWN,VK_PRIOR,0);cancel_panel.message(nullptr,WM_KEYDOWN,VK_DOWN,0);cancel_panel.message(nullptr,WM_KEYDOWN,VK_DOWN,0);cancel_panel.message(nullptr,WM_KEYDOWN,VK_LEFT,0);cancel_panel.message(nullptr,WM_KEYDOWN,VK_ESCAPE,0);require(cancel_panel.back()&&runtime->config.left_deadzone==25&&load_input(p,loaded)&&loaded.left_deadzone==25);
  // Reset the analog draft, then save without touching the custom button bindings.
  ControllerPanel reset_panel(p,runtime);auto reset_key=[&](WPARAM k){reset_panel.message(nullptr,WM_KEYDOWN,k,0);};reset_key(VK_PRIOR);reset_key(VK_UP);reset_key(VK_UP);reset_key(VK_RETURN);reset_key(VK_UP);reset_key(VK_RETURN);require(runtime->config.left_deadzone==24&&runtime->config.keyboard[4]=='F'&&runtime->config.gamepad[4]==5);
  ControllerPanel reopened(p,runtime);auto reopen_key=[&](WPARAM k){reopened.message(nullptr,WM_KEYDOWN,k,0);};
  runtime->config.left_deadzone=37;save_input(p,runtime->config); // Another menu saved after this panel was constructed.
  for(int i=0;i<6;++i)reopen_key(VK_DOWN);reopen_key(VK_RETURN);require(reopened.capturing());reopened.discard_changes();require(!reopened.capturing()&&!reopened.back());
  for(int i=0;i<6;++i)reopen_key(VK_DOWN);reopen_key(VK_RETURN);require(runtime->config.left_deadzone==37&&load_input(p,loaded)&&loaded.left_deadzone==37&&loaded.keyboard[4]=='F');
  runtime->config.left_deadzone=24;
  if(argc>1)render_panel(std::filesystem::path(argv[1]),runtime,p);
  std::filesystem::remove(p);std::filesystem::remove(dir);
 }catch(...){std::error_code ec;std::filesystem::remove(p,ec);std::filesystem::remove(dir,ec);throw;}
 std::cout<<"Controller INPUT v1 migration/v2 persistence, custom bindings, bounded analog tuning, radial normalization, remapped continuous values, release edges, focus/reconnect/slot neutral gating, run hysteresis, menu dispatch, capture, analog Apply/Cancel/reset passed with synthetic XInput.\n";
}
