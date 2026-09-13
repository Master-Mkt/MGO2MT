#include "clan_emblem.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace mgo2win::clan;
static void check(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
template<class F>static void rejects(F action,const char*message){try{action();}catch(const std::runtime_error&){return;}throw std::runtime_error(message);}
int main(){try{
 std::vector<uint8_t> bytes(565);check(!decode_blob(bytes),"all-zero server blob means no emblem");
 bytes[0]='E';bytes[1]='M';bytes[2]='B';bytes[3]='D';bytes[4]=0x80;
 for(unsigned i=0;i<16;++i){bytes[5+i*3]=uint8_t(10+i);bytes[6+i*3]=uint8_t(40+i);bytes[7+i*3]=uint8_t(90+i);}
 bytes[53]=0x12;bytes[54]=0xf0;bytes[53+15]=0xf1;bytes[53+16]=0x23;bytes[564]=0xe0;
 auto image=decode_blob(bytes);check(bool(image),"verified EMBD header accepted");
 check(image->bgra[0]==0xff0b295b&&image->bgra[1]==0xff0c2a5c,"high nibble first and RGB-to-BGRA channel order");
 check(image->bgra[2]==0xff193769&&image->bgra[3]==0,"palette index zero transparent despite nonzero RGB palette entry");
 check(image->bgra[31]==image->bgra[0]&&image->bgra[32]==image->bgra[1]&&image->bgra[33]==0xff0d2b5d,"linear order crosses rows without reversal");
 check(image->bgra[1022]==0xff183668&&image->bgra[1023]==0,"last packed pair remains in bounds");
 auto flags=bytes;flags[4]=0xff;check(decode_blob(flags)==image,"original high-bit test preserves ignored lower flag bits");
 for(size_t size=0;size<bytes.size();++size)rejects([&]{decode_blob(std::span(bytes).first(size));},"truncated emblem admitted");
 auto extra=bytes;extra.push_back(0);rejects([&]{decode_blob(extra);},"trailing blob data admitted");
 auto bad=bytes;bad[0]='P';rejects([&]{decode_blob(bad);},"unverified magic admitted");bad=bytes;bad[4]=0x7f;rejects([&]{decode_blob(bad);},"inactive flag admitted as valid image");
 std::vector<uint8_t> reply(4);reply.insert(reply.end(),bytes.begin(),bytes.end());check(decode_reply(reply)==image,"current server result prefix decoded");reply[0]=0xc0;reply[1]=0xff;reply[2]=0xee;reply[3]=1;rejects([&]{decode_reply(reply);},"error response displayed as image");
 rejects([&]{decode_reply(std::vector<uint8_t>{0,0,0,0});},"success without image extent admitted");
 Bitmap bitmap(*image);BITMAP actual{};check(GetObjectW(bitmap.get(),sizeof(actual),&actual)==sizeof(actual)&&actual.bmWidth==32&&actual.bmHeight==32&&actual.bmBitsPixel==32,"decoded emblem has native 32-bit bitmap");
 check(actual.bmBits&&std::memcmp(actual.bmBits,image->bgra.data(),4096)==0,"bitmap keeps BGRA and alpha exactly");
 Cache cache;check(!cache.wanted()&&!cache.state().image,"no default clan image");cache.want(11);check(cache.wanted()==11&&!cache.state().image,"new clan starts without an invented image");cache.put(11,image);auto first=cache.state();check(first.image==image,"matching request populates clan cache");
 cache.want(12);auto changed=cache.state();check(changed.clan==12&&changed.serial>first.serial&&!changed.image,"clan identity change clears old emblem");cache.put(11,image);check(cache.state().serial==changed.serial&&!cache.state().image,"late prior-clan response cannot overwrite cache");
 cache.want(12);check(cache.state().serial==changed.serial,"repeated roster identity does not reset cache");cache.put(12,std::nullopt);check(cache.state().serial>changed.serial&&!cache.state().image,"confirmed absent emblem clears cache");cache.want(0);cache.put(0,image);check(!cache.state().image,"clanless player never receives an emblem");
 std::cout<<"Original EMBD extent, palette, nibble order, transparency, reply and bitmap passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
