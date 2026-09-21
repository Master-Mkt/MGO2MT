#include "player_menu.h"
#include "chat_view.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
static void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
static void capture(const void*p,const std::filesystem::path&path){BITMAPFILEHEADER h{};BITMAPINFOHEADER i{};h.bfType=0x4d42;h.bfOffBits=sizeof(h)+sizeof(i);h.bfSize=h.bfOffBits+1280*720*4;i.biSize=sizeof(i);i.biWidth=1280;i.biHeight=-720;i.biPlanes=1;i.biBitCount=32;std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<char*>(&h),sizeof(h));f.write(reinterpret_cast<char*>(&i),sizeof(i));f.write(static_cast<const char*>(p),1280*720*4);check(bool(f),"capture");}
int main(int argc,char**argv){try{
 const auto path=std::filesystem::temp_directory_path()/("MGO2MT-chat-menu-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
 std::filesystem::create_directories(path);auto input=std::make_shared<ControllerInput>(path/"input.cfg");auto graphics=std::make_shared<GraphicsSettings>(path/"graphics.cfg");PlayerMenu menu(path/"input.cfg",input,graphics);
 auto session=std::make_shared<chat::Session>();session->connect(7);session->enter(17);session->roster({{7,"DOLL-01",1},{8,"DOLL-02",2}},true);session->capabilities(chat::Encoding::utf8,false);menu.chat_session(session);
 menu.open(player::Menu::chat);check(menu.text_entry()&&menu.visible(),"chat opens as text entry rather than mapped gameplay input");
 auto key=[&](unsigned k){menu.message(nullptr,WM_KEYDOWN,k,0);};auto text=[&](std::wstring_view s){for(auto c:s)menu.message(nullptr,WM_CHAR,c,0);};
 text(L"日本語とASCII ");text(L"POAW");check(!session->take(GetTickCount64()),"ordinary typing never submits");
 // IME conversion Enter is distinct from a new explicit send Enter.
 menu.message(nullptr,WM_IME_STARTCOMPOSITION,0,0);key(VK_RETURN);check(!session->take(GetTickCount64()),"IME Enter cannot submit");
 menu.message(nullptr,WM_IME_ENDCOMPOSITION,0,0);key(VK_RETURN);check(!session->take(GetTickCount64()),"composition-end Enter stays guarded");
 menu.message(nullptr,WM_KEYUP,VK_RETURN,0);key(VK_RETURN);auto sent=session->take(GetTickCount64());check(sent&&sent->text=="日本語とASCII POAW","new Enter queues exact Unicode draft");
 menu.message(nullptr,WM_KEYDOWN,VK_RETURN,1LL<<30);check(!session->take(GetTickCount64()),"held Enter does not duplicate");
 session->receive({7,0,sent->text},GetTickCount64());menu.draw();check(session->state().delivery==chat::Delivery::echo_received,"self echo observed through menu");
 for(unsigned i=0;i<5;++i)session->receive({8,0,"方向キーで選択します。日本語の長い文章も折り返して表示します。"},GetTickCount64());
 text(L"メッセージを入力しています");
 if(argc>1){std::filesystem::create_directories(argv[1]);capture(menu.draw(),std::filesystem::path(argv[1])/"chat_history.bmp");}
 menu.select_chat_radio();check(menu.radio_visible()&&!menu.text_entry(),"second SELECT opens presets");
 if(argc>1)capture(menu.draw(),std::filesystem::path(argv[1])/"preset_categories.bmp");
 menu.radio_digital_mask(3);check(menu.radio_visible(),"diagonal does not choose two levels");
 menu.radio_digital_mask(1);if(argc>1)capture(menu.draw(),std::filesystem::path(argv[1])/"preset_attack.bmp");
 menu.message(nullptr,WM_KEYDOWN,VK_UP,1LL<<30);check(!session->take(GetTickCount64()),"held direction cannot emit radio or ordinary chat");
 menu.radio_digital_mask(8);if(argc>1)capture(menu.draw(),std::filesystem::path(argv[1])/"preset_selected.bmp");
 check(!session->take(GetTickCount64()),"preset selection never impersonates an ordinary chat send");
 menu.select_chat_radio();check(menu.text_entry(),"SELECT returns to chat");
 key(VK_ESCAPE);check(!menu.visible()&&session->state().joined,"closing composer does not leave room");
 menu.open(player::Menu::chat);session->leave();menu.chat_session(session);check(!menu.visible()&&session->state().lines.empty(),"leave closes composer and history");
 session->enter(18);session->roster({{7,"DOLL-01",1}},true);menu.chat_session(session);menu.open(player::Menu::chat);key(VK_RETURN);check(!session->take(GetTickCount64()),"old draft cannot cross into another room");
 menu.select_chat_radio();check(menu.radio_visible(),"radio before pointer change");menu.chat_session(nullptr);check(!menu.visible(),"disconnect pointer change closes radio");
 menu.chat_session(session);menu.open(player::Menu::chat);menu.select_chat_radio();menu.chat_session(std::make_shared<chat::Session>());check(!menu.visible(),"new unconnected session cannot inherit radio modal");
 menu.close();session->disconnect();menu.chat_session(session);check(!std::filesystem::exists(path/"input.cfg")&&!std::filesystem::exists(path/"player.cfg"),"chat neither saves credentials nor edits gameplay options");
 std::filesystem::remove(path);std::cout<<"chat UI Unicode, IME Enter guard, explicit submission, echo, leave isolation and render PASS\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
