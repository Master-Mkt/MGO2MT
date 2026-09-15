#include "menu_audio.h"
#include "character_screen.h"
#include "menu_theme.h"
#include "native_loadout.h"
#include <algorithm>
namespace mgo2win {
namespace {
constexpr int gridX=140,gridY=192,cardW=194,cardH=80,stepX=202,stepY=86;
constexpr size_t columns=5,pageSize=20;
constexpr const wchar_t* categories[]={L"PRIMARY",L"SECONDARY",L"SUPPORT"};
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
 weaponIcons_.load(path.parent_path()/"weapon-icons/index.tsv",error);
 weaponSelection_.reset();weaponRequest_.reset();
}
void CharacterScreen::update_weapons(){
 auto request=stage_load_request();
 if(!request){weaponsVisible_=false;weaponMusicRequest_=false;weaponSelection_.reset();weaponRequest_.reset();return;}
 if(request!=weaponRequest_){weaponRequest_=request;weaponSelection_=weaponCatalog_?std::make_unique<weapons::Selection>(weaponCatalog_):nullptr;weaponCategory_=0;weaponFocus_=0;weaponSection_=0;weaponMusicRequest_=false;weaponNotice_.clear();}
 if(!weaponSelection_)return;
 std::optional<weapons::SelectionContext> context;
 if(detailReply_.preparation){const auto&p=*detailReply_.preparation;context=weapons::SelectionContext{p.dpEnabled,p.dpBalance,p.restrictions};}
 else if(detailReply_.detail&&detailReply_.detail->environment_known&&request->rotation.map==20&&request->rotation.rule==1&&request->rotation.flags==0){
  context=weapons::SelectionContext{};context->room_restrictions=detailReply_.detail->weapon_restrictions;
 }
 // A catalog's starting value must not masquerade as a received live balance.
 if(context&&detailReply_.preparation)context->native_operator_grant=supported_weapon(3);
 weaponSelection_->context(context);
 for(unsigned c=0;c<3;++c)if(!weaponSelection_->selected(weapons::Category(c))&&supported_weapon(weapons::native_loadout::initial[c]))weaponSelection_->choose(weapons::Category(c),weapons::native_loadout::initial[c]);
}
void CharacterScreen::open_weapons(){
 if(detailBusy_||detailReply_.join_status!=RoomJoinStatus::joined)return;
 if(detailReply_.preparation&&detailReply_.preparation->phase==combat::wire::RoundPhase::ended){detailNotice_=L"時間終了。次のラウンドを準備しています。";return;}
 update_weapons();weaponsVisible_=true;weaponSection_=0;weaponMusicRequest_=false;matchVisible_=false;cues_.push_back(menu_audio::Confirm);
}
bool CharacterScreen::weapon_message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 auto leave=[&]{weaponsVisible_=false;weaponMusicRequest_=false;cues_.push_back(menu_audio::Cancel);};
 auto music=[&]{if(!weapon_music_available()){weaponNotice_=L"BGMは出撃前に選択できます。";return;}if(!weaponMusicReady_){weaponNotice_=L"BGM一覧を読み込んでいます…";return;}weaponSection_=1;weaponMusicRequest_=true;weaponNotice_.clear();cues_.push_back(menu_audio::Confirm);};
 auto rows=weaponSelection_?weaponSelection_->choices(weapons::Category(weaponCategory_)):std::vector<const weapons::Entry*>{};
 auto select=[&]{if(!weaponSelection_||weaponFocus_>=rows.size())return;if(!supported_weapon(rows[weaponFocus_]->id)){weaponNotice_=L"この武器はホストの動作対応待ちです。";return;}auto result=weaponSelection_->choose(weapons::Category(weaponCategory_),rows[weaponFocus_]->id);weaponNotice_=access_notice(result);if(result==weapons::Access::allowed)cues_.push_back(menu_audio::Confirm);};
 auto confirm=[&]{
  if(!detailReply_.preparation||!detailReply_.preparation->runtimeReady){weaponNotice_=L"ホストの武器・出撃データの準備を待っています。";return;}
  if(loadoutPending_){weaponNotice_=L"出撃の承認を待っています…";return;}
  const auto&p=*detailReply_.preparation;if(p.phase==combat::wire::RoundPhase::waiting){weaponNotice_=L"参加者一覧で出撃OKにするか、カウントダウンの終了を待ってください。";return;}
  combat::wire::Command command;command.kind=combat::wire::CommandKind::loadout;
  for(unsigned c=0;c<3;++c)if(p.requiredCategories&(1u<<c)){
   auto e=weaponSelection_?weaponSelection_->selected(weapons::Category(c)):nullptr;
   if(!e||!supported_weapon(e->id)){weaponNotice_=L"対応している種類の武器を選択してください。";return;}command.weapons[c]=e->id;
  }
  if(round_command(command)){loadoutPending_=combatCommandSequence_;weaponNotice_=L"ホストに出撃を申請しています…";cues_.push_back(menu_audio::Confirm);}
 };
 if(msg==WM_KEYDOWN){if(lp&(1LL<<30))return true;
  if(wp==VK_ESCAPE||wp==VK_F4)leave();else if(wp==VK_F8)music();else if(wp==VK_F1||wp==VK_F2||wp==VK_TAB){bool previous=wp==VK_F1||(wp==VK_TAB&&(GetKeyState(VK_SHIFT)&0x8000));weaponCategory_=(weaponCategory_+(previous?2:1))%3;weaponFocus_=0;weaponSection_=0;weaponNotice_.clear();cues_.push_back(menu_audio::Cursor);}
  else if(wp==VK_HOME||wp==VK_END){const auto next=wp==VK_HOME?0u:2u;if(weaponSection_!=next||(next==0&&weaponFocus_!=0)){weaponSection_=next;if(!next)weaponFocus_=0;cues_.push_back(menu_audio::Cursor);}}
  else if(weaponSection_&&(wp==VK_UP||wp==VK_DOWN||wp==VK_LEFT||wp==VK_RIGHT)){
   const auto before=weaponSection_;
   if(wp==VK_UP)weaponSection_=weaponSection_==1?0:1;
   else if(wp==VK_DOWN)weaponSection_=weaponSection_==1?2:0;
   else if(weaponSection_>=2)weaponSection_=weaponSection_==2?3:2;
   if(before!=weaponSection_){weaponNotice_.clear();cues_.push_back(menu_audio::Cursor);}
  }
  else if(rows.empty()&&wp==VK_DOWN){weaponSection_=1;cues_.push_back(menu_audio::Cursor);}
  else if(!rows.empty()&&(wp==VK_UP||wp==VK_DOWN||wp==VK_LEFT||wp==VK_RIGHT||wp==VK_PRIOR||wp==VK_NEXT)){
   auto last=rows.size()-1;const auto before=weaponFocus_;const auto previousSection=weaponSection_;
   if(wp==VK_LEFT)weaponFocus_=(weaponFocus_+last)%rows.size();
   else if(wp==VK_RIGHT)weaponFocus_=(weaponFocus_+1)%rows.size();
   else if(wp==VK_UP)weaponFocus_=weaponFocus_>=columns?weaponFocus_-columns:std::min((last/columns)*columns+weaponFocus_%columns,last);
   else if(wp==VK_DOWN){if(weaponFocus_/columns<last/columns)weaponFocus_=std::min(weaponFocus_+columns,last);else weaponSection_=1;}
   else if(wp==VK_PRIOR)weaponFocus_=weaponFocus_>=pageSize?weaponFocus_-pageSize:0;
   else weaponFocus_=std::min(weaponFocus_+pageSize,last);
   weaponNotice_.clear();if(before!=weaponFocus_||previousSection!=weaponSection_)cues_.push_back(menu_audio::Cursor);
  }
  else if(wp==VK_RETURN||wp==VK_SPACE){if(weaponSection_==1)music();else if(weaponSection_==2)confirm();else if(weaponSection_==3)leave();else select();}else if(wp==VK_F10||wp==VK_F9)confirm();return true;
 }
 if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(!r.right||!r.bottom)return true;int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;
  if(y>=647&&y<687){if(x>=780&&x<1140){weaponSection_=3;leave();}else if(x>=390&&x<750){weaponSection_=2;confirm();}}
  else if(x>=140&&x<1144&&y>=582&&y<612)music();
  else if(x>=140&&x<998&&y>=125&&y<167){const auto next=std::min(2u,unsigned(x-140)/286);if(next!=weaponCategory_||weaponSection_!=0){weaponCategory_=next;weaponFocus_=0;weaponSection_=0;weaponNotice_.clear();cues_.push_back(menu_audio::Cursor);}}
  else if(x>=gridX&&x<gridX+stepX*5&&y>=gridY&&y<gridY+stepY*4){int dx=x-gridX,dy=y-gridY;auto i=(weaponFocus_/pageSize)*pageSize+size_t(dy/stepY)*columns+dx/stepX;if(dx%stepX<cardW&&dy%stepY<cardH&&i<rows.size()){weaponFocus_=i;weaponSection_=0;select();}}return true;
 }
 return msg==WM_CHAR||msg==WM_KEYUP||msg==WM_KILLFOCUS;
}
void CharacterScreen::draw_room_weapons(){
 auto text=[&](std::wstring_view s,int x,int y,int w,int h,int font,COLORREF c,UINT flags=DT_LEFT){RECT r{x,y,x+w,y+h};SelectObject(dc_,fonts_[font]);SetTextColor(dc_,menu_text_color(c));SetBkMode(dc_,TRANSPARENT);DrawTextW(dc_,s.data(),int(s.size()),&r,flags|DT_NOPREFIX);};
 auto light=RGB(237,231,218),orange=RGB(255,208,150),muted=RGB(155,159,157);
 menu_heading(dc_,fonts_[0],L"WEAPON SELECT");
 auto request=stage_load_request();bool dp=request&&request->rotation.flags==2;
 text(detailReply_.preparation?(detailReply_.preparation->dpEnabled?L"DP "+std::to_wstring(detailReply_.preparation->dpBalance):std::wstring(L"DP OFF")):(dp?L"DP 待機中":L"DP OFF"),1004,134,142,30,2,orange,DT_RIGHT);
 for(unsigned c=0;c<3;++c){int x=140+int(c)*286;menu_tab(dc_,x,125,278,42,c==weaponCategory_);text(categories[c],x+9,135,260,25,1,c==weaponCategory_?light:muted,DT_CENTER);}
 if(!weaponSelection_){text(L"武器一覧を読み込めませんでした。",150,310,970,90,1,orange,DT_WORDBREAK);}
 else{
  auto rows=weaponSelection_->choices(weapons::Category(weaponCategory_));auto top=(weaponFocus_/pageSize)*pageSize;auto selected=weaponSelection_->selected(weapons::Category(weaponCategory_));
  text(L"使用武器の選択",140,169,700,22,3,muted);text(std::to_wstring(top/pageSize+1)+L" / "+std::to_wstring(std::max(size_t(1),(rows.size()+pageSize-1)/pageSize)),1040,169,105,22,3,muted,DT_RIGHT);
  for(size_t i=top;i<std::min(top+pageSize,rows.size());++i){
   int x=gridX+int((i-top)%columns)*stepX,y=gridY+int((i-top)/columns)*stepY;auto e=rows[i];bool allowed=supported_weapon(e->id)&&weaponSelection_->access_to(*e)==weapons::Access::allowed;
   menu_row(dc_,x,y,cardW,cardH,(i-top)/columns,weaponSection_==0&&i==weaponFocus_);
   if(!weaponIcons_.find(e->id))text(e->id?L"NO IMAGE":L"NONE",x+8,y+11,cardW-16,31,1,muted,DT_CENTER);
   menu_rect(dc_,x+1,y+53,cardW-2,26,RGB(35,43,43));
   text(weapon_name(*e),x+8,y+56,132,23,3,allowed?light:muted,DT_SINGLELINE|DT_END_ELLIPSIS);
   text(e->dp_cost?std::to_wstring(*e->dp_cost):L"—",x+139,y+56,48,23,3,allowed?orange:muted,DT_RIGHT);
   if(selected&&selected->id==e->id){menu_rect(dc_,x+2,y+2,3,cardH-4,RGB(255,208,150));text(L"✓",x+cardW-23,y+2,21,22,3,light,DT_CENTER);}
  }
  for(unsigned c=0;c<3;++c){int x=140+int(c)*338;menu_band(dc_,x,537,324,39,c);auto e=weaponSelection_->selected(weapons::Category(c));text(categories[c],x+9,538,306,18,3,muted);text(e?weapon_name(*e):L"未選択",x+9,555,306,21,2,e?orange:muted,DT_SINGLELINE|DT_END_ELLIPSIS);}
  auto quote=weaponSelection_->quote();text(L"合計DP  "+(quote.access==weapons::Access::unverified?std::wstring(L"—"):std::to_wstring(quote.cost)),958,618,188,23,3,orange,DT_RIGHT);
  if(weaponNotice_.empty()&&weaponFocus_<rows.size()){
   auto e=rows[weaponFocus_];auto access=weaponSelection_->access_to(*e);
   text(weaponSection_==1?L"出撃中に再生するBGMを選択します。":!supported_weapon(e->id)?L"この武器はホストの動作対応待ちです。":access!=weapons::Access::allowed?access_notice(access):L"決定で武器を選択します。選択が終わったら出撃してください。",140,618,800,25,3,orange,DT_SINGLELINE|DT_END_ELLIPSIS);
  }
 }
 menu_row(dc_,140,582,1004,30,3,weaponSection_==1);
 text(L"MUSIC  [F8]",151,586,210,24,2,light);
 text(!weaponMusicReady_?L"BGM一覧を読み込み中…":weaponMusicTitle_.empty()?L"未選択":weaponMusicTitle_,360,586,750,24,2,weaponMusicReady_?orange:muted,DT_SINGLELINE|DT_END_ELLIPSIS);
 text(L"▶",1110,586,26,24,2,weapon_music_available()?light:muted,DT_CENTER);
 if(!weaponNotice_.empty())text(weaponNotice_,140,618,800,25,3,orange,DT_SINGLELINE|DT_END_ELLIPSIS);
 for(unsigned i=0;i<2;++i){int x=390+int(i)*390;menu_row(dc_,x,647,360,40,i,weaponSection_==i+2);text(i?L"参加者一覧へ戻る":L"出撃する [F10 / START]",x,653,360,30,1,light,DT_CENTER);}
 text(L"方向キー：武器・MUSIC・出撃    LB / RB：種類    B：決定    START：出撃    A：戻る",120,693,1060,25,3,light);
 if(weaponSection_==1)menu_focus_guides(dc_,140,582);
 else if(weaponSection_>=2)menu_focus_guides(dc_,390+int(weaponSection_-2)*390,647);
 else if(weaponSelection_&&!weaponSelection_->choices(weapons::Category(weaponCategory_)).empty())menu_focus_guides(dc_,gridX+int(weaponFocus_%columns)*stepX,gridY+int(weaponFocus_%pageSize/columns)*stepY);
}
void CharacterScreen::draw_weapon_icons(){
 if(!weaponSelection_)return;auto rows=weaponSelection_->choices(weapons::Category(weaponCategory_));auto top=(weaponFocus_/pageSize)*pageSize;
 for(size_t i=top;i<std::min(top+pageSize,rows.size());++i)if(auto icon=weaponIcons_.find(rows[i]->id)){
  bool allowed=supported_weapon(rows[i]->id)&&weaponSelection_->access_to(*rows[i])==weapons::Access::allowed;
  weapons::paint_icon(*icon,{static_cast<uint32_t*>(pixels_),1280*720},1280,720,gridX+int((i-top)%columns)*stepX+12,gridY+int((i-top)/columns)*stepY+5,170,46,!allowed);
 }
}
}
