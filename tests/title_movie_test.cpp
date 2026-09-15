#include "title_movie.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using mgo2win::title_movie::Player;
using mgo2win::title_movie::Status;
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Window {
 HWND handle=nullptr;
 Window(){
  // Deliberately never shown or activated: tests must not interrupt another
  // app, synthesize input, or defeat the adapter's foreground pause policy.
  handle=CreateWindowExW(WS_EX_NOACTIVATE,L"STATIC",L"MGO2WIN hidden title movie test",
   WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,0,0,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
  require(handle!=nullptr,"hidden test parent creation");
 }
 ~Window(){if(handle)DestroyWindow(handle);}
};
struct CorruptFile {
 std::filesystem::path path;
 CorruptFile(){
  path=std::filesystem::temp_directory_path()/(L"mgo2win-title-movie-invalid-"+
   std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".mp4");
  std::ofstream file(path,std::ios::binary);file<<"not an MP4\n";
  require(bool(file),"invalid local media fixture creation");
 }
 ~CorruptFile(){std::error_code ec;std::filesystem::remove(path,ec);}
};
void dispatch(Player& player){
 MSG message{};
 while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
  require(message.message!=WM_QUIT,"unexpected test-window quit");
  TranslateMessage(&message);DispatchMessageW(&message);
 }
 player.pump();
}
void settle(Player& player,ULONGLONG milliseconds){
 const auto began=GetTickCount64();
 do{dispatch(player);Sleep(5);}while(GetTickCount64()-began<milliseconds);
}
void await(Player& player,Status expected,const char* why){
 const auto began=GetTickCount64();
 while(GetTickCount64()-began<35000){
  dispatch(player);
  if(player.status()==expected)return;
  if(player.status()==Status::failed&&expected!=Status::failed)
   throw std::runtime_error(std::string(why)+": "+player.error());
  Sleep(5);
 }
 throw std::runtime_error(std::string(why)+": bounded wait expired; "+player.error());
}
void run(const std::filesystem::path& media){
 require(media.is_absolute()&&std::filesystem::is_regular_file(media),"supply absolute real movie_01.mp4 path");
 Window window;
 Player player(window.handle);
 require(player.status()==Status::idle&&player.error().empty(),"initial idle state");
 player.mute(true);player.pause(true);player.stop();
 require(player.status()==Status::idle,"control before opening remains idle");
 player.open(media);player.place(10,20,320,240);
 require(player.status()==Status::loading,"real media open is asynchronous");
 await(player,Status::ready,"real MP4 asynchronously reaches ready with selected video");
 require(player.error().empty(),"real MP4 setup reports no error");
 auto child=GetWindow(window.handle,GW_CHILD);
 require(child!=nullptr&&(GetWindowLongPtrW(child,GWL_STYLE)&WS_DISABLED),"video child cannot steal input focus");
 require(GetWindowLongPtrW(child,GWL_EXSTYLE)&WS_EX_NOACTIVATE,"video child cannot activate its parent");
 RECT rect{};require(GetClientRect(child,&rect)!=FALSE,"child dimensions available");
 require(rect.right==320&&rect.bottom==180,"320 by 240 viewport fits a 320 by 180 video child");
 require(!IsWindowVisible(window.handle)&&!IsWindowVisible(child),"hidden test does not expose video");
 player.play();player.pause(false);settle(player,250);
 require(player.status()==Status::ready,"reserved playback stays paused without foreground ownership");
 player.pause(true);player.mute(false);player.mute(true);settle(player,100);
 player.pause(false);settle(player,100);
 require(player.status()==Status::ready&&player.error().empty(),"mute and pause toggles preserve a loaded background presentation");
 player.stop();settle(player,100);
 require(player.status()==Status::idle&&player.error().empty(),"stop releases playback and remains idle after callback drain");

 // Race the asynchronous parser with retirement, then open a fresh player.
 // A completion from the retired source must never change the new generation.
 player.open(media);player.play();player.stop();settle(player,150);
 require(player.status()==Status::idle,"late source-created callback cannot revive stopped media");
 player.open(media);await(player,Status::ready,"reopen after immediate cancellation");
 player.stop();
 auto missing=media.parent_path()/(L"missing-title-movie-"+std::to_wstring(GetCurrentProcessId())+L".mp4");
 require(!std::filesystem::exists(missing),"missing-file fixture unexpectedly exists");
 player.open(missing);
 require(player.status()==Status::failed&&!player.error().empty(),"missing local movie yields explicit failure");
 player.play();settle(player,100);
 require(player.status()==Status::failed,"play cannot revive a failed media open");
 {
  CorruptFile bad;
  player.open(bad.path);await(player,Status::failed,"invalid MP4 codec/parser reports asynchronous failure");
  require(!player.error().empty(),"asynchronous media failure supplies error details");
 }
 player.open(media);await(player,Status::ready,"valid source recovers after missing and corrupt source errors");
 require(player.error().empty(),"successful reopen clears prior failure");
 player.stop();
 std::cout<<"title movie PASS: real MFPlay asynchronous MP4 setup, hidden 16:9 child, focus suppression, mute/control calls, stop/cancel/reopen, missing/corrupt recovery\n"
  <<"limits: no foreground activation; actual playback/pause/resume, audio track selection, visible rendering and end-of-file are NOT verified\n";
}
}
int wmain(int argc,wchar_t** argv){
 try{require(argc==2,"Usage: title_movie_test ABSOLUTE_movie_01.mp4");run(argv[1]);return 0;}
 catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
