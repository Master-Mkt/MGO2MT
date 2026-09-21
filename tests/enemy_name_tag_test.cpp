#include "enemy_name_tag.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using mgo2mt::hud::EnemyNameTagRenderer;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Bitmap {
    HBITMAP value=nullptr;uint32_t* pixels=nullptr;int edge;
    explicit Bitmap(int n):edge(n){BITMAPINFO info{};info.bmiHeader.biSize=40;info.bmiHeader.biWidth=n;info.bmiHeader.biHeight=-n;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        value=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);check(value&&pixels,"DIB allocation");std::fill_n(pixels,n*n,0u);}
    ~Bitmap(){if(value)DeleteObject(value);}
};
static RECT bounds(const std::vector<uint32_t>& p,int width){RECT r{width,int(p.size()/width),-1,-1};
    for(size_t i=0;i<p.size();++i)if(p[i]){r.left=std::min(r.left,LONG(i%width));r.right=std::max(r.right,LONG(i%width));r.top=std::min(r.top,LONG(i/width));r.bottom=std::max(r.bottom,LONG(i/width));}return r;}
static void bmp(const std::filesystem::path& path,const std::vector<uint32_t>& pixels,int width,int height){
    std::vector<unsigned char> header(54);header[0]='B';header[1]='M';auto put=[&](size_t at,uint32_t v){for(unsigned i=0;i<4;++i)header[at+i]=static_cast<unsigned char>(v>>(i*8));};
    put(2,54+uint32_t(pixels.size()*4));put(10,54);put(14,40);put(18,width);put(22,uint32_t(-height));header[26]=1;header[28]=32;
    std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(header.data()),header.size());f.write(reinterpret_cast<const char*>(pixels.data()),pixels.size()*4);check(bool(f),"BMP output");
}
int wmain(int argc,wchar_t** argv){try{
    EnemyNameTagRenderer renderer;constexpr int w=400,h=200;std::vector<uint32_t> plain(w*h);
    check(renderer.paint(plain,w,h,200,100,"Enemy-01"),"ASCII name");auto r=bounds(plain,w);check(r.bottom==99&&r.top==60&&r.right-r.left<256,"anchor bottom and bounded panel");
    auto first=plain;std::fill(plain.begin(),plain.end(),0);check(renderer.paint(plain,w,h,200,100,"Enemy-01")&&plain==first,"cached glyph repeat");
    check(std::any_of(plain.begin(),plain.end(),[](uint32_t p){return (p>>24)>112;}),"GDI glyph alpha exists");
    const std::string japanese="\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e";
    std::fill(plain.begin(),plain.end(),0);check(renderer.paint(plain,w,h,200,100,japanese)&&plain!=first,"UTF8 Japanese renders");
    for(const auto& invalid:std::vector<std::string>{"",std::string("bad\0name",8),"line\nname",std::string("\xc0\xaf",2),std::string("\xed\xa0\x80",3),std::string(257,'A')}){
        auto before=plain;check(!renderer.paint(plain,w,h,200,100,invalid)&&plain==before,"invalid name must not paint stale name");}
    check(!renderer.paint({},w,h,200,100,"A"),"short surface");auto before=plain;
    check(!renderer.paint(plain,w,h,std::numeric_limits<int>::min(),100,"A")&&plain==before,"negative extreme clipped");
    check(!renderer.paint(plain,w,h,std::numeric_limits<int>::max(),100,"A")&&plain==before,"positive extreme clipped");
    Bitmap transparent64(64),pattern64(64),pattern32(32),invalid16(16);
    for(int y=0;y<64;++y)for(int x=0;x<64;++x)pattern64.pixels[y*64+x]=x<32?0xff000000u:0x80800000u;
    for(int y=0;y<32;++y)for(int x=0;x<32;++x)pattern32.pixels[y*32+x]=x<16?0xff000000u:0x80800000u;
    std::vector<uint32_t> a(w*h),b(w*h),c(w*h);
    check(renderer.paint(a,w,h,200,100,"A",pattern64.value),"64px clan");
    check(renderer.paint(b,w,h,200,100,"A",pattern32.value)&&a==b,"32 and64 equivalent premultiplied clan");
    check(renderer.paint(c,w,h,200,100,"A",transparent64.value),"transparent clan");auto cr=bounds(a,w);
    const size_t black=size_t(cr.top+4+8)*w+cr.left+8+8,red=size_t(cr.top+4+8)*w+cr.left+8+24;
    check(a[black]==0xff000000u,"opaque black clan survives final surface");
    check((a[red]>>24)==184&&((a[red]>>16)&255)>180&&((a[red]>>8)&255)<20,"half-alpha red composites without dark fringe");
    check((c[black]>>24)==112&&c[black]==c[red],"transparent clan leaves panel unchanged");
    std::fill(a.begin(),a.end(),0);std::fill(b.begin(),b.end(),0);
    check(renderer.paint(a,w,h,200,100,"A")&&renderer.paint(b,w,h,200,100,"A",invalid16.value)&&a==b,"missing/invalid clan has no invented icon slot");
    std::vector<uint32_t> guarded(w*h+2,0x12345678);std::span<uint32_t> canvas(guarded.data()+1,w*h);std::fill(canvas.begin(),canvas.end(),0);
    check(renderer.paint(canvas,w,h,0,15,"A",pattern64.value),"partial top/left clipping");
    check(renderer.paint(canvas,w,h,w,h+15,"A",pattern32.value),"partial right/bottom clipping");
    check(guarded.front()==0x12345678&&guarded.back()==0x12345678,"clipping retains canaries");
    std::fill(a.begin(),a.end(),0);check(renderer.paint(a,w,h,200,100,std::string(256,'W')),"long valid name ellipsized");check(bounds(a,w).right-bounds(a,w).left+1==352,"non-name oversized text remains capped336");
    const std::string japanese16="日本語名前十六文字表示確認用兵士";
    {
        const std::wstring text=L"日本語名前十六文字表示確認用兵士";
        auto dc=CreateCompatibleDC(nullptr);auto font=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"MS Gothic");
        check(dc&&font,"measure original enemy font");auto old=SelectObject(dc,font);std::vector<WORD>glyphs(text.size());SIZE size{};
        check(GetGlyphIndicesW(dc,text.data(),int(text.size()),glyphs.data(),GGI_MARK_NONEXISTING_GLYPHS)!=GDI_ERROR&&std::none_of(glyphs.begin(),glyphs.end(),[](WORD v){return v==0xffff;}),"enemy Japanese16 has real glyphs");
        check(GetTextExtentPoint32W(dc,text.data(),int(text.size()),&size)&&size.cx>240&&size.cx<=336,"original18px full Japanese16 fits expanded name budget");
        std::fill(a.begin(),a.end(),0);check(renderer.paint(a,w,h,200,100,japanese16,pattern64.value),"complete Japanese16 enemy name");auto full=bounds(a,w);
        check(full.right-full.left+1==16+38+size.cx&&full.bottom-full.top+1==40,"exact measured whole-name width with same32clan and panel height");
        std::cout<<"Enemy Japanese16 measured width "<<size.cx<<", panel "<<full.right-full.left+1<<'\n';
        SelectObject(dc,old);DeleteObject(font);DeleteDC(dc);
    }
    std::vector<uint32_t> fullHp(w*h),halfHp(w*h),noHp(w*h);
    using mgo2mt::hud::EnemyVitals;
    check(renderer.paint(fullHp,w,h,200,100,"Enemy",pattern64.value,EnemyVitals{22,1000,1000}),"target stats render");
    check(renderer.paint(halfHp,w,h,200,100,"Enemy",pattern64.value,EnemyVitals{22,500,1000}),"target half HP");
    check(renderer.paint(noHp,w,h,200,100,"Enemy",pattern64.value,EnemyVitals{22,0,1000}),"target empty HP");
    auto fr=bounds(fullHp,w);check(fr.top==48&&fr.bottom==99,"stats bottom anchor");
    size_t filled=0,empty=0;for(int x=fr.left+8;x<=fr.right-8;++x){auto at=size_t(fr.top+42)*w+x;filled+=halfHp[at]==fullHp[at];empty+=halfHp[at]==noHp[at];}check(filled&&empty&&std::abs(int(filled)-int(empty))<=1,"HP bar tracks exact authoritative fraction");
    auto unchanged=halfHp;check(!renderer.paint(halfHp,w,h,200,100,"Enemy",nullptr,EnemyVitals{22,1,0})&&halfHp==unchanged,"invalid HP atomic");
    check(renderer.paint(halfHp,w,h,200,100,"Enemy",nullptr,EnemyVitals{std::nullopt,1,1}),"unknown LV placeholder");
    if(argc==2){std::vector<uint32_t> preview(640*360,0xff343b43);
        renderer.paint(preview,640,360,320,75,"Enemy-01");renderer.paint(preview,640,360,320,145,japanese16,pattern64.value);
        renderer.paint(preview,640,360,22,240,"Enemy-name-clipped-left",pattern32.value);renderer.paint(preview,640,360,630,330,"Enemy-right-edge");bmp(argv[1],preview,640,360);}
    std::cout<<"enemy_name_tag_test: native GDI UTF8/ellipsis, optional32/64clan, original alpha/black, invalid names and clipped final-surface canaries passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
