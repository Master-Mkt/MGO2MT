#include "chat_view.h"
#include "combat_standings.h"
#include "menu_audio.h"
#include "native_loadout.h"
#include "character_screen.h"
#include "menu_theme.h"
#include "name_text_fit.h"
#include <algorithm>
namespace mgo2mt {
namespace {void fitted_name(HDC dc,HFONT font,std::wstring_view name,RECT rect,COLORREF color){NameTextFit fit(dc,font,name,rect.right-rect.left);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);DrawTextW(dc,name.data(),int(name.size()),&rect,DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);}}
bool CharacterScreen::room_loading()const{
 if(!detailVisible_)return false;
 const auto status=detailReply_.join_status;
 return (detailBusy_&&detailAction_.event==RoomEvent::join&&status!=RoomJoinStatus::joined)||
  status==RoomJoinStatus::host_connecting||status==RoomJoinStatus::host_profile||status==RoomJoinStatus::host_sync||
  (status==RoomJoinStatus::joined&&(stageStatus_==stage::Status::idle||stageStatus_==stage::Status::loading));
}
void CharacterScreen::draw_room_loading(){
 // Near-black is opaque through the existing black color-key UI compositor.
 menu_rect(dc_,0,0,1280,720,RGB(1,1,1));
 SelectObject(dc_,fonts_[1]);SetTextColor(dc_,RGB(220,214,198));SetBkMode(dc_,TRANSPARENT);
 RECT logo{730,635,1230,678};DrawTextW(dc_,L"METAL GEAR ONLINE",-1,&logo,DT_RIGHT|DT_SINGLELINE|DT_NOPREFIX);
 SelectObject(dc_,fonts_[3]);RECT label{730,681,1230,707};
 const auto dots=std::wstring(1+(clock_()/400)%3,L'.');const auto text=L"NOW LOADING"+dots;
 DrawTextW(dc_,text.data(),int(text.size()),&label,DT_RIGHT|DT_SINGLELINE|DT_NOPREFIX);
}
void CharacterScreen::toggle_gameplay_briefing(){
 if(detailBusy_||detailReply_.join_status!=RoomJoinStatus::joined||room_loading())return;
 if(briefingPanel_!=briefing::Panel::none){briefingPanel_=briefing::Panel::none;briefingYes_=false;cues_.push_back(menu_audio::Cancel);return;}
 if(matchVisible_){matchVisible_=false;briefingFocus_=0;cues_.push_back(menu_audio::Confirm);}
 else if(combatEntered_){matchVisible_=true;cues_.push_back(menu_audio::Cancel);}
}
std::wstring CharacterScreen::player_display_name(uint32_t id,std::wstring_view fallback)const{
 const auto name=roomRequests_.nameDirectory->display(id,{});if(!name.empty())return hud::utf8(name);
 if(id&&id==selectionReply_.character.id)return selectionReply_.character.name;
 return std::wstring(fallback);
}
void CharacterScreen::open_room_detail(){
 if(roomReply_.status!=RoomStatus::ready||roomFocus_>=roomReply_.rooms.size())return;
 if(*roomRequests_.uncertain){roomNotice_=L"参加結果が不明です。ログインし直してください。";return;}
 detailAction_={};detailAction_.id=roomReply_.rooms[roomFocus_].id;detailReply_={};detailNotice_.clear();detailFocus_=1;briefingFocus_=0;briefingPanel_=briefing::Panel::none;matchVisible_=false;update_weapons();
 if(!roomRequests_.submit(detailAction_))return;detailBusy_=true;detailVisible_=true;cues_.push_back(menu_audio::Confirm);
}
void CharacterScreen::request_room_join(){
 if(detailBusy_||!detailReply_.detail||roomReply_.status!=RoomStatus::ready||*roomRequests_.uncertain)return;
 if(detailReply_.join_status==RoomJoinStatus::joined){matchVisible_=!matchVisible_;cues_.push_back(matchVisible_?menu_audio::Confirm:menu_audio::Cancel);return;}
 if(detailReply_.join_status==RoomJoinStatus::permission_checked)return;
 if(detailReply_.detail->players>=detailReply_.detail->capacity){detailNotice_=L"満員のため参加できません。";return;}
 detailAction_.event=RoomEvent::join;detailAction_.subtype=detailReply_.detail->subtype;
 if(roomRequests_.submit(detailAction_)){detailBusy_=true;stageStatus_=stage::Status::idle;detailNotice_.clear();cues_.push_back(menu_audio::Confirm);}
 SecureZeroMemory(detailAction_.password.data(),sizeof(detailAction_.password));
}
bool CharacterScreen::detail_message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 if(weaponsVisible_)return weapon_message(hwnd,msg,wp,lp);
 if(msg==WM_KEYDOWN&&!(lp&(1LL<<30))&&detailReply_.join_status==RoomJoinStatus::joined&&matchVisible_&&(wp==VK_F9||wp==VK_ESCAPE)){toggle_gameplay_briefing();return true;}
 if(room_loading()&&detailReply_.join_status==RoomJoinStatus::joined)return true;
 // Gameplay actions are polled separately, never activate hidden room buttons.
 if(matchVisible_&&detailReply_.join_status==RoomJoinStatus::joined)return true;
 const bool briefing=detailReply_.join_status==RoomJoinStatus::joined&&!matchVisible_&&!detailBusy_;
 if(briefing&&briefing_message(hwnd,msg,wp,lp))return true;
 if(msg==WM_KEYDOWN&&!(lp&(1LL<<30))&&detailReply_.join_status==RoomJoinStatus::joined){if(wp==VK_F9){if(combatEntered_)toggle_gameplay_briefing();else round_ready();return true;}if(wp==VK_F7){round_team();return true;}}
 if(msg==WM_LBUTTONUP&&detailReply_.preparation){RECT r{};GetClientRect(hwnd,&r);if(r.right&&r.bottom){int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;if(x>=140&&x<760&&y>=475&&y<507){round_ready();return true;}}}
 if(msg==WM_KEYDOWN&&wp==VK_F4&&!(lp&(1LL<<30))&&detailReply_.join_status==RoomJoinStatus::joined){open_weapons();return true;}
 auto close=[&]{if(detailBusy_||detailReply_.join_status==RoomJoinStatus::joined){if(detailAction_.event==RoomEvent::join){if(!roomRequests_.cancel_join.exchange(true))cues_.push_back(menu_audio::Cancel);detailBusy_=true;detailNotice_=L"接続を中止し、参加解除を確認しています…";}return;}detailVisible_=false;SecureZeroMemory(detailAction_.password.data(),sizeof(detailAction_.password));cues_.push_back(menu_audio::Cancel);};
 auto activate=[&]{if(detailFocus_==2)close();else if(detailFocus_==1)request_room_join();};
 if(msg==WM_KILLFOCUS){SecureZeroMemory(detailAction_.password.data(),sizeof(detailAction_.password));return true;}
 if(detailBusy_){if(msg==WM_KEYDOWN&&wp==VK_ESCAPE)close();if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(r.right&&r.bottom){int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;if(x>=780&&x<1140&&y>=619&&y<663)close();}}return true;}
 bool password=detailReply_.detail&&detailReply_.detail->password&&detailReply_.join_status!=RoomJoinStatus::joined;
 if(msg==WM_CHAR){if(detailFocus_==0&&password){auto&b=detailAction_.password;auto n=size_t(std::find(b.begin(),b.end(),0)-b.begin());if(wp==8){if(n)b[n-1]=0;}else if(wp>=32&&wp!=127&&n<16)b[n]=wchar_t(wp);}return true;}
 if(msg==WM_KEYUP)return true;
 if(msg==WM_KEYDOWN){if((lp&(1LL<<30))&&(wp==VK_RETURN||wp==VK_ESCAPE||wp==VK_SPACE))return true;
  if(wp==VK_ESCAPE)close();else if(wp==VK_TAB||wp==VK_DOWN||wp==VK_RIGHT){detailFocus_=(detailFocus_+1)%3;if(!password&&!detailFocus_)detailFocus_=1;cues_.push_back(menu_audio::Cursor);}
  else if(wp==VK_UP||wp==VK_LEFT){detailFocus_=(detailFocus_+2)%3;if(!password&&!detailFocus_)detailFocus_=2;cues_.push_back(menu_audio::Cursor);}
  else if(wp==VK_RETURN){if(detailFocus_==0){detailFocus_=1;cues_.push_back(menu_audio::Confirm);}else activate();}else if(wp==VK_SPACE&&detailFocus_!=0)activate();return true;
 }
 if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(!r.right||!r.bottom)return true;int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;
  if(password&&x>=270&&x<1110&&y>=478&&y<517){if(detailFocus_!=0)cues_.push_back(menu_audio::Cursor);detailFocus_=0;}
  else if(y>=619&&y<663){if(x>=780&&x<1140){detailFocus_=2;close();}else if(x>=390&&x<750){detailFocus_=1;request_room_join();}}SetFocus(hwnd);return true;
 }return false;
}
void CharacterScreen::draw_room_detail(){
 if(room_loading()){draw_room_loading();return;}
 if(weaponsVisible_){draw_room_weapons();return;}
 if(matchVisible_&&stageDebugNotice_.empty()&&!stageResetConfirm_&&detailReply_.combat_offer&&detailReply_.combat_state){
  const auto&offer=*detailReply_.combat_offer;const auto&state=*detailReply_.combat_state;
  const auto&self=state.players[offer.self.slot];
  if(state.epoch==offer.epoch&&self&&self->identity==offer.self){
   hud::Model model;model.name=selectionReply_.character.name;model.hp=self->hp;model.maxHp=self->maxHp;model.stamina=self->stamina;model.maxStamina=self->maxStamina;model.oxygen=self->oxygen;model.faceSubmerged=self->faceSubmerged;model.ammo=self->ammo;model.reserve=self->reserve;model.alive=self->alive;model.reloading=self->reloadUntil!=0;
   if(detailReply_.host_roster)for(const auto&p:detailReply_.host_roster->slots)if(p&&p->character==selectionReply_.character.id){model.name=player_display_name(p->character,hud::utf8(p->name));model.clan=hud::utf8(p->clan);break;}
   if(detailReply_.host_match&&detailReply_.host_match->request)model.rule=detailReply_.host_match->request->rotation.rule;
   if(detailReply_.preparation){model.ended=detailReply_.preparation->phase==combat::wire::RoundPhase::ended;model.respawnWaiting=detailReply_.preparation->respawnWaiting;model.respawnRemainingMs=detailReply_.preparation->respawnRemainingMs;model.dpKnown=detailReply_.preparation->dpEnabled;model.dp=detailReply_.preparation->dpBalance;if(detailReply_.preparation->roundClock)model.remainingMs=detailReply_.preparation->roundRemainingMs;}
   if(detailReply_.preparation){auto rows=combat::standings(*detailReply_.preparation);for(const auto&r:rows)if(r.id==offer.self){model.kills=r.kills;model.deaths=r.deaths;model.rank=r.rank;model.tied=std::count_if(rows.begin(),rows.end(),[&](const auto&x){return x.rank==r.rank;})>1;}}
   model.weapon=self->weapon?L"WEAPON "+std::to_wstring(self->weapon):L"装備なし";if(weaponCatalog_&&!weaponCatalog_->name(self->weapon).empty())model.weapon=hud::utf8(std::string(weaponCatalog_->name(self->weapon)));
   auto emblem=roomRequests_.clanEmblem->state();if(emblem.serial!=clanSerial_){clanBitmap_.reset();clanSerial_=emblem.serial;if(emblem.image)clanBitmap_=std::make_unique<clan::Bitmap>(*emblem.image);}drawClanImage_=bool(clanBitmap_);
   model.skills=skill_hud_labels();
   if(model.skills.empty())model.skills.push_back(L"登録済みスキルなし");
   else model.skills.insert(model.skills.begin(),L"登録済みスキル");
   const auto delivery=roomRequests_.inventorySession->state().delivery;
   if(delivery==items::Delivery::pending)model.actionNotice=L"装備の変更を確認しています…";
   else if(delivery==items::Delivery::unconfirmed)model.actionNotice=L"変更結果を確認できません。部屋へ入り直してください。";
   else if(weapons::native_loadout::held_only(self->weapon))model.actionNotice=L"この武器は所持・切替に対応しています。使用動作は準備中です。";
   if(self->mountedId)if(auto request=stage_load_request())if(auto*i=mountedCatalog_.find(request->rotation.map,self->mountedId))if(auto*t=mountedCatalog_.find(i->type)){model.infiniteAmmo=t->infiniteAmmo;model.actionNotice=L"設置重火器 / Yで降りる";}
   if(self->specialPc.kind==special_pc::Kind::gekko){model.infiniteAmmo=true;model.weapon=self->weapon==128?L"GEKKO VULCAN":self->weapon==129?L"GEKKO MISSILE":self->weapon==130?L"GEKKO KICK":L"GEKKO STOMP";model.actionNotice=L"特殊キャラクター：月光";model.skills.clear();}
   hud::draw(dc_,fonts_,model,roundIntro_.opacity(clock_()));chat::draw_history(dc_,fonts_[3],roomRequests_.chatSession->state(),{40,535,900,673},GetTickCount64(),12000);return;
  }
 }
 auto fill=[&](int x,int y,int w,int h,COLORREF c){menu_fill(dc_,x,y,w,h,c);};
 auto text=[&](std::wstring_view s,int x,int y,int w,int h,int font,COLORREF c,UINT flags=DT_LEFT){RECT r{x,y,x+w,y+h};SelectObject(dc_,fonts_[font]);SetTextColor(dc_,menu_text_color(c));SetBkMode(dc_,TRANSPARENT);DrawTextW(dc_,s.data(),int(s.size()),&r,flags|DT_NOPREFIX);};
 bool briefing=detailReply_.join_status==RoomJoinStatus::joined&&!matchVisible_;
 auto light=RGB(237,231,218),orange=RGB(255,208,150);
 const bool debug=matchVisible_&&!stageDebugNotice_.empty();
 if(debug)draw_room_match();else{
 if(briefing){draw_briefing();if(briefingPanel_==briefing::Panel::none)chat::draw_history(dc_,fonts_[3],roomRequests_.chatSession->state(),{50,535,980,673},GetTickCount64(),12000);return;}
 menu_heading(dc_,fonts_[0],matchVisible_?L"STAGE INFO":briefing?L"BRIEFING":L"ROOM");
 if(briefing){
  const wchar_t* labels[]={L"参加者",L"ルール",L"出撃",L"スキル",L"装身具",L"操作設定",L"出撃OK",L"ステージ情報",L"退室"};
  for(unsigned i=0;i<6;++i){int x=128+74*i;menu_tab(dc_,x,145,66,54,briefingFocus_==i);}
  text(labels[briefingFocus_],128,201,438,28,2,orange);
  if(detailReply_.preparation){const auto&p=*detailReply_.preparation;text(p.phase==combat::wire::RoundPhase::waiting?L"BRIEFING  "+hud::time_label(p.countdown?std::optional(p.remainingMs):std::nullopt):p.phase==combat::wire::RoundPhase::ended?L"時間終了":L"ROUND START",710,143,445,30,1,orange,DT_RIGHT);}
  if(detailReply_.host_match&&detailReply_.host_match->request){const auto&r=*detailReply_.host_match->request;text(hud::mode_label(r.rotation.rule)+L" / "+hud::utf8(stage::name(r.rotation.map)),710,179,445,28,2,light,DT_RIGHT);}
 }else text(detailReply_.join_status==RoomJoinStatus::joined?L"ルーム接続済み":L"ルーム詳細",120,152,900,44,0,light);
 text(L"OpenMGO2",850,82,310,30,1,light,DT_RIGHT);
 if(auto&d=detailReply_.detail;d){
  const auto&live=detailReply_.host_roster;bool joined=detailReply_.join_status==RoomJoinStatus::joined;
  bool synced=joined&&live&&live->complete;
  bool inspection=stageInspection_&&matchVisible_;
  if((!matchVisible_||stageDebugNotice_.empty())&&!briefing){fill(120,206,inspection?470:1040,56,RGB(73,96,60));text(d->name,140,218,inspection?430:740,35,1,light,DT_SINGLELINE|DT_END_ELLIPSIS);if(!inspection)text((joined&&!synced?L"—":std::to_wstring(synced?live->count():d->players))+L" / "+std::to_wstring(d->capacity),900,218,220,35,2,light,DT_RIGHT);}
  if(joined){
   if(matchVisible_)draw_room_match();else{
   menu_section(dc_,fonts_[3],L"PLAYER LIST",140,275,1000);
   if(synced){
    auto wide=[](const std::string&s){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0);std::wstring out(size_t(n),L'\0');if(n)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),out.data(),n);return out;};
    auto hostId=d->roster.empty()?0:d->roster.front().id;
    for(size_t i=0;i<24;++i){int x=140+int(i/8)*336,y=307+int(i%8)*24;const auto&p=live->slots[i];
     menu_band(dc_,x,y,324,23,i%8);if(p&&p->character==selectionReply_.character.id)fill(x,y,324,23,RGB(73,96,60));
     std::wstring name=(i<9?L"0":L"")+std::to_wstring(i+1)+L"  ";
     name+=p?player_display_name(p->character,wide(p->name)):L"—";if(p&&p->character==hostId)name+=L" [HOST]";else if(p&&p->character==selectionReply_.character.id)name+=L" [YOU]";
     fitted_name(dc_,fonts_[3],name,{x+5,y+1,x+319,y+24},menu_text_color(p?light:RGB(159,163,159)));
    }
   }else text(L"参加者情報を同期しています…",155,321,900,38,1,light);
   }
  }else{
  text(d->comment.empty()?L"コメントなし":d->comment,140,279,630,123,1,light,DT_WORDBREAK|DT_END_ELLIPSIS);
  text(L"ホスト："+player_display_name(d->roster.front().id,d->roster.front().name),140,414,630,32,1,orange);
  fill(795,279,345,181,RGB(35,48,37));menu_section(dc_,fonts_[3],L"PLAYER LIST",795,279,345);
  std::wstring names;for(size_t i=0;i<d->roster.size();++i){if(i)names+=L" / ";names+=player_display_name(d->roster[i].id,d->roster[i].name);}
  text(names,809,319,315,133,3,light,DT_WORDBREAK|DT_END_ELLIPSIS);
  text(d->password?L"パスワード":L"パスワード不要",140,485,200,30,3,light);
  if(d->password){fill(340,478,800,39,detailFocus_==0?RGB(73,96,60):RGB(35,48,37));auto n=size_t(std::find(detailAction_.password.begin(),detailAction_.password.end(),0)-detailAction_.password.begin());text(std::wstring(n,L'●')+(detailFocus_==0?L"｜":L""),355,487,760,29,1,light);}
  }
 }else text(detailBusy_?L"ルーム詳細を取得しています…":L"ルーム詳細を取得できませんでした。",140,270,1000,90,1,light,DT_WORDBREAK);
 std::wstring message=!detailNotice_.empty()?detailNotice_:detailBusy_?(detailAction_.event==RoomEvent::join?L"参加許可を確認しています…（Escで中止）":L"ルーム詳細を取得しています…"):L"参加するルームを確認してください。";
 if(detailReply_.preparation&&!matchVisible_){const auto&p=*detailReply_.preparation;const auto&self=p.players[p.self.slot];
  fill(140,475,620,32,RGB(73,96,60));text(p.phase==combat::wire::RoundPhase::waiting?(self&&self->ready?L"出撃OKを取り消す [F9 / START]":L"出撃OK [F9 / START]"):L"武器を選択 [F4]",150,479,600,27,2,light);
 }
 menu_description(dc_,fonts_[3],140,514,1000);
 if(matchVisible_&&!detailBusy_){switch(stageStatus_){
 case stage::Status::loading:message=L"ステージのモデル・照明データを読み込んでいます…（Escで退室）";break;
 case stage::Status::preview_ready:message=stageInspection_?L"地形と受信した物体のローカル操作確認。移動・視点はキー設定に従います。戦闘・移動送信は準備中です。":L"背景・テクスチャ・半球照明の参考表示。F12→F10：歩行確認。F6：環境音。対戦は未対応です。";break;
 case stage::Status::unknown_map:message=L"このマップ番号には対応していません。参加者一覧へ戻るか退室できます。";break;
 case stage::Status::unavailable:message=L"このマップのネイティブ資産はまだ用意されていません。";break;
 case stage::Status::invalid:message=L"ステージのモデル・照明・当たり判定データを読み込めませんでした。";break;
 case stage::Status::graphics_error:message=L"3Dプレビューを作成できませんでした。参加者一覧へ戻るか退室できます。";break;
 default:message=L"ホストのステージ情報を確認しています。対戦はまだ開始できません。";break;
 }}
 if(detailReply_.preparation)message=round_notice();
 if(matchVisible_&&!stageAudioNotice_.empty())message+=L" "+stageAudioNotice_;
 text(message,140,542,1000,67,1,orange,DT_WORDBREAK);
 for(unsigned i=1;i<=2;++i){int x=390+int(i-1)*390;bool joined=detailReply_.join_status==RoomJoinStatus::joined;bool enabled=i==2?(!detailBusy_||detailAction_.event==RoomEvent::join):(!detailBusy_&&(joined||(detailReply_.detail&&roomReply_.status==RoomStatus::ready&&!*roomRequests_.uncertain&&detailReply_.join_status!=RoomJoinStatus::permission_checked&&detailReply_.detail->players<detailReply_.detail->capacity)));fill(x,619,360,44,detailFocus_==i&&enabled?RGB(151,168,126):RGB(50,66,52));text(i==1?(joined?(matchVisible_?L"参加者一覧":L"ステージ情報"):L"ルームに参加"):joined?L"退室する":detailBusy_?L"接続を中止":L"ルーム一覧へ戻る",x,629,360,30,1,enabled?(detailFocus_==i?RGB(30,20,12):light):RGB(175,167,158),DT_CENTER);}
 text(detailReply_.join_status==RoomJoinStatus::joined?L"↑ ↓ / Tab：項目    Enter：決定    F4：武器    F9 / START：出撃OK・取消    F7：チーム    Esc：退室":detailBusy_&&detailAction_.event==RoomEvent::join?L"Esc：接続を中止":L"↑ ↓ / Tab：項目    Enter：決定    Esc：一覧へ戻る",120,690,1040,25,3,light);
 }
 if(stageResetConfirm_&&stage_request()){
  fill(300,278,680,200,RGB(135,156,113));fill(303,281,674,194,RGB(25,37,28));menu_section(dc_,fonts_[3],L"CONFIRM",303,281,674);
  text(L"ステージをリセットしますか？",320,322,640,40,0,light,DT_CENTER);
  text(L"← → / Tab：選択    Enter：決定    Esc：取消",320,363,640,27,3,light,DT_CENTER);
  for(int i=0;i<2;++i){int x=i?675:385;bool active=stageResetYes_==!i;fill(x,397,220,45,active?RGB(151,168,126):RGB(50,66,52));text(i?L"NO":L"YES",x,406,220,32,1,active?RGB(18,28,19):light,DT_CENTER);}
 }
 if(briefing)menu_focus_guides(dc_,briefingFocus_<6?128+int(briefingFocus_)*74:briefingFocus_==6?140:briefingFocus_==7?390:780,briefingFocus_<6?145:briefingFocus_==6?475:619);
 else if(stageResetConfirm_&&stage_request())menu_focus_guides(dc_,stageResetYes_?385:675,397);
 else if(!debug&&detailFocus_==0&&detailReply_.detail&&detailReply_.detail->password&&detailReply_.join_status!=RoomJoinStatus::joined)menu_focus_guides(dc_,340,478);
 else if(!debug&&detailFocus_>=1&&detailFocus_<=2)menu_focus_guides(dc_,390+int(detailFocus_-1)*390,619);
}
bool CharacterScreen::briefing_message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 using Panel=briefing::Panel;
 auto close=[&]{briefingPanel_=Panel::none;briefingYes_=false;cues_.push_back(menu_audio::Cancel);};
 auto activate=[&]{
  briefingYes_=false;
  switch(briefingFocus_){
   case 0:if(combatEntered_){toggle_gameplay_briefing();return;}briefingPanel_=Panel::ready;break;
   case 1:briefingPanel_=Panel::map;break;
   case 2:briefingPanel_=Panel::rules;break;
   case 3:open_skills();return;
   case 4:briefingPanel_=Panel::host;hostVoteFocus_=0;break;
   case 5:gameplayOptionsRequest_=true;break;
   case 6:briefingPanel_=Panel::quit;break;
  }
  cues_.push_back(menu_audio::Confirm);
 };
 auto confirm=[&]{
  if(!briefingYes_){close();return;}
  const auto panel=briefingPanel_;briefingPanel_=Panel::none;briefingYes_=false;
  if(panel==Panel::ready)round_ready();
  else if(panel==Panel::quit){if(!roomRequests_.cancel_join.exchange(true))cues_.push_back(menu_audio::Cancel);detailBusy_=true;detailNotice_=L"参加解除を確認しています…";}
 };
 const bool modal=briefingPanel_==Panel::ready||briefingPanel_==Panel::quit;
 if(msg==WM_KILLFOCUS){if(modal)close();return true;}
 if(msg==WM_KEYDOWN){
  if(lp&(1LL<<30))return true;
  if(briefingPanel_!=Panel::none){
   if(wp==VK_ESCAPE){close();return true;}
   if(modal){
    if(wp==VK_LEFT||wp==VK_RIGHT||wp==VK_TAB){bool yes=wp==VK_LEFT?true:wp==VK_RIGHT?false:!briefingYes_;if(yes!=briefingYes_){briefingYes_=yes;cues_.push_back(menu_audio::Cursor);}}
    else if(wp==VK_RETURN||wp==VK_SPACE)confirm();
   }else if(briefingPanel_==Panel::host&&(wp==VK_UP||wp==VK_DOWN||wp==VK_TAB)){hostVoteFocus_^=1;cues_.push_back(menu_audio::Cursor);}
   // No READY, team change, weapon shortcut, or room action leaks through a panel.
   return true;
  }
  if(wp==VK_ESCAPE){toggle_gameplay_briefing();return true;}
  if(wp==VK_LEFT||wp==VK_RIGHT||wp==VK_TAB||wp==VK_UP||wp==VK_DOWN){briefingFocus_=(briefingFocus_+(wp==VK_LEFT||wp==VK_UP?6:1))%7;cues_.push_back(menu_audio::Cursor);return true;}
  if(wp==VK_RETURN||wp==VK_SPACE){activate();return true;}
  if(wp==VK_F3){briefingFocus_=3;open_skills();return true;}
  if(wp==VK_F10){request_room_join();return true;}
  return false; // Existing F4/F7/F9 contracts remain available on the toolbar.
 }
 if(msg==WM_LBUTTONUP){
  RECT r{};GetClientRect(hwnd,&r);if(!r.right||!r.bottom)return true;
  const int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;
  if(modal){if(y>=350&&y<400&&((x>=360&&x<580)||(x>=700&&x<920))){briefingYes_=x<580;confirm();}return true;}
  if(briefingPanel_!=Panel::none){
   if(x<100&&y>=240&&y<580){close();return true;}
   if(briefingPanel_==Panel::host&&x>=75&&x<920&&y>=235&&y<327){hostVoteFocus_=unsigned((y-235)/46);}
   return true;
  }
  if(y>=briefing::toolbarY&&y<briefing::toolbarY+briefing::toolbarSize&&x>=briefing::toolbarX){
   const unsigned i=unsigned((x-briefing::toolbarX)/briefing::toolbarStep);
   if(i<7&&(x-briefing::toolbarX)%briefing::toolbarStep<briefing::toolbarSize){briefingFocus_=i;activate();}
  }
  return true;
 }
 return briefingPanel_!=Panel::none;
}

