#pragma once
#include "motion_blend_settings.h"
#include <windows.h>
namespace mgo2win::motion_blend {
class Dialog {
 HWND window_=nullptr;Settings* settings_=nullptr;std::filesystem::path path_;
 static INT_PTR CALLBACK procedure(HWND,UINT,WPARAM,LPARAM);
public:
 ~Dialog(){close();}
 void open(HWND owner,Settings&,const std::filesystem::path&,bool show=true);
 void close();
 bool message(MSG& msg){
  if(!window_)return false;
  if(msg.message==WM_KEYDOWN&&msg.wParam==VK_F12&&(msg.hwnd==window_||IsChild(window_,msg.hwnd))){SendMessageW(GetParent(window_),WM_KEYDOWN,msg.wParam,msg.lParam|(1LL<<25));return true;}
  return IsDialogMessageW(window_,&msg);
 }
 bool visible()const{return window_!=nullptr;}
 HWND handle()const{return window_;}
};
}
