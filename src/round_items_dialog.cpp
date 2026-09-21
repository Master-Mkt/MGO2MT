#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "round_items_dialog.h"
#include "gcx_round_items.h"
#include "stage_profiles.h"
#include "build_version.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <set>
#include <string>
namespace mgo2mt::items {namespace {
enum {mapId=100,envgId,drumId,sourceInfoId,resetId};
constexpr std::array<uint8_t,5> maps{20,1,4,21,7};
constexpr std::array<const wchar_t*,5> mapLabels{L"n022a / マップ20",L"Ambush Alley (AA)",L"Midtown Maelstrom (MM)",L"Jade Junction (JJ)",L"Blood Bath (BB)"};
struct Choice {Domain domain=Domain::equipment;uint32_t item=0;bool operator==(const Choice&)const=default;};
struct State {
 RoundItems original;DropPolicies catalog;std::filesystem::path path,capture;
 std::vector<PolicyEntry> entries;std::map<uint8_t,GcxItemLayout> layouts;
 std::map<uint8_t,std::array<Choice,2>> choices;
 size_t selected=0;bool captured=false,legacy=false;
};
std::wstring wide(std::string_view s){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0);if(n<=0)return L"?";std::wstring out(size_t(n),0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),out.data(),n);return out;}
HWND control(HWND d,const wchar_t*k,const wchar_t*t,int id,int x,int y,int w,int h,DWORD style=0){RECT r{x,y,x+w,y+h};MapDialogRect(d,&r);auto c=CreateWindowExW((style&WS_BORDER)?WS_EX_CLIENTEDGE:0,k,t,WS_CHILD|WS_VISIBLE|style,r.left,r.top,r.right-r.left,r.bottom-r.top,d,HMENU(INT_PTR(id)),nullptr,nullptr);SendMessageW(c,WM_SETFONT,SendMessageW(d,WM_GETFONT,0,0),TRUE);return c;}
Choice previous_choice(const RoundItems& settings,const GcxItemLayout& layout,uint32_t item,bool& legacy){
 std::optional<Choice> common;
 for(const auto& g:layout.groups)if(g.item==item)for(const auto& a:g.anchors){
  const GcxReplacement* selected=nullptr;
  for(const auto& r:settings.replacements)if(r.map==layout.map&&r.source==GcxSource::pickup){if(r.sourceOffset==a.sourceOffset){selected=&r;break;}if(!r.sourceOffset)selected=&r;}
  Choice choice=selected?Choice{selected->domain,selected->item}:Choice{Domain::equipment,item};
  if(common&&*common!=choice){legacy=true;return {Domain::equipment,item};}common=choice;
 }
 return common.value_or(Choice{Domain::equipment,item});
}
void show_map(HWND d,State&s){
 const auto map=maps[s.selected];const bool available=s.layouts.contains(map);
 for(unsigned i=0;i<2;++i){
  const auto& choice=s.choices.at(map)[i];int index=-1;
  for(size_t j=0;j<s.entries.size();++j)if(s.entries[j].domain==choice.domain&&s.entries[j].id==choice.item)index=int(j);
  SendDlgItemMessageW(d,envgId+i,CB_SETCURSEL,index,0);EnableWindow(GetDlgItem(d,envgId+i),available);
 }
 const auto info=available?L"このステージの原配置を使用します。各ラウンドでENVG枠・DRUM枠を1個ずつ配置します。\n位置と候補数は原データのままで、置くアイテムだけ変更します。":L"このステージの配置データを読み込めません。保存前にステージデータを確認してください。";
 SetDlgItemTextW(d,sourceInfoId,info);
 EnableWindow(GetDlgItem(d,IDOK),s.layouts.size()==maps.size());
}
bool remember(HWND d,State&s){
 for(unsigned i=0;i<2;++i){auto index=SendDlgItemMessageW(d,envgId+i,CB_GETCURSEL,0,0);if(index<0||size_t(index)>=s.entries.size())return false;const auto&e=s.entries[size_t(index)];s.choices[maps[s.selected]][i]={e.domain,e.id};}return true;
}
bool capture(HWND d,const std::filesystem::path&path){RECT rect{};if(!GetClientRect(d,&rect)||rect.right<=0||rect.bottom<=0)return false;const auto w=rect.right,h=rect.bottom;HDC source=GetDC(d),dc=CreateCompatibleDC(source);HBITMAP bitmap=CreateCompatibleBitmap(source,w,h);if(!source||!dc||!bitmap)return false;auto old=SelectObject(dc,bitmap);FillRect(dc,&rect,HBRUSH(COLOR_BTNFACE+1));for(auto child=GetWindow(d,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){RECT bounds{};GetWindowRect(child,&bounds);MapWindowPoints(nullptr,d,reinterpret_cast<POINT*>(&bounds),2);auto saved=SaveDC(dc);SetViewportOrgEx(dc,bounds.left,bounds.top,nullptr);SendMessageW(child,WM_PRINT,WPARAM(dc),PRF_CLIENT|PRF_ERASEBKGND|PRF_CHILDREN|PRF_NONCLIENT);RestoreDC(dc,saved);}GdiFlush();BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;std::vector<uint8_t> pixels(size_t(w)*h*4);SelectObject(dc,old);bool ok=GetDIBits(dc,bitmap,0,h,pixels.data(),&info,DIB_RGB_COLORS)==h;DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(d,source);if(!ok)return false;BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=DWORD(file.bfOffBits+pixels.size());std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&file),sizeof(file));out.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(info.bmiHeader));out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());return bool(out);}

