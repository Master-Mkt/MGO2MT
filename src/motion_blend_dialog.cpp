#include "motion_blend_dialog.h"
#include <array>
#include <vector>
#include <cwchar>
namespace mgo2win::motion_blend {
namespace {
enum {rateId=100,descriptionId=101,statusId=102,defaultId=103};
HWND control(HWND dialog,const wchar_t* klass,const wchar_t* text,int id,int x,int y,int width,int height,DWORD style=0){
 RECT r{x,y,x+width,y+height};MapDialogRect(dialog,&r);auto c=CreateWindowExW((style&WS_BORDER)?WS_EX_CLIENTEDGE:0,klass,text,WS_CHILD|WS_VISIBLE|style,r.left,r.top,r.right-r.left,r.bottom-r.top,dialog,HMENU(INT_PTR(id)),nullptr,nullptr);
 SendMessageW(c,WM_SETFONT,SendMessageW(dialog,WM_GETFONT,0,0),TRUE);return c;
}
std::optional<Settings> input(HWND dialog){std::array<wchar_t,16> text{};auto n=GetDlgItemTextW(dialog,rateId,text.data(),int(text.size()));if(!n||n>=text.size()-1)return {};unsigned value=0;for(unsigned i=0;i<n;++i){if(text[i]<L'0'||text[i]>L'9')return {};value=value*10+unsigned(text[i]-L'0');if(value>Settings::maximum)return {};}Settings s;if(!s.set(value))return {};return s;}
void describe(HWND dialog){auto s=input(dialog);if(!s){SetDlgItemTextW(dialog,descriptionId,L"10～2000 の整数で指定してください。0・ブレンド無効は選べません。");return;}wchar_t text[192]{};swprintf_s(text,L"1秒に %u%% 進行。0%% → 100%% の切り替えは約 %.3f 秒です。",s->percentPerSecond,s->completion_seconds());SetDlgItemTextW(dialog,descriptionId,text);}
void text(std::vector<uint16_t>& b,const wchar_t* s){do{b.push_back(uint16_t(*s));}while(*s++);}
}
INT_PTR CALLBACK Dialog::procedure(HWND h,UINT m,WPARAM wp,LPARAM lp){auto* s=reinterpret_cast<Dialog*>(GetWindowLongPtrW(h,DWLP_USER));
 if(m==WM_INITDIALOG){s=reinterpret_cast<Dialog*>(lp);s->window_=h;SetWindowLongPtrW(h,DWLP_USER,lp);
  control(h,L"STATIC",L"モーション切り替えのブレンド進行率",0,12,10,295,16);
  control(h,L"EDIT",std::to_wstring(s->settings_->percentPerSecond).c_str(),rateId,12,33,70,20,WS_BORDER|WS_TABSTOP|ES_NUMBER|ES_AUTOHSCROLL);SendDlgItemMessageW(h,rateId,EM_SETLIMITTEXT,4,0);
  control(h,L"STATIC",L"% / 秒  （10～2000）",0,90,37,215,16);
  control(h,L"STATIC",L"",descriptionId,12,64,296,32);
  control(h,L"STATIC",L"自己・他PC・キャラクター選択の切り替えに共通で適用します。\nブレンド途中の変更でも現在の合成率を保持します。",0,12,101,296,36);
  control(h,L"STATIC",L"",statusId,12,143,296,24);
  control(h,L"BUTTON",L"初期値 500",defaultId,12,178,78,20,WS_TABSTOP|BS_PUSHBUTTON);
  control(h,L"BUTTON",L"適用・保存",IDOK,145,178,78,20,WS_TABSTOP|BS_DEFPUSHBUTTON);
  control(h,L"BUTTON",L"閉じる",IDCANCEL,232,178,76,20,WS_TABSTOP|BS_PUSHBUTTON);describe(h);
  RECT parent{},bounds{};if(GetWindowRect(GetParent(h)?GetParent(h):GetDesktopWindow(),&parent)&&GetWindowRect(h,&bounds))SetWindowPos(h,nullptr,parent.left+((parent.right-parent.left)-(bounds.right-bounds.left))/2,parent.top+((parent.bottom-parent.top)-(bounds.bottom-bounds.top))/2,0,0,SWP_NOSIZE|SWP_NOZORDER);
  return TRUE;
 }
 if(!s)return FALSE;
 if(m==WM_KEYDOWN&&wp==VK_F12){SendMessageW(GetParent(h),WM_KEYDOWN,wp,lp|(1LL<<25));return TRUE;}
 if(m==WM_COMMAND){auto id=LOWORD(wp);
  if(id==rateId&&HIWORD(wp)==EN_CHANGE){describe(h);return TRUE;}
  if(id==defaultId){SetDlgItemTextW(h,rateId,L"500");return TRUE;}
  if(id==IDCANCEL){DestroyWindow(h);return TRUE;}
  if(id==IDOK){auto value=input(h);if(!value){SetDlgItemTextW(h,statusId,L"設定範囲は10～2000%/秒です。");return TRUE;}
   if(!save(s->path_,*value)){SetDlgItemTextW(h,statusId,L"保存できませんでした。以前の設定を保持します。");return TRUE;}
   *s->settings_=*value;SetDlgItemTextW(h,statusId,L"適用・保存しました。閉じると操作へ戻れます。");return TRUE;
  }
 }
 if(m==WM_CLOSE){DestroyWindow(h);return TRUE;}
 if(m==WM_NCDESTROY){s->window_=nullptr;SetWindowLongPtrW(h,DWLP_USER,0);return FALSE;}return FALSE;
}
void Dialog::open(HWND owner,Settings& settings,const std::filesystem::path& path,bool show){
 if(window_){if(show){ShowWindow(window_,SW_SHOW);SetForegroundWindow(window_);}return;}settings_=&settings;path_=path;
 std::vector<uint16_t> data(sizeof(DLGTEMPLATE)/2);auto* d=reinterpret_cast<DLGTEMPLATE*>(data.data());d->style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_SETFONT;d->cdit=0;d->cx=320;d->cy=210;data.push_back(0);data.push_back(0);text(data,L"F12 デバッグ：モーションブレンド");data.push_back(9);text(data,L"MS UI Gothic");
 auto h=CreateDialogIndirectParamW(GetModuleHandleW(nullptr),reinterpret_cast<DLGTEMPLATE*>(data.data()),owner,procedure,LPARAM(this));if(h&&show){ShowWindow(h,SW_SHOW);SetForegroundWindow(h);}
}
void Dialog::close(){if(window_)DestroyWindow(window_);}
}
