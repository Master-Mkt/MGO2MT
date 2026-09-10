#include "graphics_settings.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <syncstream>
namespace mgo2win {
bool valid_graphics(const GraphicsConfig& c){return c.fullscreen<=1&&c.width>=640&&c.width<=7680&&c.height>=480&&c.height<=4320&&c.refresh_den>=1&&c.refresh_den<=100000&&(c.refresh_num==0||(double(c.refresh_num)/c.refresh_den>=20&&double(c.refresh_num)/c.refresh_den<=360))&&(c.shadow==512||c.shadow==1024||c.shadow==2048||c.shadow==4096||c.shadow==8192)&&c.vsync<=1;}
bool load_graphics(const std::filesystem::path& p,GraphicsConfig& c){if(!std::filesystem::exists(p))return false;if(std::filesystem::file_size(p)>256)throw std::runtime_error("Graphics settings size");std::ifstream f(p);std::string tag,extra;unsigned version;GraphicsConfig d;
 if(!(f>>tag>>version>>d.fullscreen>>d.width>>d.height>>d.refresh_num>>d.refresh_den>>d.shadow>>d.vsync)||f>>extra||!f.eof()||tag!="MGO2WIN.GRAPHICS"||version!=1||!valid_graphics(d))throw std::runtime_error("Invalid graphics settings");c=d;return true;}
void save_graphics(const std::filesystem::path& p,const GraphicsConfig& c){if(!valid_graphics(c))throw std::runtime_error("Invalid graphics settings");std::filesystem::create_directories(p.parent_path());auto temp=p;temp+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
 try{{std::ofstream f(temp);f<<"MGO2WIN.GRAPHICS 1\n"<<c.fullscreen<<' '<<c.width<<' '<<c.height<<' '<<c.refresh_num<<' '<<c.refresh_den<<' '<<c.shadow<<' '<<c.vsync<<'\n';f.close();if(!f)throw std::runtime_error("Graphics write failed");}if(!MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Graphics replace failed");}catch(...){std::error_code ec;std::filesystem::remove(temp,ec);throw;}}
GraphicsSettings::GraphicsSettings(std::filesystem::path p):path_(std::move(p)){try{load_graphics(path_,draft);}catch(...){draft={};notice=L"保存した画質設定を読めませんでした。初期値を使用します。";}}
bool GraphicsSettings::confirm(){if(!pending())return false;try{save_graphics(path_,active);until_=0;notice=L"画質設定を保存しました。";std::osyncstream(std::cout)<<"{\"graphics_confirmed\":true}"<<std::endl;return true;}catch(...){undo_=true;notice=L"保存できなかったため元の設定へ戻します。";return false;}}
void GraphicsSettings::tick(ULONGLONG now,bool foreground){
 if(pending()&&(undo_||now>=until_||!foreground)){
  if(!apply||!apply(previous_))throw std::runtime_error("Graphics rollback failed");active=previous_;draft=active;until_=0;undo_=false;request_=false;notice=L"元の画質設定へ戻しました。";std::osyncstream(std::cout)<<"{\"graphics_reverted\":true}"<<std::endl;
 }
 if(request_&&!pending()){
  request_=false;if(!foreground||!apply){notice=L"画面を前面にしてから適用してください。";return;}
  previous_=active;
  if(!valid_graphics(draft)||!apply(draft)){if(!apply(previous_))throw std::runtime_error("Graphics recovery failed");draft=active;notice=L"この表示設定を適用できませんでした。元の設定を維持します。";std::osyncstream(std::cout)<<"{\"graphics_apply_failed\":true}"<<std::endl;return;}
  active=draft;until_=now+15000;undo_=false;notice=L"この表示設定を維持しますか？ Enterで保存、Escまたは15秒で元に戻ります。";
  std::osyncstream(std::cout)<<"{\"graphics_applied\":true,\"fullscreen\":"<<active.fullscreen<<",\"width\":"<<active.width<<",\"height\":"<<active.height<<",\"refresh_num\":"<<active.refresh_num<<",\"refresh_den\":"<<active.refresh_den<<",\"vsync\":"<<active.vsync<<",\"shadow_size\":"<<active.shadow<<",\"shadow_rendering_connected\":false}"<<std::endl;
 }
}
void GraphicsSettings::change(int step){
 if(focus_==0){draft.fullscreen=1-draft.fullscreen;draft.refresh_num=0;draft.refresh_den=1;}
 if(focus_==1){std::vector<std::pair<unsigned,unsigned>> sizes;if(!draft.fullscreen)sizes={{1280,720},{1600,900},{1920,1080},{2560,1440},{3840,2160}};for(auto m:modes)sizes.push_back({m.width,m.height});std::sort(sizes.begin(),sizes.end());sizes.erase(std::unique(sizes.begin(),sizes.end()),sizes.end());if(sizes.empty())return;auto it=std::find(sizes.begin(),sizes.end(),std::pair{draft.width,draft.height});int i=it==sizes.end()?0:int(it-sizes.begin());i=(i+step+int(sizes.size()))%int(sizes.size());draft.width=sizes[i].first;draft.height=sizes[i].second;draft.refresh_num=0;draft.refresh_den=1;}
 if(focus_==2){if(!draft.fullscreen){notice=L"ウィンドウ表示のリフレッシュレートはWindowsの設定に従います。";return;}std::vector<std::pair<unsigned,unsigned>> rates={{0,1}};for(auto m:modes)if(m.width==draft.width&&m.height==draft.height&&std::find(rates.begin(),rates.end(),std::pair{m.num,m.den})==rates.end())rates.push_back({m.num,m.den});auto it=std::find(rates.begin(),rates.end(),std::pair{draft.refresh_num,draft.refresh_den});int i=it==rates.end()?0:int(it-rates.begin());i=(i+step+int(rates.size()))%int(rates.size());draft.refresh_num=rates[i].first;draft.refresh_den=rates[i].second;}
 if(focus_==3){const unsigned values[]={512,1024,2048,4096,8192};int i=0;while(i<4&&values[i]!=draft.shadow)++i;draft.shadow=values[(i+step+5)%5];}
 if(focus_==4)draft.vsync=1-draft.vsync;
 notice=L"変更後に「適用」を押してください。";cues_.push_back(94);
}
void GraphicsSettings::activate(){cues_.push_back(93);if(focus_<5)change(1);else if(focus_==5)request();else if(focus_==6){draft={};notice=L"初期値を選択しました。適用で反映します。";}else{draft=active;back_=true;}}
bool GraphicsSettings::message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 if(pending()){
  if(msg==WM_KEYDOWN){if(wp==VK_ESCAPE)cancel();else if(wp==VK_RETURN&&!(lp&(1LL<<30)))confirm();return true;}
  if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(r.right&&r.bottom){int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;if(y>=599&&y<644){if(x>=120&&x<600)confirm();else if(x>=660&&x<1160)cancel();}}return true;}
  return msg==WM_CHAR;
 }
 if(msg==WM_CHAR)return true;
 if(msg==WM_KEYDOWN){if(wp==VK_ESCAPE){draft=active;back_=true;}else if(wp==VK_TAB||wp==VK_UP||wp==VK_DOWN){bool prev=wp==VK_UP||(wp==VK_TAB&&(GetKeyState(VK_SHIFT)&0x8000));focus_=(focus_+(prev?7:1))%8;cues_.push_back(94);}else if(wp==VK_LEFT||wp==VK_RIGHT)change(wp==VK_LEFT?-1:1);else if((wp==VK_RETURN||wp==VK_SPACE)&&!(lp&(1LL<<30)))activate();return true;}
 if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(!r.right||!r.bottom)return true;int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;if(x>=500&&x<1160&&y>=221&&y<491){focus_=(y-221)/54;change(x<570?-1:1);}else if(y>=599&&y<644){if(x>=120&&x<440)focus_=5;else if(x>=470&&x<800)focus_=6;else if(x>=830&&x<1160)focus_=7;else return true;activate();}SetFocus(hwnd);return true;}return false;
}
void GraphicsSettings::draw(HDC dc,const std::vector<HFONT>& fonts){
 auto fill=[&](int x,int y,int w,int h,COLORREF c){RECT r{x,y,x+w,y+h};auto b=CreateSolidBrush(c);FillRect(dc,&r,b);DeleteObject(b);};
 auto text=[&](std::wstring s,int x,int y,int w,int h,int font,COLORREF c){RECT r{x,y,x+w,y+h};SelectObject(dc,fonts[font]);SetTextColor(dc,c);SetBkMode(dc,TRANSPARENT);DrawTextW(dc,s.c_str(),int(s.size()),&r,DT_LEFT|DT_NOPREFIX|DT_WORDBREAK);};
 std::wstring hz=L"自動（モニター推奨）";if(!draft.fullscreen)hz=L"Windowsの設定に従う";else if(draft.refresh_num){wchar_t b[50];swprintf_s(b,L"%.3f Hz",double(draft.refresh_num)/draft.refresh_den);hz=b;}
 const wchar_t* labels[]={L"表示モード",L"解像度",L"リフレッシュレート",L"影のバッファサイズ",L"垂直同期 (VSync)"};std::wstring values[]={draft.fullscreen?L"フルスクリーン":L"ウィンドウ",std::to_wstring(draft.width)+L" × "+std::to_wstring(draft.height),hz,std::to_wstring(draft.shadow)+L" × "+std::to_wstring(draft.shadow),draft.vsync?L"ON":L"OFF"};
 for(int i=0;i<5;++i){int y=221+i*54;text(labels[i],125,y+12,365,32,1,RGB(224,232,212));fill(500,y,660,43,focus_==i?RGB(70,93,59):RGB(23,33,27));text(L"◀  "+values[i]+L"  ▶",516,y+10,630,32,1,RGB(232,237,218));}
 text(L"影：設定値の保存に対応。3Dの影描画は今後の実装で反映します。",120,502,1040,30,3,RGB(186,204,169));
 std::wstring message=notice;if(pending())message+=L"  残り "+std::to_wstring((until_>GetTickCount64()?(until_-GetTickCount64()+999)/1000:0))+L" 秒";
 text(message,120,545,1040,49,2,RGB(244,218,161));
 auto button=[&](int x,int w,int i,std::wstring label){fill(x,599,w,44,focus_==i?RGB(151,168,126):RGB(37,53,42));text(label,x+15,610,w-30,32,1,focus_==i?RGB(18,28,19):RGB(232,237,218));};
 if(pending()){button(120,480,5,L"この設定を保存 [Enter]");button(660,500,7,L"元に戻す [Esc]");}else{button(120,320,5,L"適用");button(470,330,6,L"初期値に戻す");button(830,330,7,L"ネットワークへ戻る");}
 text(L"F1/F2/F3：タブ   ↑ ↓ / Tab：項目   ← →：変更   Enter：決定",83,690,1120,25,3,RGB(174,185,165));
}
}