void CharacterScreen::draw_briefing(){
 using Panel=briefing::Panel;
 auto text=[&](std::wstring_view s,int x,int y,int w,int h,int font,COLORREF c,UINT flags=DT_LEFT){RECT r{x,y,x+w,y+h};SelectObject(dc_,briefing::fonts().values[font]?briefing::fonts().values[font]:fonts_[font]);SetTextColor(dc_,c);SetBkMode(dc_,TRANSPARENT);DrawTextW(dc_,s.data(),int(s.size()),&r,flags|DT_NOPREFIX);};
 constexpr auto gold=RGB(255,214,132),amber=RGB(227,169,85),dim=RGB(180,153,108),cyan=RGB(74,194,241);
 SYSTEMTIME now{};GetLocalTime(&now);wchar_t stamp[40];swprintf_s(stamp,L"%04u-%02u-%02u  %02u:%02u",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute);
 menu_rect(dc_,48,27,280,23,RGB(38,48,49));text(stamp,53,27,270,23,3,RGB(218,221,203));
 menu_rect(dc_,0,56,380,23,RGB(238,226,168));text(L"BRIEFING",48,55,350,26,2,RGB(53,47,29));
 for(unsigned i=0;i<7;++i){int x=briefing::toolbarX+int(i)*briefing::toolbarStep;bool selected=i==briefingFocus_;
  menu_rect(dc_,x,briefing::toolbarY,48,48,selected?RGB(215,151,52):RGB(67,51,29));
  menu_rect(dc_,x,briefing::toolbarY,48,2,selected?gold:dim);
  menu_rect(dc_,x,briefing::toolbarY+46,48,2,selected?amber:RGB(119,94,55));
 }
 menu_rect(dc_,48,143,672,29,RGB(38,48,49));text(briefing::labels[briefingFocus_],59,144,650,28,2,briefingFocus_==0?cyan:amber);
 const auto request=stage_load_request();
 const std::wstring rule=request?hud::mode_label(request->rotation.rule):L"—",map=request?(request->rotation.map==7?L"Blood Bath":request->rotation.map==20?L"Q.Q.":L"MAP "+std::to_wstring(request->rotation.map)):L"—";
 briefing::panel(dc_,760,49,470,103);text(L"RULE",772,55,430,19,3,dim);text(rule,783,72,423,30,2,gold);text(L"MAP",772,102,430,18,3,dim);text(map,783,120,423,29,2,gold);
 briefing::panel(dc_,40,181,1200,467);
 if(briefingPanel_==Panel::rules||(stageStatus_==stage::Status::loading&&briefingPanel_==Panel::none)){
  menu_cursor(dc_,64,206,1152,43);text(rule,80,212,560,35,1,gold);text(map,650,212,550,35,1,gold,DT_RIGHT);
  text(request?briefing::rule_description(request->rotation.rule):L"ルール情報を受信しています…",76,277,1120,155,0,gold,DT_WORDBREAK);
  if(stageStatus_==stage::Status::loading)text(L"ステージを読み込んでいます…",76,563,1120,35,2,amber);
 }else if(briefingPanel_==Panel::map){
  text(L"MAP  /  "+map,74,196,1100,35,1,gold);
  if(!briefingMapReady_||!request||request->rotation.map!=20)text(L"このマップの地図はまだ用意されていません。",128,364,1040,65,1,dim,DT_CENTER|DT_WORDBREAK);
  // The original map texture is painted after the menu alpha pass.
 }else if(briefingPanel_==Panel::host){
  text(L"VOTE",81,198,900,29,2,amber);
  for(unsigned i=0;i<2;++i){int y=235+int(i)*46;menu_row(dc_,75,y,850,42,i,hostVoteFocus_==i);text(i?L"キックを上申する":L"ステージ変更を上申する",97,y+5,800,33,1,gold);}
  text(L"上申は参加者の投票によって決まります。",81,365,1075,38,1,gold);
  text(L"このホストは投票に対応していません。",81,416,1075,75,1,dim,DT_WORDBREAK);
 }else{
  const bool twoColumns=detailReply_.host_roster&&detailReply_.host_roster->count()>18;
  for(unsigned column=0;column<(twoColumns?2u:1u);++column){int x=56+int(column)*584;text(L"PLAYER NAME",x+24,186,twoColumns?320:610,26,3,dim);text(L"TEAM",x+(twoColumns?350:678),186,twoColumns?85:125,26,3,dim);text(L"STATUS",x+(twoColumns?440:906),186,twoColumns?120:230,26,3,dim);}
  unsigned row=0;
  if(detailReply_.host_roster&&detailReply_.host_roster->complete){
   for(const auto& p:detailReply_.host_roster->slots)if(p){
    int x=56+(twoColumns?int(row/12)*584:0),y=216+(twoColumns?int(row%12)*32:int(row)*23);++row;const bool own=p->character==selectionReply_.character.id;menu_row(dc_,x,y,twoColumns?570:1168,23,row,own);
    fitted_name(dc_,briefing::fonts().values[2]?briefing::fonts().values[2]:fonts_[2],player_display_name(p->character,hud::utf8(p->name)),{x+24,y,x+24+(twoColumns?318:630),y+24},own?RGB(144,168,255):gold);
    if(detailReply_.preparation&&p->slot<detailReply_.preparation->players.size())if(const auto&rp=detailReply_.preparation->players[p->slot];rp){text(rp->team?L"BLUE":L"RED",x+(twoColumns?350:678),y,twoColumns?85:180,24,3,rp->team?RGB(140,181,255):RGB(255,157,125));text(rp->deployed?L"IN GAME":rp->ready?L"READY":L"—",x+(twoColumns?440:906),y,twoColumns?120:235,24,2,rp->ready?cyan:dim);}
   }
  }else text(L"参加者情報を同期しています…",80,224,1080,45,1,gold);
 }
 if(briefingPanel_==Panel::ready||briefingPanel_==Panel::quit){
  briefing::panel(dc_,52,265,1176,150);
  bool alreadyReady=false;if(const auto&p=detailReply_.preparation;p&&p->self.slot<p->players.size()&&p->players[p->self.slot])alreadyReady=p->players[p->self.slot]->ready;
  text(briefingPanel_==Panel::quit?L"この部屋から退出しますか？":alreadyReady?L"出撃準備を取り消しますか？":L"出撃準備はできましたか？",75,291,1130,46,0,gold,DT_CENTER);
  for(unsigned i=0;i<2;++i){int x=i?700:360;const bool selected=briefingYes_==!i;if(selected)menu_cursor(dc_,x,350,220,43,true);text(i?L"NO":L"YES",x,354,220,38,1,selected?RGB(255,239,145):amber,DT_CENTER);}
 }
 if(briefingPanel_==Panel::map||briefingPanel_==Panel::rules||briefingPanel_==Panel::host)text(L"◀",49,388,38,42,0,gold);
 if(briefingPanel_==Panel::none){
  auto notice=detailBusy_?detailNotice_:round_notice();text(notice,56,653,1165,35,3,gold,DT_SINGLELINE|DT_END_ELLIPSIS);
 }
 text(briefingPanel_==Panel::none?(combatEntered_?L"← →：選択   Enter：決定   START / Esc：ゲームへ戻る   退出：QUIT → YES":L"← →：選択   Enter：決定   F4：武器   F9：出撃準備   退出：QUIT → YES"):L"← → / ↑ ↓：選択   Enter：決定   Esc：戻る",48,690,1182,24,3,dim);
}

