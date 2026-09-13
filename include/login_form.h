#pragma once
#include "menu_audio.h"
#include <array>
#include <algorithm>
#include <string>
#include <string_view>
namespace mgo2win {
// Native UI policy, not a claim about the original server's credential contract.
// Fixed storage avoids retaining password copies after edits/back navigation.
class LoginForm {
 struct Field {
  std::array<wchar_t,65> bytes{};size_t size=0,caret=0;bool selected=false;
  void clear(){volatile wchar_t* p=bytes.data();for(size_t i=0;i<bytes.size();++i)p[i]=0;size=caret=0;selected=false;}
  void erase(bool back){if(selected){clear();return;}if(back){if(!caret)return;--caret;}else if(caret==size)return;
   for(size_t i=caret;i<size;++i)bytes[i]=bytes[i+1];--size;}
  void insert(wchar_t c){if(selected)clear();if(size==64)return;for(size_t i=size;i>caret;--i)bytes[i]=bytes[i-1];bytes[caret++]=c;bytes[++size]=0;}
 };
 std::array<Field,2> fields_;int focus_=0;bool back_=false;unsigned attempts_=0;
 std::wstring notice_;unsigned saveMode_=0;
public:
 enum Key { previous,next,left,right,home,end,eraseForward,eraseBack,selectAll,confirm,cancel };
 LoginForm()=default;LoginForm(const LoginForm&)=delete;LoginForm& operator=(const LoginForm&)=delete;
 ~LoginForm(){clear();}
 void clear(){for(auto&f:fields_)f.clear();focus_=0;back_=false;notice_.clear();saveMode_=0;}
 unsigned save_mode()const{return saveMode_;}
 bool valid()const{return fields_[0].size&&fields_[1].size;}
 void clear_password(){fields_[1].clear();}
 std::wstring_view credential(int i)const{return {fields_.at(i).bytes.data(),fields_.at(i).size};}
 void notice(std::wstring s){notice_=std::move(s);}
 void restore(unsigned mode,std::wstring_view id,std::wstring_view password){clear();saveMode_=mode;for(auto c:id)character(c);focus_=1;for(auto c:password)character(c);focus_=mode==2?2:1;}
 int focus()const{return focus_;}size_t length(int i)const{return fields_.at(i).size;}
 size_t caret()const{return focus_<2?fields_[focus_].caret:0;}
 bool selected()const{return focus_<2&&fields_[focus_].selected;}
 bool back()const{return back_;}unsigned attempts()const{return attempts_;}
 const std::wstring& notice()const{return notice_;}
 std::wstring display(int i)const{return i==1?std::wstring(fields_[1].size,L'•'):std::wstring(fields_[0].bytes.data(),fields_[0].size);}
 int focus(int i){if(i<0||i>6||i==focus_)return -1;for(auto&f:fields_)f.selected=false;focus_=i;return menu_audio::Cursor;}
 void character(wchar_t c){if(focus_>=2)return;if(c<33||c>126){notice_=L"半角英数字・記号を入力してください。";return;}fields_[focus_].insert(c);notice_.clear();}
 int key(Key k){
  if(k==cancel){clear();back_=true;return menu_audio::Cancel;}
  if(k==next||k==previous)return focus((focus_+(k==next?1:6))%7);
  if(k==confirm){if(focus_<2){focus(focus_+1);return menu_audio::Confirm;}if(focus_==3)return key(cancel);
   if(focus_>=4){saveMode_=focus_-4;notice_=L"保存設定はログインボタンで適用されます。";return menu_audio::Confirm;}
   ++attempts_;if(!fields_[0].size){focus(0);notice_=L"GAME IDを入力してください。";}
   else if(!fields_[1].size){focus(1);notice_=L"パスワードを入力してください。";}
   else notice_=L"ログイン接続は準備中です。入力内容は送信されていません。";
   return valid()?int(menu_audio::Confirm):-1;
  }
  if(focus_>=4){if(k==left||k==right){focus(4+(focus_-4+(k==right?1:2))%3);saveMode_=focus_-4;return menu_audio::Cursor;}return -1;}
  if(focus_>=2){if(k==left||k==right)return focus(focus_==2?3:2);return -1;}
  auto&f=fields_[focus_];
  if(k==selectAll){f.selected=true;return -1;}
  if(k==eraseBack||k==eraseForward)f.erase(k==eraseBack);
  else {if(k==home)f.caret=0;else if(k==end)f.caret=f.size;
   else if(k==left)f.caret=f.selected?0:f.caret?f.caret-1:0;
   else if(k==right)f.caret=f.selected?f.size:std::min(f.size,f.caret+1);f.selected=false;}
  return -1;
 }
};
}
