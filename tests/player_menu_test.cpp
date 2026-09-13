#include "player_menu.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <iostream>
using namespace mgo2win;
static void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
static void bitmap(const void*p,const std::filesystem::path&path){BITMAPFILEHEADER h{};BITMAPINFOHEADER i{};h.bfType=0x4d42;h.bfOffBits=sizeof(h)+sizeof(i);h.bfSize=h.bfOffBits+1280*720*4;i.biSize=sizeof(i);i.biWidth=1280;i.biHeight=-720;i.biPlanes=1;i.biBitCount=32;std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<char*>(&h),sizeof(h));f.write(reinterpret_cast<char*>(&i),sizeof(i));f.write(static_cast<const char*>(p),1280*720*4);check(bool(f),"menu capture");}
static std::string read(const std::filesystem::path&path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
static void write(const std::filesystem::path&path,const std::string&text){std::ofstream f(path,std::ios::binary|std::ios::trunc);f<<text;f.close();check(bool(f),"fixture write");}
int main(int argc,char**argv){try{
 auto path=std::filesystem::temp_directory_path()/("MGO2WIN-player-menu-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));std::filesystem::create_directories(path);
 auto input=std::make_shared<ControllerInput>(path/"input.cfg");auto graphics=std::make_shared<GraphicsSettings>(path/"graphics.cfg");PlayerMenu menu(path/"input.cfg",input,graphics);
 auto key=[&](unsigned k){menu.message(nullptr,WM_KEYDOWN,k,0);};
 check(menu.enemy_name_tags()&&!std::filesystem::exists(path/"player.cfg"),"new personal enemy tags default ON without startup write");
 check(menu.camera_settings()==camera::Settings{}&&!std::filesystem::exists(path/"camera.cfg"),"camera defaults retain native directions without startup write");
 graphics->draft.width=1920;menu.close();check(graphics->draft.width==1920,"closed overlay must not reset another screen draft");
 menu.open(player::Menu::settings);check(menu.visible(),"START opens settings");key(VK_F3);key(VK_RIGHT);check(menu.prone_y_first_person(),"prone Y policy selectable and saved");
 check(menu.enemy_name_tags()&&read(path/"player.cfg")=="MGO2WIN.PLAYER 2 1 1\n","Y save preserves enemy preference in version 2");
 if(argc>1){std::filesystem::create_directories(argv[1]);bitmap(menu.draw(),std::filesystem::path(argv[1])/"gameplay_settings.bmp");}
 key(VK_DOWN);key(VK_RIGHT);check(!menu.enemy_name_tags()&&menu.prone_y_first_person(),"enemy OFF changes no prone-Y preference");
 check(read(path/"player.cfg")=="MGO2WIN.PLAYER 2 1 0\n","enemy OFF persisted");
 menu.message(nullptr,WM_KEYDOWN,VK_RETURN,1LL<<30);check(!menu.enemy_name_tags()&&read(path/"player.cfg")=="MGO2WIN.PLAYER 2 1 0\n","held confirm cannot repeat enemy toggle");
 if(argc>1)bitmap(menu.draw(),std::filesystem::path(argv[1])/"enemy_name_tags_off.bmp");
 key(VK_ESCAPE);check(!menu.visible(),"cancel returns to stage");PlayerMenu restored(path/"input.cfg",input,graphics);check(restored.prone_y_first_person()&&!restored.enemy_name_tags(),"both gameplay preferences persisted");
 menu.open(player::Menu::settings);key(VK_F3);
 const auto window=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,1280,720,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);check(window!=nullptr,"mouse fixture window");
 menu.message(window,WM_LBUTTONUP,0,MAKELPARAM(900,338));DestroyWindow(window);
 check(menu.enemy_name_tags()&&menu.prone_y_first_person(),"mouse enemy row toggles only enemy preference");
 if(argc>1)bitmap(menu.draw(),std::filesystem::path(argv[1])/"enemy_name_tags_on.bmp");
 // A failed atomic replacement must preserve both the old file and active UI.
 const auto beforeFailure=read(path/"player.cfg");
 HANDLE locked=CreateFileW((path/"player.cfg").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);check(locked!=INVALID_HANDLE_VALUE,"lock existing settings against replacement");
 key(VK_RETURN);CloseHandle(locked);
 check(menu.enemy_name_tags()&&menu.prone_y_first_person()&&read(path/"player.cfg")==beforeFailure,"save failure preserves existing file and live choices");
 for(const auto&entry:std::filesystem::directory_iterator(path))check(entry.path().filename().wstring().find(L".tmp.")==std::wstring::npos,"failed save leaves no owned temporary file");
 key(VK_ESCAPE);
 // Independent normal / shoulder / subjective directions persist only after
 // successful replacement, including when OPTIONS was opened by briefing.
 const auto playerFile=read(path/"player.cfg");menu.open(player::Menu::settings,true);
 check(menu.tab_action(9)&&menu.tab_action(9),"controller next tab skips unavailable graphics and reaches camera");
 key(VK_RETURN);check(menu.camera_settings().motion(false,false,.25f,.5f)==std::array<float,2>{.25f,-.5f},"normal Y inversion reaches manual motion");
 check(menu.camera_settings().motion(false,true,.25f,.5f)==std::array<float,2>{.25f,.5f},"normal setting does not change shoulder camera");
 key(VK_DOWN);key(VK_DOWN);key(VK_DOWN);key(VK_DOWN);key(VK_RETURN);
 check(menu.camera_settings().motion(false,true,.25f,.5f)==std::array<float,2>{-.25f,.5f},"shoulder X is independent of normal Y");
 key(VK_DOWN);key(VK_DOWN);key(VK_RETURN);
 check(menu.camera_settings().motion(true,true,.25f,.5f)==std::array<float,2>{.25f,-.5f},"subjective camera takes precedence over shoulder while aiming");
 check(read(path/"player.cfg")==playerFile,"camera writes preserve existing gameplay choices");
 check(menu.tab_action(8)&&menu.tab_action(8)&&menu.tab_action(8),"controller previous tab wraps through available tabs");
 check(!menu.tab_action(4),"non-tab controller actions continue through normal menu mapping");
 {PlayerMenu reloaded(path/"input.cfg",input,graphics);check(reloaded.camera_settings()==menu.camera_settings(),"camera directions restored after restart");}
 const auto cameraFile=read(path/"camera.cfg");auto cameraBefore=menu.camera_settings();
 locked=CreateFileW((path/"camera.cfg").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);check(locked!=INVALID_HANDLE_VALUE,"lock camera settings against replacement");key(VK_HOME);CloseHandle(locked);
 check(menu.camera_settings()==cameraBefore&&read(path/"camera.cfg")==cameraFile,"failed camera reset preserves live and disk choices");
 if(argc>1)bitmap(menu.draw(),std::filesystem::path(argv[1])/"camera_save_failure.bmp");
 key(VK_HOME);check(menu.camera_settings()==camera::Settings{},"explicit camera reset restores all six directions");
 menu.message(nullptr,WM_KEYDOWN,VK_RETURN,1LL<<30);check(menu.camera_settings()==camera::Settings{},"held camera confirm cannot toggle repeatedly");
 key(VK_UP);key(VK_UP);key(VK_UP);key(VK_UP);key(VK_UP);key(VK_UP);key(VK_UP);key(VK_UP);key(VK_RETURN);
 check(menu.camera_settings().reversed[5],"camera row navigation wraps");
 if(argc>1)bitmap(menu.draw(),std::filesystem::path(argv[1])/"camera_settings.bmp");
 const auto cameraWindow=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,1280,720,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);check(cameraWindow!=nullptr,"camera mouse fixture");
 menu.message(cameraWindow,WM_LBUTTONUP,0,MAKELPARAM(900,250));DestroyWindow(cameraWindow);
 check(menu.camera_settings().reversed[0]&&menu.camera_settings().reversed[5],"camera mouse row changes only selected axis");
 key(VK_ESCAPE);
 menu.open(player::Menu::chat);key(VK_RIGHT);for(auto c:std::wstring(L"動作確認です"))menu.message(nullptr,WM_CHAR,c,0);key(VK_RETURN);
 if(argc>1)bitmap(menu.draw(),std::filesystem::path(argv[1])/"chat_menu.bmp");key(VK_ESCAPE);check(!menu.visible(),"chat cancel");
 menu.open(player::Menu::equipment);key(VK_RETURN);if(argc>1)bitmap(menu.draw(),std::filesystem::path(argv[1])/"equipment_empty.bmp");key(VK_ESCAPE);check(!menu.visible(),"empty equipment cancel");
 // Version 1 is read without migration writes; its new preference defaults ON.
 write(path/"player.cfg","MGO2WIN.PLAYER 1 1\n");
 {PlayerMenu legacy(path/"input.cfg",input,graphics);check(legacy.prone_y_first_person()&&legacy.enemy_name_tags(),"version 1 preserves Y and defaults enemy tags ON");
  check(read(path/"player.cfg")=="MGO2WIN.PLAYER 1 1\n","reading legacy settings does not rewrite them");
  legacy.open(player::Menu::settings);legacy.message(nullptr,WM_KEYDOWN,VK_F3,0);legacy.message(nullptr,WM_KEYDOWN,VK_DOWN,0);legacy.message(nullptr,WM_KEYDOWN,VK_RETURN,0);
  check(!legacy.enemy_name_tags()&&legacy.prone_y_first_person()&&read(path/"player.cfg")=="MGO2WIN.PLAYER 2 1 0\n","explicit legacy edit migrates both values");}
 for(const auto&invalid:std::vector<std::string>{"MGO2WIN.PLAYER 2 1","MGO2WIN.PLAYER 2 1 2","MGO2WIN.PLAYER 2 2 0","MGO2WIN.PLAYER 1 1 extra","MGO2WIN.PLAYER 3 1 0","MGO2WIN.PLAYER 2 1 0 extra",std::string(129,'x')}){
  write(path/"player.cfg",invalid);PlayerMenu rejected(path/"input.cfg",input,graphics);
  check(!rejected.prone_y_first_person()&&rejected.enemy_name_tags(),"invalid gameplay settings retain complete defaults");check(read(path/"player.cfg")==invalid,"rejected settings not overwritten on load");
 }
 for(const auto&invalid:std::vector<std::string>{"MGO2WIN.CAMERA 1 1 0 0 0 0\n","MGO2WIN.CAMERA 2 1 0 0 0 0 0\n","MGO2WIN.CAMERA 1 1 0 0 0 0 2\n","MGO2WIN.CAMERA 1 1 0 0 0 0 0\nextra",std::string(129,'x')}){
  write(path/"camera.cfg",invalid);PlayerMenu rejected(path/"input.cfg",input,graphics);check(rejected.camera_settings()==camera::Settings{},"malformed camera file rejected without partial choices");check(read(path/"camera.cfg")==invalid,"invalid camera file preserved");
 }
 {PlayerMenu speedMenu(path/"input.cfg",input,graphics);speedMenu.open(player::Menu::settings,true);auto speedKey=[&](unsigned k){speedMenu.message(nullptr,WM_KEYDOWN,k,0);};speedKey(VK_F4);speedKey(VK_DOWN);speedKey(VK_DOWN);speedKey(VK_RIGHT);
  check(speedMenu.camera_settings().speed==std::array<unsigned,3>{6,5,5},"normal speed row is independently connected");
  check(speedMenu.camera_settings().rates(false,false)[0]>2.f&&speedMenu.camera_settings().rates(false,true)[0]==2.f,"normal speed affects turn rate without saturating stick input");
  for(unsigned i=0;i<20;++i)speedKey(VK_RIGHT);check(speedMenu.camera_settings().speed[0]==10,"camera maximum clamps to10");
  for(unsigned i=0;i<20;++i)speedKey(VK_LEFT);check(speedMenu.camera_settings().speed[0]==1,"camera minimum clamps to1");
  {PlayerMenu restoredSpeed(path/"input.cfg",input,graphics);check(restoredSpeed.camera_settings().speed==std::array<unsigned,3>{1,5,5},"speed restores after atomic save");}
  speedKey(VK_HOME);check(speedMenu.camera_settings()==camera::Settings{},"reset restores directions and all speeds5");
  if(argc>1)bitmap(speedMenu.draw(),std::filesystem::path(argv[1])/"camera_speed_settings.bmp");
 }
 for(const auto&entry:std::filesystem::directory_iterator(path))check(entry.path().filename().wstring().find(L".tmp.")==std::wstring::npos,"camera save failures remove owned temporary file");
 std::filesystem::remove(path/"player.cfg");std::filesystem::remove(path/"camera.cfg");std::filesystem::remove(path);std::cout<<"player menus, camera directions, default-ON enemy tags, v1/v2 settings, atomic failure preservation and cancel isolation passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
