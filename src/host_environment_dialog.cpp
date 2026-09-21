#include "host_environment_dialog.h"
#include <commdlg.h>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace mgo2mt {
namespace {
enum {Mode=100,Fog,Rain,Snow,Sand,Time,Sound,Hemisphere,Upper,Lower,Gain,Help};
struct Dialog {
 HWND window=nullptr;HFONT font=nullptr;environment::Config value;bool accepted=false,inspect=false;std::filesystem::path output;std::wstring failure;
 HWND control(int id)const{return GetDlgItem(window,id);}
 void add(const wchar_t*type,const wchar_t*text,int id,int x,int y,int w,int h,DWORD style=0){auto c=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|(id?WS_TABSTOP:0)|style,x,y,w,h,window,HMENU(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);if(!c)throw std::runtime_error("Environment control");SendMessageW(c,WM_SETFONT,WPARAM(font),TRUE);}
 void choice(int id,const wchar_t*label,int y,std::initializer_list<const wchar_t*> names,unsigned selected){add(L"STATIC",label,0,24,y+5,218,26);add(L"COMBOBOX",L"",id,250,y,324,220,CBS_DROPDOWNLIST|WS_VSCROLL);for(auto name:names)SendMessageW(control(id),CB_ADDSTRING,0,LPARAM(name));SendMessageW(control(id),CB_SETCURSEL,selected,0);}
 void initialize(){
  font=CreateFontW(-19,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Yu Gothic UI");
  choice(Mode,L"天候",18,{L"ステージの設定",L"手動で設定"},value.weatherOverride);
  choice(Fog,L"フォグ",58,{L"なし",L"あり"},value.fog);choice(Rain,L"雨",98,{L"なし",L"あり"},value.rain);choice(Snow,L"雪・吹雪",138,{L"なし",L"あり"},value.snow);choice(Sand,L"砂嵐",178,{L"なし",L"あり"},value.sandstorm);
  choice(Time,L"時間帯の明かり",228,{L"ステージの設定",L"夜明け",L"朝",L"昼",L"夕方",L"夜"},unsigned(value.time));
  choice(Sound,L"背景環境音",268,{L"ステージの設定",L"雨（未収録）",L"吹雪（未収録）",L"砂嵐（未収録）",L"森（未収録）",L"無音"},unsigned(value.sound));
  choice(Hemisphere,L"半球ライト",318,{L"ステージ・時間帯の設定",L"色を手動で設定"},value.manualHemisphere);
  add(L"STATIC",L"上側の色",0,24,367,210,26);add(L"BUTTON",L"",Upper,250,360,324,34,BS_OWNERDRAW);add(L"STATIC",L"下側の色",0,24,407,210,26);add(L"BUTTON",L"",Lower,250,400,324,34,BS_OWNERDRAW);
  add(L"STATIC",L"明るさ（0.000～4.000）",0,24,447,220,26);std::wostringstream gain;gain<<std::fixed<<std::setprecision(3)<<value.gainMilli/1000.;add(L"EDIT",gain.str().c_str(),Gain,250,441,154,30,ES_AUTOHSCROLL|WS_BORDER);SendMessageW(control(Gain),EM_SETLIMITTEXT,8,0);
  add(L"STATIC",L"手動の半球ライトは時間帯より優先します。\n適用すると参加中のプレイヤーにも反映されます。",Help,24,485,550,52);
  add(L"BUTTON",L"適用",IDOK,270,550,144,38,BS_DEFPUSHBUTTON);add(L"BUTTON",L"取り消す",IDCANCEL,430,550,144,38,BS_PUSHBUTTON);enable();if(inspect)SetTimer(window,1,100,nullptr);
 }
 unsigned selected(int id,unsigned limit)const{auto value=SendMessageW(control(id),CB_GETCURSEL,0,0);if(value<0||value>limit)throw std::runtime_error("Environment selection");return unsigned(value);}
 void enable(){const bool weather=SendMessageW(control(Mode),CB_GETCURSEL,0,0)==1,manual=SendMessageW(control(Hemisphere),CB_GETCURSEL,0,0)==1;for(int id:{Fog,Rain,Snow,Sand})EnableWindow(control(id),weather);for(int id:{Upper,Lower,Gain})EnableWindow(control(id),manual);}
 bool commit(){try{
  auto next=value;next.weatherOverride=selected(Mode,1)!=0;next.fog=selected(Fog,1)!=0;next.rain=selected(Rain,1)!=0;next.snow=selected(Snow,1)!=0;next.sandstorm=selected(Sand,1)!=0;next.time=environment::Time(selected(Time,5));next.sound=environment::Sound(selected(Sound,5));next.manualHemisphere=selected(Hemisphere,1)!=0;
  wchar_t text[32]{};GetWindowTextW(control(Gain),text,32);wchar_t* end=nullptr;const double gain=wcstod(text,&end);if(end==text||*end||!std::isfinite(gain)||gain<0||gain>4)throw std::runtime_error("brightness");next.gainMilli=uint16_t(std::lround(gain*1000));if(!environment::valid(next))throw std::runtime_error("Environment range");value=next;accepted=true;return true;
 }catch(...){SetWindowTextW(control(Help),L"明るさは 0.000～4.000 の数値で入力してください。");SetFocus(control(Gain));return false;}}
 void color(int id){auto&rgb=id==Upper?value.upper:value.lower;static COLORREF custom[16]{};CHOOSECOLORW cc{sizeof(cc)};cc.hwndOwner=window;cc.rgbResult=RGB(rgb[0],rgb[1],rgb[2]);cc.lpCustColors=custom;cc.Flags=CC_FULLOPEN|CC_RGBINIT;if(ChooseColorW(&cc)){rgb={GetRValue(cc.rgbResult),GetGValue(cc.rgbResult),GetBValue(cc.rgbResult)};InvalidateRect(control(id),nullptr,TRUE);}}
 void draw(const DRAWITEMSTRUCT&d){const auto rgb=d.CtlID==Upper?value.upper:value.lower;auto brush=CreateSolidBrush(RGB(rgb[0],rgb[1],rgb[2]));FillRect(d.hDC,&d.rcItem,brush);DeleteObject(brush);FrameRect(d.hDC,&d.rcItem,HBRUSH(GetStockObject(BLACK_BRUSH)));SetBkMode(d.hDC,TRANSPARENT);SetTextColor(d.hDC,2*rgb[1]+rgb[0]+rgb[2]>510?RGB(0,0,0):RGB(255,255,255));SelectObject(d.hDC,font);const auto label=L"色を選ぶ  "+std::to_wstring(rgb[0])+L","+std::to_wstring(rgb[1])+L","+std::to_wstring(rgb[2]);auto rect=d.rcItem;DrawTextW(d.hDC,label.c_str(),-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
 void capture(){std::filesystem::create_directories(output);RECT r{};GetClientRect(window,&r);auto dc=GetDC(window),mem=CreateCompatibleDC(dc);BITMAPINFO bi{};bi.bmiHeader={sizeof(BITMAPINFOHEADER),r.right,-r.bottom,1,32,BI_RGB};void*pixels=nullptr;auto bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&pixels,nullptr,0);if(!bmp||!mem)throw std::runtime_error("Environment capture");auto prior=SelectObject(mem,bmp);PrintWindow(window,mem,PW_CLIENTONLY);GdiFlush();BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+r.right*r.bottom*4;std::ofstream out(output/"environment-settings.bmp",std::ios::binary);out.write(reinterpret_cast<char*>(&file),sizeof(file));out.write(reinterpret_cast<char*>(&bi.bmiHeader),sizeof(bi.bmiHeader));out.write(static_cast<char*>(pixels),r.right*r.bottom*4);SelectObject(mem,prior);DeleteObject(bmp);DeleteDC(mem);ReleaseDC(window,dc);if(!out)throw std::runtime_error("Environment capture write");}
};
LRESULT CALLBACK procedure(HWND w,UINT m,WPARAM a,LPARAM b){auto*d=reinterpret_cast<Dialog*>(GetWindowLongPtrW(w,GWLP_USERDATA));if(m==WM_NCCREATE){d=static_cast<Dialog*>(reinterpret_cast<CREATESTRUCTW*>(b)->lpCreateParams);d->window=w;SetWindowLongPtrW(w,GWLP_USERDATA,LONG_PTR(d));}if(!d)return DefWindowProcW(w,m,a,b);try{switch(m){
 case WM_CREATE:d->initialize();return 0;
 case WM_COMMAND:if(HIWORD(a)==CBN_SELCHANGE){d->enable();return 0;}if(HIWORD(a)==BN_CLICKED){auto id=LOWORD(a);if(id==Upper||id==Lower)d->color(id);if(id==IDOK&&d->commit())DestroyWindow(w);if(id==IDCANCEL)DestroyWindow(w);}return 0;
 case WM_DRAWITEM:d->draw(*reinterpret_cast<DRAWITEMSTRUCT*>(b));return TRUE;
 case WM_TIMER:if(d->inspect){KillTimer(w,1);const auto original=d->value;SetWindowTextW(d->control(Gain),L"nan");if(d->commit()||d->value!=original)throw std::runtime_error("Invalid environment edit mutated draft");for(int id:{Mode,Fog,Rain,Snow,Sand,Hemisphere})SendMessageW(d->control(id),CB_SETCURSEL,1,0);SendMessageW(d->control(Time),CB_SETCURSEL,5,0);SendMessageW(d->control(Sound),CB_SETCURSEL,3,0);SetWindowTextW(d->control(Gain),L"1.750");SetWindowTextW(d->control(Help),L"手動の半球ライトは時間帯より優先します。\n適用すると参加中のプレイヤーにも反映されます。");d->enable();UpdateWindow(w);d->capture();if(!d->commit()||d->value.gainMilli!=1750)throw std::runtime_error("Environment edit apply failed");DestroyWindow(w);}return 0;
 case WM_CLOSE:DestroyWindow(w);return 0;case WM_DESTROY:if(d->font){DeleteObject(d->font);d->font=nullptr;}return 0;
 }}catch(...){d->failure=L"環境設定の表示を完了できませんでした。";DestroyWindow(w);}return DefWindowProcW(w,m,a,b);}
bool run(HWND owner,Dialog&dialog){WNDCLASSW c{};c.lpfnWndProc=procedure;c.hInstance=GetModuleHandleW(nullptr);c.hCursor=LoadCursor(nullptr,IDC_ARROW);c.hbrBackground=HBRUSH(COLOR_BTNFACE+1);c.lpszClassName=L"MGO2MTHOST.Environment";RegisterClassW(&c);const bool enabled=owner&&IsWindowEnabled(owner);if(enabled)EnableWindow(owner,FALSE);auto window=CreateWindowExW(WS_EX_DLGMODALFRAME,c.lpszClassName,L"天候・照明・環境音 / HOST",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,620,638,owner,nullptr,c.hInstance,&dialog);if(!window){if(enabled)EnableWindow(owner,TRUE);throw std::runtime_error("Environment dialog");}ShowWindow(window,SW_SHOW);MSG msg{};while(IsWindow(window)&&GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(window,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}if(enabled){EnableWindow(owner,TRUE);SetActiveWindow(owner);}if(!dialog.failure.empty())throw std::runtime_error("Environment dialog failed");return dialog.accepted;}
}
bool edit_host_environment(HWND owner,environment::Config& config){Dialog dialog;dialog.value=config;if(!run(owner,dialog))return false;config=dialog.value;return true;}
void inspect_host_environment(const std::filesystem::path&output){Dialog d;d.inspect=true;d.output=output;if(!run(nullptr,d))throw std::runtime_error("Environment inspection");environment::save(output/"environment.cfg",d.value);if(environment::load(output/"environment.cfg")!=d.value)throw std::runtime_error("Environment persist test");std::ofstream(output/"validation.json")<<"{\"offline\":true,\"network\":false,\"invalid_draft_rejected\":true,\"dropdowns\":8,\"colors\":2,\"gain\":1.75,\"persist\":true}\n";}
}
