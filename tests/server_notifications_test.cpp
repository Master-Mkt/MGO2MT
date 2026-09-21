#include "server_notifications.h"
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace mgo2mt::notifications;
namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
Scope scope(){return {1,2,3};}
Alert alert(){return {10,1,1000,100000,"最新のお知らせ：メンテナンス情報を確認してください。"};}
void state(){
 Presentation p;std::array initial{Notice{Kind::mail,10},Notice{Kind::tournament,20,5000},Notice{Kind::survival,30,6000}};
 auto v=p.update(scope(),initial,alert(),2000,100);check(v.badges==std::array{true,true,true}&&!p.take_ping(),"initial unread and invitation badges without receipt sound");
 const auto began=v.news->beganMs;for(unsigned i=0;i<4;++i){v=p.update(scope(),initial,alert(),2000+i,100+i*15000);check(!p.take_ping()&&v.news->beganMs==began,"15-second poll never repeats receipt or restarts news");}
 auto arrived=initial;arrived[0].id=11;arrived[1].id=21;arrived[2].id=31;v=p.update(scope(),arrived,alert(),3000,60100);check(p.take_ping()&&!p.take_ping(),"three simultaneous arrivals coalesce into one drainable sound edge");
 p.update(scope(),arrived,alert(),3001,60101);check(!p.take_ping(),"render/update duplicates never repeat receipt");
 check(!p.acknowledge({2,2,3},Kind::mail,11)&&p.acknowledge(scope(),Kind::mail,11)&&!p.view().badges[0],"ack scope checked and local mail badge removed");
 p.update(scope(),arrived,alert(),3002,60102);check(!p.view().badges[0]&&!p.take_ping(),"acknowledged still-unread server snapshot cannot reannounce");
 arrived[0].id=9;p.update(scope(),arrived,alert(),3003,60103);check(p.view().badges[0]&&!p.take_ping(),"aggregate mail falls back to older unread ID without false arrival");
 arrived[0].id=12;p.update(scope(),arrived,alert(),3004,60104);check(p.take_ping(),"higher mail arrival still announces");
 arrived[1].active=arrived[2].active=false;p.update(scope(),arrived,alert(),4000,61000);check(!p.view().badges[1]&&!p.view().badges[2],"accept decline cancel remove invitation badges");
 arrived[1].id=22;arrived[1].active=true;arrived[1].expiresUnixMs=4100;p.update(scope(),arrived,alert(),4099,61099);p.update(scope(),arrived,alert(),4100,61100);check(!p.take_ping()&&!p.view().badges[1],"expiry cancels undrained arrival sound and badge at exact boundary");
 arrived[2]={Kind::survival,32,5000,true};p.update(scope(),arrived,alert(),4200,61200);check(p.acknowledge(scope(),Kind::survival,32)&&!p.take_ping(),"local acknowledgement cancels pending sound");
 auto changed=scope();changed.generation++;v=p.update(changed,initial,alert(),2000,62000);check(v.badges[0]&&!p.take_ping(),"new generation resets old acknowledgement and baselines unread quietly");
 changed.character++;p.update(changed,initial,alert(),2000,62001);check(!p.take_ping(),"character identity change baselines quietly");changed.connection++;p.update(changed,initial,alert(),2000,62002);check(!p.take_ping(),"connection change baselines quietly");
 p.update(changed,arrived,alert(),2000,1);check(!p.take_ping(),"monotonic rollback baselines without replay");p.reset();check(p.view().badges==std::array{false,false,false}&&!p.take_ping(),"disconnect clears badges and pending audio");
 p.update(scope(),{},std::nullopt,2000,100,{false,true,true});std::array mail{Notice{Kind::mail,40}};v=p.update(scope(),mail,std::nullopt,2000,200,{true,true,true});check(v.badges[0]&&!p.take_ping(),"first completed mail snapshot is quiet even after invitation subscription started");
 std::array invitation{Notice{Kind::tournament,50}};p.update(scope(),invitation,std::nullopt,2000,201);check(p.take_ping(),"new push invitation after initialized empty cache announces");
 p.reset();std::vector<Notice> capacity;for(unsigned i=1;i<=256;++i)capacity.push_back({Kind::mail,i});p.update(scope(),capacity,std::nullopt,2000,0);std::array extra{Notice{Kind::mail,257}};v=p.update(scope(),extra,std::nullopt,2000,1);check(v.badges[0]&&!p.take_ping(),"bounded seen memory does not evict old IDs and replay sound");extra[0].id=258;p.update(scope(),extra,std::nullopt,2000,2);check(!p.take_ping(),"sound remains safely saturated until new scope");
 auto bad=initial;bad[1]=bad[0];p.update(scope(),bad,std::nullopt,2000,3);check(p.view().badges==std::array{false,false,false}&&!p.take_ping(),"duplicate authoritative IDs rejected without partial updates");
}
void news(){
 auto jp=news_text("日本語🎮\r\n次の行\t情報");check(jp&&jp->find(L"日本語")==0&&jp->find(L'\n')==std::wstring::npos&&jp->find(L'\t')==std::wstring::npos,"strict Unicode and native single-line whitespace flattening");
 for(const auto& s:std::vector<std::string>{"",std::string(4097,'a'),"\xc0\xaf","\xed\xa0\x80","\xf4\x90\x80\x80",std::string("a\0b",3),"\xe2\x80\xae"})check(!news_text(s),"malformed/control/bidi/oversized text rejected");
 Presentation p;auto a=alert();auto v=p.update(scope(),{},a,999,100);check(!v.news,"future publication excluded");v=p.update(scope(),{},a,1000,200);check(v.news&&v.news->id==a.id&&v.news->beganMs==200&&!p.take_ping(),"publication boundary admits latest text without notification cue");
 a.version=2;a.text="更新された最新情報";v=p.update(scope(),{},a,1001,300);check(v.news&&v.news->version==2&&v.news->beganMs==300,"new alert revision starts one new scrolling pass");
 auto older=a;older.version=1;older.text="古い内容";v=p.update(scope(),{},older,1002,301);check(v.news&&v.news->version==2&&v.news->text==L"更新された最新情報","old revision cannot replace latest message");
 auto rewrite=a;rewrite.text="同じ版を改変";v=p.update(scope(),{},rewrite,1003,302);check(v.news&&v.news->text==L"更新された最新情報","same ID/version text rewrite cannot replace verified content");
 v=p.update(scope(),{},a,a.expiresUnixMs,400);check(!v.news,"expired alert removed at exact boundary");p.update(scope(),{},alert(),2000,500);check(!p.update(scope(),{},std::nullopt,2000,501).news,"successful empty latest alert clears prior ticker");
}
void save(const std::filesystem::path& path,const BITMAPINFOHEADER& info,const void* data){BITMAPFILEHEADER h{};h.bfType=0x4d42;h.bfOffBits=sizeof(h)+sizeof(info);h.bfSize=h.bfOffBits+1280*720*4;std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(&h),sizeof(h));f.write(reinterpret_cast<const char*>(&info),sizeof(info));f.write(static_cast<const char*>(data),1280*720*4);check(bool(f),"notification BMP written");}
void render(const std::filesystem::path& out){
 HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO b{};b.bmiHeader.biSize=40;b.bmiHeader.biWidth=1280;b.bmiHeader.biHeight=-720;b.bmiHeader.biPlanes=1;b.bmiHeader.biBitCount=32;void* data=nullptr;auto bmp=CreateDIBSection(dc,&b,DIB_RGB_COLORS,&data,nullptr,0);check(dc&&bmp&&data,"actual notification DIB allocated");auto old=SelectObject(dc,bmp);std::span<uint32_t> pixels(static_cast<uint32_t*>(data),1280*720);constexpr uint32_t bg=0xff20252b;
 Renderer r;Presentation p;std::array all{Notice{Kind::mail,1},Notice{Kind::tournament,2},Notice{Kind::survival,3}};auto v=p.update(scope(),all,alert(),2000,0);std::fill(pixels.begin(),pixels.end(),bg);check(r.paint(pixels,1280,720,v,6000),"all badges and Japanese ticker draw");
 for(unsigned kind=0;kind<3;++kind){unsigned count=0;int left=1140+int(kind)*40;for(int y=76;y<108;++y)for(int x=left;x<left+32;++x)if(pixels[y*1280+x]==orange)++count;check(count>80,"each distinct native orange glyph has visible pixels");}
 unsigned text=0;for(int y=112;y<140;++y)for(int x=24;x<1256;++x){auto c=pixels[y*1280+x];if((c&255)>150&&((c>>8)&255)>150&&((c>>16)&255)>150)++text;}check(text>150,"Japanese flash-news has actual white glyph pixels");
 for(int y=0;y<720;++y)for(int x=0;x<1280;++x)if(!((y>=76&&y<108&&x>=1140&&x<1256)||(y>=112&&y<140&&x>=24&&x<1256)))check(pixels[size_t(y)*1280+x]==bg,"notification paint avoids other HUD and original invitation ticker bands");
 if(!out.empty())save(out/"three_icons_news.bmp",b.bmiHeader,data);
 v=p.update(scope(),all,alert(),2500,500);std::fill(pixels.begin(),pixels.end(),bg);r.paint(pixels,1280,720,v,6500);check(std::count(pixels.begin(),pixels.end(),dimOrange)>240&&std::count(pixels.begin(),pixels.end(),orange)==0,"blink dims all three orange icons without changing their identity");if(!out.empty())save(out/"three_icons_dim.bmp",b.bmiHeader,data);
 PaintLayout layout;layout.excluded=Rect{1000,0,1280,200};std::fill(pixels.begin(),pixels.end(),bg);r.paint(pixels,1280,720,v,6500,layout);for(int y=0;y<200;++y)for(int x=1000;x<1280;++x)check(pixels[y*1280+x]==bg,"caller video exclusion prevents both icon and ticker overlap");if(!out.empty())save(out/"video_exclusion.bmp",b.bmiHeader,data);
 std::fill(pixels.begin(),pixels.end(),bg);auto empty=View{};check(!r.paint(pixels,1280,720,empty,6500)&&std::all_of(pixels.begin(),pixels.end(),[](auto c){return c==bg;}),"disconnect on fresh frame has no stale icons or news");check(!r.paint(pixels.first(1),1280,720,v,6500),"undersized buffer rejected");
 SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);
}
}
int main(int argc,char** argv){try{state();news();std::filesystem::path out;if(argc==2){out=argv[1];std::filesystem::create_directories(out);}render(out);std::cout<<"server notifications PASS: per-kind baseline/arrival/ack/expiry/scope, bounded audio edge, strict latest Japanese news, real orange GDI pixels and movie exclusion\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