INT_PTR CALLBACK procedure(HWND d,UINT msg,WPARAM wp,LPARAM lp){
 auto*s=reinterpret_cast<State*>(GetWindowLongPtrW(d,DWLP_USER));
 if(msg==WM_INITDIALOG){
  s=reinterpret_cast<State*>(lp);SetWindowLongPtrW(d,DWLP_USER,lp);
  control(d,L"STATIC",L"ステージの標準アイテムを変更",0,12,10,386,16);
  control(d,L"STATIC",L"ステージ",0,12,37,78,12);
  auto map=control(d,L"COMBOBOX",L"",mapId,100,33,298,160,WS_TABSTOP|CBS_DROPDOWNLIST);
  for(auto label:mapLabels)SendMessageW(map,CB_ADDSTRING,0,LPARAM(label));SendMessageW(map,CB_SETCURSEL,0,0);
  for(unsigned i=0;i<2;++i){
   control(d,L"STATIC",i?L"DRUM の枠":L"ENVG の枠",0,12,74+42*i,78,14);
   auto box=control(d,L"COMBOBOX",L"",envgId+i,100,70+42*i,298,220,WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL);
   for(const auto&e:s->entries){auto name=(e.domain==Domain::equipment?L"装備 / ":L"武器 / ")+wide(e.name);if(e.domain==Domain::equipment&&e.id==(i?10u:22u))name+=L"（初期設定）";SendMessageW(box,CB_ADDSTRING,0,LPARAM(name.c_str()));}
  }
  control(d,L"STATIC",L"",sourceInfoId,12,154,386,37);
  control(d,L"BUTTON",L"このステージを初期設定に戻す",resetId,12,199,210,23,WS_TABSTOP|BS_PUSHBUTTON);
  control(d,L"STATIC",s->legacy?L"保存すると、旧手動配置・個別設定はこの2枠の設定に置き換わります。":L"ホストの使用禁止設定は引き続き適用されます。",0,12,232,386,26);
  control(d,L"STATIC",L"保存した設定は、次に作る部屋から使用します。",0,12,264,386,13);
  control(d,L"BUTTON",L"保存",IDOK,250,286,68,23,WS_TABSTOP|BS_DEFPUSHBUTTON);
  control(d,L"BUTTON",L"キャンセル",IDCANCEL,330,286,68,23,WS_TABSTOP|BS_PUSHBUTTON);
  show_map(d,*s);if(!s->capture.empty())SetTimer(d,1,300,nullptr);return TRUE;
 }
 if(!s)return FALSE;
 if(msg==WM_TIMER&&!s->capture.empty()){KillTimer(d,1);s->captured=capture(d,s->capture);EndDialog(d,0);return TRUE;}
 if(msg==WM_COMMAND){switch(LOWORD(wp)){
  case mapId:if(HIWORD(wp)==CBN_SELCHANGE){remember(d,*s);auto selected=SendDlgItemMessageW(d,mapId,CB_GETCURSEL,0,0);if(selected>=0&&size_t(selected)<maps.size()){s->selected=size_t(selected);show_map(d,*s);}}return TRUE;
  case envgId:case drumId:if(HIWORD(wp)==CBN_SELCHANGE)remember(d,*s);return TRUE;
  case resetId:s->choices[maps[s->selected]]={Choice{Domain::equipment,22},Choice{Domain::equipment,10}};show_map(d,*s);return TRUE;
  case IDOK:{
   try{
    if(!remember(d,*s)||s->layouts.size()!=maps.size())throw std::runtime_error("Stage pickup data or selection unavailable");
    RoundItems next;next.useGcx=true;next.enabled=false;
    for(auto map:maps){
     // Re-read from this stage at save time; stale UI state cannot silently
     // save offsets from a replaced or mismatched source file.
     auto layout=load_gcx_item_layout(s->path.parent_path()/"stage",map);
     if(!layout||!layout->verified)throw std::runtime_error("Stage pickup data unavailable");
     for(unsigned i=0;i<2;++i){const auto& c=s->choices.at(map)[i];replace_pickup_item(next,*layout,i?10u:22u,c.domain,c.item);}
    }
    std::string error;if(!next.save(s->path,error))throw std::runtime_error(error);EndDialog(d,1);
   }catch(const std::exception&e){MessageBoxW(d,(L"保存できませんでした。\n"+wide(e.what())).c_str(),L"標準アイテム",MB_OK|MB_ICONERROR);}return TRUE;
  }
  case IDCANCEL:EndDialog(d,0);return TRUE;
 }}
 if(msg==WM_CLOSE){EndDialog(d,0);return TRUE;}return FALSE;
}
}
bool edit_round_items(HWND owner,const std::filesystem::path&path,const std::filesystem::path&catalog,const std::filesystem::path&image){try{
 State s;s.path=path;s.capture=image;std::string error;
 if(!s.catalog.load(catalog,error)||(std::filesystem::exists(path)&&!s.original.load(path,error)))throw std::runtime_error(error);
 s.legacy=s.original.enabled||!s.original.rules.empty()||!s.original.useGcx||std::any_of(s.original.replacements.begin(),s.original.replacements.end(),[](const auto&r){return r.source==GcxSource::cbox;});
 for(const auto&[key,e]:s.catalog.entries())if(e.id&&e.id<=65535&&e.domain<=Domain::equipment)s.entries.push_back(e);
 for(auto map:maps){
  s.choices[map]={Choice{Domain::equipment,22},Choice{Domain::equipment,10}};
  try{auto layout=load_gcx_item_layout(path.parent_path()/"stage",map);if(layout&&layout->verified){
   s.choices[map]={previous_choice(s.original,*layout,22,s.legacy),previous_choice(s.original,*layout,10,s.legacy)};s.layouts.emplace(map,std::move(*layout));
  }}catch(...){/* Display source unavailability; never invent placements. */}
 }
 std::vector<uint16_t> data(sizeof(DLGTEMPLATE)/2);auto*d=reinterpret_cast<DLGTEMPLATE*>(data.data());d->style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_SETFONT;d->cx=410;d->cy=322;
 data.push_back(0);data.push_back(0);auto text=[&](const std::wstring&t){for(auto c:t)data.push_back(uint16_t(c));data.push_back(0);};
 text(versioned_title(L"MGO2MTHOST 標準アイテム"));data.push_back(9);text(L"MS UI Gothic");
 const auto result=DialogBoxIndirectParamW(GetModuleHandleW(nullptr),reinterpret_cast<DLGTEMPLATE*>(data.data()),owner,procedure,LPARAM(&s));return image.empty()?result==1:s.captured;
 }catch(const std::exception&e){if(image.empty())MessageBoxW(owner,(L"設定を開けません。\n"+wide(e.what())).c_str(),L"標準アイテム",MB_OK|MB_ICONERROR);return false;}
}
}
