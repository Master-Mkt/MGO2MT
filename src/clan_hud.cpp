#include "clan_hud.h"
#include <bcrypt.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace mgo2win::clan {
namespace {
constexpr uint32_t magic=0x454d3634,extent=64*64*4,chunk=896;
uint32_t read(std::span<const uint8_t> p,size_t at,size_t length=4) {
    if(at>p.size()||length>p.size()-at)throw std::runtime_error("Short clan image reply");
    uint32_t value=0;for(size_t i=0;i<length;++i)value=(value<<8)|p[at+i];return value;
}
void put(std::span<uint8_t> p,size_t at,uint32_t value) {
    for(int i=3;i>=0;--i){p[at+i]=uint8_t(value);value>>=8;}
}
std::array<uint8_t,32> digest(std::span<const uint8_t> bytes) {
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    auto check=[](NTSTATUS status){if(status<0)throw std::runtime_error("Clan image checksum provider");};
    std::array<uint8_t,32> result{};
    try {
        check(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0));
        check(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0));
        if(!bytes.empty())check(BCryptHashData(hash,const_cast<PUCHAR>(bytes.data()),ULONG(bytes.size()),0));
        check(BCryptFinishHash(hash,result.data(),ULONG(result.size()),0));
    }catch(...){if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);throw;}
    BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);return result;
}
}
Image image_from_rgba(std::span<const uint8_t> bytes) {
    if(bytes.size()!=extent)throw std::runtime_error("Invalid clan image dimensions");
    Image image;
    for(size_t i=0;i<image.bgra.size();++i) {
        const uint32_t alpha=bytes[4*i+3];
        auto channel=[&](size_t at){return (uint32_t(bytes[4*i+at])*alpha+127)/255;};
        image.bgra[i]=(alpha<<24)|(channel(0)<<16)|(channel(1)<<8)|channel(2);
    }
    return image;
}
void Download::begin(uint32_t clan) {
    cancel();if(!clan||clan>0x7fffffffu)throw std::invalid_argument("Invalid clan image identity");
    clan_=clan;bytes_.reserve(extent);
}
void Download::cancel(){clan_=0;done_=false;digest_={};bytes_.clear();image_.reset();}
std::vector<uint8_t> Download::requestPayload()const {
    if(!clan_||done_||bytes_.size()>=extent)throw std::logic_error("No pending clan image request");
    std::vector<uint8_t> request(16);put(request,0,clan_);put(request,4,magic);put(request,8,uint32_t(bytes_.size()));request[13]=1;return request;
}
bool Download::accept(std::span<const uint8_t> p) {
    if(!clan_||done_)throw std::logic_error("Unexpected clan image reply");
    if(p.size()<64||p.size()>64+chunk||read(p,0)!=0||read(p,4)!=magic||read(p,8)!=clan_
       ||read(p,12,2)!=1||read(p,14,2)!=64||read(p,16,2)!=64||read(p,18,2)!=1||read(p,30,2)!=0)
        throw std::runtime_error("Invalid clan image reply");
    const uint32_t total=read(p,20),offset=read(p,24),count=read(p,28,2);
    std::array<uint8_t,32> hash{};std::copy_n(p.begin()+32,32,hash.begin());
    if(total==0) {
        // The server may clear an image between chunks; it answers offset zero.
        if(offset||count||p.size()!=64||hash!=digest({}))throw std::runtime_error("Invalid empty clan image reply");
        bytes_.clear();image_.reset();done_=true;return true;
    }
    if(total!=extent||offset!=bytes_.size()||count!=std::min(chunk,extent-offset)||p.size()!=64+count)
        throw std::runtime_error("Invalid clan image chunk");
    if(!bytes_.empty()&&hash!=digest_)throw std::runtime_error("Clan image changed during download");
    auto next=bytes_;next.insert(next.end(),p.begin()+64,p.end());
    if(next.size()==extent) {
        if(digest(next)!=hash)throw std::runtime_error("Clan image checksum mismatch");
        auto image=image_from_rgba(next);image_=std::move(image);done_=true;
    }
    if(bytes_.empty())digest_=hash;
    bytes_=std::move(next);return done_;
}
void Cache::want(uint32_t clan) {
    std::lock_guard lock(mutex_);if(clan==state_.clan)return;
    state_.clan=clan;state_.image.reset();++state_.serial;
}
uint32_t Cache::wanted()const{std::lock_guard lock(mutex_);return state_.clan;}
void Cache::put(uint32_t clan,std::optional<Image> image) {
    std::lock_guard lock(mutex_);if(!clan||clan!=state_.clan)return;
    state_.image=std::move(image);++state_.serial;
}
State Cache::state()const{std::lock_guard lock(mutex_);return state_;}
Bitmap::Bitmap(const Image& image) {
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=Image::width;info.bmiHeader.biHeight=-LONG(Image::height);
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* data=nullptr;bitmap_=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&data,nullptr,0);
    if(!bitmap_||!data){if(bitmap_)DeleteObject(bitmap_);bitmap_=nullptr;throw std::runtime_error("Cannot create clan image bitmap");}
    std::memcpy(data,image.bgra.data(),sizeof(image.bgra));
}
Bitmap::~Bitmap(){if(bitmap_)DeleteObject(bitmap_);}
}
