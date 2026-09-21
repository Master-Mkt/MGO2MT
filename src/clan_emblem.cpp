#include "clan_emblem.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace mgo2mt::clan {
std::optional<Image> decode_blob(std::span<const uint8_t> bytes){
 if(bytes.size()!=565)throw std::runtime_error("Clan emblem extent");
 if(std::all_of(bytes.begin(),bytes.end(),[](uint8_t value){return value==0;}))return {};
 // Current MGO2 C610E8: EMBD compare; signed byte at +4 must be negative;
 // RGB palette at +5, packed high/low nibbles at +53; palette[0] forced zero.
 if(bytes[0]!='E'||bytes[1]!='M'||bytes[2]!='B'||bytes[3]!='D'||!(bytes[4]&0x80))throw std::runtime_error("Unsupported clan emblem format");
 std::array<uint32_t,16> palette{};
 for(size_t i=1;i<palette.size();++i){size_t at=5+i*3;palette[i]=0xff000000u|(uint32_t(bytes[at])<<16)|(uint32_t(bytes[at+1])<<8)|bytes[at+2];}
 Image result;for(size_t i=0;i<512;++i){auto packed=bytes[53+i];result.bgra[i*2]=palette[packed>>4];result.bgra[i*2+1]=palette[packed&15];}return result;
}
std::optional<Image> decode_reply(std::span<const uint8_t> bytes){
 if(bytes.size()<4)throw std::runtime_error("Clan emblem reply extent");
 uint32_t result=(uint32_t(bytes[0])<<24)|(uint32_t(bytes[1])<<16)|(uint32_t(bytes[2])<<8)|bytes[3];
 if(result)throw std::runtime_error("Clan emblem request rejected");
 return decode_blob(bytes.subspan(4));
}
void Cache::want(uint32_t clan){std::lock_guard lock(mutex_);if(state_.clan==clan)return;state_.clan=clan;state_.image.reset();++state_.serial;}
uint32_t Cache::wanted()const{std::lock_guard lock(mutex_);return state_.clan;}
void Cache::put(uint32_t clan,std::optional<Image> image){std::lock_guard lock(mutex_);if(!clan||clan!=state_.clan)return;state_.image=std::move(image);++state_.serial;}
State Cache::state()const{std::lock_guard lock(mutex_);return state_;}
Bitmap::Bitmap(const Image&image){
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=Image::width;info.bmiHeader.biHeight=-int(Image::height);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
 void*pixels=nullptr;value_=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
 if(!value_||!pixels){if(value_)DeleteObject(value_);value_=nullptr;throw std::runtime_error("Clan emblem bitmap allocation");}
 std::memcpy(pixels,image.bgra.data(),image.bgra.size()*sizeof(uint32_t));
}
Bitmap::~Bitmap(){if(value_)DeleteObject(value_);}
}
