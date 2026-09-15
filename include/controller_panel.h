#pragma once
#include "controller_input.h"
#include <memory>
#include <vector>
namespace mgo2win {
class ControllerPanel {
 std::filesystem::path path_;std::shared_ptr<ControllerInput> input_;InputConfig draft_;
 int focus_=0,page_=0,capture_=-1;bool armed_=false,dirty_=false,back_=false,connected_=false;
 std::wstring notice_=L"機器を選び、変更する項目を決定してください。";std::vector<unsigned> cues_;
 std::wstring back_label_=L"ネットワークへ戻る";
 uint32_t held_=0;
 float left_magnitude_=0,right_magnitude_=0;bool preview_running_=false;
 void cue(unsigned sound){if(cues_.size()<32)cues_.push_back(sound);}
 void activate();void select_device(int);void bind(unsigned);void save();void change_analog(int);void select_page(int);void move_focus(bool);
public:
 ControllerPanel(std::filesystem::path,std::shared_ptr<ControllerInput>);
 bool message(HWND,UINT,WPARAM,LPARAM);
 bool sample(const PadSample&); // True consumes the sample while capturing.
 POINT draw(HDC,const std::vector<HFONT>&); // Active item origin for focus guides.
 void paint_original(void* pixels)const;
 void cancel_capture(){capture_=-1;armed_=false;}
 void discard_changes(); // Reopen/close refreshes the draft from current saved runtime settings.
 void return_label(std::wstring label){back_label_=std::move(label);}
 bool capturing()const{return capture_>=0;}
 bool back()const{return back_;}
 void clear_back(){back_=false;}
 std::vector<unsigned> cues(){auto r=std::move(cues_);cues_.clear();return r;}
 unsigned slot()const{return draft_.slot;}
 void report()const;
};
}
