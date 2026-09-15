#include "menu_font.h"
#include "skill_menu.h"
#include "menu_audio.h"
#include "menu_theme.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace mgo2win {
namespace {
constexpr int left=138,top=202,listWidth=604,rowHeight=42,rows=8;
std::wstring wide(const std::string&s){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0);std::wstring value(size_t(std::max(n,0)),0);if(n)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),value.data(),n);return value;}
const wchar_t* problem(skills::Validation value){switch(value){
 case skills::Validation::over_budget:return L"消費枠が上限を超えます。ほかのスキルを外すかレベルを下げてください。";
 case skills::Validation::invalid_capacity:return L"スキルの利用枠を確認できません。";
 case skills::Validation::valid:return L"";
 default:return L"設定を確認できません。選び直してください。";
}}
}
SkillMenu::SkillMenu(){
 dc_=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=1280;info.bmiHeader.biHeight=-720;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
 bitmap_=CreateDIBSection(dc_,&info,DIB_RGB_COLORS,&pixels_,nullptr,0);
 if(!dc_||!bitmap_){if(bitmap_)DeleteObject(bitmap_);if(dc_)DeleteDC(dc_);throw std::runtime_error("Skill menu surface");}
 old_=SelectObject(dc_,bitmap_);font_=create_menu_font(22,FW_NORMAL);
}
SkillMenu::~SkillMenu(){SelectObject(dc_,old_);DeleteObject(bitmap_);DeleteObject(font_);DeleteDC(dc_);}
void SkillMenu::icons(const std::filesystem::path&path){std::string error;icons_.load(path,error);}
void SkillMenu::cue(unsigned value){if(cues_.size()<32)cues_.push_back(value);}
void SkillMenu::open(std::shared_ptr<const skills::Catalog>catalog,const skills::Loadout&state,unsigned capacity,bool editable){
 catalog_=std::move(catalog);ids_=catalog_?catalog_->ids():std::vector<uint16_t>{};editor_=catalog_?std::make_unique<skills::Editor>(catalog_,state,capacity):nullptr;
 focus_=0;visible_=true;editable_=editable;applyAllowed_=true;saved_.reset();notice_.clear();contextNotice_.clear();
 if(!editor_||ids_.empty())notice_=L"スキル一覧を読み込めませんでした。";
 else if(!editor_->status())notice_=problem(editor_->status().result);
 // The entry that opens this menu supplies its confirmation cue.
}
void SkillMenu::close(bool feedback){if(!visible_)return;visible_=false;if(editor_)editor_->reset();if(feedback)cue(menu_audio::Cancel);}
void SkillMenu::synchronize(const skills::Loadout& value,unsigned capacity,bool preserveDraft){
 if(!editor_||!visible_)return;
 editor_->synchronize(value,capacity,preserveDraft||editor_->changed());notice_.clear();
 if(!editor_->status())notice_=problem(editor_->status().result);
}
const skills::Loadout& SkillMenu::draft()const{static const skills::Loadout empty;return editor_?editor_->draft():empty;}
void SkillMenu::move(int delta){auto next=size_t(std::clamp(int(focus_)+delta,0,int(ids_.size()+1)));if(next!=focus_){focus_=next;notice_.clear();cue(menu_audio::Cursor);}}
void SkillMenu::change(int direction,bool toggle){
 if(!editor_||focus_>=ids_.size())return;
 if(!editable_){notice_=L"スキルの変更はキャラクター選択後、出撃前に行えます。";return;}
 auto id=ids_[focus_];auto levels=catalog_->levels(id);if(levels.empty())return;
 auto current=std::find_if(draft().entries.begin(),draft().entries.end(),[&](auto e){return e.id==id;});uint8_t before=current==draft().entries.end()?0:current->level;uint8_t after=0;
 if(toggle)after=before?0:levels.front()->level;
 else {size_t at=0;for(size_t i=0;i<levels.size();++i)if(levels[i]->level==before){at=i+1;break;}
  auto next=std::clamp(int(at)+direction,0,int(levels.size()));after=next?levels[size_t(next)-1]->level:0;}
 if(before==after)return;auto result=editor_->set(id,after);
 if(result){notice_.clear();cue(toggle?menu_audio::Confirm:menu_audio::Cursor);}else notice_=problem(result.result);
}
void SkillMenu::apply(){
 if(!editor_||ids_.empty())return;
 if(!applyAllowed_){notice_=contextNotice_.empty()?L"サーバーのスキル設定を確認してから適用してください。":contextNotice_;return;}
 if(!editable_){close(true);return;}
 auto result=editor_->status();if(!result){notice_=problem(result.result);return;}
 saved_=editor_->draft();visible_=false;cue(menu_audio::Confirm);
}
void SkillMenu::activate(){if(focus_<ids_.size())change(1,true);else if(focus_==ids_.size())apply();else close(true);}
bool SkillMenu::message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 if(!visible_)return false;
 if(msg==WM_KEYDOWN){if(lp&(1LL<<30))return true;
  if(wp==VK_ESCAPE)close(true);else if(wp==VK_RETURN||wp==VK_SPACE)activate();
  else if(wp==VK_UP)move(-1);else if(wp==VK_DOWN)move(1);
  else if(wp==VK_LEFT||wp==VK_RIGHT){if(focus_<ids_.size())change(wp==VK_LEFT?-1:1);else move(wp==VK_LEFT?-1:1);}
  else if(wp==VK_PRIOR)move(-rows);else if(wp==VK_NEXT)move(rows);
  else if(wp==VK_HOME)move(-int(focus_));else if(wp==VK_END){if(focus_!=ids_.size()){focus_=ids_.size();cue(menu_audio::Cursor);}}
  else if(wp==VK_TAB){size_t next=focus_<ids_.size()?ids_.size():focus_==ids_.size()?ids_.size()+1:0;if(next!=focus_){focus_=next;cue(menu_audio::Cursor);}}
  else if(wp==VK_DELETE&&focus_<ids_.size()&&editable_&&editor_){auto before=draft();auto result=editor_->set(ids_[focus_],0);if(result&&before!=draft())cue(menu_audio::Confirm);}
  else if(wp==VK_F6&&editable_&&editor_&&!draft().entries.empty()){editor_->clear();notice_.clear();cue(menu_audio::Confirm);}
  else if(wp==VK_F10)apply();return true;
 }
 if(msg==WM_MOUSEWHEEL){move(short(HIWORD(wp))>0?-1:1);return true;}
 if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(!r.right||!r.bottom)return true;
  int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;
  if(x>=left&&x<left+listWidth&&y>=top&&y<top+rows*rowHeight){size_t page=std::min(focus_,ids_.empty()?size_t(0):ids_.size()-1)/rows;size_t at=page*rows+size_t((y-top)/rowHeight);
   if(at<ids_.size()){focus_=at;if(x>=left+listWidth-38)change(1);else if(x>=left+listWidth-166&&x<left+listWidth-132)change(-1);else change(1,true);}
  }else if(y>=620&&y<665){if(x>=138&&x<630){focus_=ids_.size();apply();}else if(x>=674&&x<1142){focus_=ids_.size()+1;close(true);}}
  return true;
 }
 return msg==WM_CHAR||msg==WM_KEYUP||msg==WM_LBUTTONDOWN;
}
const void* SkillMenu::draw(){
 std::memset(pixels_,0,1280*720*4);SetBkMode(dc_,TRANSPARENT);auto old=SelectObject(dc_,font_);
 auto text=[&](int x,int y,int w,int h,const std::wstring&s,COLORREF color=RGB(235,242,240),UINT flags=DT_SINGLELINE){SetTextColor(dc_,color);RECT r{x,y,x+w,y+h};DrawTextW(dc_,s.c_str(),int(s.size()),&r,flags|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);};
 auto orange=RGB(255,203,132),muted=RGB(157,169,165);menu_heading(dc_,font_,L"SKILL SETTINGS");
 menu_section(dc_,font_,L"SKILL",left,166,listWidth);menu_section(dc_,font_,L"EQUIPPED SKILLS",770,166,373);
 size_t page=std::min(focus_,ids_.empty()?size_t(0):ids_.size()-1)/rows;
 text(left+500,166,95,23,std::to_wstring(ids_.empty()?0:page+1)+L" / "+std::to_wstring((ids_.size()+rows-1)/rows),muted,DT_RIGHT);
 for(int row=0;row<rows;++row){size_t at=page*rows+size_t(row);if(at>=ids_.size())break;auto id=ids_[at];auto levels=catalog_->levels(id);if(levels.empty())continue;
  auto chosen=std::find_if(draft().entries.begin(),draft().entries.end(),[&](auto e){return e.id==id;});const auto*selected=chosen==draft().entries.end()?nullptr:catalog_->find(id,chosen->level);
  int y=top+row*rowHeight;menu_row(dc_,left,y,listWidth,rowHeight-2,at,focus_==at);
  if(!icons_.find(id))text(left+10,y,34,rowHeight,L"★",selected?orange:muted,DT_CENTER);
  text(left+52,y,357,rowHeight,wide(levels.front()->display_name),selected?RGB(248,243,225):muted);
  text(left+432,y,36,rowHeight,L"◀",muted,DT_CENTER);
  text(left+470,y,91,rowHeight,selected?L"Lv."+std::to_wstring(selected->level):L"OFF",selected?orange:muted,DT_CENTER);
  text(left+565,y,33,rowHeight,L"▶",muted,DT_CENTER);
 }
 unsigned capacity=editor_?editor_->capacity():skills::base_capacity,used=editor_?editor_->status().used:0;
 text(770,199,370,31,L"消費枠  "+std::to_wstring(used)+L" / "+std::to_wstring(capacity),orange);
 for(unsigned i=0;i<skills::maximum_capacity;++i){int x=770+int(i)*46;menu_rect(dc_,x,237,40,22,i<used?RGB(194,143,83):i<capacity?RGB(66,70,65):RGB(38,48,49));if(i>=capacity)text(x,235,40,25,L"—",muted,DT_CENTER);}
 text(770,266,372,27,L"標準4枠 / 拡張上限8枠",muted);
 for(size_t i=0;i<draft().entries.size()&&i<skills::maximum_capacity;++i){auto choice=draft().entries[i];const auto*entry=catalog_?catalog_->find(choice.id,choice.level):nullptr;int y=307+int(i)*30;
  menu_band(dc_,770,y,373,29,i);text(777,y,276,29,entry?wide(entry->display_name):L"確認できないスキル",entry?RGB(235,242,240):muted);
  text(1055,y,82,29,L"Lv."+std::to_wstring(choice.level),orange,DT_RIGHT);
 }
 if(focus_<ids_.size()&&catalog_){auto id=ids_[focus_];auto levels=catalog_->levels(id);std::wstring cost=L"消費枠：";for(size_t i=0;i<levels.size();++i){if(i)cost+=L"   ";cost+=L"Lv."+std::to_wstring(levels[i]->level)+L" = "+std::to_wstring(levels[i]->cost);}text(left,548,1000,30,cost,orange);}
 const auto& notice=notice_.empty()?contextNotice_:notice_;
 text(left,580,1004,28,notice.empty()?editable_?L"スキルの消費枠の合計が上限以内になるように設定します。":L"ラウンド中は確認のみです。変更は出撃前に行ってください。":notice,notice.empty()?muted:orange);
 menu_row(dc_,138,620,492,45,0,focus_==ids_.size());text(138,620,492,45,!applyAllowed_?L"保存待機 / 設定を確認中":editable_?L"設定を保存 / F10":L"戻る",RGB(235,242,240),DT_CENTER);
 menu_row(dc_,674,620,468,45,1,focus_==ids_.size()+1);text(674,620,468,45,L"取消 / A・Esc",RGB(235,242,240),DT_CENTER);
 text(138,686,1004,28,L"↑↓：スキル　←→：レベル　B：設定・解除　Tab：保存　F6：すべて解除",muted);
 if(focus_<ids_.size())menu_focus_guides(dc_,left,top+int(focus_%rows)*rowHeight);else menu_focus_guides(dc_,focus_==ids_.size()?138:674,620);
 SelectObject(dc_,old);finish_menu_surface(pixels_);
 for(int row=0;row<rows;++row){size_t at=page*rows+size_t(row);if(at>=ids_.size())break;if(const auto*icon=icons_.find(ids_[at]))weapons::paint_icon(*icon,{static_cast<uint32_t*>(pixels_),1280*720},1280,720,left+8,top+row*rowHeight+3,36,36);}
 return pixels_;
}
}
