#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "item_settings_dialog.h"
#include "item_settings.h"
#include <array>
#include <charconv>
#include <vector>
namespace mgo2win::items {
namespace {
enum {listId=100,dropId,emptyId,droppedId,installedId,recoverId,nameId};
std::wstring wide(std::string_view s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0);if(n<=0)return L"?";std::wstring out(size_t(n),L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),out.data(),n);return out;}
struct State {Settings settings;DropPolicies catalog;std::filesystem::path path;std::vector<uint32_t> ids;std::optional<uint32_t> selected;};
HWND control(HWND dialog,const wchar_t* klass,const wchar_t* text,int id,int x,int y,int width,int height,DWORD style=0){RECT r{x,y,x+width,y+height};MapDialogRect(dialog,&r);HWND c=CreateWindowExW((style&WS_BORDER)?WS_EX_CLIENTEDGE:0,klass,text,WS_CHILD|WS_VISIBLE|style,r.left,r.top,r.right-r.left,r.bottom-r.top,dialog,HMENU(INT_PTR(id)),nullptr,nullptr);SendMessageW(c,WM_SETFONT,SendMessageW(dialog,WM_GETFONT,0,0),TRUE);return c;}
void remember(HWND dialog,State& s){if(!s.selected)return;const auto drop=SendDlgItemMessageW(dialog,dropId,CB_GETCURSEL,0,0),empty=SendDlgItemMessageW(dialog,emptyId,CB_GETCURSEL,0,0);if(drop<0||drop>2||empty<0||empty>2)return;WeaponOverride v;v.drop=drop==1?DropOverride::deny:drop==2?DropOverride::allow:DropOverride::original_default;if(empty)v.emptyDiscard=empty==2;s.settings.weapons.insert_or_assign(*s.selected,v);}
void selection(HWND dialog,State& s){auto index=SendDlgItemMessageW(dialog,listId,LB_GETCURSEL,0,0);if(index<0||size_t(index)>=s.ids.size())return;s.selected=s.ids[size_t(index)];const auto* entry=s.catalog.find(*s.selected);if(!entry)return;auto label=L"ID "+std::to_wstring(*s.selected)+L"  "+wide(entry->name);SetDlgItemTextW(dialog,nameId,label.c_str());auto p=s.settings.policy(*s.selected);SendDlgItemMessageW(dialog,dropId,CB_SETCURSEL,p.drop==DropOverride::deny?1:p.drop==DropOverride::allow?2:0,0);SendDlgItemMessageW(dialog,emptyId,CB_SETCURSEL,p.emptyDiscard?(*p.emptyDiscard?2:1):0,0);}
bool capacity(HWND dialog,int id,uint32_t& value){std::array<wchar_t,32> text{};int length=int(GetDlgItemTextW(dialog,id,text.data(),int(text.size())));if(length<=0||length>=int(text.size())-1)return false;uint64_t n=0;for(int i=0;i<length;++i){if(text[i]<L'0'||text[i]>L'9')return false;n=n*10+uint32_t(text[i]-L'0');if(n>4096)return false;}value=uint32_t(n);return true;}
INT_PTR CALLBACK procedure(HWND dialog,UINT message,WPARAM wp,LPARAM lp){auto* s=reinterpret_cast<State*>(GetWindowLongPtrW(dialog,DWLP_USER));
 if(message==WM_INITDIALOG){s=reinterpret_cast<State*>(lp);SetWindowLongPtrW(dialog,DWLP_USER,lp);
  control(dialog,L"STATIC",L"武器ごとの破棄・配置設定",0,10,8,400,14);
  control(dialog,L"LISTBOX",L"",listId,10,28,185,215,WS_BORDER|WS_VSCROLL|WS_TABSTOP|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT);
  control(dialog,L"STATIC",L"",nameId,207,28,205,28);
  control(dialog,L"STATIC",L"地面へ捨てる",0,207,62,195,12);
  auto drop=control(dialog,L"COMBOBOX",L"",dropId,207,77,195,70,WS_TABSTOP|CBS_DROPDOWNLIST);for(auto text:{L"既定（原仕様が未確認なら不可）",L"不可",L"許可"})SendMessageW(drop,CB_ADDSTRING,0,LPARAM(text));
  control(dialog,L"STATIC",L"残量が空になった場合",0,207,104,195,12);
  auto empty=control(dialog,L"COMBOBOX",L"",emptyId,207,119,195,70,WS_TABSTOP|CBS_DROPDOWNLIST);for(auto text:{L"既定（未確認なら保持）",L"消さずに保持",L"捨てる・使用後に消滅"})SendMessageW(empty,CB_ADDSTRING,0,LPARAM(text));
  control(dialog,L"BUTTON",L"他のプレイヤーが設置した武器も回収可",recoverId,207,149,205,24,WS_TABSTOP|BS_AUTOCHECKBOX);SendDlgItemMessageW(dialog,recoverId,BM_SETCHECK,s->settings.recoverOthers?BST_CHECKED:BST_UNCHECKED,0);
  control(dialog,L"STATIC",L"地面のアイテム数（0～4096）",0,207,179,150,12);control(dialog,L"EDIT",std::to_wstring(s->settings.capacity.dropped).c_str(),droppedId,358,177,45,16,WS_BORDER|WS_TABSTOP|ES_NUMBER);SendDlgItemMessageW(dialog,droppedId,EM_SETLIMITTEXT,4,0);
  control(dialog,L"STATIC",L"設置した武器数（0～4096）",0,207,202,150,12);control(dialog,L"EDIT",std::to_wstring(s->settings.capacity.installed).c_str(),installedId,358,200,45,16,WS_BORDER|WS_TABSTOP|ES_NUMBER);SendDlgItemMessageW(dialog,installedId,EM_SETLIMITTEXT,4,0);
  control(dialog,L"STATIC",L"現在の配置・回収動作はAK102に対応。他の項目も設定を保存できます。",0,10,249,400,25);
  control(dialog,L"BUTTON",L"保存",IDOK,287,280,55,18,WS_TABSTOP|BS_DEFPUSHBUTTON);control(dialog,L"BUTTON",L"キャンセル",IDCANCEL,348,280,65,18,WS_TABSTOP|BS_PUSHBUTTON);
  size_t initial=0;for(const auto&[key,e]:s->catalog.entries())if(e.domain==Domain::weapon){if(e.id==25)initial=s->ids.size();s->ids.push_back(e.id);const auto label=std::to_wstring(e.id)+L"  "+wide(e.name);SendDlgItemMessageW(dialog,listId,LB_ADDSTRING,0,LPARAM(label.c_str()));}
  SendDlgItemMessageW(dialog,listId,LB_SETCURSEL,initial,0);selection(dialog,*s);
  RECT parent{},bounds{};if(GetWindowRect(GetParent(dialog)?GetParent(dialog):GetDesktopWindow(),&parent)&&GetWindowRect(dialog,&bounds))SetWindowPos(dialog,nullptr,parent.left+((parent.right-parent.left)-(bounds.right-bounds.left))/2,parent.top+((parent.bottom-parent.top)-(bounds.bottom-bounds.top))/2,0,0,SWP_NOSIZE|SWP_NOZORDER);
  return TRUE;
 }
 if(!s)return FALSE;
 if(message==WM_COMMAND){const auto id=LOWORD(wp);if(id==listId&&HIWORD(wp)==LBN_SELCHANGE){remember(dialog,*s);selection(dialog,*s);return TRUE;}
  if(id==IDCANCEL){EndDialog(dialog,0);return TRUE;}
  if(id==IDOK){remember(dialog,*s);uint32_t dropped=0,installed=0;if(!capacity(dialog,droppedId,dropped)||!capacity(dialog,installedId,installed)){MessageBoxW(dialog,L"配置数は0～4096の整数で入力してください。",L"武器設定",MB_OK|MB_ICONWARNING);return TRUE;}s->settings.capacity={dropped,installed};s->settings.recoverOthers=SendDlgItemMessageW(dialog,recoverId,BM_GETCHECK,0,0)==BST_CHECKED;std::string error;if(!s->settings.save(s->path,error)){auto detail=L"設定を保存できませんでした。以前のファイルは保持しています。\n"+wide(error);MessageBoxW(dialog,detail.c_str(),L"武器設定",MB_OK|MB_ICONERROR);return TRUE;}EndDialog(dialog,1);return TRUE;}
 }
 if(message==WM_CLOSE){EndDialog(dialog,0);return TRUE;}return FALSE;
}
void word(std::vector<uint16_t>& data,uint16_t value){data.push_back(value);}void text(std::vector<uint16_t>& data,const wchar_t* value){do{data.push_back(uint16_t(*value));}while(*value++);}
}
bool edit_item_settings(HWND owner,const std::filesystem::path& settingsPath,const std::filesystem::path& catalogPath){
 try{State state;state.path=settingsPath;std::string error;const auto catalog=catalogPath.empty()?settingsPath.parent_path()/L"item_drop_policy.json":catalogPath;
  if(!state.catalog.load(catalog,error)){MessageBoxW(owner,(L"武器一覧を読み込めません。\n"+wide(error)).c_str(),L"武器設定",MB_OK|MB_ICONERROR);return false;}
  if(std::filesystem::exists(settingsPath)&&!state.settings.load(settingsPath,error)){MessageBoxW(owner,(L"設定ファイルが不正です。変更せず終了します。\n"+wide(error)).c_str(),L"武器設定",MB_OK|MB_ICONERROR);return false;}
  std::vector<uint16_t> data(sizeof(DLGTEMPLATE)/2);auto* dialog=reinterpret_cast<DLGTEMPLATE*>(data.data());dialog->style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_SETFONT;dialog->dwExtendedStyle=0;dialog->cdit=0;dialog->x=0;dialog->y=0;dialog->cx=425;dialog->cy=309;word(data,0);word(data,0);text(data,L"武器・アイテム設定");word(data,9);text(data,L"MS UI Gothic");
  return DialogBoxIndirectParamW(GetModuleHandleW(nullptr),reinterpret_cast<DLGTEMPLATE*>(data.data()),owner,procedure,LPARAM(&state))==1;
 }catch(...){MessageBoxW(owner,L"武器設定を開けませんでした。",L"武器設定",MB_OK|MB_ICONERROR);return false;}
}
}