void CharacterScreen::draw_briefing_icons(){
 if(!detailVisible_||matchVisible_||weaponsVisible_||room_loading()||detailReply_.join_status!=RoomJoinStatus::joined)return;
 for(unsigned i=0;i<7;++i)if(auto*icon=briefingIcons_.find(uint16_t((briefingFocus_==i?101:1)+i)))weapons::paint_icon(*icon,{static_cast<uint32_t*>(pixels_),1280*720},1280,720,briefing::toolbarX+6+int(i)*briefing::toolbarStep,briefing::toolbarY+6,36,36);
 if(briefingPanel_==briefing::Panel::map)if(auto request=stage_load_request())briefingMap_.paint({static_cast<uint32_t*>(pixels_),1280*720},1280,720,request->rotation.map,448,235,384);
}
void CharacterScreen::draw_room_match(){
 auto text=[&](std::wstring_view s,int x,int y,int w,int h,int font,COLORREF c){RECT r{x,y,x+w,y+h};SelectObject(dc_,fonts_[font]);SetTextColor(dc_,menu_text_color(c));SetBkMode(dc_,TRANSPARENT);DrawTextW(dc_,s.data(),int(s.size()),&r,DT_LEFT|DT_NOPREFIX|DT_WORDBREAK);};
 auto light=RGB(237,231,218),orange=RGB(255,208,150);
 if(stageDebugNotice_.empty())menu_section(dc_,fonts_[3],L"STAGE",140,275,stageInspection_?450:1000);
 const auto&m=detailReply_.host_match;
 if(!m||!m->request){text(L"ホストからステージ情報を取得しています…",155,320,940,46,1,light);text(L"情報がそろうと、ここにマップとルールが表示されます。",155,379,940,64,1,orange);return;}
 const auto&r=*m->request;
 if(!stageDebugNotice_.empty()){
  // Keep the walking viewport clear. Measure wrapped text in the same font
  // and width used below; diagnostics may grow, but never beyond the canvas.
  constexpr int x=120,y=110,maxHeight=590;
  const int width=stageInspection_?490:680;
  const int saved=SaveDC(dc_);if(!saved)return;
  SelectObject(dc_,fonts_[3]);SetBkMode(dc_,TRANSPARENT);
  constexpr UINT flags=DT_LEFT|DT_NOPREFIX|DT_WORDBREAK|DT_EDITCONTROL;
  RECT body{x+16,y+45,x+width-16,y+45},measured=body;
  DrawTextW(dc_,stageDebugNotice_.data(),int(stageDebugNotice_.size()),&measured,flags|DT_CALCRECT);
  const int height=std::clamp(int(measured.bottom-measured.top)+59,140,maxHeight);
  body.bottom=y+height-14;
  const bool clipped=measured.bottom>body.bottom;
  if(clipped)body.bottom-=25;
  menu_rect(dc_,x,y,width,height,RGB(25,32,33));
  menu_rect(dc_,x+3,y+3,width-6,height-6,RGB(48,59,61));
  menu_section(dc_,fonts_[3],L"DEBUG MENU",x+10,y+10,width-20);
  SelectObject(dc_,fonts_[3]);SetTextColor(dc_,orange);
  IntersectClipRect(dc_,body.left,body.top,body.right,body.bottom);
  DrawTextW(dc_,stageDebugNotice_.data(),int(stageDebugNotice_.size()),&body,flags);
  RestoreDC(dc_,saved);
  if(clipped)text(L"… 診断が長いため末尾を省略",x+16,y+height-36,width-32,22,3,orange);
  return;
 }
 if(detailReply_.combat_offer&&detailReply_.combat_state&&detailReply_.combat_state->epoch==detailReply_.combat_offer->epoch){
  const auto&p=detailReply_.combat_state->players[detailReply_.combat_offer->self.slot];
  if(p&&p->identity==detailReply_.combat_offer->self){
   menu_section(dc_,fonts_[3],L"PLAYER",140,275,450);
   text(L"HP  "+std::to_wstring(p->hp)+L" / "+std::to_wstring(p->maxHp),155,313,430,36,1,light);
   std::wstring weaponName=L"WEAPON "+std::to_wstring(p->weapon);
   if(weaponCatalog_&&!weaponCatalog_->name(p->weapon).empty())weaponName=hud::utf8(std::string(weaponCatalog_->name(p->weapon)));
   text(weaponName+L"   "+std::to_wstring(p->ammo)+L" / "+std::to_wstring(p->reserve),155,352,430,36,1,light);
   text(!p->alive?L"戦闘不能（再出撃は未対応）":p->reloadUntil?L"リロード中":L"移動・構え・射撃：操作設定に従います。",155,395,430,62,1,orange);
   text(L"他PCの外見・銃声は復旧中です。",155,465,430,40,3,light);return;
  }
 }
 // Map/rule names are not guessed from an unrelated ordering or loadout table.
 text(L"MAP ID  "+std::to_wstring(r.rotation.map),155,313,450,36,1,light);
 auto stageName=stage::name(r.rotation.map);std::wstring stageLabel(stageName.begin(),stageName.end());text(L"STAGE  "+stageLabel,155,352,590,34,1,light);
 text(L"RULE ID  "+std::to_wstring(r.rotation.rule),155,391,590,34,1,light);
 text(L"ローテーション  "+std::to_wstring(unsigned(r.index)+1)+L" / 16",155,430,590,34,1,light);
 std::wstring change;
 switch(r.transition){
 case host::MatchTransition::initial:change=L"入室時の設定を受信しました。";break;
 case host::MatchTransition::round_restart:change=L"ラウンドの切り替え通知を受信しました。";break;
 case host::MatchTransition::next_round:change=L"次のラウンドの設定を受信しました。";break;
 case host::MatchTransition::map_change:case host::MatchTransition::map_and_round_change:change=L"マップ変更の設定を受信しました。";break;
 }
 text(change,155,469,590,30,3,orange);
 if(!r.rotation.map)text(L"有効なマップが指定されていません。",155,459,940,32,3,orange);
}
}
