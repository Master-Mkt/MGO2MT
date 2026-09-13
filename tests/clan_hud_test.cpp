#include "clan_hud.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string_view>
using namespace mgo2win::clan;
static void check(bool value,const char* what){if(!value)throw std::runtime_error(what);}
template<class F>static void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,"malformed image accepted");}
static void put(std::vector<uint8_t>& p,size_t at,uint32_t value,size_t n=4){for(size_t i=0;i<n;++i){p[at+n-i-1]=uint8_t(value);value>>=8;}}
static uint32_t read(std::span<const uint8_t> p,size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v=(v<<8)|p[at+i];return v;}
static std::vector<uint8_t> rgba(){std::vector<uint8_t> bytes(16384);for(size_t i=0;i<bytes.size();++i)bytes[i]=uint8_t(i*37+11);return bytes;}
static std::vector<uint8_t> reply(uint32_t clan,uint32_t offset,bool empty=false){
    const auto data=rgba();const uint32_t count=empty?0:std::min(896u,16384-offset);
    std::vector<uint8_t> p(64+count);put(p,4,0x454d3634);put(p,8,clan);put(p,12,1,2);put(p,14,64,2);put(p,16,64,2);put(p,18,1,2);
    put(p,20,empty?0:16384);put(p,24,empty?0:offset);put(p,28,count,2);
    // Independent SHA-256 fixtures, not computed by the production decoder.
    std::string_view hash=empty?"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855":"ce61cc72a84bd3526a8f955235c5f4f67439820d6ffb910b5ab07e54741262b8";
    auto nibble=[](char c){return unsigned(c<='9'?c-'0':c-'a'+10);};
    for(size_t i=0;i<32;++i)p[32+i]=uint8_t(nibble(hash[2*i])*16+nibble(hash[2*i+1]));
    if(count)std::copy_n(data.begin()+offset,count,p.begin()+64);return p;
}
int main(){try{
    auto pixels=rgba();pixels[0]=255;pixels[1]=128;pixels[2]=64;pixels[3]=128;pixels[4]=200;pixels[5]=100;pixels[6]=50;pixels[7]=0;
    const size_t bottom=63*64*4;pixels[bottom]=11;pixels[bottom+1]=22;pixels[bottom+2]=33;pixels[bottom+3]=255;
    auto image=image_from_rgba(pixels);check(image.bgra[0]==0x80804020u&&image.bgra[1]==0,"RGBA becomes premultiplied BGRA including zero alpha");
    rejects([&]{image_from_rgba(std::span(pixels).first(16383));});
    Bitmap bitmap(image);DIBSECTION dib{};check(GetObjectW(bitmap.get(),sizeof(dib),&dib)==sizeof(dib),"DIB allocation");
    check(dib.dsBm.bmWidth==64&&dib.dsBm.bmHeight==64&&dib.dsBm.bmBitsPixel==32,"64x64 bitmap dimensions");
    check(!std::memcmp(dib.dsBm.bmBits,image.bgra.data(),sizeof(image.bgra)),"bitmap alpha retained");
    // GetObject normalizes biHeight positive on Windows; query logical image
    // coordinates to verify top-down orientation rather than its returned sign.
    HDC dc=CreateCompatibleDC(nullptr);check(bool(dc),"bitmap DC");auto old=SelectObject(dc,bitmap.get());
    const auto topPixel=GetPixel(dc,0,0),bottomPixel=GetPixel(dc,0,63);SelectObject(dc,old);DeleteDC(dc);
    check(topPixel==RGB(128,64,32)&&bottomPixel==RGB(11,22,33),"top-down logical pixel orientation");
    Download d;rejects([&]{d.requestPayload();});rejects([&]{d.begin(0);});rejects([&]{d.begin(0x80000000u);});d.begin(7);
    unsigned calls=0;
    for(uint32_t offset=0;offset<16384;offset+=896){
        auto request=d.requestPayload();check(request.size()==16&&read(request,0)==7&&read(request,4)==0x454d3634&&read(request,8)==offset&&request[12]==0&&request[13]==1&&request[14]==0&&request[15]==0,"incremental request contract");
        check(!d.image(),"partial image not published");bool complete=d.accept(reply(7,offset));++calls;
        check(complete==(offset+896>=16384),"completion requires last chunk");
    }
    check(calls==19&&d.done()&&d.image()==image_from_rgba(rgba()),"complete verified 64x64 image");rejects([&]{d.requestPayload();});rejects([&]{d.accept(reply(7,0));});
    for(size_t at: {size_t(0),size_t(4),size_t(8),size_t(12),size_t(14),size_t(16),size_t(18),size_t(20),size_t(24),size_t(28),size_t(30)}){
        d.begin(7);auto p=reply(7,0);p[at]^=1;rejects([&]{d.accept(p);});check(!d.done()&&!d.image()&&read(d.requestPayload(),8)==0,"bad envelope preserves no partial commit");
    }
    d.begin(7);auto p=reply(7,0);for(size_t n:{size_t(0),size_t(4),size_t(63),size_t(64),size_t(959)})rejects([&]{d.accept(std::span(p).first(n));});p.push_back(0);rejects([&]{d.accept(p);});
    d.begin(7);d.accept(reply(7,0));p=reply(7,896);p[32]^=1;rejects([&]{d.accept(p);});rejects([&]{d.accept(reply(7,0));});check(!d.image()&&!d.done(),"changed hash and duplicate offset cannot complete");
    d.begin(7);for(uint32_t offset=0;offset<16128;offset+=896)d.accept(reply(7,offset));p=reply(7,16128);p.back()^=1;rejects([&]{d.accept(p);});check(!d.image()&&!d.done(),"whole-image checksum rejects payload corruption");
    d.begin(7);check(d.accept(reply(7,0,true))&&d.done()&&!d.image(),"absent image finishes cleanly");
    d.begin(7);d.accept(reply(7,0));check(d.accept(reply(7,896,true))&&!d.image(),"server clear between chunks discards incomplete image");
    d.begin(7);p=reply(7,0,true);p[32]^=1;rejects([&]{d.accept(p);});
    d.begin(7);d.accept(reply(7,0));d.begin(8);rejects([&]{d.accept(reply(7,896));});check(d.clan()==8&&read(d.requestPayload(),8)==0,"changing clan cancels old download");d.cancel();rejects([&]{d.accept(reply(8,0));});
    Cache cache;check(cache.wanted()==0&&!cache.state().image,"empty cache");cache.want(7);auto s=cache.state();check(s.clan==7&&s.serial==1&&!s.image,"clan request invalidates old image");cache.put(7,image);check(cache.state().serial==2&&cache.state().image==image,"cache image ready");cache.want(7);check(cache.state().serial==2,"same clan stable");cache.want(8);cache.put(7,image);check(cache.state().serial==3&&!cache.state().image,"late other clan ignored");cache.put(8,std::nullopt);check(cache.state().serial==4&&!cache.state().image,"empty published result");cache.want(0);cache.put(0,image);check(cache.state().serial==5&&!cache.state().image,"leave clears image");
    std::cout<<"64x64 clan incremental transfer/checksum/identity/alpha/cache tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
