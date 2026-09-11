#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include "port_settings.h"
#include "stun.h"
#include <thread>
#include <functional>
#include "controller_panel.h"
#include "graphics_settings.h"
namespace mgo2win {
class PortScreen {
 std::filesystem::path store_;PortSettings settings_;PortReservation reservation_;PortResult result_;
 std::wstring number_,notice_;int focus_=2;bool selected_=false,back_=false,saved_=false,restored_=false;
 bool speed_open_=false;int speed_choice_=1;
 HDC dc_=nullptr;HBITMAP bitmap_=nullptr;HGDIOBJ old_=nullptr;void* pixels_=nullptr;std::vector<HFONT> fonts_;
 std::vector<unsigned> cues_;unsigned checks_=0;
 bool external_=true,pending_=false;std::atomic_bool cancel_{false},done_{false};std::thread worker_;StunResult stun_;
 std::function<StunResult(uintptr_t,const std::atomic_bool&)> probe_;
 std::shared_ptr<ControllerInput> input_;std::unique_ptr<ControllerPanel> controls_;bool controls_tab_=false;std::shared_ptr<GraphicsSettings> graphics_;bool graphics_tab_=false;
 void stop_probe();void update_probe();
 void invalidate();void activate();void check();void save();void focus(int);
 void open_speed();void accept_speed();
public:
 bool continue_enabled=false;bool proceed=false;
 explicit PortScreen(std::filesystem::path,bool external=true,
   std::function<StunResult(uintptr_t,const std::atomic_bool&)> probe=check_stun,std::shared_ptr<ControllerInput> input={},std::shared_ptr<GraphicsSettings> graphics={});~PortScreen();
 bool controller_sample(const PadSample& s){return controls_tab_&&controls_->sample(s);}
 unsigned input_slot()const{return controls_tab_?controls_->slot():input_->config.slot;}
 bool message(HWND,UINT,WPARAM,LPARAM);const void* draw();
 bool back()const{return back_;}std::vector<unsigned> cues(){auto r=std::move(cues_);cues_.clear();return r;}
 uintptr_t game_socket()const{return !pending_&&result_.status==PortStatus::available?reservation_.native_socket():~uintptr_t(0);}
 void report()const;
};
}
