#include "menu_font.h"
#include "enemy_name_tag.h"
#include <algorithm>
#include <string>
#include <stdexcept>

namespace mgo2win::hud {
namespace {
constexpr int maxTextWidth=416,textHeight=24,panelHeight=40,iconSize=32;
constexpr uint32_t amber=0x00ffd183,brown=0x002b2014;
std::wstring decode_name(std::string_view name){
    if(name.empty()||name.size()>256)return {};
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name.data(),int(name.size()),nullptr,0);
    if(!n)return {};
    std::wstring result(n,L'\0');
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name.data(),int(name.size()),result.data(),n)!=n)return {};
    for(auto c:result)if(c<32||c==127)return {};
    return result;
}
uint32_t premultiply(uint32_t rgb,unsigned alpha){
    uint32_t pixel=alpha<<24;
    for(unsigned shift:{0u,8u,16u})pixel|=((((rgb>>shift)&255)*alpha+127)/255)<<shift;
    return pixel;
}
void over(uint32_t& destination,uint32_t source){
    const uint32_t sa=source>>24;if(!sa)return;
    const uint32_t da=destination>>24,inverse=255-sa,alpha=sa*255+da*inverse;
    uint32_t result=((alpha+127)/255)<<24;
    for(unsigned shift:{0u,8u,16u}){
        const auto sp=std::min((source>>shift)&255,sa);
        const auto numerator=sp*65025+((destination>>shift)&255)*da*inverse;
        result|=std::min(255u,(numerator+alpha/2)/alpha)<<shift;
    }
    destination=result;
}
struct ClanPixels {const uint32_t* pixels=nullptr;int edge=0;};
ClanPixels clan_pixels(HBITMAP image){
    DIBSECTION dib{};
    if(!image||GetObjectW(image,sizeof(dib),&dib)!=sizeof(dib)||!dib.dsBm.bmBits||
       (dib.dsBm.bmWidth!=32&&dib.dsBm.bmWidth!=64)||dib.dsBm.bmHeight!=dib.dsBm.bmWidth||
       dib.dsBm.bmBitsPixel!=32||dib.dsBm.bmWidthBytes!=dib.dsBm.bmWidth*4)return {};
    return {static_cast<const uint32_t*>(dib.dsBm.bmBits),dib.dsBm.bmWidth};
}
uint32_t clan_sample(const ClanPixels& image,int x,int y){
    const int scale=image.edge/iconSize;uint32_t sums[4]{};
    for(int dy=0;dy<scale;++dy)for(int dx=0;dx<scale;++dx){
        const auto p=image.pixels[(y*scale+dy)*image.edge+x*scale+dx],a=p>>24;
        for(unsigned c=0;c<3;++c)sums[c]+=std::min((p>>(c*8))&255,a);
        sums[3]+=a;
    }
    const unsigned count=unsigned(scale*scale);uint32_t result=0;
    for(unsigned c=0;c<4;++c)result|=((sums[c]+count/2)/count)<<(c*8);
    return result;
}
}
struct EnemyNameTagRenderer::Impl {
    HDC dc=nullptr;HBITMAP mask=nullptr;HFONT font=nullptr;HGDIOBJ oldBitmap=nullptr,oldFont=nullptr;
    uint32_t* pixels=nullptr;std::wstring cached;int textWidth=0;
    Impl(){
        dc=CreateCompatibleDC(nullptr);
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=maxTextWidth;
        info.bmiHeader.biHeight=-textHeight;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        if(dc)mask=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
        font=create_menu_font(18,FW_NORMAL);
        if(!dc||!mask||!pixels||!font){release();throw std::runtime_error("Enemy tag GDI allocation");}
        oldBitmap=SelectObject(dc,mask);oldFont=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
    }
    void release(){
        if(dc&&oldFont)SelectObject(dc,oldFont);if(dc&&oldBitmap)SelectObject(dc,oldBitmap);
        if(font)DeleteObject(font);if(mask)DeleteObject(mask);if(dc)DeleteDC(dc);
        dc=nullptr;mask=nullptr;font=nullptr;pixels=nullptr;
    }
    ~Impl(){release();}
    bool prepare(const std::wstring& name,int limit){
        if(name==cached)return textWidth>0;
        cached.clear(); // Failed GDI preparation cannot reuse stale glyph pixels.
        SIZE extent{};if(!GetTextExtentPoint32W(dc,name.data(),int(name.size()),&extent))return false;
        textWidth=std::clamp(int(extent.cx),1,limit);
        std::fill_n(pixels,maxTextWidth*textHeight,0u);
        RECT rect{0,0,textWidth,textHeight};
        if(!DrawTextW(dc,name.data(),int(name.size()),&rect,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS))return false;
        GdiFlush();cached=name;return true;
    }
};
EnemyNameTagRenderer::EnemyNameTagRenderer()=default;
EnemyNameTagRenderer::~EnemyNameTagRenderer()=default;
bool EnemyNameTagRenderer::paint(std::span<uint32_t> destination,int width,int height,int anchorX,int anchorY,
                                std::string_view utf8Name,HBITMAP clan,std::optional<EnemyVitals> vitals){
    if(width<=0||height<=0||destination.size()<size_t(width)*size_t(height))return false;
    try{
        auto name=decode_name(utf8Name);if(name.empty())return false;
        if(vitals){if(!vitals->maxHp||vitals->hp>vitals->maxHp)return false;name+=vitals->level?L"  LV "+std::to_wstring(*vitals->level):L"  LV --";}
        if(!impl_)impl_=std::make_unique<Impl>();if(!impl_->prepare(name,vitals?416:336))return false;
        const auto image=clan_pixels(clan);const int iconSpace=image.pixels?iconSize+6:0;
        const int panelWidth=16+iconSpace+impl_->textWidth;
        const int actualHeight=panelHeight+(vitals?12:0);
        const int64_t left=int64_t(anchorX)-panelWidth/2,top=int64_t(anchorY)-actualHeight;
        if(left>=width||top>=height||left+panelWidth<=0||top+actualHeight<=0)return false;
        auto put=[&](int x,int y,uint32_t source){const auto px=left+x,py=top+y;
            if(px>=0&&py>=0&&px<width&&py<height)over(destination[size_t(py)*size_t(width)+size_t(px)],source);};
        const auto background=premultiply(brown,112);
        for(int y=0;y<actualHeight;++y)for(int x=0;x<panelWidth;++x)put(x,y,background);
        if(vitals){const int barWidth=panelWidth-16;const int filled=int(uint64_t(barWidth)*vitals->hp/vitals->maxHp);
            for(int y=40;y<47;++y)for(int x=0;x<barWidth;++x)put(8+x,y,premultiply(x<filled?amber:0x0014110d,240));}
        if(image.pixels)for(int y=0;y<iconSize;++y)for(int x=0;x<iconSize;++x)put(8+x,4+y,clan_sample(image,x,y));
        for(int y=0;y<textHeight;++y)for(int x=0;x<impl_->textWidth;++x){
            const auto p=impl_->pixels[y*maxTextWidth+x];const auto coverage=std::max({p&255,(p>>8)&255,(p>>16)&255});
            if(coverage)put(8+iconSpace+x,8+y,premultiply(amber,coverage));
        }
        return true;
    }catch(const std::exception&){return false;}
}
}
