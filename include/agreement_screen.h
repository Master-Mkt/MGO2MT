#pragma once
#include <windows.h>
#include <atomic>
#include <thread>
#include <vector>
#include "http_text.h"
namespace mgo2mt {
// Text/layout are native Windows rendering; no HTML or script execution.
class AgreementScreen {
 std::thread worker_;std::atomic_bool cancel_{false},done_{false};HttpText response_;
 bool reported_=false,yes_=false,accepted_=false;int scroll_=0,maximum_=0;
 std::wstring url_;HDC dc_=nullptr;HBITMAP bitmap_=nullptr;HGDIOBJ old_=nullptr;void* pixels_=nullptr;
 std::vector<HFONT> fonts_;
public:
 enum Input:unsigned { left=1,right=2,confirm=4,up=8,down=16,pageUp=32,pageDown=64,home=128,end=256,retry=512,cancel=1024 };
 explicit AgreementScreen(std::wstring url);
 ~AgreementScreen();
 void start();bool ready()const{return done_.load();}bool ok()const{return ready()&&response_.error.empty();}
 void return_from_login(){accepted_=false;yes_=false;}
 bool accepted()const{return accepted_;}
 // Returns a reviewed UI cue, or -1. NO exits this bounded preview.
 int input(unsigned buttons,bool& close);
 const void* draw();
 void report();
};
}
