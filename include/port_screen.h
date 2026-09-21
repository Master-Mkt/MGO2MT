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
namespace mgo2mt {
class PortScreen {
 uint16_t fixedPort_=0;
 std::filesystem::path store_;PortSettings settings_;PortReservation reservation_;PortResult result_;
 std::wstring number_,notice_;int focus_=2;bool selected_=false,back_=false,saved_=false,restored_=false;
 bool speed_open_=false;int speed_choice_=1;
 HDC dc_=nullptr;HBITMAP bitmap_=nullptr;HGDIOBJ old_=nullptr;void* pixels_=nullptr;std::vector<HFONT> fonts_;
 std::vector<unsigned> cues_;unsigned checks_=0;
 bool external_=true,pending_=false;std::atomic_bool cancel_{false},done_{false};std::thread worker_;StunResult stun_;
 std::function<StunResult(uintptr_t,const std::atomic_bool&)> probe_;
 std::shared_ptr<ControllerInput> input_;std::unique_ptr<ControllerPanel> controls_;bool controls_tab_=false;std::shared_ptr<GraphicsSettings> graphics_;bool graphics_tab_=false;
 void enforce_fixed_port(){if(fixedPort_){settings_.automatic=false;settings_.port=fixedPort_;number_=std::to_wstring(fixedPort_);}}
 void fixed_port_notice(){notice_=L"ローカル試験のポートは固定です：UDP "+std::to_wstring(fixedPort_);}
 void stop_probe();void update_probe();
 void cue(unsigned sound){if(cues_.size()<32)cues_.push_back(sound);}
 void invalidate();void activate();void check();void save();void focus(int,bool sound=true);
 void open_speed();void accept_speed();
public:
 bool continue_enabled=false;bool proceed=false;
 explicit PortScreen(std::filesystem::path,bool external=true,
   std::function<StunResult(uintptr_t,const std::atomic_bool&)> probe=check_stun,std::shared_ptr<ControllerInput> input={},std::shared_ptr<GraphicsSettings> graphics={},uint16_t fixedPort=0);~PortScreen();
 bool controller_sample(const PadSample& s){return controls_tab_&&controls_->sample(s);}
 bool capturing()const{return controls_tab_&&controls_->capturing();}
 bool text_entry()const{return !controls_tab_&&!graphics_tab_&&!speed_open_&&focus_==1;}
 uint64_t input_context()const{return uint64_t(controls_tab_)|(uint64_t(graphics_tab_)<<1)|(uint64_t(speed_open_)<<2)|(uint64_t(pending_)<<3)|(uint64_t(graphics_->pending())<<4);}
 unsigned input_slot()const{return controls_tab_?controls_->slot():input_->config.slot;}
 bool message(HWND,UINT,WPARAM,LPARAM);const void* draw();
 bool back()const{return back_;}std::vector<unsigned> cues();
 uintptr_t game_socket()const{return !pending_&&result_.status==PortStatus::available?reservation_.native_socket():~uintptr_t(0);}
 void report()const;
};
}
