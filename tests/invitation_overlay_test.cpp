#include "invitation_overlay.h"
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace mgo2win::invitation_ui;
namespace {
void check(bool b,const char*why){if(!b)throw std::runtime_error(why);}
void bmp(const std::filesystem::path&path,const std::vector<uint32_t>&pixels){
 BITMAPFILEHEADER f{};BITMAPINFOHEADER b{};b.biSize=40;b.biWidth=1280;b.biHeight=-720;b.biPlanes=1;b.biBitCount=32;b.biSizeImage=DWORD(pixels.size()*4);f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(b);f.bfSize=f.bfOffBits+b.biSizeImage;
 std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&f),sizeof(f));out.write(reinterpret_cast<const char*>(&b),sizeof(b));out.write(reinterpret_cast<const char*>(pixels.data()),b.biSizeImage);check(bool(out),"BMP write");
}
int text_width(const std::wstring&text){auto dc=CreateCompatibleDC(nullptr);auto font=CreateFontW(-23,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FIXED_PITCH,L"MS Gothic");auto old=SelectObject(dc,font);SIZE extent{};check(GetTextExtentPoint32W(dc,text.data(),int(text.size()),&extent),"measure Japanese full text");SelectObject(dc,old);DeleteObject(font);DeleteDC(dc);return extent.cx;}
bool ink(const std::vector<uint32_t>&p){return std::any_of(p.begin(),p.end(),[](auto x){return x!=0;});}
}
int wmain(int argc,wchar_t**argv){try{
 std::filesystem::path output=argc>1?argv[1]:L"";if(!output.empty())std::filesystem::create_directories(output);
 std::vector<uint32_t> canvas(1280*720);Overlay ui;check(!ui.paint(canvas,1280,720,100)&&!ink(canvas)&&!ui.open(100),"no invitation means no demo or modal");
 View view{10,77,100,120100,L"日本語十六文字の隊長から、サバイバル登録ロビーへの招待が届きました。START メニューの「招待への返答」から内容を確認してください。長いメッセージも末尾まで表示します。【案内の末尾】",true,true};
 ui.sync(view,100);check(ui.available()&&!ui.visible(),"received message never opens a combat-stealing modal");
 check(ui.paint(canvas,1280,720,2100)&&ink(canvas),"white marquee enters view");auto early=canvas;
 for(int y=0;y<720;++y)for(int x=0;x<1280;++x)if(canvas[y*1280+x])check(x>=48&&x<1232&&y>=28&&y<60,"marquee clips only inside top safe area");
 check(std::any_of(canvas.begin(),canvas.end(),[](auto p){return (p>>24)>0&&(p&0xffffff)==0xffffff;}),"white text coverage has valid alpha");
 if(!output.empty())bmp(output/L"marquee-enter.bmp",canvas);
 const uint64_t tail=100+uint64_t((1184+text_width(view.marquee)-90)/.096);std::fill(canvas.begin(),canvas.end(),0);check(ui.paint(canvas,1280,720,tail)&&ink(canvas)&&canvas!=early,"long Japanese tail crosses viewport without ellipsis");
 if(!output.empty())bmp(output/L"marquee-tail.bmp",canvas);
 const auto done=tail+3000;ui.sync(view,done);std::fill(canvas.begin(),canvas.end(),0);check(!ui.paint(canvas,1280,720,done)&&!ink(canvas)&&ui.available(),"single pass finishes while reply remains available; duplicate sync never restarts");
 ui.set_menu_hint(true);check(ui.paint(canvas,1280,720,done)&&!ui.visible(),"menu hint does not open dialog");if(!output.empty())bmp(output/L"menu-hint.bmp",canvas);
 for(int y=0;y<720;++y)for(int x=0;x<1280;++x)if(canvas[y*1280+x])check(x>=320&&x<960&&y>=652&&y<688,"menu hint stays within bounded bottom line");
 check(!ui.hit_test(400,330),"closed dialog cannot be clicked");ui.set_menu_hint(false);std::fill(canvas.begin(),canvas.end(),0);check(!ui.paint(canvas,1280,720,done),"menu hint disappears outside menu");
 check(ui.open(done)&&ui.visible()&&!ui.selected_accept(),"explicit open defaults to decline");ui.move(-1);check(ui.selected_accept(),"menu move selects accept");ui.choose(false);check(!ui.selected_accept(),"explicit selection");
 check(ui.hit_test(150,325)==std::optional(true)&&ui.hit_test(529,368)==std::optional(true)&&ui.hit_test(900,350)==std::optional(false),"horizontal accept-left decline-right hit rectangles");check(!ui.hit_test(530,325)&&!ui.hit_test(749,325)&&!ui.hit_test(1130,325)&&!ui.hit_test(400,369),"gaps and exclusive right/bottom borders never activate");
 check(ui.paint(canvas,1280,720,done),"Japanese amber dialog paints");if(!output.empty())bmp(output/L"dialog-decline.bmp",canvas);
 ui.choose(true);std::fill(canvas.begin(),canvas.end(),0);check(ui.paint(canvas,1280,720,done),"accept option paints");if(!output.empty())bmp(output/L"dialog-accept.bmp",canvas);
 auto response=ui.confirm(done);check(response&&response->scope==10&&response->id==77&&response->accept&&!ui.visible(),"response retains exact scope/id and explicit choice");check(!ui.confirm(done),"one confirmation only");ui.sync(view,done);check(!ui.open(done)&&!ui.available(),"duplicate original invitation cannot replay answered UI");
 ++view.id;view.receivedAt=done;view.expiresAt=done+1000;ui.sync(view,done);check(ui.open(done),"new invitation can be opened");ui.close();check(!ui.visible()&&ui.available(),"cancel dialog preserves invitation");ui.open(done);check(!ui.confirm(done+1000)&&!ui.visible(),"expiry boundary rejects stale response");
 view.receivedAt=done+1000;view.expiresAt=done+2000;view.id++;ui.sync(view,done+1000);ui.open(done+1000);view.scope++;ui.sync(view,done+1000);check(!ui.visible(),"connection scope change closes modal");view.available=false;ui.sync(view,done+1000);check(!ui.available()&&!ui.open(done+1000),"disconnect clears invitation eligibility");
 view.available=true;view.respondable=false;view.id++;ui.sync(view,done+1000);check(ui.available()&&!ui.open(done+1000),"unsupported response mode cannot open action dialog");ui.set_menu_hint(true);std::fill(canvas.begin(),canvas.end(),0);ui.paint(canvas,1280,720,done+1000);check(!std::any_of(canvas.begin()+652*1280,canvas.end(),[](auto p){return p!=0;}),"unsupported mode has no actionable hint");
 for(auto bad:std::vector<std::wstring>{L"",L"bad\nline",std::wstring(513,L'長'),std::wstring(1,wchar_t(0xd800)),std::wstring(1,wchar_t(0xdc00)),std::wstring(L"embedded\0nul",12)}){view.marquee=bad;view.id++;ui.sync(view,done+1000);check(!ui.available(),"invalid or unbounded UTF16 cannot paint stale invitation");}
 view.marquee=L"有効な招待";view.respondable=true;view.id++;ui.sync(view,done+1000);check(ui.open(done+1000),"valid view restored");check(!ui.confirm(done+999),"clock rollback prevents response");
 view.id++;ui.sync(view,done+1000);check(ui.open(done+1000),"new scoped invitation after clock reset");std::vector<uint32_t> guarded(640*360+2,0x12345678);auto clippedCanvas=std::span<uint32_t>(guarded).subspan(1,640*360);std::fill(clippedCanvas.begin(),clippedCanvas.end(),0);check(ui.paint(clippedCanvas,640,360,done+1001),"clippedCanvas surface clips dialog");check(guarded.front()==0x12345678&&guarded.back()==0x12345678,"surface clipping preserves canaries");check(!ui.paint({},1280,720,done+1001),"short span rejected");
 std::cout<<"Invitation UI Japanese marquee/dialog, scope, expiry, explicit response, alpha/clipping PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
