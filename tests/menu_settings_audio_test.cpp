#include "menu_audio.h"
#include "controller_panel.h"
#include "graphics_settings.h"
#include "port_screen.h"
#include "player_menu.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace mgo2mt;
static void check(bool value,const char* label){if(!value)throw std::runtime_error(label);}
template<class Screen> static void sounds(Screen& screen,std::initializer_list<unsigned> expected,const char* label){check(screen.cues()==std::vector<unsigned>(expected),label);}
template<class Screen> static void key(Screen& screen,unsigned value){screen.message(nullptr,WM_KEYDOWN,value,0);}
template<class Screen> static void click(Screen& screen,HWND window,int x,int y){screen.message(window,WM_LBUTTONUP,0,MAKELPARAM(x,y));}

int main(){try{
 const auto dir=std::filesystem::current_path()/(L"menu-settings-audio-"+std::to_wstring(GetCurrentProcessId()));
 check(std::filesystem::create_directory(dir),"unique settings test directory");
 struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ec;for(const auto* file:{L"input.cfg",L"graphics.cfg",L"player.cfg",L"network.cfg"})std::filesystem::remove(path/file,ec);std::filesystem::remove(path,ec);}} cleanup{dir};
 struct Window{HWND handle=CreateWindowExW(0,L"STATIC",L"MGO2MT audio routing test",WS_POPUP,0,0,1280,720,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);~Window(){if(handle)DestroyWindow(handle);}} window;
 check(window.handle!=nullptr,"hidden mouse-coordinate test window");
 auto input=std::make_shared<ControllerInput>(dir/L"input.cfg");
 const auto confirm=menu_audio::Confirm,cancel=menu_audio::Cancel,cursor=menu_audio::Cursor;
 {
  ControllerPanel panel(dir/L"input.cfg",input);
  key(panel,VK_RETURN);sounds(panel,{cursor},"device activation changes value once");
  key(panel,VK_RIGHT);sounds(panel,{cursor},"device direction does not double cursor cue");
  key(panel,VK_DOWN);key(panel,VK_DOWN);sounds(panel,{cursor,cursor},"controller focus steps");
  key(panel,menu_key(4));check(panel.capturing(),"normalized confirm starts capture");sounds(panel,{confirm},"capture starts with confirm");
  key(panel,menu_key(5));check(!panel.capturing()&&!panel.back(),"normalized cancel only dismisses capture");sounds(panel,{cancel},"capture cancellation cue");
  key(panel,VK_RETURN);sounds(panel,{confirm},"capture starts again");click(panel,window.handle,500,310);sounds(panel,{cancel},"mouse cancellation cue");
  key(panel,VK_RETURN);sounds(panel,{confirm},"binding starts");key(panel,'F');sounds(panel,{confirm},"binding accepted cue");
  key(panel,VK_ESCAPE);check(panel.back(),"settings back state");sounds(panel,{cancel},"settings back cue");
  ControllerPanel back(dir/L"input.cfg",input);key(back,VK_UP);sounds(back,{cursor},"focus wraps to back");key(back,VK_RETURN);sounds(back,{cancel},"back button is cancellation");
 }
 {
  ControllerPanel panel(dir/L"input.cfg",input);key(panel,VK_PRIOR);key(panel,VK_DOWN);key(panel,VK_DOWN);sounds(panel,{cursor,cursor,cursor},"analog page navigation");
  for(unsigned i=0;i<input->config.left_deadzone;++i){key(panel,VK_LEFT);sounds(panel,{cursor},"analog value changed");}
  key(panel,VK_LEFT);sounds(panel,{},"clamped analog value has no cursor movement sound");
  click(panel,window.handle,740,275);sounds(panel,{},"selected controller page stays silent");
  click(panel,window.handle,550,275);sounds(panel,{cursor},"mouse controller page change");
  for(unsigned i=0;i<100;++i)key(panel,VK_DOWN);check(panel.cues().size()==32,"controller cue queue is bounded");
 }
 {
  input->config.device=1;ControllerPanel panel(dir/L"input.cfg",input);for(unsigned i=0;i<6;++i)key(panel,VK_DOWN);panel.cues();key(panel,VK_RETURN);sounds(panel,{confirm},"pad binding capture starts");
  panel.sample({true,0,0});panel.sample({true,1u<<5,1u<<5});check(!panel.capturing(),"neutral-armed pad binding accepted");sounds(panel,{confirm},"pad binding confirmation");input->config.device=0;
 }
 {
  GraphicsSettings graphics(dir/L"graphics.cfg");graphics.apply=[](const GraphicsConfig&){return true;};
  key(graphics,VK_RETURN);sounds(graphics,{cursor},"display mode activation does not double cue");key(graphics,VK_DOWN);key(graphics,VK_DOWN);graphics.cues();key(graphics,VK_RETURN);sounds(graphics,{},"no alternate refresh rate remains silent");
  click(graphics,window.handle,150,620);sounds(graphics,{confirm},"apply display choice");graphics.tick(100,true);check(graphics.pending(),"display confirmation pending");
  key(graphics,VK_ESCAPE);sounds(graphics,{cancel},"display confirmation cancellation");graphics.tick(101,true);check(!graphics.pending(),"display rolled back");
  click(graphics,window.handle,150,620);sounds(graphics,{confirm},"display apply again");graphics.tick(200,true);click(graphics,window.handle,200,620);sounds(graphics,{confirm},"display accepted and saved");
  key(graphics,VK_RIGHT);sounds(graphics,{},"direction on apply button does not pretend to change a value");
  click(graphics,window.handle,900,620);check(graphics.back(),"graphics mouse back");sounds(graphics,{cancel},"graphics mouse back cue");
 }
 {
  PortScreen screen(dir/L"network.cfg",false);sounds(screen,{confirm},"network menu entered");
  key(screen,VK_F1);sounds(screen,{},"selected network tab silent");key(screen,VK_F2);sounds(screen,{cursor},"controller tab cursor");
  key(screen,VK_ESCAPE);check(!screen.back(),"child settings back returns to network");sounds(screen,{cancel},"closed controller child preserves cancel without draw");
  key(screen,VK_F3);sounds(screen,{cursor},"graphics tab cursor");key(screen,VK_ESCAPE);sounds(screen,{cancel},"closed graphics child preserves cancel without draw");
  key(screen,VK_UP);sounds(screen,{cursor},"network speed field focus");key(screen,VK_RETURN);sounds(screen,{confirm},"speed selector entered");key(screen,VK_END);sounds(screen,{cursor},"speed selection moved");key(screen,VK_END);sounds(screen,{},"speed boundary silent");key(screen,VK_ESCAPE);sounds(screen,{cancel},"speed selector cancelled");
  key(screen,VK_RETURN);sounds(screen,{confirm},"speed selector reopened");click(screen,window.handle,1100,450);sounds(screen,{cancel},"click outside speed selector cancels");
  click(screen,window.handle,500,240);sounds(screen,{cursor},"mouse value activation emits one cue");
  click(screen,window.handle,1000,580);check(screen.back(),"network back button closes");sounds(screen,{cancel},"mouse focus and back produce only cancel");
 }
 {
  auto graphics=std::make_shared<GraphicsSettings>(dir/L"graphics.cfg");PlayerMenu menu(dir/L"input.cfg",input,graphics);
  menu.close();sounds(menu,{},"automatic close when hidden is silent");
  menu.open(player::Menu::settings);sounds(menu,{confirm},"pause settings opened");menu.open(player::Menu::settings);sounds(menu,{},"same open state is not repeated");
  key(menu,VK_F1);sounds(menu,{},"same pause tab silent");key(menu,VK_ESCAPE);check(!menu.visible(),"pause controller back hides menu");sounds(menu,{cancel},"hidden pause menu preserves child cancellation");
  menu.open(player::Menu::settings);menu.cues();key(menu,VK_F2);sounds(menu,{cursor},"pause graphics tab");key(menu,VK_ESCAPE);sounds(menu,{cancel},"hidden graphics pause menu preserves cancellation");
  menu.open(player::Menu::settings);menu.cues();key(menu,VK_F3);sounds(menu,{cursor},"gameplay tab");key(menu,VK_RIGHT);sounds(menu,{cursor},"gameplay value adjustment");key(menu,VK_RETURN);sounds(menu,{confirm},"gameplay value accepted and saved");key(menu,VK_ESCAPE);sounds(menu,{cancel},"gameplay settings back");
  menu.open(player::Menu::chat);sounds(menu,{confirm},"chat opened");click(menu,window.handle,500,180);sounds(menu,{cursor},"chat destination changed");click(menu,window.handle,500,180);sounds(menu,{},"same chat destination silent");key(menu,VK_RETURN);sounds(menu,{},"unsupported send is not falsely confirmed");key(menu,VK_ESCAPE);sounds(menu,{cancel},"chat closed");
  menu.open(player::Menu::equipment);menu.cues();menu.close();sounds(menu,{},"automatic room transition closes silently");
 }
 std::cout<<"Settings menu confirm/cancel/cursor dispatch, mouse and normalized pad inputs, unchanged-value silence, and hidden-child cue preservation passed offline.\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
