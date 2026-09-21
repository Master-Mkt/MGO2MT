#include "menu_audio.h"
#include "menu_theme.h"
#include "character_creation.h"
#include "unicode_character_name.h"
#include <algorithm>
#include <iostream>
#include <syncstream>
#include <imm.h>
namespace mgo2mt {
namespace {
constexpr unsigned ids[]={1,2,3,13,14,15,16,17,18,19};
constexpr unsigned colors[]={4,5,6,20,21,22,23,24,25,26};
constexpr unsigned kinds[]={100,200,300,600,700,400,800,500,900,550};
constexpr unsigned rows[]={6,3,7};
const wchar_t* labels[]={L"顔",L"上半身",L"下半身",L"頭",L"胸",L"手",L"腰",L"足",L"アクセサリー 1",L"アクセサリー 2"};
unsigned slot_for(unsigned tab,unsigned row){return tab==1?row:row+3;}
bool optional(unsigned slot){return slot==3||slot==4||slot==6||slot>=8;}
std::wstring type_label(unsigned slot,unsigned id){
 if(optional(slot)&&id==0)return L"なし";
 if(slot==5&&id==46)return L"素手";
 return L"タイプ "+std::to_wstring(id+1-(slot==1?11:slot==2?22:0));
}
}
CharacterCreation::CharacterCreation(const CharacterCatalog*c,bool enabled):catalog_(c),registrationEnabled_(enabled){appearance_[2]=11;appearance_[3]=22;appearance_[15]=46;appearance_[17]=57;normalize();}
std::vector<AppearanceRule> CharacterCreation::rules(unsigned slot)const{return catalog_?catalog_->creation_choices(appearance_[0],kinds[slot]):std::vector<AppearanceRule>{};}
std::vector<unsigned> CharacterCreation::choices(unsigned slot,bool color)const{
 std::vector<unsigned> out;
 if(optional(slot)&&(!color||appearance_[ids[slot]]==0))out.push_back(0);
 if(color&&optional(slot)&&appearance_[ids[slot]]==0)return out;
 for(const auto&r:rules(slot))if(!color||r.id==appearance_[ids[slot]])out.push_back(color?r.color:r.id);
 std::sort(out.begin(),out.end());out.erase(std::unique(out.begin(),out.end()),out.end());
 // Bare hands follow the selected face's skin texture in the original mapping.
 if(color&&slot==5&&appearance_[15]==46)return {unsigned(appearance_[1]/2)};
 return out;
}
void CharacterCreation::normalize(){
 for(unsigned i=0;i<10;++i){auto list=choices(i,false);if(!list.empty()&&std::find(list.begin(),list.end(),appearance_[ids[i]])==list.end())appearance_[ids[i]]=uint8_t(list.front());auto palette=choices(i,true);if(!palette.empty()&&std::find(palette.begin(),palette.end(),appearance_[colors[i]])==palette.end())appearance_[colors[i]]=uint8_t(palette.front());}
}
std::wstring CharacterCreation::name_error(std::wstring_view name,bool unicode){
 if(name.empty())return L"キャラクター名を入力してください。";
 if(unicode){
  if(name.size()>32)return L"キャラクター名は4〜16文字で入力してください。";
  const int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,name.data(),int(name.size()),nullptr,0,nullptr,nullptr);
  if(!bytes)return L"名前に使用できない文字が含まれています。";
  std::string utf8(bytes,'\0');
  if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,name.data(),int(name.size()),utf8.data(),bytes,nullptr,nullptr)!=bytes)return L"名前に使用できない文字が含まれています。";
  const auto valid=unicode_character_name::validate(utf8);
  switch(valid.error){
   case unicode_character_name::Error::none:return {};
   case unicode_character_name::Error::empty:return L"キャラクター名を入力してください。";
   case unicode_character_name::Error::codepoint_limit:
   case unicode_character_name::Error::byte_limit:return L"キャラクター名は4〜16文字で入力してください。";
   case unicode_character_name::Error::edge_space:return L"名前の先頭・末尾には空白を使えません。";
   case unicode_character_name::Error::reserved:return L"この名前は予約されています。別の名前を入力してください。";
   default:return L"この名前には使用できない文字が含まれています。";
  }
 }
 unsigned count=0;for(auto c:name)if(c<0xdc00||c>0xdfff)++count;
 if(count<4)return L"キャラクター名は4文字以上で入力してください。";
 if(name.front()==L' '||name.back()==L' '||name.front()==0x3000||name.back()==0x3000)return L"名前の先頭・末尾には空白を使えません。";
 auto n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,name.data(),int(name.size()),nullptr,0,nullptr,nullptr);
 if(!n)return L"名前に使用できない文字が含まれています。";
 if(n>16)return L"名前が長すぎます。半角16文字、日本語・ハングルは通常5文字までです。";
 for(auto c:name)if(c<32||(c>=127&&c<=159)||c==L'\\'||c==0x200b||c==0x200c||c==0x200d||(c>=0x2028&&c<=0x202e)||(c>=0x2066&&c<=0x2069)||c==0xfeff)return L"この名前には使用できない文字が含まれています。";
 std::wstring lower(name);std::transform(lower.begin(),lower.end(),lower.begin(),[](wchar_t c){return c>=L'A'&&c<=L'Z'?c+32:c;});
 if(lower==L"openmgo2"||name.starts_with(L":#")||name.starts_with(L"GM_")||name.starts_with(L"GM-")||name.starts_with(L"GM.")||name.starts_with(L"GM,"))return L"この名前は予約されています。別の名前を入力してください。";
 return {};
}
void CharacterCreation::change_tab(unsigned tab){if(tab_==tab)return;tab_=tab;row_=0;color_=false;notice_.clear();cues_.push_back(menu_audio::Cursor);}
void CharacterCreation::audition(){audition_=CharacterVoicePreview{appearance_[0],appearance_[7],pitch_};++auditions_;}
void CharacterCreation::step(int direction){
 if(row_>=rows[tab_])return;
 const auto previous=appearance_;const auto previousPitch=pitch_;
 if(tab_==0){if(row_==0||row_==5)return;if(row_==1){appearance_[0]^=1;normalize();}else if(row_==2){auto list=choices(0,false);if(list.empty())return;auto it=std::find(list.begin(),list.end(),appearance_[1]);auto at=it==list.end()?0:int(it-list.begin());appearance_[1]=uint8_t(list[(at+int(list.size())+direction)%list.size()]);normalize();}else if(row_==3)appearance_[7]=uint8_t((appearance_[7]+8+direction)%8);else{int next=std::clamp(pitch_+direction,-7,7);if(next==pitch_)return;pitch_=next;}}
 else{auto slot=slot_for(tab_,row_);auto list=choices(slot,color_);if(list.empty()){notice_=L"この項目の表示データはまだ用意されていません。";return;}auto field=color_?colors[slot]:ids[slot];auto it=std::find(list.begin(),list.end(),appearance_[field]);int at=it==list.end()?0:int(it-list.begin());appearance_[field]=uint8_t(list[(at+int(list.size())+direction)%list.size()]);normalize();}
 if(previous==appearance_&&previousPitch==pitch_)return;
 dirty_=true;++changes_;notice_.clear();cues_.push_back(menu_audio::Cursor);
 if(tab_==0&&(row_==3||row_==4))audition();
}
void CharacterCreation::cancel(){if(dirty_){discard_=true;yes_=false;}else closed_=true;cues_.push_back(menu_audio::Cancel);}
void CharacterCreation::activate(){
 if(discard_){const bool accepted=yes_;if(yes_)closed_=true;discard_=false;yes_=false;cues_.push_back(accepted?menu_audio::Confirm:menu_audio::Cancel);return;}
 if(confirm_){
  if(registrationEnabled_&&yes_){
   try{if(!catalog_)throw std::runtime_error("Missing creation catalog");
    for(unsigned i=0;i<10;++i){auto types=choices(i,false),palette=choices(i,true);if(std::find(types.begin(),types.end(),appearance_[ids[i]])==types.end()||std::find(palette.begin(),palette.end(),appearance_[colors[i]])==palette.end())throw std::runtime_error("Invalid creation choice");}
    registration_=character_create_request(name_,appearance_,pitch_,unicodeNames_);registrationBusy_=registrationRequested_=true;yes_=false;cues_.push_back(menu_audio::Confirm);
   }catch(...){registration_failed(L"選択内容を確認できませんでした。装備を選び直してください。");}
  }else{confirm_=false;yes_=false;cues_.push_back(menu_audio::Cancel);}return;
 }
 if(tab_==0&&row_==5){audition();return;}
 if(row_<rows[tab_]){step(1);return;}
 if(row_==rows[tab_]){notice_=name_error(name_,unicodeNames_);if(notice_.empty()){if(skillsRequired_&&!skillsConfigured_){skillRequested_=true;return;}confirm_=true;yes_=false;++confirmations_;report();cues_.push_back(menu_audio::Confirm);}else{tab_=0;row_=0;}}else cancel();
}
void CharacterCreation::append_name(wchar_t ch){
 if(ch>=0xd800&&ch<=0xdbff){pendingHigh_=ch;return;}
 if(ch>=0xdc00&&ch<=0xdfff){if(pendingHigh_&&name_.size()+2<=32){name_+=pendingHigh_;name_+=ch;dirty_=true;}pendingHigh_=0;return;}
 pendingHigh_=0;
 if(ch>=32&&ch!=127&&name_.size()<32){name_+=ch;dirty_=true;}
}
bool CharacterCreation::message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 if(registrationBusy_)return true;
 if(text_entry()&&msg==WM_IME_STARTCOMPOSITION){imeComposing_=true;imeEnterGuard_=true;pendingHigh_=0;}
 if(msg==WM_IME_ENDCOMPOSITION){imeComposing_=false;return false;}
 if(msg==WM_KEYUP&&wp==VK_RETURN){imeEnterGuard_=false;return false;}
 if(hwnd&&text_entry()&&(msg==WM_IME_STARTCOMPOSITION||msg==WM_IME_COMPOSITION)){
  if(auto im=ImmGetContext(hwnd)){RECT r{};GetClientRect(hwnd,&r);COMPOSITIONFORM form{};form.dwStyle=CFS_POINT;form.ptCurrentPos={346*r.right/1280,290*r.bottom/720};ImmSetCompositionWindow(im,&form);ImmReleaseContext(hwnd,im);}
 }
 if(msg==WM_KILLFOCUS||(msg==WM_ACTIVATEAPP&&!wp)){discard_=false;yes_=false;audition_.reset();imeComposing_=false;pendingHigh_=0;return false;}
 if(msg==WM_CHAR){if(text_entry()){
  if(wp==VK_BACK){if(pendingHigh_)pendingHigh_=0;else if(!name_.empty()){auto c=name_.back();name_.pop_back();if(c>=0xdc00&&c<=0xdfff&&!name_.empty()&&name_.back()>=0xd800&&name_.back()<=0xdbff)name_.pop_back();dirty_=true;}}
  else append_name(wchar_t(wp));notice_.clear();}return true;}
 if(msg==WM_KEYDOWN){
  if(text_entry()&&wp==VK_PROCESSKEY){imeEnterGuard_=true;return false;}
  if(imeComposing_)return true;
  if(wp==VK_RETURN&&imeEnterGuard_)return true;
  bool repeat=(lp&(1LL<<30))!=0;
  if(confirm_||discard_){if(wp==VK_ESCAPE&&!repeat){confirm_=discard_=false;yes_=false;cues_.push_back(menu_audio::Cancel);}else if((discard_||(confirm_&&registrationEnabled_))&&(wp==VK_LEFT||wp==VK_RIGHT||wp==VK_TAB)){const bool next=wp==VK_TAB?!yes_:wp==VK_LEFT;if(next!=yes_){yes_=next;cues_.push_back(menu_audio::Cursor);}}else if((wp==VK_RETURN||wp==VK_SPACE)&&!repeat)activate();return true;}
  if(wp>=VK_F1&&wp<=VK_F3){change_tab(unsigned(wp-VK_F1));return true;}
  if(wp==VK_F4){if(!repeat)audition();return true;}
  if(wp==VK_ESCAPE&&!repeat){cancel();return true;}
  if(wp==VK_UP||wp==VK_DOWN||wp==VK_TAB){int d=wp==VK_UP||(wp==VK_TAB&&(GetKeyState(VK_SHIFT)&0x8000))?-1:1;row_=(row_+rows[tab_]+2+d)%(rows[tab_]+2);color_=false;notice_.clear();cues_.push_back(menu_audio::Cursor);return true;}
  if(wp==VK_HOME||wp==VK_END){const auto next=wp==VK_HOME?0:rows[tab_]+1;if(row_!=next||color_){row_=next;color_=false;cues_.push_back(menu_audio::Cursor);}return true;}
  if(wp==VK_LEFT||wp==VK_RIGHT){step(wp==VK_LEFT?-1:1);return true;}
  if(wp==VK_SPACE&&tab_>0&&row_<rows[tab_]&&!repeat){color_=!color_;cues_.push_back(menu_audio::Cursor);return true;}
  if(wp==VK_RETURN&&!repeat){if(text_entry()){row_=1;cues_.push_back(menu_audio::Confirm);}else activate();return true;}
  if(wp==VK_DELETE&&text_entry()){name_.clear();pendingHigh_=0;dirty_=true;return true;}
  if(wp=='V'&&text_entry()&&(GetKeyState(VK_CONTROL)&0x8000)){
   if(OpenClipboard(hwnd)){auto h=GetClipboardData(CF_UNICODETEXT);if(h){auto*p=static_cast<const wchar_t*>(GlobalLock(h));if(p){auto n=GlobalSize(h)/sizeof(wchar_t);for(size_t i=0;i<n&&p[i]&&name_.size()<32;++i)append_name(p[i]);pendingHigh_=0;GlobalUnlock(h);dirty_=true;}}CloseClipboard();}return true;
  }
  return true;
 }
 if(msg==WM_LBUTTONUP){RECT rect{};GetClientRect(hwnd,&rect);if(!rect.right||!rect.bottom)return true;int x=int(short(LOWORD(lp)))*1280/rect.right,y=int(short(HIWORD(lp)))*720/rect.bottom;
  if(discard_){if(y>=409&&y<463){if(x>=390&&x<610){yes_=true;activate();}else if(x>=670&&x<890){yes_=false;activate();}}return true;}
  if(confirm_){if(y>=590&&y<640){if(registrationEnabled_){if(x>=320&&x<590){yes_=true;activate();}else if(x>=690&&x<960){yes_=false;activate();}}else if(x>=430&&x<850){confirm_=false;cues_.push_back(menu_audio::Cancel);}}return true;}
  if(y>=205&&y<244&&x>=120&&x<690){change_tab(unsigned((x-120)/190));return true;}
  if(y>=270&&y<270+int(rows[tab_])*37&&x>=120&&x<690){const auto previous=row_;const bool previousColor=color_;const auto before=cues_.size();row_=unsigned((y-270)/37);color_=tab_>0&&x>=500;if(tab_==0&&row_==5)audition();else if(x>=340)step(x<(color_?580:415)?-1:1);if(cues_.size()==before&&(previous!=row_||previousColor!=color_))cues_.push_back(menu_audio::Cursor);SetFocus(hwnd);return true;}
  if(y>=617&&y<662){if(x>=120&&x<480){row_=rows[tab_];activate();}else if(x>=510&&x<690){row_=rows[tab_]+1;cancel();}}SetFocus(hwnd);return true;
 }
 return false;
}
void CharacterCreation::draw(HDC dc,std::span<const HFONT>fonts)const{
 auto fill=[&](int x,int y,int w,int h,COLORREF c){menu_fill(dc,x,y,w,h,c);};
 auto text=[&](std::wstring_view s,int x,int y,int w,int h,int font,COLORREF c,UINT flags=DT_LEFT){RECT r{x,y,x+w,y+h};SelectObject(dc,fonts[font]);SetTextColor(dc,menu_text_color(c));SetBkMode(dc,TRANSPARENT);DrawTextW(dc,s.data(),int(s.size()),&r,flags|DT_NOPREFIX);};
 auto light=RGB(230,238,216),muted=RGB(178,194,164),selected=RGB(80,103,63);
 menu_heading(dc,fonts[0],L"PC EDIT");menu_character_frame(dc,fonts[3]);
 text(L"PCを新規登録",120,153,600,43,0,light);text(L"OpenMGO2",850,82,310,30,1,muted,DT_RIGHT);
 const wchar_t*tabs[]={L"基本設定 [F1]",L"服装 [F2]",L"装備 [F3]"};
 for(unsigned i=0;i<3;++i){menu_tab(dc,120+i*190,205,185,37,tab_==i);text(tabs[i],120+i*190,213,185,29,2,light,DT_CENTER);}
 menu_section(dc,fonts[3],tab_==0?L"CHARACTER":tab_==1?L"CLOTHING":L"EQUIPMENT",120,244,570);
 for(unsigned r=0;r<rows[tab_];++r){int y=270+r*37;bool active=r==row_;menu_row(dc,120,y,570,34,r,active);
  if(tab_==0){const wchar_t*names[]={L"キャラクター名",L"性別",L"顔",L"音声",L"ピッチ",L"音声を試聴 [F4]"};text(names[r],132,y+9,200,27,2,light);
   auto value=r==0?(name_.empty()?L"名前を入力":name_):r==1?(appearance_[0]?L"女性":L"男性"):r==2?type_label(0,appearance_[1]):r==3?L"タイプ "+std::to_wstring(appearance_[7]+1):r==4?L"◀  "+std::wstring(pitch_>0?L"+":L"")+std::to_wstring(pitch_)+L"  ▶":L"再生";
   if(r==0&&unicodeNames_&&!name_.empty()){
    SelectObject(dc,fonts[2]);SIZE extent{};GetTextExtentPoint32W(dc,value.data(),int(value.size()),&extent);
    LOGFONTW lf{};HFONT fitted=nullptr;
    if(extent.cx>330&&GetObjectW(fonts[2],sizeof(lf),&lf)){
     lf.lfHeight=-std::max(8,int(std::abs(lf.lfHeight)*330/std::max(1L,extent.cx)));
     for(;;){
      fitted=CreateFontIndirectW(&lf);if(!fitted)break;
      auto oldFont=SelectObject(dc,fitted);GetTextExtentPoint32W(dc,value.data(),int(value.size()),&extent);SelectObject(dc,oldFont);
      if(extent.cx<=330||lf.lfHeight>=-8)break;
      DeleteObject(fitted);fitted=nullptr;++lf.lfHeight;
     }
    }
    auto prior=SelectObject(dc,fitted?fitted:fonts[2]);RECT rect{346,y+8,676,y+38};SetTextColor(dc,menu_text_color(light));SetBkMode(dc,TRANSPARENT);
    DrawTextW(dc,value.data(),int(value.size()),&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    SelectObject(dc,prior);if(fitted)DeleteObject(fitted);
   }else text(value,346,y+8,330,30,2,r==0&&name_.empty()?muted:light,DT_CENTER);
  }else{auto slot=slot_for(tab_,r);text(labels[slot],132,y+9,185,27,2,light);auto types=choices(slot,false);auto palette=choices(slot,true);
   text(types.empty()?L"準備中":type_label(slot,appearance_[ids[slot]]),326,y+9,170,27,2,active&&!color_?RGB(255,235,173):light,DT_CENTER);
   bool skin=slot==5&&appearance_[15]==46;auto color=skin?L"肌に合わせる":optional(slot)&&appearance_[ids[slot]]==0?L"—":L"カラー "+std::to_wstring(appearance_[colors[slot]]+1);
   text(color,501,y+9,182,27,2,active&&color_?RGB(255,235,173):muted,DT_CENTER);
  }
 }

 text(L"PgUp / PgDn：モデル回転",720,595,440,27,3,muted,DT_CENTER);
 menu_description(dc,fonts[3],120,560,570);
 text(tab_==0?L"F4：短い声を試聴　ピッチ：−7〜＋7（0が標準）":color_?L"左右：色を変更　Space：種類／色を切替":L"左右：種類を変更　Space：種類／色を切替",120,534,570,26,3,muted);
 text(notice_.empty()?L"選択内容を確認してから登録へ進みます。":notice_,120,585,570,30,3,RGB(237,221,181));
 for(unsigned i=0;i<2;++i){int x=i?510:120,w=i?180:360;fill(x,617,w,45,row_==rows[tab_]+i?RGB(151,168,126):RGB(50,66,52));text(i?L"戻る":skillsRequired_&&!skillsConfigured_?L"スキル設定へ":L"内容を確認",x,628,w,30,1,row_==rows[tab_]+i?RGB(18,28,19):light,DT_CENTER);}
 text(skillsRequired_?L"↑ ↓：項目　← →：変更　F1〜F3：タブ　F6：スキル　Enter：決定　Esc：戻る":L"↑ ↓ / Tab：項目移動　← →：変更　F1〜F3：タブ　Enter：決定　Esc：戻る",83,690,1120,25,3,muted);
 if(confirm_){fill(170,180,940,480,RGB(21,33,25));menu_section(dc,fonts[3],L"CONFIRM",172,182,936);text(L"作成内容の確認",205,204,860,45,0,light,DT_CENTER);text(name_,220,268,820,43,0,light,DT_CENTER);
  text(std::wstring(appearance_[0]?L"女性":L"男性")+L"　／　顔 "+std::to_wstring(appearance_[1]+1)+L"　／　音声 "+std::to_wstring(appearance_[7]+1)+L"　／　ピッチ "+(pitch_>0?L"+":L"")+std::to_wstring(pitch_),220,325,820,34,1,light,DT_CENTER);
  text(L"服装・装備は選択したプレビューの内容です。",220,377,820,38,2,muted,DT_CENTER);
  text(registrationBusy_?L"OpenMGO2にPCを登録しています…":registrationEnabled_?L"この内容でPCを登録しますか？\nYESでOpenMGO2へ送信します。":L"この画面では登録できません。\n前回の登録結果が不明な場合は一覧を確認してください。",220,438,820,75,1,RGB(237,221,181),DT_CENTER|DT_WORDBREAK);
  if(!registrationBusy_){if(registrationEnabled_){for(int i=0;i<2;++i){int x=i?690:320;bool active=yes_==!i;fill(x,590,270,47,active?RGB(151,168,126):RGB(50,66,52));text(i?L"NO / 編集に戻る":L"YES / 登録",x,600,270,32,1,active?RGB(18,28,19):light,DT_CENTER);}}else{fill(430,590,420,47,selected);text(L"編集に戻る",430,600,420,32,1,light,DT_CENTER);}}
 }
 if(discard_){fill(300,275,680,220,RGB(21,33,25));menu_section(dc,fonts[3],L"CONFIRM",302,277,676);text(L"作成内容を破棄して戻りますか？",320,305,640,45,1,light,DT_CENTER);
  for(int i=0;i<2;++i){int x=i?670:390;fill(x,409,220,54,yes_==!i?RGB(151,168,126):RGB(50,66,52));text(i?L"NO":L"YES",x,423,220,34,1,yes_==!i?RGB(18,28,19):light,DT_CENTER);}}
 if(discard_)menu_focus_guides(dc,yes_?390:670,409);
 else if(confirm_){if(!registrationBusy_)menu_focus_guides(dc,registrationEnabled_?(yes_?320:690):430,590);}
 else if(row_<rows[tab_])menu_focus_guides(dc,120,270+int(row_)*37);
 else menu_focus_guides(dc,row_==rows[tab_]?120:510,617);
}
void CharacterCreation::report()const{std::osyncstream(std::cout)<<"{\"character_creation\":true,\"tab\":"<<tab_<<",\"gender\":"<<unsigned(appearance_[0])<<",\"voice\":"<<unsigned(appearance_[7])<<",\"pitch\":"<<pitch_<<",\"auditions\":"<<auditions_<<",\"changes\":"<<changes_<<",\"confirmations\":"<<confirmations_<<",\"name_valid\":"<<(name_error(name_,unicodeNames_).empty()?"true":"false")<<",\"registration_requested\":"<<(registrationRequested_?"true":"false")<<"}"<<std::endl;}
}
