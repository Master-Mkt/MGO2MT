#include "product_identity.h"
#include "menu_theme.h"
#include "graphics_settings.h"
#include "render_options.h"
#include "menu_audio.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <syncstream>
namespace mgo2mt {
bool valid_graphics(const GraphicsConfig& c){return c.fullscreen<=1&&c.width>=640&&c.width<=7680&&c.height>=480&&c.height<=4320&&c.refresh_den>=1&&c.refresh_den<=100000&&(c.refresh_num==0||(double(c.refresh_num)/c.refresh_den>=20&&double(c.refresh_num)/c.refresh_den<=360))&&(c.shadow==512||c.shadow==1024||c.shadow==2048||c.shadow==4096||c.shadow==8192)&&c.vsync<=1&&c.renderScale>=50&&c.renderScale<=200&&c.renderScale%25==0&&render_backend::valid({c.anisotropy})&&c.mipmaps<=1&&c.linearColor<=1&&c.footIk<=1&&c.hdr<=1&&c.aa<=1&&c.ao<=1&&c.bloom<=1&&c.reflections<=1&&c.lod<=1&&c.softParticles<=1&&c.gpuTiming<=1&&c.exposureMilli>=250&&c.exposureMilli<=4000&&c.shadowEnabled<=1&&c.shadowDebug<=1&&c.shadowBias<=1000&&shadows::valid(c.shadows());}
bool load_graphics(const std::filesystem::path& p,GraphicsConfig& c){if(!std::filesystem::exists(p))return false;if(std::filesystem::file_size(p)>512)throw std::runtime_error("Graphics settings size");std::ifstream f(p);std::string tag,extra;unsigned version;GraphicsConfig d;
 if(!(f>>tag>>version>>d.fullscreen>>d.width>>d.height>>d.refresh_num>>d.refresh_den>>d.shadow>>d.vsync)||tag!=mgo2mt::brand::Format{"MGO2MT.GRAPHICS"}||(version<1||version>5))throw std::runtime_error("Invalid graphics settings");
 if(version>=2&&!(f>>d.renderScale>>d.anisotropy>>d.mipmaps>>d.linearColor))throw std::runtime_error("Invalid render settings");
 if(version>=3&&!(f>>d.shadowEnabled>>d.shadowCascades>>d.shadowPcf>>d.shadowBias>>d.shadowNormal>>d.shadowSlope>>d.shadowDebug))throw std::runtime_error("Invalid shadow settings");
 if(version>=4&&!(f>>d.footIk))throw std::runtime_error("Invalid foot IK setting");
 if(version>=5&&!(f>>d.hdr>>d.aa>>d.ao>>d.bloom>>d.reflections>>d.lod>>d.softParticles>>d.gpuTiming>>d.exposureMilli))throw std::runtime_error("Invalid enhancement settings");
 if(f>>extra||!f.eof()||!valid_graphics(d))throw std::runtime_error("Invalid graphics settings");c=d;return true;}
void save_graphics(const std::filesystem::path& p,const GraphicsConfig& c){if(!valid_graphics(c))throw std::runtime_error("Invalid graphics settings");std::filesystem::create_directories(p.parent_path());auto temp=p;temp+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
 try{{std::ofstream f(temp);f<<"MGO2MT.GRAPHICS 5\n"<<c.fullscreen<<' '<<c.width<<' '<<c.height<<' '<<c.refresh_num<<' '<<c.refresh_den<<' '<<c.shadow<<' '<<c.vsync<<' '<<c.renderScale<<' '<<c.anisotropy<<' '<<c.mipmaps<<' '<<c.linearColor<<' '<<c.shadowEnabled<<' '<<c.shadowCascades<<' '<<c.shadowPcf<<' '<<c.shadowBias<<' '<<c.shadowNormal<<' '<<c.shadowSlope<<' '<<c.shadowDebug<<' '<<c.footIk<<' '<<c.hdr<<' '<<c.aa<<' '<<c.ao<<' '<<c.bloom<<' '<<c.reflections<<' '<<c.lod<<' '<<c.softParticles<<' '<<c.gpuTiming<<' '<<c.exposureMilli<<'\n';f.close();if(!f)throw std::runtime_error("Graphics write failed");}if(!MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Graphics replace failed");}catch(...){std::error_code ec;std::filesystem::remove(temp,ec);throw;}}
GraphicsSettings::GraphicsSettings(std::filesystem::path p):path_(std::move(p)){try{load_graphics(path_,draft);}catch(...){draft={};notice=L"保存した画質設定を読めませんでした。初期値を使用します。";}}
bool GraphicsSettings::confirm(){if(!pending())return false;try{save_graphics(path_,active);until_=0;notice=L"画質設定を保存しました。";cue(menu_audio::Confirm);std::osyncstream(std::cout)<<"{\"graphics_confirmed\":true}"<<std::endl;return true;}catch(...){undo_=true;notice=L"保存できなかったため元の設定へ戻します。";return false;}}
void GraphicsSettings::tick(ULONGLONG now,bool foreground){
 if(pending()&&(undo_||now>=until_||!foreground)){
  if(!apply||!apply(previous_))throw std::runtime_error("Graphics rollback failed");active=previous_;draft=active;until_=0;undo_=false;request_=false;notice=L"元の画質設定へ戻しました。";std::osyncstream(std::cout)<<"{\"graphics_reverted\":true}"<<std::endl;
 }
 if(request_&&!pending()){
  request_=false;if(!foreground||!apply){notice=L"画面を前面にしてから適用してください。";return;}
  previous_=active;
  if(!valid_graphics(draft)||!apply(draft)){if(!apply(previous_))throw std::runtime_error("Graphics recovery failed");draft=active;notice=L"この表示設定を適用できませんでした。元の設定を維持します。";std::osyncstream(std::cout)<<"{\"graphics_apply_failed\":true}"<<std::endl;return;}
  active=draft;until_=now+15000;undo_=false;notice=L"この表示設定を維持しますか？ Enterで保存、Escまたは15秒で元に戻ります。";
  std::osyncstream(std::cout)<<"{\"graphics_applied\":true,\"fullscreen\":"<<active.fullscreen<<",\"width\":"<<active.width<<",\"height\":"<<active.height<<",\"refresh_num\":"<<active.refresh_num<<",\"refresh_den\":"<<active.refresh_den<<",\"vsync\":"<<active.vsync<<",\"shadow_size\":"<<active.shadow<<",\"shadow_rendering_connected\":true}"<<std::endl;
 }
}
void GraphicsSettings::change(int step){
 if(focus_>=9)return;const auto previous=draft;
 if(page_==2){
  unsigned* values[]={&draft.hdr,&draft.aa,&draft.ao,&draft.bloom,&draft.reflections,&draft.lod,&draft.softParticles};
  if(focus_<7)*values[focus_]=1-*values[focus_];
  else if(focus_==7)draft.exposureMilli=250+unsigned((int((draft.exposureMilli-250)/250)+step+16)%16)*250;
  else{bool all=true;for(auto value:values)all=all&&*value;for(auto value:values)*value=all?0:1;}
  notice=L"追加効果は「適用」で反映します。初期値はすべてOFFです。";cue(menu_audio::Cursor);return;
 }
 if(focus_==8){if(page_==1){draft.gpuTiming=1-draft.gpuTiming;notice=L"GPU時間はF12の診断画面を表示中だけ計測します。";}else{draft.footIk=1-draft.footIk;notice=L"足の接地補正は「適用」で反映します。";}cue(menu_audio::Cursor);return;}
 if(page_==1){
  auto cycle=[&](unsigned& value,unsigned min,unsigned max,unsigned delta=1){int count=int((max-min)/delta+1);value=min+unsigned((int((value-min)/delta)+step+count)%count)*delta;};
  switch(focus_){case 0:draft.shadowEnabled=1-draft.shadowEnabled;if(draft.shadow<1024)draft.shadow=1024;break;case 1:{unsigned values[]={1024,2048,4096,8192};int index=0;while(index<3&&values[index]!=draft.shadow)++index;draft.shadow=values[(index+step+4)%4];break;}case 2:cycle(draft.shadowCascades,2,6);break;case 3:cycle(draft.shadowPcf,0,2);break;case 4:cycle(draft.shadowBias,0,1000,10);break;case 5:cycle(draft.shadowNormal,0,200,10);break;case 6:cycle(draft.shadowSlope,0,8);break;case 7:draft.shadowDebug=1-draft.shadowDebug;break;}
  if(draft!=previous){notice=L"影の変更は「適用」で反映します。";cue(menu_audio::Cursor);}return;
 }
 if(focus_==0){draft.fullscreen=1-draft.fullscreen;draft.refresh_num=0;draft.refresh_den=1;}
 if(focus_==1){std::vector<std::pair<unsigned,unsigned>> sizes;if(!draft.fullscreen)sizes={{1280,720},{1600,900},{1920,1080},{2560,1440},{3840,2160}};for(auto m:modes)sizes.push_back({m.width,m.height});std::sort(sizes.begin(),sizes.end());sizes.erase(std::unique(sizes.begin(),sizes.end()),sizes.end());if(sizes.empty())return;auto it=std::find(sizes.begin(),sizes.end(),std::pair{draft.width,draft.height});int i=it==sizes.end()?0:int(it-sizes.begin());i=(i+step+int(sizes.size()))%int(sizes.size());draft.width=sizes[i].first;draft.height=sizes[i].second;draft.refresh_num=0;draft.refresh_den=1;}
 if(focus_==2){if(!draft.fullscreen){notice=L"ウィンドウ表示のリフレッシュレートはWindowsの設定に従います。";return;}std::vector<std::pair<unsigned,unsigned>> rates={{0,1}};for(auto m:modes)if(m.width==draft.width&&m.height==draft.height&&std::find(rates.begin(),rates.end(),std::pair{m.num,m.den})==rates.end())rates.push_back({m.num,m.den});auto it=std::find(rates.begin(),rates.end(),std::pair{draft.refresh_num,draft.refresh_den});int i=it==rates.end()?0:int(it-rates.begin());i=(i+step+int(rates.size()))%int(rates.size());draft.refresh_num=rates[i].first;draft.refresh_den=rates[i].second;}
 if(focus_==3)draft.renderScale=50+((int(draft.renderScale)-50)/25+step+7)%7*25;
 if(focus_==4)draft.vsync=1-draft.vsync;
 if(focus_==5){const unsigned values[]={0,2,4,8,16};int i=0;while(i<4&&values[i]!=draft.anisotropy)++i;draft.anisotropy=values[(i+step+5)%5];}
 if(focus_==6)draft.mipmaps=1-draft.mipmaps;
 if(focus_==7)draft.linearColor=1-draft.linearColor;
 if(draft==previous)return;notice=L"変更後に「適用」を押してください。";cue(menu_audio::Cursor);
}
void GraphicsSettings::activate(){if(focus_<9)change(1);else if(focus_==9){request();cue(menu_audio::Confirm);}else if(focus_==10){draft={};notice=L"初期値を選択しました。適用で反映します。";cue(menu_audio::Confirm);}else{draft=active;back_=true;cue(menu_audio::Cancel);}}
bool GraphicsSettings::message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 if(pending()){
  if(msg==WM_KEYDOWN){if(wp==VK_ESCAPE&&!(lp&(1LL<<30))){cancel();cue(menu_audio::Cancel);}else if(wp==VK_RETURN&&!(lp&(1LL<<30)))confirm();return true;}
  if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(r.right&&r.bottom){int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;if(y>=599&&y<644){if(x>=120&&x<600)confirm();else if(x>=660&&x<1160){cancel();cue(menu_audio::Cancel);}}}return true;}
  return msg==WM_CHAR;
 }
 if(msg==WM_CHAR)return true;
 if(msg==WM_KEYDOWN&&(wp==VK_NEXT||wp==VK_PRIOR)&&!(lp&(1LL<<30))){page_=(page_+(wp==VK_PRIOR?2:1))%3;focus_=0;cue(menu_audio::Cursor);return true;}
 if(msg==WM_KEYDOWN){if(wp==VK_ESCAPE){draft=active;back_=true;cue(menu_audio::Cancel);}else if(wp==VK_TAB||wp==VK_UP||wp==VK_DOWN){bool prev=wp==VK_UP||(wp==VK_TAB&&(GetKeyState(VK_SHIFT)&0x8000));focus_=(focus_+(prev?11:1))%12;cue(menu_audio::Cursor);}else if(wp==VK_LEFT||wp==VK_RIGHT)change(wp==VK_LEFT?-1:1);else if((wp==VK_RETURN||wp==VK_SPACE)&&!(lp&(1LL<<30)))activate();return true;}
 if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(!r.right||!r.bottom)return true;int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;int oldFocus=focus_;auto queued=cues_.size();if(x>=900&&x<1160&&y>=500&&y<539){page_=(page_+1)%3;focus_=0;cue(menu_audio::Cursor);return true;}if(x>=500&&x<1160&&y>=211&&y<499){focus_=(y-211)/32;change(x<570?-1:1);}else if(y>=599&&y<644){if(x>=120&&x<440)focus_=9;else if(x>=470&&x<800)focus_=10;else if(x>=830&&x<1160)focus_=11;else return true;activate();}if(focus_!=oldFocus&&cues_.size()==queued)cue(menu_audio::Cursor);SetFocus(hwnd);return true;}return false;
}
POINT GraphicsSettings::draw(HDC dc,const std::vector<HFONT>& fonts){
 auto fill=[&](int x,int y,int w,int h,COLORREF c){menu_fill(dc,x,y,w,h,c);};
 auto text=[&](std::wstring s,int x,int y,int w,int h,int font,COLORREF c){RECT r{x,y,x+w,y+h};SelectObject(dc,fonts[font]);SetTextColor(dc,menu_text_color(c));SetBkMode(dc,TRANSPARENT);DrawTextW(dc,s.c_str(),int(s.size()),&r,DT_LEFT|DT_NOPREFIX|DT_WORDBREAK);};
 std::wstring hz=L"自動（モニター推奨）";if(!draft.fullscreen)hz=L"Windowsの設定に従う";else if(draft.refresh_num){wchar_t b[50];swprintf_s(b,L"%.3f Hz",double(draft.refresh_num)/draft.refresh_den);hz=b;}
 const wchar_t* labels[]={L"表示モード",L"出力解像度",L"リフレッシュレート",L"内部描画解像度",L"垂直同期 (VSync)",L"異方性フィルタ",L"ミップマップ",L"色処理",L"足の接地補正 (IK)"};
 std::wstring values[]={draft.fullscreen?L"フルスクリーン":L"ウィンドウ",std::to_wstring(draft.width)+L" × "+std::to_wstring(draft.height),hz,std::to_wstring(draft.renderScale)+L"%",draft.vsync?L"ON":L"OFF",draft.anisotropy?std::to_wstring(draft.anisotropy)+L"x":L"OFF",draft.mipmaps?L"ON":L"OFF",draft.linearColor?L"Linear（強化・試験）":L"Original / Legacy",draft.footIk?L"ON":L"OFF"};
 const wchar_t* shadowLabels[]={L"方向光の影",L"影の解像度",L"カスケード数",L"輪郭のぼかし (PCF)",L"深度バイアス",L"法線バイアス",L"傾斜バイアス",L"分割の色表示"};
 if(page_==1){for(unsigned i=0;i<8;++i)labels[i]=shadowLabels[i];values[0]=draft.shadowEnabled?L"ON":L"OFF（Legacy）";values[1]=std::to_wstring(draft.shadow);values[2]=std::to_wstring(draft.shadowCascades);values[3]=std::to_wstring(draft.shadowPcf*2+1)+L" × "+std::to_wstring(draft.shadowPcf*2+1);values[4]=std::to_wstring(draft.shadowBias)+L" × 0.00001";values[5]=std::to_wstring(draft.shadowNormal)+L" mm";values[6]=std::to_wstring(draft.shadowSlope);values[7]=draft.shadowDebug?L"ON":L"OFF";labels[8]=L"GPU時間グラフ (F12)";values[8]=draft.gpuTiming?L"ON":L"OFF";}
 if(page_==2){const wchar_t* effectLabels[]={L"HDR / トーンマッピング",L"輪郭のなめらかさ (AA)",L"接触部の陰影 (AO)",L"光のにじみ (Bloom)",L"高度な反射",L"遠距離メッシュ (LOD)",L"煙と床のなじみ",L"明るさ（HDR露出）",L"追加効果をまとめて変更"};unsigned valuesOn[]={draft.hdr,draft.aa,draft.ao,draft.bloom,draft.reflections,draft.lod,draft.softParticles};bool all=true;for(unsigned i=0;i<9;++i)labels[i]=effectLabels[i];for(unsigned i=0;i<7;++i){values[i]=valuesOn[i]?L"ON":L"OFF";all=all&&valuesOn[i];}wchar_t b[32];swprintf_s(b,L"%.2f",draft.exposureMilli/1000.0);values[7]=b;values[8]=all?L"すべてOFFにする":L"すべてONにする";}
 for(int i=0;i<9;++i){int y=211+i*32;menu_row(dc,120,y,1040,30,i,!pending()&&focus_==i);text(labels[i],125,y+3,365,30,2,RGB(224,232,212));text(L"◀  "+values[i]+L"  ▶",516,y+3,630,30,2,RGB(232,237,218));}
 text(page_==1?L"影は60mまで。GPU上限時は解像度を縮小。":page_==2?L"すべてOFFで従来表示。LODは静的ステージに適用。":L"初期値はLegacy。内部解像度は負荷上限で縮小。",120,508,765,30,3,RGB(186,204,169));text(page_==0?L"影・診断へ →":page_==1?L"描画効果へ →":L"基本設定へ →",900,508,250,30,2,RGB(244,218,161));
 std::wstring message=notice;if(pending())message+=L"  残り "+std::to_wstring((until_>GetTickCount64()?(until_-GetTickCount64()+999)/1000:0))+L" 秒";
 text(message,120,545,1040,49,2,RGB(244,218,161));
 auto button=[&](int x,int w,int i,std::wstring label){fill(x,599,w,44,focus_==i?RGB(151,168,126):RGB(37,53,42));text(label,x+15,610,w-30,32,1,focus_==i?RGB(18,28,19):RGB(232,237,218));};
 if(pending()){button(120,480,9,L"この設定を保存 [Enter]");button(660,500,11,L"元に戻す [Esc]");}else{button(120,320,9,L"適用");button(470,330,10,L"初期値に戻す");button(830,330,11,L"ネットワークへ戻る");}
 text(L"PageUp/Down：基本/影/効果   ↑ ↓：項目   ← →：変更   Enter：決定",83,690,1120,25,3,RGB(174,185,165));
 if(pending())return {120,599};
 return focus_<9?POINT{120,211+focus_*32}:POINT{focus_==9?120:focus_==10?470:830,599};
}
}

