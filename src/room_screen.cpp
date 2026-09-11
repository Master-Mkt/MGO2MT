#include "character_screen.h"
#include "menu_theme.h"
#include <algorithm>
namespace mgo2win {
void CharacterScreen::open_room_detail(){
 if(roomReply_.status!=RoomStatus::ready||roomFocus_>=roomReply_.rooms.size())return;
 if(*roomRequests_.uncertain){roomNotice_=L"参加結果が不明です。ログインし直してください。";return;}
 detailAction_={};detailAction_.id=roomReply_.rooms[roomFocus_].id;detailReply_={};detailNotice_.clear();detailFocus_=1;matchVisible_=false;
 if(!roomRequests_.submit(detailAction_))return;detailBusy_=true;detailVisible_=true;cues_.push_back(93);
}
void CharacterScreen::request_room_join(){
 if(detailBusy_||!detailReply_.detail||roomReply_.status!=RoomStatus::ready||*roomRequests_.uncertain)return;
 if(detailReply_.join_status==RoomJoinStatus::joined){matchVisible_=!matchVisible_;cues_.push_back(93);return;}
 if(detailReply_.join_status==RoomJoinStatus::permission_checked)return;
 if(detailReply_.detail->players>=detailReply_.detail->capacity){detailNotice_=L"満員のため参加できません。";return;}
 detailAction_.event=RoomEvent::join;detailAction_.subtype=detailReply_.detail->subtype;
 if(roomRequests_.submit(detailAction_)){detailBusy_=true;detailNotice_.clear();cues_.push_back(93);}
 SecureZeroMemory(detailAction_.password.data(),sizeof(detailAction_.password));
}
bool CharacterScreen::detail_message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 auto close=[&]{if(detailBusy_||detailReply_.join_status==RoomJoinStatus::joined){if(detailAction_.event==RoomEvent::join){roomRequests_.cancel_join=true;detailBusy_=true;detailNotice_=L"接続を中止し、参加解除を確認しています…";}return;}detailVisible_=false;SecureZeroMemory(detailAction_.password.data(),sizeof(detailAction_.password));cues_.push_back(93);};
 auto activate=[&]{if(detailFocus_==2)close();else if(detailFocus_==1)request_room_join();};
 if(msg==WM_KILLFOCUS){SecureZeroMemory(detailAction_.password.data(),sizeof(detailAction_.password));return true;}
 if(detailBusy_){if(msg==WM_KEYDOWN&&wp==VK_ESCAPE)close();if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(r.right&&r.bottom){int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;if(x>=780&&x<1140&&y>=619&&y<663)close();}}return true;}
 bool password=detailReply_.detail&&detailReply_.detail->password&&detailReply_.join_status!=RoomJoinStatus::joined;
 if(msg==WM_CHAR){if(detailFocus_==0&&password){auto&b=detailAction_.password;auto n=size_t(std::find(b.begin(),b.end(),0)-b.begin());if(wp==8){if(n)b[n-1]=0;}else if(wp>=32&&wp!=127&&n<16)b[n]=wchar_t(wp);}return true;}
 if(msg==WM_KEYUP)return true;
 if(msg==WM_KEYDOWN){if((lp&(1LL<<30))&&(wp==VK_RETURN||wp==VK_ESCAPE||wp==VK_SPACE))return true;
  if(wp==VK_ESCAPE)close();else if(wp==VK_TAB||wp==VK_DOWN||wp==VK_RIGHT){detailFocus_=(detailFocus_+1)%3;if(!password&&!detailFocus_)detailFocus_=1;cues_.push_back(94);}
  else if(wp==VK_UP||wp==VK_LEFT){detailFocus_=(detailFocus_+2)%3;if(!password&&!detailFocus_)detailFocus_=2;cues_.push_back(94);}
  else if(wp==VK_RETURN){if(detailFocus_==0)detailFocus_=1;else activate();}else if(wp==VK_SPACE&&detailFocus_!=0)activate();return true;
 }
 if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(!r.right||!r.bottom)return true;int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;
  if(password&&x>=270&&x<1110&&y>=478&&y<517)detailFocus_=0;
  else if(y>=619&&y<663){if(x>=780&&x<1140){detailFocus_=2;close();}else if(x>=390&&x<750){detailFocus_=1;request_room_join();}}SetFocus(hwnd);return true;
 }return false;
}
void CharacterScreen::draw_room_detail(){
 auto fill=[&](int x,int y,int w,int h,COLORREF c){menu_fill(dc_,x,y,w,h,c);};
 auto text=[&](std::wstring_view s,int x,int y,int w,int h,int font,COLORREF c,UINT flags=DT_LEFT){RECT r{x,y,x+w,y+h};SelectObject(dc_,fonts_[font]);SetTextColor(dc_,menu_text_color(c));SetBkMode(dc_,TRANSPARENT);DrawTextW(dc_,s.data(),int(s.size()),&r,flags|DT_NOPREFIX);};
 menu_heading(dc_,fonts_[0],matchVisible_?L"STAGE INFO":L"ROOM");
 auto light=RGB(237,231,218),orange=RGB(255,208,150);
 text(detailReply_.join_status==RoomJoinStatus::joined?L"ルーム接続済み":L"ルーム詳細",120,152,900,44,0,light);text(L"OpenMGO2",850,82,310,30,1,light,DT_RIGHT);
 if(auto&d=detailReply_.detail;d){
  const auto&live=detailReply_.host_roster;bool joined=detailReply_.join_status==RoomJoinStatus::joined;
  bool synced=joined&&live&&live->complete;
  fill(120,206,1040,56,RGB(73,96,60));text(d->name,140,218,740,35,1,light);text((joined&&!synced?L"—":std::to_wstring(synced?live->count():d->players))+L" / "+std::to_wstring(d->capacity),900,218,220,35,2,light,DT_RIGHT);
  if(joined){
   if(matchVisible_)draw_room_match();else{
   menu_section(dc_,fonts_[3],L"PLAYER LIST",140,275,1000);
   if(synced){
    auto wide=[](const std::string&s){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0);std::wstring out(size_t(n),L'\0');if(n)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),out.data(),n);return out;};
    auto hostId=d->roster.empty()?0:d->roster.front().id;
    for(size_t i=0;i<24;++i){int x=140+int(i/8)*336,y=307+int(i%8)*24;const auto&p=live->slots[i];
     if(p&&p->character==selectionReply_.character.id)fill(x,y,324,23,RGB(73,96,60));
     std::wstring name=(i<9?L"0":L"")+std::to_wstring(i+1)+L"  ";
     name+=p?wide(p->name):L"—";if(p&&p->character==hostId)name+=L" [HOST]";else if(p&&p->character==selectionReply_.character.id)name+=L" [YOU]";
     text(name,x+5,y+1,314,23,3,p?light:RGB(159,163,159),DT_SINGLELINE|DT_END_ELLIPSIS);
    }
   }else text(L"参加者情報を同期しています…",155,321,900,38,1,light);
   }
  }else{
  text(d->comment.empty()?L"コメントなし":d->comment,140,279,630,123,1,light,DT_WORDBREAK|DT_END_ELLIPSIS);
  text(L"ホスト："+d->roster.front().name,140,414,630,32,1,orange);
  fill(795,279,345,181,RGB(35,48,37));menu_section(dc_,fonts_[3],L"PLAYER LIST",795,279,345);
  std::wstring names;for(size_t i=0;i<d->roster.size();++i){if(i)names+=L" / ";names+=d->roster[i].name;}
  text(names,809,319,315,133,3,light,DT_WORDBREAK|DT_END_ELLIPSIS);
  text(d->password?L"パスワード":L"パスワード不要",140,485,200,30,3,light);
  if(d->password){fill(340,478,800,39,detailFocus_==0?RGB(73,96,60):RGB(35,48,37));auto n=size_t(std::find(detailAction_.password.begin(),detailAction_.password.end(),0)-detailAction_.password.begin());text(std::wstring(n,L'●')+(detailFocus_==0?L"｜":L""),355,487,760,29,1,light);}
  }
 }else text(detailBusy_?L"ルーム詳細を取得しています…":L"ルーム詳細を取得できませんでした。",140,270,1000,90,1,light,DT_WORDBREAK);
 std::wstring message=!detailNotice_.empty()?detailNotice_:detailBusy_?(detailAction_.event==RoomEvent::join?L"参加許可を確認しています…（Escで中止）":L"ルーム詳細を取得しています…"):L"参加するルームを確認してください。";
 menu_description(dc_,fonts_[3],140,514,1000);
 if(matchVisible_&&!detailBusy_)message=L"マップ読み込み・対戦操作は未実装です。現在は受信したステージ情報を確認できます。";
 text(message,140,542,1000,67,1,orange,DT_WORDBREAK);
 for(unsigned i=1;i<=2;++i){int x=390+int(i-1)*390;bool joined=detailReply_.join_status==RoomJoinStatus::joined;bool enabled=i==2?(!detailBusy_||detailAction_.event==RoomEvent::join):(!detailBusy_&&(joined||(detailReply_.detail&&roomReply_.status==RoomStatus::ready&&!*roomRequests_.uncertain&&detailReply_.join_status!=RoomJoinStatus::permission_checked&&detailReply_.detail->players<detailReply_.detail->capacity)));fill(x,619,360,44,detailFocus_==i&&enabled?RGB(151,168,126):RGB(50,66,52));text(i==1?(joined?(matchVisible_?L"参加者一覧":L"ステージ情報"):L"ルームに参加"):joined?L"退室する":detailBusy_?L"接続を中止":L"ルーム一覧へ戻る",x,629,360,30,1,enabled?(detailFocus_==i?RGB(30,20,12):light):RGB(175,167,158),DT_CENTER);}
 text(detailReply_.join_status==RoomJoinStatus::joined?L"↑ ↓ / Tab：項目    Enter：決定    Esc：退室":detailBusy_&&detailAction_.event==RoomEvent::join?L"Esc：接続を中止":L"↑ ↓ / Tab：項目    Enter：決定    Esc：一覧へ戻る",120,690,1040,25,3,light);
}
void CharacterScreen::draw_room_match(){
 auto text=[&](std::wstring_view s,int x,int y,int w,int h,int font,COLORREF c){RECT r{x,y,x+w,y+h};SelectObject(dc_,fonts_[font]);SetTextColor(dc_,menu_text_color(c));SetBkMode(dc_,TRANSPARENT);DrawTextW(dc_,s.data(),int(s.size()),&r,DT_LEFT|DT_NOPREFIX|DT_WORDBREAK);};
 auto light=RGB(237,231,218),orange=RGB(255,208,150);
 menu_section(dc_,fonts_[3],L"STAGE",140,275,1000);
 const auto&m=detailReply_.host_match;
 if(!m||!m->request){text(L"ホストからステージ情報を取得しています…",155,320,940,46,1,light);text(L"情報がそろうと、ここにマップとルールが表示されます。",155,379,940,64,1,orange);return;}
 const auto&r=*m->request;
 // Map/rule names are not guessed from an unrelated ordering or loadout table.
 text(L"MAP ID  "+std::to_wstring(r.rotation.map),155,313,450,36,1,light);
 text(L"RULE ID  "+std::to_wstring(r.rotation.rule),650,313,450,36,1,light);
 text(L"ローテーション  "+std::to_wstring(unsigned(r.index)+1)+L" / 16",155,363,600,36,1,light);
 std::wstring change;
 switch(r.transition){
 case host::MatchTransition::initial:change=L"入室時の設定を受信しました。";break;
 case host::MatchTransition::round_restart:change=L"ラウンドの切り替え通知を受信しました。";break;
 case host::MatchTransition::next_round:change=L"次のラウンドの設定を受信しました。";break;
 case host::MatchTransition::map_change:case host::MatchTransition::map_and_round_change:change=L"マップ変更の設定を受信しました。";break;
 }
 text(change,155,414,940,36,1,orange);
 if(!r.rotation.map)text(L"有効なマップが指定されていません。",155,459,940,32,3,orange);
}
}
