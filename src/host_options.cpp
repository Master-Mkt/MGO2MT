#include "host_options.h"
#include "weapon_restrictions.h"
#include "menu_theme.h"
#include <vector>
#include <functional>
#include <fstream>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <utility>

namespace mgo2win {
namespace {
constexpr int TabCommon=2000,TabWeapons=2001,Master=2002,AllAllow=2003,AllLock=2004,Accept=IDOK,Cancel=IDCANCEL,CategoryBase=2100,FieldBase=3000,WeaponBase=4000;
enum class Inspection { apply,cancel,close };
std::wstring wide(std::string_view s){return {s.begin(),s.end()};}
struct Field {
 std::wstring label;HWND widget=nullptr;unsigned value=0,maximum=1;bool numeric=false,readonly=false;
 std::function<void(host::Settings&,unsigned)> assign;
};
struct Options {
 HWND window=nullptr,viewport=nullptr,owner=nullptr;HFONT font=nullptr;HBRUSH background=CreateSolidBrush(RGB(30,39,41));
 host::Settings draft;bool accepted=false,smoke=false;unsigned smokeStep=0;int page=0,category=0,scroll=0,focusRow=-1,wheelRemainder=0;
 Inspection inspection=Inspection::apply;std::string failure;
 std::filesystem::path output;std::vector<Field> fields;std::vector<HWND> weapons;std::vector<size_t> visibleWeapons;
 HWND control(int id)const{return GetDlgItem(window,id);}
 HWND add(HWND parent,const wchar_t*kind,std::wstring label,int id,int x,int y,int w,int h,DWORD style){
  auto v=CreateWindowExW(0,kind,label.c_str(),WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,x,y,w,h,parent,HMENU(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
  if(!v)throw std::runtime_error("host option control");SendMessageW(v,WM_SETFONT,WPARAM(font),TRUE);return v;
 }
 void notice(const wchar_t*s){SetWindowTextW(control(2200),s);}
 void boolean(std::wstring label,bool value,std::function<void(host::Settings&,unsigned)> assign){fields.push_back({std::move(label),nullptr,unsigned(value),1,false,false,std::move(assign)});}
 void number(std::wstring label,unsigned value,unsigned maximum,std::function<void(host::Settings&,unsigned)> assign){fields.push_back({std::move(label),nullptr,value,maximum,true,false,std::move(assign)});}
 void initialize(){
  font=CreateFontW(-19,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Yu Gothic UI");
  add(window,L"BUTTON",L"共通設定",TabCommon,20,14,370,44,BS_OWNERDRAW);
  add(window,L"BUTTON",L"武器制限",TabWeapons,400,14,370,44,BS_OWNERDRAW);
  for(int i=0;i<5;++i)add(window,L"BUTTON",wide(restrictions::category_name(restrictions::Category(i))),CategoryBase+i,20+i*151,70,147,36,BS_OWNERDRAW);
  add(window,L"BUTTON",L"武器制限を有効にする",Master,24,114,340,31,BS_AUTOCHECKBOX);
  SendMessageW(control(Master),BM_SETCHECK,restrictions::enabled(draft.weapon_restrictions)?BST_CHECKED:BST_UNCHECKED,0);
  add(window,L"BUTTON",L"この分類をすべて許可",AllAllow,380,114,185,31,BS_PUSHBUTTON);
  add(window,L"BUTTON",L"この分類をすべて禁止",AllLock,580,114,190,31,BS_PUSHBUTTON);
  viewport=CreateWindowExW(WS_EX_CONTROLPARENT,L"MGO2HOST.OptionsViewport",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL,20,76,750,454,window,nullptr,GetModuleHandleW(nullptr),this);
  if(!viewport)throw std::runtime_error("host option viewport");
  auto label=add(window,L"STATIC",L"",2200,20,548,750,65,SS_LEFT);SetWindowLongPtrW(label,GWL_STYLE,GetWindowLongPtrW(label,GWL_STYLE)&~WS_TABSTOP);
  add(window,L"BUTTON",L"この設定を使用",Accept,380,630,185,40,BS_DEFPUSHBUTTON);
  add(window,L"BUTTON",L"変更を取り消す",Cancel,580,630,190,40,BS_PUSHBUTTON);
  fields.push_back({L"ホスト専用",nullptr,1,1,false,true,{}});
  number(L"最大対戦人数（専用ホストを除く）",draft.capacity?unsigned(draft.capacity)-1:0,16,[](auto&s,unsigned v){if(v<1)throw std::invalid_argument("capacity");s.capacity=uint8_t(v+1);});
  number(L"ブリーフィング時間（分）",draft.briefing_minutes,60,[](auto&s,unsigned v){s.briefing_minutes=v;});
  boolean(L"個人戦績への反映",!draft.non_stat,[](auto&s,unsigned v){s.non_stat=!v;});
  boolean(L"フレンドリーファイアー",draft.friendly_fire,[](auto&s,unsigned v){s.friendly_fire=v!=0;});
  boolean(L"ロックオン（AUTO AIM）",draft.auto_aim,[](auto&s,unsigned v){s.auto_aim=v!=0;});
  boolean(L"ユニークキャラクターの登場",draft.uniques,[](auto&s,unsigned v){s.uniques=v!=0;});
  boolean(L"敵のネームタグ表示",draft.enemy_nametags,[](auto&s,unsigned v){s.enemy_nametags=v!=0;});
  boolean(L"サイレントモード",draft.silent,[](auto&s,unsigned v){s.silent=v!=0;});
  boolean(L"チーム自動振り分け",draft.auto_assign,[](auto&s,unsigned v){s.auto_assign=v!=0;});
  boolean(L"チーム場所交代",draft.teams_switch,[](auto&s,unsigned v){s.teams_switch=v!=0;});
  boolean(L"幽霊のいたずら",draft.ghosts,[](auto&s,unsigned v){s.ghosts=v!=0;});
  fields.push_back({L"クイック参加許可（現在は許可しない）",nullptr,0,1,false,true,{}});
  boolean(L"LEVEL制限",draft.level_limit,[](auto&s,unsigned v){s.level_limit=v!=0;});
  number(L"LEVEL基準（現在は22固定）",22,64,[](auto&s,unsigned){s.level_limit_base=22;});fields.back().readonly=true;
  number(L"LEVEL制限の許容幅",draft.level_limit_tolerance,63,[](auto&s,unsigned v){s.level_limit_tolerance=uint8_t(v);});
  boolean(L"ボイスチャット",draft.voice_chat,[](auto&s,unsigned v){s.voice_chat=v!=0;});
  number(L"チームキルキック（0：無効）",draft.team_kill_kick,99,[](auto&s,unsigned v){s.team_kill_kick=uint8_t(v);});
  number(L"アイドルキック（分・0：無効）",draft.idle_kick_minutes,99,[](auto&s,unsigned v){s.idle_kick_minutes=uint8_t(v);});
  for(size_t i=0;i<fields.size();++i){auto&f=fields[i];f.widget=add(viewport,f.numeric?L"EDIT":L"COMBOBOX",f.numeric?std::to_wstring(f.value):L"",FieldBase+int(i),390,int(i)*36+4,326,f.numeric?28:240,f.numeric?ES_NUMBER|ES_AUTOHSCROLL:CBS_DROPDOWNLIST|WS_VSCROLL);
   if(f.numeric)SendMessageW(f.widget,EM_SETLIMITTEXT,3,0);else{SendMessageW(f.widget,CB_ADDSTRING,0,LPARAM(L"無効"));SendMessageW(f.widget,CB_ADDSTRING,0,LPARAM(L"有効"));SendMessageW(f.widget,CB_SETCURSEL,f.value,0);}EnableWindow(f.widget,!f.readonly);
  }
  for(size_t i=0;i<restrictions::catalog().size();++i)weapons.push_back(add(viewport,L"BUTTON",L"",WeaponBase+int(i),4,0,716,34,BS_OWNERDRAW|BS_NOTIFY));
  select_page(0);if(smoke&&!SetTimer(window,1,80,nullptr))throw std::runtime_error("host option inspection timer");
 }
 int row_count()const{return page?int(visibleWeapons.size()):int(fields.size());}
 void layout(){
  int height=page?380:454;SetWindowPos(viewport,nullptr,20,page?150:76,750,height,SWP_NOZORDER);
  scroll=std::clamp(scroll,0,std::max(0,row_count()*36-height));SCROLLINFO info{sizeof(info),SIF_RANGE|SIF_PAGE|SIF_POS,0,std::max(0,row_count()*36-1),UINT(height),scroll,0};SetScrollInfo(viewport,SB_VERT,&info,TRUE);
  for(size_t i=0;i<fields.size();++i){ShowWindow(fields[i].widget,page?SW_HIDE:SW_SHOW);if(!page)SetWindowPos(fields[i].widget,nullptr,390,int(i)*36+4-scroll,326,fields[i].numeric?28:240,SWP_NOZORDER);}
  for(size_t i=0;i<weapons.size();++i)if(!page||int(restrictions::catalog()[i].category)!=category)ShowWindow(weapons[i],SW_HIDE);
  if(page)for(size_t row=0;row<visibleWeapons.size();++row){auto i=visibleWeapons[row];auto&e=restrictions::catalog()[i];auto state=restrictions::state(draft.weapon_restrictions,e);std::wstring text=state==restrictions::LockState::unlocked?L"許可  ":state==restrictions::LockState::mixed?L"一部禁止  ":L"禁止  ";text+=wide(e.display_name);if(e.dp_only.value_or(false))text+=L"    [DP]";SetWindowTextW(weapons[i],text.c_str());SetWindowPos(weapons[i],nullptr,4,int(row)*36-scroll,716,34,SWP_NOZORDER|SWP_SHOWWINDOW);}
  InvalidateRect(viewport,nullptr,TRUE);
 }
 void select_page(int selected){page=selected;scroll=0;focusRow=-1;visibleWeapons.clear();for(size_t i=0;i<restrictions::catalog().size();++i)if(int(restrictions::catalog()[i].category)==category)visibleWeapons.push_back(i);
  for(int i:{Master,AllAllow,AllLock,CategoryBase,CategoryBase+1,CategoryBase+2,CategoryBase+3,CategoryBase+4})ShowWindow(control(i),page?SW_SHOW:SW_HIDE);
  layout();InvalidateRect(window,nullptr,TRUE);
  notice(page?L"各項目で許可／禁止を切り替えます。[DP]はDP用の候補です。制限を無効にしても選択は保持します。":L"部屋を作成する際に使用する共通設定です。実対戦での各設定の効果・特殊キャラクター選択は開発中です。");
 }
 void focus(int id){
  if(id>=FieldBase&&id<FieldBase+int(fields.size()))focusRow=id-FieldBase;
  else if(id>=WeaponBase){auto it=std::find(visibleWeapons.begin(),visibleWeapons.end(),size_t(id-WeaponBase));if(it==visibleWeapons.end())return;focusRow=int(it-visibleWeapons.begin());}else return;
  int height=page?380:454,top=focusRow*36;if(top<scroll)scroll=top;else if(top+36>scroll+height)scroll=top+36-height;layout();
 }
 unsigned read(const Field&f){
  if(!f.numeric){auto v=SendMessageW(f.widget,CB_GETCURSEL,0,0);if(v<0||v>1)throw std::invalid_argument("choice");return unsigned(v);}
  const int length=GetWindowTextLengthW(f.widget);if(length<1||length>3)throw std::invalid_argument("number length");
  wchar_t value[4]{};if(GetWindowTextW(f.widget,value,4)!=length)throw std::invalid_argument("number");unsigned n=0;for(auto c:std::wstring_view(value)){if(c<L'0'||c>L'9')throw std::invalid_argument("number");n=n*10+unsigned(c-L'0');if(n>f.maximum)throw std::invalid_argument("range");}return n;
 }
 bool commit(){auto next=draft;for(size_t i=0;i<fields.size();++i){auto&f=fields[i];if(!f.assign)continue;try{f.assign(next,f.readonly?f.value:read(f));}catch(...){select_page(0);focus(FieldBase+int(i));SetFocus(f.widget);std::wstring message=f.label+L" の値を確認してください（上限 "+std::to_wstring(f.maximum)+L"）。";notice(message.c_str());return false;}}
  try{host::settings_payload(next);host::room_environment(next);}catch(...){notice(L"設定値を送信形式へ変換できません。入力を確認してください。");return false;}
  draft=std::move(next);accepted=true;return true;
 }
 void command(int id,int code){
  if(id>=FieldBase&&id<WeaponBase){if(code==CBN_SETFOCUS||code==EN_SETFOCUS)focus(id);return;}
  if(id>=WeaponBase&&id<WeaponBase+int(weapons.size())){if(code==BN_SETFOCUS)focus(id);if(code==BN_CLICKED){focus(id);const auto&e=restrictions::catalog()[id-WeaponBase];restrictions::set_locked(draft.weapon_restrictions,e,!restrictions::locked(draft.weapon_restrictions,e));layout();}return;}
  if(code!=BN_CLICKED)return;
  if(id==TabCommon||id==TabWeapons){select_page(id==TabWeapons);return;}
  if(id>=CategoryBase&&id<CategoryBase+5){category=id-CategoryBase;select_page(1);return;}
  if(id==Master){restrictions::set_enabled(draft.weapon_restrictions,SendMessageW(control(Master),BM_GETCHECK,0,0)==BST_CHECKED);return;}
  if(id==AllAllow||id==AllLock){restrictions::set_category_locked(draft.weapon_restrictions,restrictions::Category(category),id==AllLock);layout();return;}
  if(id==Accept){if(commit())DestroyWindow(window);}else if(id==Cancel)DestroyWindow(window);
 }
 void paint(HDC dc,bool view){RECT r{};GetClientRect(view?viewport:window,&r);FillRect(dc,&r,background);if(!view)return;
  if(!page){SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(233,236,224));for(size_t i=0;i<fields.size();++i){int y=int(i)*36-scroll;menu_row(dc,0,y,r.right,35,i,int(i)==focusRow);RECT label{12,y+6,382,y+32};DrawTextW(dc,fields[i].label.c_str(),-1,&label,DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);}}
 }
 void draw_item(const DRAWITEMSTRUCT&d){int id=int(d.CtlID);auto r=d.rcItem;int width=r.right-r.left,height=r.bottom-r.top;bool selected=id==TabCommon?!page:id==TabWeapons?page:id>=CategoryBase&&id<CategoryBase+5?id-CategoryBase==category:false;
  if(id>=WeaponBase){auto index=size_t(id-WeaponBase);auto it=std::find(visibleWeapons.begin(),visibleWeapons.end(),index);menu_row(d.hDC,r.left,r.top,width,height,size_t(it-visibleWeapons.begin()),(d.itemState&ODS_FOCUS)!=0);}
  else{menu_rect(d.hDC,r.left,r.top,width,height,RGB(30,39,41));menu_tab(d.hDC,r.left,r.top,width,height,selected);}
  wchar_t text[160]{};GetWindowTextW(d.hwndItem,text,160);SelectObject(d.hDC,font);SetTextColor(d.hDC,RGB(240,237,222));SetBkMode(d.hDC,TRANSPARENT);r.left+=10;r.right-=8;if(id<WeaponBase)r.top+=5;DrawTextW(d.hDC,text,-1,&r,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|(id<WeaponBase?DT_CENTER:DT_LEFT));
  if(d.itemState&ODS_FOCUS){r=d.rcItem;InflateRect(&r,-2,-2);DrawFocusRect(d.hDC,&r);}
 }
 void capture(std::wstring name){
  if(inspection!=Inspection::apply)return;
  RECT r{};GetClientRect(window,&r);auto dc=GetDC(window),mem=CreateCompatibleDC(dc);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;info.bmiHeader.biHeight=-r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;void*pixels=nullptr;auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);if(!bitmap||!mem){if(bitmap)DeleteObject(bitmap);if(mem)DeleteDC(mem);if(dc)ReleaseDC(window,dc);throw std::runtime_error("options capture");}auto old=SelectObject(mem,bitmap);
  // Print every client at its actual origin. WM_PRINT on a hidden top-level
  // window offsets standard children by its non-client frame on some hosts.
  paint(mem,false);print_children(window,mem);GdiFlush();BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+r.right*r.bottom*4;std::ofstream out(output/name,std::ios::binary);out.write(reinterpret_cast<char*>(&file),sizeof(file));out.write(reinterpret_cast<char*>(&info.bmiHeader),sizeof(info.bmiHeader));out.write(static_cast<char*>(pixels),r.right*r.bottom*4);SelectObject(mem,old);DeleteObject(bitmap);DeleteDC(mem);ReleaseDC(window,dc);if(!out)throw std::runtime_error("options capture write");
 }
 void print_children(HWND parent,HDC dc){
  for(auto child=GetWindow(parent,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
   if(!(GetWindowLongPtrW(child,GWL_STYLE)&WS_VISIBLE))continue;
   RECT rect{};GetClientRect(child,&rect);POINT origin{};ClientToScreen(child,&origin);ScreenToClient(window,&origin);
   auto saved=SaveDC(dc);SetViewportOrgEx(dc,origin.x,origin.y,nullptr);IntersectClipRect(dc,0,0,rect.right,rect.bottom);
   if(child==viewport){paint(dc,true);print_children(child,dc);}
   else if((GetWindowLongPtrW(child,GWL_STYLE)&BS_TYPEMASK)==BS_OWNERDRAW&&GetDlgCtrlID(child)<FieldBase){DRAWITEMSTRUCT d{ODT_BUTTON,UINT(GetDlgCtrlID(child)),0,ODA_DRAWENTIRE,0,child,dc,rect,0};draw_item(d);}
   else if(GetDlgCtrlID(child)>=WeaponBase){DRAWITEMSTRUCT d{ODT_BUTTON,UINT(GetDlgCtrlID(child)),0,ODA_DRAWENTIRE,0,child,dc,rect,0};draw_item(d);}
   else SendMessageW(child,WM_PRINTCLIENT,WPARAM(dc),PRF_CLIENT|PRF_ERASEBKGND);
   RestoreDC(dc,saved);
  }
 }
 void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
 void inspect_inputs(){
  check(fields.size()==19,"options field count changed; update inspection");
  check(!IsWindowEnabled(fields[0].widget)&&!IsWindowEnabled(fields[12].widget)&&!IsWindowEnabled(fields[14].widget),"fixed options must be disabled");
  for(auto [row,value]:{std::pair{1,L"7"},{2,L"7"},{15,L"63"},{17,L"99"},{18,L"0"}})SetWindowTextW(fields[row].widget,value);
  for(auto [row,value]:{std::pair{3,0},{4,1},{5,0},{6,1},{7,0},{8,1},{9,0},{10,0},{11,1},{13,1},{16,0}})SendMessageW(fields[row].widget,CB_SETCURSEL,value,0);
 }
 void inspect_rejection(unsigned row,const wchar_t* value){
  wchar_t previous[16]{};GetWindowTextW(fields[row].widget,previous,16);
  const auto original=host::settings_payload(draft);const auto environment=host::room_environment(draft);
  SetWindowTextW(fields[row].widget,value);command(Accept,BN_CLICKED);
  check(IsWindow(window)&&!accepted&&page==0&&focusRow==int(row),"invalid input must stay open and identify its field");
  check(host::settings_payload(draft)==original&&host::room_environment(draft)==environment,"invalid Apply must not partially update the draft");
  check(int(row)*36>=scroll&&int(row)*36+36<=scroll+454,"invalid field must scroll into view");
  SetWindowTextW(fields[row].widget,previous);
 }
 void inspect(){
  if(++smokeStep==1){inspect_inputs();focus(FieldBase+4);check(scroll==0,"top focus scroll");capture(L"host_common_top.bmp");}
  else if(smokeStep==2){focus(FieldBase+18);check(scroll>0,"bottom field must scroll into view");capture(L"host_common_bottom.bmp");}
  else if(smokeStep==3){
   command(TabWeapons,BN_CLICKED);capture(L"host_weapons.bmp");
   SendMessageW(control(Master),BM_SETCHECK,BST_CHECKED,0);command(Master,BN_CLICKED);
   const auto saved=draft.weapon_restrictions;command(AllLock,BN_CLICKED);
   auto mask=restrictions::category_mask(restrictions::Category::primary);
   for(size_t i=0;i<mask.size();++i)check(draft.weapon_restrictions[i]==uint8_t(saved[i]|mask[i]),"category All Lock boundary");
   capture(L"host_weapons_locked.bmp");command(AllAllow,BN_CLICKED);check(draft.weapon_restrictions==saved,"category All Allow boundary");
   const auto* ak=restrictions::find("ak");check(ak!=nullptr,"AK restriction entry");
   const auto index=ak-restrictions::catalog().data();command(WeaponBase+int(index),BN_CLICKED);
   check(restrictions::effective_locked(draft.weapon_restrictions,*ak),"individual weapon control must edit the draft");
   command(CategoryBase+2,BN_CLICKED);const auto primary=draft.weapon_restrictions;command(AllLock,BN_CLICKED);command(AllAllow,BN_CLICKED);
   check(draft.weapon_restrictions==primary,"another category must preserve AK and unknown bits");
   if(inspection==Inspection::cancel)command(Cancel,BN_CLICKED);
   else if(inspection==Inspection::close)SendMessageW(window,WM_CLOSE,0,0);
  }
  else if(smokeStep==4){
   for(auto value:{L"0",L"17"})inspect_rejection(1,value);
   for(auto value:{L"61",L"-1",L"",L"1a",L"00000000000000000000100"})inspect_rejection(2,value);
   inspect_rejection(15,L"64");inspect_rejection(17,L"100");inspect_rejection(18,L"100");
   SendMessageW(fields[4].widget,CB_SETCURSEL,WPARAM(-1),0);command(Accept,BN_CLICKED);
   check(IsWindow(window)&&!accepted&&focusRow==4,"invalid boolean selection must be rejected");SendMessageW(fields[4].widget,CB_SETCURSEL,1,0);
   // Capture a visible range error after proving empty, nonnumeric and overflow rejection.
   SetWindowTextW(fields[2].widget,L"61");command(Accept,BN_CLICKED);capture(L"host_invalid_range.bmp");SetWindowTextW(fields[2].widget,L"7");
  }
  else if(smokeStep==5){command(Accept,BN_CLICKED);check(accepted&&!IsWindow(window),"valid Apply must close and accept");}
 }
 ~Options(){if(font)DeleteObject(font);DeleteObject(background);}
};
LRESULT CALLBACK procedure(HWND w,UINT message,WPARAM wp,LPARAM lp){
 auto*p=reinterpret_cast<Options*>(GetWindowLongPtrW(w,GWLP_USERDATA));if(message==WM_NCCREATE){p=static_cast<Options*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(w,GWLP_USERDATA,LONG_PTR(p));if(!p->window)p->window=w;}if(!p)return DefWindowProcW(w,message,wp,lp);
 bool view=w!=p->window;
 try{switch(message){
 case WM_CREATE:if(!view)p->initialize();return 0;
 case WM_PAINT:{PAINTSTRUCT paint;auto dc=BeginPaint(w,&paint);p->paint(dc,view);EndPaint(w,&paint);return 0;}
 case WM_PRINTCLIENT:p->paint(HDC(wp),view);return 0;
 case WM_ERASEBKGND:p->paint(HDC(wp),view);return 1;
 case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:SetTextColor(HDC(wp),RGB(237,239,226));SetBkColor(HDC(wp),RGB(30,39,41));return LRESULT(p->background);
 case WM_DRAWITEM:p->draw_item(*reinterpret_cast<DRAWITEMSTRUCT*>(lp));return TRUE;
 case WM_COMMAND:p->command(LOWORD(wp),HIWORD(wp));return 0;
 case DM_GETDEFID:if(!view)return MAKELRESULT(Accept,DC_HASDEFID);break;
 case WM_VSCROLL:if(view){SCROLLINFO s{sizeof(s),SIF_TRACKPOS};GetScrollInfo(w,SB_VERT,&s);switch(LOWORD(wp)){case SB_LINEUP:p->scroll-=36;break;case SB_LINEDOWN:p->scroll+=36;break;case SB_PAGEUP:p->scroll-=324;break;case SB_PAGEDOWN:p->scroll+=324;break;case SB_TOP:p->scroll=0;break;case SB_BOTTOM:p->scroll=p->row_count()*36;break;case SB_THUMBTRACK:case SB_THUMBPOSITION:p->scroll=s.nTrackPos;break;}p->layout();}return 0;
 case WM_MOUSEWHEEL:p->wheelRemainder+=GET_WHEEL_DELTA_WPARAM(wp);p->scroll-=p->wheelRemainder/WHEEL_DELTA*108;p->wheelRemainder%=WHEEL_DELTA;p->layout();return 0;
 case WM_TIMER:if(p->smoke)p->inspect();return 0;
 case WM_CLOSE:DestroyWindow(p->window);return 0;
 }}catch(const std::exception& e){p->failure=e.what();p->accepted=false;if(message==WM_CREATE)return -1;if(p->smoke)DestroyWindow(p->window);else p->notice(L"設定画面の処理に失敗しました。変更を取り消して開き直してください。");return 0;}
 catch(...){p->failure="unknown options error";p->accepted=false;if(message==WM_CREATE)return -1;if(p->smoke)DestroyWindow(p->window);else p->notice(L"設定画面の処理に失敗しました。変更を取り消して開き直してください。");return 0;}
 return DefWindowProcW(w,message,wp,lp);
}
bool run(HWND owner,host::Settings&settings,const std::filesystem::path&output,Inspection inspection=Inspection::apply){
 Options p;p.owner=owner;p.draft=settings;p.smoke=!output.empty();p.output=output;p.inspection=inspection;if(p.smoke)std::filesystem::create_directories(output);
 for(auto name:{L"MGO2HOST.Options",L"MGO2HOST.OptionsViewport"}){WNDCLASSW c{};c.lpfnWndProc=procedure;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=name;c.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&c);}
 auto w=CreateWindowExW(WS_EX_CONTROLPARENT|WS_EX_DLGMODALFRAME,L"MGO2HOST.Options",L"MGO2HOST — 共通設定 / 武器制限",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,810,725,owner,nullptr,GetModuleHandleW(nullptr),&p);
 if(!w)throw std::runtime_error(p.failure.empty()?"host options window":p.failure);
 const auto priorFocus=GetFocus();const bool ownerEnabled=owner&&IsWindowEnabled(owner);
 if(ownerEnabled)EnableWindow(owner,FALSE);ShowWindow(w,p.smoke?SW_HIDE:SW_SHOW);UpdateWindow(w);if(!p.smoke)SetFocus(p.control(TabCommon));
 MSG m{};while(IsWindow(w)){auto value=GetMessageW(&m,nullptr,0,0);if(value<=0){if(value==0)PostQuitMessage(int(m.wParam));break;}if(!IsDialogMessageW(w,&m)){TranslateMessage(&m);DispatchMessageW(&m);}}
 if(IsWindow(w))DestroyWindow(w);if(ownerEnabled&&IsWindow(owner)){EnableWindow(owner,TRUE);SetActiveWindow(owner);if(priorFocus&&IsWindow(priorFocus)&&(priorFocus==owner||IsChild(owner,priorFocus)))SetFocus(priorFocus);}
 if(p.smoke&&!p.failure.empty())throw std::runtime_error(p.failure);
 if(p.accepted)settings=p.draft;return p.accepted;
}
}
bool edit_host_options(HWND owner,host::Settings&settings){return run(owner,settings,{});}
void inspect_host_options(const std::filesystem::path&output){
 host::Settings original;original.weapon_restrictions[15]=0xa5;auto applied=original;
 if(!run(nullptr,applied,output))throw std::runtime_error("host options inspection failed");
 auto expected=original;expected.capacity=8;expected.briefing_minutes=7;expected.non_stat=true;expected.friendly_fire=true;expected.auto_aim=false;expected.uniques=true;expected.enemy_nametags=false;expected.silent=true;expected.auto_assign=false;expected.teams_switch=false;expected.ghosts=true;expected.level_limit=true;expected.level_limit_base=22;expected.level_limit_tolerance=63;expected.voice_chat=false;expected.team_kill_kick=99;expected.idle_kick_minutes=0;
 restrictions::set_enabled(expected.weapon_restrictions,true);restrictions::set_locked(expected.weapon_restrictions,*restrictions::find("ak"),true);
 if(host::settings_payload(applied)!=host::settings_payload(expected)||host::room_environment(applied)!=host::room_environment(expected))throw std::runtime_error("Apply did not preserve exact edited settings/wire fields");
 for(auto mode:{Inspection::cancel,Inspection::close}){
  auto cancelled=original;if(run(nullptr,cancelled,output,mode))throw std::runtime_error("Cancel or Close unexpectedly accepted settings");
  if(host::settings_payload(cancelled)!=host::settings_payload(original)||host::room_environment(cancelled)!=host::room_environment(original))throw std::runtime_error("Cancel or Close changed caller settings");
 }
 std::ofstream report(output/L"validation.json");report<<"{\"mode\":\"offline_hidden_WM_PRINT\",\"apply_edits_verified\":true,\"cancel_unchanged\":true,\"close_unchanged\":true,\"invalid_numeric_cases\":10,\"invalid_choice_rejected\":true,\"readonly_fields_verified\":true,\"category_masks_preserved\":true,\"wire_fields_verified\":true}\n";
 if(!report)throw std::runtime_error("host options inspection report");
}
}
