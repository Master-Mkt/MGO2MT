#include "character_screen.h"
#include "menu_theme.h"
#include <algorithm>
namespace mgo2win {
namespace {
std::wstring weapon_name(const weapons::Entry&e){
 int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,e.display_name.data(),int(e.display_name.size()),nullptr,0);
 std::wstring s(size_t(std::max(n,0)),0);if(n)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,e.display_name.data(),int(e.display_name.size()),s.data(),n);return s;
}
const wchar_t* access_notice(weapons::Access a){switch(a){
 case weapons::Access::restricted:return L"この部屋では使用できません。";
 case weapons::Access::dp_disabled:return L"DPが有効な部屋で選択できます。";
 case weapons::Access::insufficient_dp:return L"選択した武器の合計に対してDPが足りません。";
 case weapons::Access::unverified:return L"選択に必要な武器設定・DP残高を確認できていません。";
 default:return L"武器を選択しました。";
}}
}
void CharacterScreen::weapon_catalog(const std::filesystem::path&path){
 auto catalog=std::make_shared<weapons::Catalog>();std::string error;
 if(catalog->load(path,error))weaponCatalog_=std::move(catalog);else weaponCatalog_.reset();
 weaponSelection_.reset();weaponRequest_.reset();
}
void CharacterScreen::update_weapons(){
 auto request=stage_load_request();
 if(!request){weaponsVisible_=false;weaponSelection_.reset();weaponRequest_.reset();return;}
 if(request!=weaponRequest_){weaponRequest_=request;weaponSelection_=weaponCatalog_?std::make_unique<weapons::Selection>(weaponCatalog_):nullptr;weaponCategory_=0;weaponFocus_=0;weaponNotice_.clear();}
 if(!weaponSelection_)return;
 std::optional<weapons::SelectionContext> context;
 if(detailReply_.detail&&detailReply_.detail->environment_known&&request->rotation.map==20&&request->rotation.rule==1&&request->rotation.flags==0){
  context=weapons::SelectionContext{};context->room_restrictions=detailReply_.detail->weapon_restrictions;
 }
 // A catalog's starting value must not masquerade as a received live balance.
 weaponSelection_->context(context);
}
void CharacterScreen::open_weapons(){
 if(detailBusy_||detailReply_.join_status!=RoomJoinStatus::joined)return;
 update_weapons();weaponsVisible_=true;matchVisible_=false;cues_.push_back(93);
}
bool CharacterScreen::weapon_message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 auto leave=[&]{weaponsVisible_=false;cues_.push_back(93);};
 auto rows=weaponSelection_?weaponSelection_->choices(weapons::Category(weaponCategory_)):std::vector<const weapons::Entry*>{};
 auto select=[&]{if(!weaponSelection_||weaponFocus_>=rows.size())return;auto result=weaponSelection_->choose(weapons::Category(weaponCategory_),rows[weaponFocus_]->id);weaponNotice_=access_notice(result);cues_.push_back(93);};
 auto confirm=[&]{if(!weaponSelection_||!weaponSelection_->complete()){weaponNotice_=L"3種類の武器を選び、使用条件を確認してください。";return;}weaponNotice_=L"武器の選択を保持しました。出撃はまだ利用できません。";cues_.push_back(93);};
 if(msg==WM_KEYDOWN){if(lp&(1LL<<30))return true;
  if(wp==VK_ESCAPE||wp==VK_F4)leave();else if(wp==VK_LEFT||wp==VK_RIGHT||wp==VK_TAB){weaponCategory_=(weaponCategory_+(wp==VK_LEFT?2:1))%3;weaponFocus_=0;weaponNotice_.clear();cues_.push_back(94);}
  else if(wp==VK_UP&&!rows.empty()){weaponFocus_=(weaponFocus_+rows.size()-1)%rows.size();cues_.push_back(94);}
  else if(wp==VK_DOWN&&!rows.empty()){weaponFocus_=(weaponFocus_+1)%rows.size();cues_.push_back(94);}
  else if(wp==VK_RETURN||wp==VK_SPACE)select();else if(wp==VK_F10)confirm();return true;
 }
 if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(!r.right||!r.bottom)return true;int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;
  if(y>=619&&y<663){if(x>=780&&x<1140)leave();else if(x>=390&&x<750)confirm();}
  else if(x>=140&&x<440&&y>=275&&y<440){weaponCategory_=std::min(2u,unsigned(y-275)/55);weaponFocus_=0;weaponNotice_.clear();cues_.push_back(94);}
  else if(x>=470&&x<1140&&y>=294&&y<494){auto i=(weaponFocus_/8)*8+size_t(y-294)/25;if(i<rows.size()){weaponFocus_=i;select();}}return true;
 }
 return msg==WM_CHAR||msg==WM_KEYUP||msg==WM_KILLFOCUS;
}
void CharacterScreen::draw_room_weapons(){
 auto text=[&](std::wstring_view s,int x,int y,int w,int h,int font,COLORREF c,UINT flags=DT_LEFT){RECT r{x,y,x+w,y+h};SelectObject(dc_,fonts_[font]);SetTextColor(dc_,menu_text_color(c));SetBkMode(dc_,TRANSPARENT);DrawTextW(dc_,s.data(),int(s.size()),&r,flags|DT_NOPREFIX);};
 auto light=RGB(237,231,218),orange=RGB(255,208,150),muted=RGB(155,159,157);
 menu_heading(dc_,fonts_[0],L"WEAPON SELECT");text(L"使用武器の選択",120,152,900,44,0,light);
 auto request=stage_load_request();bool dp=request&&request->rotation.flags==2;
 text(dp?L"DP ON / 残高の受信待ち":L"DP OFF",140,219,740,32,1,orange);
 if(!weaponSelection_){text(L"武器一覧を読み込めませんでした。",150,310,970,90,1,orange,DT_WORDBREAK);}
 else{
  constexpr const wchar_t* categories[]={L"メインウェポン",L"サブウェポン",L"サポートウェポン"};
  for(unsigned c=0;c<3;++c){int y=275+int(c)*55;menu_fill(dc_,140,y,300,48,c==weaponCategory_?RGB(151,168,126):RGB(50,66,52));text(categories[c],150,y+4,280,23,2,light);auto e=weaponSelection_->selected(weapons::Category(c));text(e?weapon_name(*e):L"未選択",150,y+26,280,21,3,e?orange:muted,DT_SINGLELINE|DT_END_ELLIPSIS);}
  auto rows=weaponSelection_->choices(weapons::Category(weaponCategory_));auto top=(weaponFocus_/8)*8;
  menu_section(dc_,fonts_[3],L"WEAPON / DP",470,264,670);
  for(size_t i=top;i<std::min(top+8,rows.size());++i){int y=294+int(i-top)*25;auto e=rows[i];bool allowed=weaponSelection_->access_to(*e)==weapons::Access::allowed;menu_row(dc_,470,y,670,25,i,i==weaponFocus_);text(weapon_name(*e),481,y+1,505,24,2,allowed?light:muted,DT_SINGLELINE|DT_END_ELLIPSIS);text(e->dp_cost?std::to_wstring(*e->dp_cost):L"—",986,y+1,138,24,2,allowed?orange:muted,DT_RIGHT);}
  auto quote=weaponSelection_->quote();text(L"合計DP  "+(quote.access==weapons::Access::unverified?std::wstring(L"—"):std::to_wstring(quote.cost)),470,500,670,30,2,orange,DT_RIGHT);
 }
 menu_description(dc_,fonts_[3],140,534,1000);text(weaponNotice_.empty()?L"左右で種類、上下で武器を選択。Enterで選択します。":weaponNotice_,140,558,1000,49,2,orange,DT_WORDBREAK);
 for(unsigned i=0;i<2;++i){int x=390+int(i)*390;menu_fill(dc_,x,619,360,44,RGB(50,66,52));text(i?L"参加者一覧へ戻る":L"選択を保持 [F10]",x,629,360,30,1,light,DT_CENTER);}
 text(L"← → / Tab：種類    ↑ ↓：武器    Enter：選択    F10：保持    Esc：戻る",120,690,1060,25,3,light);
 if(weaponSelection_&&!weaponSelection_->choices(weapons::Category(weaponCategory_)).empty())menu_focus_guides(dc_,470,294+int(weaponFocus_%8)*25);
}
}
