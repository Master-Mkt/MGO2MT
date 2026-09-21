#include "build_version.h"
#include <algorithm>
#include <iostream>
#include <windows.h>
#include <shellapi.h>
#include "dedicated_service.h"
#include "login_store.h"
#include "port_settings.h"
#include "lobby_groups.h"
#include "host_options.h"
#include "host_environment_dialog.h"
#include "item_settings_dialog.h"
#include "round_items_dialog.h"
#include "round_items.h"
#include "combat_health_rules.h"
#include "breakable_light_settings.h"
#include "gameplay_config.h"
#include "host_hit_geometry.h"
#include "gekko_jump_curve.h"
#include "evade_travel_curve.h"
#include "gameplay_fingerprint.h"
#include "combat_initial_profile.h"
#include "mounted_weapons.h"
#include <thread>
#include <mutex>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <syncstream>
#include <set>

using namespace mgo2mt;
namespace {
enum {Login=100,Start,Stop,Account,Password,Remember,Character,Lobby,RoomName,RoomPassword,Port,Capacity,Briefing,DP,Map,Rule,Status,Players,OptionsButton,PlacementOptions,RoundPlacementOptions,EnvironmentOptions,SpecialOptions,AllowSpecial,SpecialNames,RandomSpecial,SpecialPlayer,AssignGekko,AssignHuman};
std::filesystem::path executable_folder(){std::wstring s(32768,0);auto n=GetModuleFileNameW(nullptr,s.data(),DWORD(s.size()));if(!n||n==s.size())throw std::runtime_error("module path");s.resize(n);return std::filesystem::path(s).parent_path();}
std::filesystem::path profile_path(){wchar_t s[32768]{};auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",s,32768);if(!n||n>=32768)throw std::runtime_error("profile path");return std::filesystem::path(s)/L"MGO2MTHOST"/L"login.dat";}
std::wstring text(HWND w){int n=GetWindowTextLengthW(w);if(n>256)throw std::runtime_error("field length");std::wstring s(size_t(n)+1,0);GetWindowTextW(w,s.data(),n+1);s.resize(n);return s;}
std::wstring wide(std::string_view s){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0);if(!n)return L"?";std::wstring out(n,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),out.data(),n);return out;}
void check_gameplay_configuration(const std::filesystem::path& data){
 const auto fingerprint=gameplay::fingerprint(data);if(!fingerprint)throw std::runtime_error("Cannot fingerprint gameplay configuration");
 std::string error;gameplay::Config config;mounted::Registry registry;
 const bool configured=std::filesystem::exists(data/"gameplay.json");
 if(configured&&!config.load(data/"gameplay.json",error))throw std::runtime_error(error);
 if(std::filesystem::exists(data/"mounted_weapons.json")&&!registry.load(data/"mounted_weapons.json",error))throw std::runtime_error(error);
 const auto profiles=configured?config.profiles(20):combat::initial_profiles(20,1,0);
 // Exercise the same profile and placement validation as a round, using a
 // tiny local fixture. HOST verification never requires client mesh assets.
 const auto geometry=std::make_shared<const stage::Collision>(stage::Collision::make({{0,0,0},{1000,0,0},{0,0,1000}},{{{0,2,1},stage::attribute::native_solid}}));
 combat::Authority authority;authority.begin(1,geometry,profiles);
 for(const auto& type:registry.types){
  const auto found=std::find_if(profiles.begin(),profiles.end(),[&](const auto& profile){return profile.id==type.weapon;});
  if(type.kind!=mounted::Kind::catapult&&(found==profiles.end()||found->heldOnly||found->meleeAttack||(found->nativeProjectile&&type.kind!=mounted::Kind::mortar)||found->nativePlaced||special_pc::weapon(found->id)))throw std::runtime_error("Mounted type "+type.id+" references an absent or unsupported weapon "+std::to_string(type.weapon));
 }
 std::set<uint8_t> maps;for(const auto& placement:registry.placements)maps.insert(placement.map);
 for(auto map:maps)if(!authority.configure_mounted(registry,map))throw std::runtime_error("Mounted scene rejected for map "+std::to_string(map));
 if(gameplay::fingerprint(data)!=fingerprint)throw std::runtime_error("Gameplay configuration changed while checking");
 std::cout<<"Gameplay configuration valid: "<<profiles.size()<<" weapons, "<<registry.types.size()<<" mounted types, "<<registry.placements.size()<<" placements\n";
}
struct CombatLog {
 std::ofstream file;std::streambuf* previous=nullptr;
 void open(){
  auto root=profile_path().parent_path()/L"logs";std::filesystem::create_directories(root);
  SYSTEMTIME t{};GetSystemTime(&t);wchar_t name[96]{};swprintf_s(name,L"combat-%04u%02u%02uT%02u%02u%02u-%u.log",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,GetCurrentProcessId());
  file.open(root/name,std::ios::app);if(!file)throw std::runtime_error("combat log");previous=std::clog.rdbuf(file.rdbuf());std::clog<<"MGO2MTHOST "<<build_version<<" combat log UTC\n";
 }
 ~CombatLog(){if(previous)std::clog.rdbuf(previous);}
};
struct LoginResult {AuthReply auth;CharacterReply characters;CharacterSelectionReply selection;std::wstring error;};
struct App {
 HWND window=nullptr,specialWindow=nullptr;HFONT font=nullptr;std::filesystem::path keys,specialReport;
 std::jthread worker;std::atomic_bool cancel=false,done=false;bool busy=false,hosting=false,closing=false,smoke=false,uncertain=false;unsigned smokeTicks=0;
 std::mutex mutex;std::optional<LoginResult> loginResult;std::optional<DedicatedReply> update;
 host::Settings commonSettings;
 std::vector<host::Player> latestPlayers;special_pc::View specialView;
 AuthReply auth;std::vector<CharacterEntry> characters;std::vector<GameLobbyEntry> lobbies;
 HWND control(int id)const{return GetDlgItem(id>=AllowSpecial?specialWindow:window,id);}
 void status(std::wstring value){SetWindowTextW(control(Status),value.c_str());}
 void enabled(){EnableWindow(control(Login),!busy);EnableWindow(control(Start),!busy&&!uncertain&&!characters.empty()&&!lobbies.empty());EnableWindow(control(Stop),busy);for(int id:{Account,Password,Remember,Character,Lobby,RoomName,RoomPassword,Port,Capacity,Briefing,DP,Map,Rule,OptionsButton,PlacementOptions,RoundPlacementOptions})EnableWindow(control(id),!busy);}
 void label(const wchar_t*s,int x,int y,int w){auto h=CreateWindowW(L"STATIC",s,WS_CHILD|WS_VISIBLE,x,y,w,24,window,nullptr,nullptr,nullptr);SendMessageW(h,WM_SETFONT,WPARAM(font),TRUE);}
 HWND add(const wchar_t*klass,const wchar_t*s,int id,int x,int y,int w,int h,DWORD style=0){auto c=CreateWindowExW(WS_EX_CLIENTEDGE,klass,s,WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,x,y,w,h,id>=AllowSpecial?specialWindow:window,HMENU(INT_PTR(id)),nullptr,nullptr);SendMessageW(c,WM_SETFONT,WPARAM(font),TRUE);return c;}
 static LRESULT CALLBACK special_proc(HWND w,UINT m,WPARAM a,LPARAM b){auto*self=reinterpret_cast<App*>(GetWindowLongPtrW(w,GWLP_USERDATA));if(m==WM_NCCREATE){self=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(b)->lpCreateParams);SetWindowLongPtrW(w,GWLP_USERDATA,LONG_PTR(self));}if(self){if(m==WM_COMMAND)return SendMessageW(self->window,m,a,b);if(m==WM_CLOSE){ShowWindow(w,SW_HIDE);return 0;}}return DefWindowProcW(w,m,a,b);}
 void initialize(){
  font=CreateFontW(-19,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Yu Gothic UI");
  label(L"MGO2MTHOST  /  OpenMGO2 専用ホスト",22,15,455);
  add(L"BUTTON",L"天候・照明・環境音",EnvironmentOptions,490,10,245,34,BS_PUSHBUTTON);
  label(L"ゲームID",22,58,110);add(L"EDIT",L"",Account,135,54,235,28,ES_AUTOHSCROLL);
  label(L"パスワード",390,58,110);add(L"EDIT",L"",Password,500,54,235,28,ES_PASSWORD|ES_AUTOHSCROLL);
  SendMessageW(control(Account),EM_SETLIMITTEXT,64,0);SendMessageW(control(Password),EM_SETLIMITTEXT,64,0);
  add(L"BUTTON",L"認証情報をこのPCに保存",Remember,135,91,320,28,BS_AUTOCHECKBOX);SendMessageW(control(Remember),BM_SETCHECK,BST_CHECKED,0);
  add(L"BUTTON",L"ログイン",Login,530,88,205,34,BS_PUSHBUTTON);
  label(L"ホスト用PC",22,143,110);add(L"COMBOBOX",L"",Character,135,138,250,180,CBS_DROPDOWNLIST|WS_VSCROLL);
  label(L"フリーロビー",405,143,110);add(L"COMBOBOX",L"",Lobby,520,138,215,180,CBS_DROPDOWNLIST|WS_VSCROLL);
  label(L"部屋名",22,191,110);add(L"EDIT",L"MGO2MTHOST",RoomName,135,186,250,29,ES_AUTOHSCROLL);SendMessageW(control(RoomName),EM_SETLIMITTEXT,16,0);
  label(L"部屋パスワード",405,191,115);add(L"EDIT",L"",RoomPassword,520,186,215,29,ES_PASSWORD|ES_AUTOHSCROLL);SendMessageW(control(RoomPassword),EM_SETLIMITTEXT,15,0);
  label(L"UDPポート",22,239,110);add(L"EDIT",L"5732",Port,135,234,115,29,ES_NUMBER);
  label(L"参加人数",285,239,95);auto cap=add(L"COMBOBOX",L"",Capacity,380,234,90,250,CBS_DROPDOWNLIST|WS_VSCROLL);for(unsigned i=1;i<=16;++i)SendMessageW(cap,CB_ADDSTRING,0,LPARAM(std::to_wstring(i).c_str()));SendMessageW(cap,CB_SETCURSEL,15,0);
  label(L"待機（分）",505,239,110);auto brief=add(L"COMBOBOX",L"",Briefing,625,234,110,200,CBS_DROPDOWNLIST);for(unsigned i=0;i<=60;++i)SendMessageW(brief,CB_ADDSTRING,0,LPARAM(std::to_wstring(i).c_str()));SendMessageW(brief,CB_SETCURSEL,2,0);
  label(L"マップ",22,288,110);auto map=add(L"COMBOBOX",L"",Map,135,282,250,160,CBS_DROPDOWNLIST);for(auto label:{L"n022a / マップ20",L"AA / n001a",L"MM / n004a",L"JJ候補 / n023a",L"BB / Blood Bath"})SendMessageW(map,CB_ADDSTRING,0,LPARAM(label));SendMessageW(map,CB_SETCURSEL,0,0);
  label(L"ルール",405,288,100);auto rule=add(L"COMBOBOX",L"",Rule,520,282,215,160,CBS_DROPDOWNLIST);SendMessageW(rule,CB_ADDSTRING,0,LPARAM(L"TDM"));SendMessageW(rule,CB_ADDSTRING,0,LPARAM(L"DM"));SendMessageW(rule,CB_SETCURSEL,0,0);
  add(L"BUTTON",L"DP（ドレビンポイント）を有効にする",DP,135,324,370,28,BS_AUTOCHECKBOX);
  add(L"BUTTON",L"特殊PC設定",SpecialOptions,22,324,105,28,BS_PUSHBUTTON);
  add(L"BUTTON",L"ラウンド配置",RoundPlacementOptions,530,324,205,28,BS_PUSHBUTTON);
  add(L"BUTTON",L"共通設定・武器制限",OptionsButton,135,361,250,32,BS_PUSHBUTTON);
  add(L"BUTTON",L"武器の設置・回収設定",PlacementOptions,405,361,330,32,BS_PUSHBUTTON);
  label(L"ローカル試作：TDM / DM。追加ステージの動的オブジェクトは解析中です。",22,394,730);
  add(L"BUTTON",L"部屋を作成",Start,135,430,250,42,BS_PUSHBUTTON);add(L"BUTTON",L"停止・部屋を閉じる",Stop,430,430,305,42,BS_PUSHBUTTON);
  add(L"EDIT",L"ログインしてホスト用のPCを選択してください。",Status,22,490,713,96,ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL);
  if(!smoke)try{commonSettings.nativeEnvironment->configure(environment::load(profile_path().parent_path()/L"environment.cfg"));}catch(...){status(L"環境設定を読み込めません。ステージの設定を使用します。");}
  label(L"参加者",22,601,110);add(L"EDIT",L"なし",Players,135,596,600,105,ES_MULTILINE|ES_READONLY|WS_VSCROLL);
  WNDCLASSW specialClass{};specialClass.lpfnWndProc=special_proc;specialClass.hInstance=GetModuleHandleW(nullptr);specialClass.lpszClassName=L"MGO2MTHOST.SpecialPC";specialClass.hCursor=LoadCursor(nullptr,IDC_ARROW);specialClass.hbrBackground=HBRUSH(COLOR_BTNFACE+1);RegisterClassW(&specialClass);
  specialWindow=CreateWindowExW(WS_EX_TOOLWINDOW,specialClass.lpszClassName,L"特殊キャラ設定 / HOST",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,700,285,window,nullptr,specialClass.hInstance,this);if(!specialWindow)throw std::runtime_error("special settings window");
  add(L"BUTTON",L"特殊キャラを許可",AllowSpecial,18,18,215,28,BS_AUTOCHECKBOX);
  add(L"BUTTON",L"特殊キャラの名前を表示",SpecialNames,235,18,270,28,BS_AUTOCHECKBOX);SendMessageW(control(SpecialNames),BM_SETCHECK,BST_CHECKED,0);
  add(L"BUTTON",L"ランダム1名",RandomSpecial,510,18,165,28,BS_AUTOCHECKBOX);
  SendMessageW(control(RandomSpecial),BM_SETCHECK,BST_CHECKED,0);
  add(L"COMBOBOX",L"",SpecialPlayer,18,63,655,180,CBS_DROPDOWNLIST|WS_VSCROLL);
  add(L"BUTTON",L"選択したPCを月光にする",AssignGekko,18,112,318,34,BS_PUSHBUTTON);
  add(L"BUTTON",L"選択したPCを通常に戻す",AssignHuman,350,112,323,34,BS_PUSHBUTTON);
  auto help=CreateWindowW(L"STATIC",L"出撃中の参加者を選択してください。同じ特殊キャラを複数指定できます。\n周囲の空間が足りない場合は変更できません。\n設定画面を開いても対戦とHOSTは進行します。",WS_CHILD|WS_VISIBLE,18,163,655,70,specialWindow,nullptr,nullptr,nullptr);SendMessageW(help,WM_SETFONT,WPARAM(font),TRUE);
  try{LoginForm saved;if(!smoke&&load_login(profile_path(),saved)){SetWindowTextW(control(Account),saved.credential(0).data());SetWindowTextW(control(Password),saved.credential(1).data());}}catch(...){status(L"保存情報を読み込めません。ID・パスワードを入力してください。");}
  if(smoke){SetWindowTextW(control(Account),L"offline-test");status(L"オフライン画面確認（通信は行いません）。");}
  enabled();SetTimer(window,1,100,nullptr);
 }
 void special_capture(){
  if(specialReport.empty())return;std::filesystem::create_directories(specialReport);
  SetWindowTextW(specialWindow,L"特殊キャラ設定 / 表示確認用（通信なし）");ShowWindow(specialWindow,SW_SHOWNORMAL);UpdateWindow(specialWindow);
  SendMessageW(control(SpecialPlayer),CB_RESETCONTENT,0,0);SendMessageW(control(SpecialPlayer),CB_ADDSTRING,0,LPARAM(L"表示確認用プレイヤー / 月光"));SendMessageW(control(SpecialPlayer),CB_SETCURSEL,0,0);
  RECT r{};GetClientRect(specialWindow,&r);auto dc=GetDC(specialWindow),mem=CreateCompatibleDC(dc);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;info.bmiHeader.biHeight=-r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void*pixels=nullptr;auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);if(!mem||!bitmap)throw std::runtime_error("special capture");auto old=SelectObject(mem,bitmap);FillRect(mem,&r,HBRUSH(COLOR_BTNFACE+1));
  for(auto child=GetWindow(specialWindow,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){RECT rect{};GetWindowRect(child,&rect);MapWindowPoints(nullptr,specialWindow,reinterpret_cast<POINT*>(&rect),2);auto state=SaveDC(mem);SetViewportOrgEx(mem,rect.left,rect.top,nullptr);SendMessageW(child,WM_PRINT,WPARAM(mem),PRF_CLIENT|PRF_ERASEBKGND|PRF_CHILDREN|PRF_NONCLIENT);RestoreDC(mem,state);}
  GdiFlush();BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+r.right*r.bottom*4;std::ofstream out(specialReport/L"special_settings.bmp",std::ios::binary);out.write(reinterpret_cast<char*>(&file),sizeof(file));out.write(reinterpret_cast<char*>(&info.bmiHeader),sizeof(info.bmiHeader));out.write(static_cast<char*>(pixels),r.right*r.bottom*4);SelectObject(mem,old);DeleteObject(bitmap);DeleteDC(mem);ReleaseDC(specialWindow,dc);if(!out)throw std::runtime_error("special capture write");
  std::ofstream(specialReport/L"validation.json")<<"{\"offline\":true,\"network\":false,\"fixture\":\"display-only\",\"mainHeight\":760,\"settingsHeight\":285,\"allowDefault\":false,\"randomDefault\":true}\n";
 }
 void environment_options(){auto draft=commonSettings.nativeEnvironment->state().config;if(edit_host_environment(window,draft)){environment::save(profile_path().parent_path()/L"environment.cfg",draft);if(!commonSettings.nativeEnvironment->configure(draft))throw std::runtime_error("Environment settings revision");status(hosting?L"環境設定を保存しました。参加中のプレイヤーへ反映します。":L"環境設定を保存しました。次の部屋でも使用します。");}}
 void special_settings(){commonSettings.nativeSpecial->configure({SendMessageW(control(AllowSpecial),BM_GETCHECK,0,0)==BST_CHECKED,SendMessageW(control(SpecialNames),BM_GETCHECK,0,0)==BST_CHECKED,SendMessageW(control(RandomSpecial),BM_GETCHECK,0,0)==BST_CHECKED});}
 void special_assign(special_pc::Kind kind){auto index=SendMessageW(control(SpecialPlayer),CB_GETCURSEL,0,0);if(!busy||!hosting||index<0||size_t(index)>=specialView.players.size())return;const auto& p=specialView.players[size_t(index)];status(commonSettings.nativeSpecial->assign(specialView.epoch,p.id,kind)?L"特殊キャラの変更を要求しました。HOSTの空間・状態確認後に反映します。":L"参加者または部屋が変更されました。選択し直してください。");}
 void special_refresh(){auto view=commonSettings.nativeSpecial->state();if(view.players!=specialView.players||view.epoch!=specialView.epoch){std::optional<combat::Identity> selected;auto index=SendMessageW(control(SpecialPlayer),CB_GETCURSEL,0,0);if(index>=0&&size_t(index)<specialView.players.size())selected=specialView.players[size_t(index)].id;SendMessageW(control(SpecialPlayer),CB_RESETCONTENT,0,0);int chosen=0;for(size_t n=0;n<view.players.size();++n){const auto& p=view.players[n];auto known=std::find_if(latestPlayers.begin(),latestPlayers.end(),[&](const auto& r){return r.slot==p.id.slot&&r.instance==p.id.instance&&r.character==p.id.character;});auto label=known==latestPlayers.end()?L"PC "+std::to_wstring(p.id.character):wide(known->name);label+=p.current==special_pc::Kind::gekko?L" / 月光":L" / 通常";if(p.result!=combat::Reject::none)label+=L" / 変更不可（空間・状態）";SendMessageW(control(SpecialPlayer),CB_ADDSTRING,0,LPARAM(label.c_str()));if(selected==p.id)chosen=int(n);}SendMessageW(control(SpecialPlayer),CB_SETCURSEL,chosen,0);}specialView=std::move(view);SendMessageW(control(RandomSpecial),BM_SETCHECK,specialView.config.random?BST_CHECKED:BST_UNCHECKED,0);const bool ready=busy&&hosting&&!specialView.players.empty();EnableWindow(control(AssignGekko),ready&&specialView.config.allow);EnableWindow(control(AssignHuman),ready);EnableWindow(control(SpecialPlayer),ready);}
 void login(){
  if(busy||smoke)return;auto id=text(control(Account)),password=text(control(Password));
  if(id.empty()||password.empty()){status(L"ゲームIDとパスワードを入力してください。");return;}
  auto credentials=std::make_shared<AuthCredentials>(id,password);SecureZeroMemory(password.data(),password.size()*sizeof(wchar_t));
  try{LoginForm form;form.restore(SendMessageW(control(Remember),BM_GETCHECK,0,0)==BST_CHECKED?2:0,id,credentials->password.data());save_login(profile_path(),form);}catch(...){status(L"認証情報を保存できません。保存設定を外して再試行してください。");return;}
  cancel=false;done=false;busy=true;hosting=false;enabled();status(L"OpenMGO2へログインし、PCとロビーを取得しています…");
  worker=std::jthread([this,credentials]{LoginResult r;
   try{r.auth=authenticate(*credentials,cancel);if(r.auth.status!=AuthStatus::success)r.error=L"ログインできませんでした。ID・パスワードと接続を確認してください。";
    else {r.characters=fetch_characters(keys,r.auth,cancel);if(r.characters.status!=CharacterStatus::success)r.error=L"PC一覧を取得できませんでした。";
     else if(r.characters.list.entries.empty())r.error=L"ホスト用PCがありません。MGO2MTでPCを作成してください。";
     else{r.selection=select_character(keys,r.auth,r.characters.list.entries.front().id,cancel,CharacterSelectionContract::channel_snapshot_v1);if(r.selection.status!=CharacterSelectionStatus::success)r.error=L"PCの選択・ロビー取得ができませんでした。";}}
   }catch(...){r.error=L"ログイン処理でエラーが発生しました。";}
   {std::lock_guard lock(mutex);loginResult=std::move(r);}done=true;
  });
 }
 void placement_options(){if(!busy)items::edit_item_settings(window,keys.parent_path()/"item_settings.cfg",keys.parent_path()/"item_drop_policy.json");}
 void options(){
  if(busy)return;auto draft=commonSettings;draft.capacity=uint8_t(SendMessageW(control(Capacity),CB_GETCURSEL,0,0)+2);draft.briefing_minutes=uint32_t(SendMessageW(control(Briefing),CB_GETCURSEL,0,0));
  if(edit_host_options(window,draft)){commonSettings=std::move(draft);SendMessageW(control(Capacity),CB_SETCURSEL,commonSettings.capacity-2,0);SendMessageW(control(Briefing),CB_SETCURSEL,commonSettings.briefing_minutes,0);status(L"共通設定と武器制限を反映しました。次の部屋作成に使用します。");}
 }
 void start(){
  if(busy||smoke||uncertain)return;auto ci=SendMessageW(control(Character),CB_GETCURSEL,0,0),li=SendMessageW(control(Lobby),CB_GETCURSEL,0,0);if(ci<0||size_t(ci)>=characters.size()||li<0||size_t(li)>=lobbies.size())return;
  uint16_t port=0;if(!parse_port(text(control(Port)),port)){status(L"UDPポートは1024〜65535で指定してください。");return;}
  host::Settings settings=commonSettings;settings.name=text(control(RoomName));settings.password=text(control(RoomPassword));settings.capacity=uint8_t(SendMessageW(control(Capacity),CB_GETCURSEL,0,0)+2);settings.briefing_minutes=uint32_t(SendMessageW(control(Briefing),CB_GETCURSEL,0,0));
  // 9B9484 writes rotation mode 2; normal 9BDD40 writes 0. This is a
  // mutually exclusive mode, not an OR mask shared with headshot-only(4).
  constexpr uint8_t maps[]={20,1,4,21,7};auto mapIndex=SendMessageW(control(Map),CB_GETCURSEL,0,0),ruleIndex=SendMessageW(control(Rule),CB_GETCURSEL,0,0);if(mapIndex<0||mapIndex>=5||ruleIndex<0||ruleIndex>=2)return;settings.rotations[0].map=maps[mapIndex];settings.rotations[0].rule=ruleIndex==0?1:0;
  settings.rotations[0].flags=SendMessageW(control(DP),BM_GETCHECK,0,0)==BST_CHECKED?2:0;
  try{host::settings_payload(settings);}catch(...){status(L"部屋名はUTF-8で3〜16バイト、部屋パスワードは空または3〜15バイトにしてください。");return;}
  cancel=false;done=false;busy=true;hosting=true;enabled();auto selected=characters[size_t(ci)];auto lobby=lobbies[size_t(li)];auto savedAuth=auth;
  worker=std::jthread([this,selected,lobby,settings,port,savedAuth]{
   try{auto selection=select_character(keys,savedAuth,selected.id,cancel,CharacterSelectionContract::channel_snapshot_v1);if(selection.status!=CharacterSelectionStatus::success)throw std::runtime_error("host selection");
    PortReservation reservation;auto bound=reservation.check({false,port,512});if(bound.status!=PortStatus::available){std::lock_guard lock(mutex);update=DedicatedReply{DedicatedStatus::network_error,0,unsigned(bound.error)};}
    else run_dedicated_lobby(keys,savedAuth,selected,lobby,settings,cancel,[this](DedicatedReply r){std::lock_guard lock(mutex);update=std::move(r);},reservation.native_socket());
   }catch(...){std::lock_guard lock(mutex);update=DedicatedReply{DedicatedStatus::protocol_error};}done=true;
  });
 }
 void tick(){
  if(smoke&&!specialReport.empty()&&smokeTicks==2)special_capture();
  if(smoke&&++smokeTicks>=10){PostMessageW(window,WM_CLOSE,0,0);return;}
  std::optional<LoginResult> result;std::optional<DedicatedReply> reply;{std::lock_guard lock(mutex);result=std::move(loginResult);loginResult.reset();reply=std::move(update);update.reset();}
  if(result){characters.clear();lobbies.clear();SendMessageW(control(Character),CB_RESETCONTENT,0,0);SendMessageW(control(Lobby),CB_RESETCONTENT,0,0);
   if(!result->error.empty())status(result->error);else{auth=result->auth;characters=result->characters.list.entries;for(auto&c:characters)SendMessageW(control(Character),CB_ADDSTRING,0,LPARAM(c.name.c_str()));for(auto&l:result->selection.lobbies)if(l.subtype==1&&l.port>=5733&&l.port<=5739){lobbies.push_back(l);SendMessageW(control(Lobby),CB_ADDSTRING,0,LPARAM(l.name.c_str()));}SendMessageW(control(Character),CB_SETCURSEL,0,0);SendMessageW(control(Lobby),CB_SETCURSEL,0,0);status(lobbies.empty()?L"接続可能なフリーロビーがありません。":L"ログインしました。設定を確認して「部屋を作成」を押してください。");}}
  if(reply){if(reply->status==DedicatedStatus::outcome_unknown)uncertain=true;std::wstring message;switch(reply->status){case DedicatedStatus::connecting:message=L"フリーロビーへ接続しています…";break;case DedicatedStatus::mapping:message=L"UDPポートを確認しています…";break;case DedicatedStatus::creating:message=L"専用ホストの部屋を作成しています…";break;case DedicatedStatus::hosting:message=L"部屋を公開中 / ID "+std::to_wstring(reply->room)+L"\r\nUDP "+std::to_wstring(reply->local_port)+L" → "+std::to_wstring(reply->public_port)+L" / 参加者 "+std::to_wstring(reply->players.size())+L"人 / ルーム情報の受信完了 "+std::to_wstring(reply->synchronized_players)+L"人";break;case DedicatedStatus::closed:message=L"部屋の閉鎖を受け付けました。";break;case DedicatedStatus::cancelled:message=L"中止しました。";break;case DedicatedStatus::outcome_unknown:message=L"部屋の状態を確認できません。ロビーで閉鎖を確認してからこのアプリを起動し直してください。";break;case DedicatedStatus::rejected:message=L"部屋の作成が受け付けられませんでした。";break;default:message=L"接続または応答の確認に失敗しました。設定を確認してください。";break;}if(reply->status==DedicatedStatus::hosting){
     using CombatStatus=combat::wire::Status;
     if(reply->combat_status==CombatStatus::awaiting_world)message+=L"\r\n戦闘同期：ホストの地形データ待ち";
     else if(reply->combat_status==CombatStatus::awaiting_profile)message+=L"\r\n戦闘同期：武器の動作データは確認中";
     else if(reply->combat_status==CombatStatus::awaiting_spawn)message+=L"\r\n戦闘同期：出撃位置・装備の確定待ち";
     if(reply->combat_status==CombatStatus::ended)message+=L"\r\n時間終了 / 次ラウンドを準備しています（勝敗集計は未対応）";
     else if(reply->phase==host::RoundPhase::preparing)message+=reply->combat_status==CombatStatus::active?L"\r\nラウンド進行中":L"\r\nラウンド開始 / 武器選択・出撃を待っています";
     else if(reply->briefing_remaining_ms){auto seconds=(*reply->briefing_remaining_ms+999)/1000;message+=seconds?L"\r\n準備開始まで "+std::to_wstring(seconds/60)+L":"+(seconds%60<10?L"0":L"")+std::to_wstring(seconds%60):L"\r\n参加者へのルーム情報の送信完了を待っています…";}
     else message+=L"\r\n参加者を待っています。";
    }if(reply->error)message+=L"\r\nエラー "+std::to_wstring(reply->error);status(message);std::wstring names;for(auto&p:reply->players){if(!names.empty())names+=L"\r\n";names+=wide(p.name);}SetWindowTextW(control(Players),names.empty()?L"なし":names.c_str());}
  if(reply){for(const auto&p:reply->players){auto old=std::find_if(latestPlayers.begin(),latestPlayers.end(),[&](const auto&o){return o.slot==p.slot&&o.instance==p.instance&&o.character==p.character&&o.name==p.name;});if(old==latestPlayers.end()){auto name=p.name;for(auto&c:name)if(uint8_t(c)<32||c==127)c=' ';std::osyncstream(std::clog)<<"combat_player slot="<<unsigned(p.slot)<<" instance="<<p.instance<<" character="<<p.character<<" name="<<std::quoted(name)<<'\n';}}latestPlayers=reply->players;}
  if(busy&&done){if(worker.joinable())worker.join();busy=false;commonSettings.nativeSpecial->reset();enabled();if(closing)DestroyWindow(window);}
  special_refresh();
 }
 ~App(){cancel=true;if(worker.joinable())worker.join();if(font)DeleteObject(font);}
};
LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l){auto*a=reinterpret_cast<App*>(GetWindowLongPtrW(window,GWLP_USERDATA));if(message==WM_NCCREATE){a=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);a->window=window;SetWindowLongPtrW(window,GWLP_USERDATA,LONG_PTR(a));}if(!a)return DefWindowProcW(window,message,w,l);
 try{switch(message){case WM_CREATE:a->initialize();return 0;case WM_COMMAND:if(HIWORD(w)==BN_CLICKED){switch(LOWORD(w)){case EnvironmentOptions:a->environment_options();break;case SpecialOptions:ShowWindow(a->specialWindow,SW_SHOWNORMAL);SetForegroundWindow(a->specialWindow);break;case AllowSpecial:case SpecialNames:case RandomSpecial:a->special_settings();break;case AssignGekko:a->special_assign(special_pc::Kind::gekko);break;case AssignHuman:a->special_assign(special_pc::Kind::human);break;case RoundPlacementOptions:if(!a->busy)items::edit_round_items(a->window,a->keys.parent_path()/"round_items.cfg",a->keys.parent_path()/"item_drop_policy.json");break;case PlacementOptions:a->placement_options();break;case OptionsButton:a->options();break;case Login:a->login();break;case Start:a->start();break;case Stop:a->cancel=true;a->status(L"停止しています… 部屋を閉じるまでお待ちください。");break;}}return 0;case WM_TIMER:a->tick();return 0;case WM_CLOSE:if(a->busy){a->closing=true;a->cancel=true;a->status(L"部屋を閉じて終了しています…");}else DestroyWindow(window);return 0;case WM_DESTROY:PostQuitMessage(0);return 0;}}
 catch(...){a->status(L"処理を完了できませんでした。入力とデータファイルを確認してください。");}return DefWindowProcW(window,message,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR command,int show){
 if(command&&std::wstring_view(command)==L"--version"){std::cout<<"MGO2MTHOST "<<build_version<<std::endl;return 0;}
 bool diagnosticCheck=false;
 try{auto folder=executable_folder();CombatLog combatLog;App app;app.keys=folder/L"data"/L"network.gnk";
  int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);bool check=false,probe=false,smokeOptions=false,smokeSpecial=false,smokeRound=false,smokeEnvironment=false;std::filesystem::path report;
  for(int i=1;i<argc;++i){auto arg=std::wstring_view(argv[i]);if(arg==L"--check")check=true;else if(arg==L"--smoke-ui")app.smoke=true;else if(arg==L"--smoke-options")smokeOptions=true;else if(arg==L"--smoke-special"){smokeSpecial=true;app.smoke=true;}else if(arg==L"--smoke-round-items")smokeRound=true;else if(arg==L"--smoke-environment")smokeEnvironment=true;else if(arg==L"--probe-account")probe=true;else if(arg==L"--report"&&i+1<argc)report=argv[++i];else if(arg==L"--data"&&i+1<argc)app.keys=std::filesystem::path(argv[++i])/L"network.gnk";else{LocalFree(argv);return 2;}}LocalFree(argv);
  diagnosticCheck=check;
  if(smokeEnvironment){if(report.empty())return 2;SetProcessDPIAware();inspect_host_environment(report);return 0;}
  if(smokeRound){if(report.empty())return 2;SetProcessDPIAware();std::filesystem::create_directories(report);return items::edit_round_items(nullptr,app.keys.parent_path()/"round_items.cfg",app.keys.parent_path()/"item_drop_policy.json",report/"round_items.bmp")?0:3;}
  if(smokeOptions){if(report.empty())return 2;SetProcessDPIAware();inspect_host_options(report);return 0;}
  std::string hitGeometryError;const auto hitGeometry=host_hit::configure(app.keys.parent_path()/"character/hit_geometry.gwhit",hitGeometryError);
  if(hitGeometry==host_hit::ResourceStatus::invalid_resource){std::cerr<<"Hit geometry check failed: "<<hitGeometryError<<std::endl;if(!check)MessageBoxW(nullptr,wide(hitGeometryError).c_str(),L"MGO2MTHOST",MB_OK|MB_ICONERROR);return 9;}
  if(!special_pc::configure_gekko_jump(app.keys.parent_path()/"special/gekko_jump.gwjc",hitGeometryError)||combat::evade_runtime::configure(app.keys.parent_path()/"motion/evade_travel.gwet",hitGeometryError)==combat::evade_runtime::ResourceStatus::invalid_resource){std::cerr<<hitGeometryError<<std::endl;if(!check)MessageBoxW(nullptr,wide(hitGeometryError).c_str(),L"MGO2MTHOST",MB_OK|MB_ICONERROR);return 9;}
  if(smokeSpecial){if(report.empty())return 2;app.specialReport=report;}if(!smokeSpecial){NetworkKeys::load(app.keys);load_lobby_membership(app.keys.parent_path()/L"lobbies.cfg");host::settings_payload(host::Settings{});}if(check){auto lights=app.keys.parent_path()/"breakable_lights.cfg";if(std::filesystem::exists(lights))combat::load_breakable_lights(lights);auto health=app.keys.parent_path()/"combat_health.cfg";if(std::filesystem::exists(health))combat::HealthRules::load(health);auto cfg=app.keys.parent_path()/"round_items.cfg";if(std::filesystem::exists(cfg)){items::RoundItems settings;std::string error;if(!settings.load(cfg,error))return 7;}try{check_gameplay_configuration(app.keys.parent_path());}catch(const std::exception& error){std::cerr<<"HOST configuration check failed: "<<error.what()<<std::endl;return 8;}return 0;}
  if(probe){if(report.empty())return 2;LoginForm saved;if(!load_login(profile_path(),saved)||!saved.valid())return 4;AuthCredentials credentials(saved.credential(0),saved.credential(1));std::atomic_bool cancel=false;auto a=authenticate(credentials,cancel);CharacterReply c;if(a.status==AuthStatus::success)c=fetch_characters(app.keys,a,cancel);std::ofstream out(report);out<<"{\"auth_status\":"<<int(a.status)<<",\"http\":"<<a.http<<",\"character_status\":"<<int(c.status)<<",\"characters\":"<<c.list.entries.size()<<",\"error\":"<<c.error<<"}\n";if(!out)return 5;return a.status==AuthStatus::success&&c.status==CharacterStatus::success?0:6;}
  if(!app.smoke)combatLog.open();
  std::clog<<"Hit geometry: "<<(hitGeometry==host_hit::ResourceStatus::local_resource?"local_resource":"native_fallback")<<std::endl;
  SetProcessDPIAware();WNDCLASSW type{};type.lpfnWndProc=procedure;type.hInstance=instance;type.lpszClassName=L"MGO2MTHOST.Window";type.hCursor=LoadCursor(nullptr,IDC_ARROW);type.hbrBackground=HBRUSH(COLOR_BTNFACE+1);RegisterClassW(&type);
  auto window=CreateWindowW(type.lpszClassName,versioned_title(L"MGO2MTHOST — OpenMGO2").c_str(),WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,780,760,nullptr,nullptr,instance,&app);if(!window)return 3;ShowWindow(window,show);UpdateWindow(window);
  MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){if(!(app.specialWindow&&IsWindowVisible(app.specialWindow)&&IsDialogMessageW(app.specialWindow,&message))&&!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}return 0;
 }catch(const std::exception& error){std::cerr<<"MGO2MTHOST: "<<error.what()<<std::endl;if(!diagnosticCheck)MessageBoxW(nullptr,L"必要なローカル資産を確認してください。README.ja.md の EXCV 手順で data を準備できます。",L"MGO2MTHOST",MB_OK|MB_ICONERROR);return 1;}
 catch(...){std::cerr<<"MGO2MTHOST: initialization failed"<<std::endl;if(!diagnosticCheck)MessageBoxW(nullptr,L"data/network.gnk と data/lobbies.cfg を確認してください。",L"MGO2MTHOST",MB_OK|MB_ICONERROR);return 1;}
}
