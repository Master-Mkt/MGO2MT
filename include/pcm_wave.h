#pragma once
#include <span>
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace mgo2win {
struct PcmWave {uint32_t rate=0,channels=0,dataAt=0,dataSize=0,loopBegin=0,loopEnd=0;};
// BGM interchange: standard 16-bit PCM RIFF/WAVE, optional single forward smpl loop.
// smpl's inclusive end is converted to an exclusive sample-frame boundary.
inline PcmWave read_pcm_wave(std::span<const unsigned char>b){
 auto require=[](bool ok){if(!ok)throw std::runtime_error("Invalid PCM WAV or loop metadata");};
 auto u=[&](size_t p){require(p<=b.size()&&b.size()-p>=4);return uint32_t(b[p])|(uint32_t(b[p+1])<<8)|(uint32_t(b[p+2])<<16)|(uint32_t(b[p+3])<<24);};
 auto eq=[&](size_t p,const char*s){return !std::memcmp(b.data()+p,s,4);};
 require(b.size()>=44&&b.size()<=256*1024*1024&&eq(0,"RIFF")&&eq(8,"WAVE")&&uint64_t(u(4))+8==b.size());
 PcmWave w;bool fmt=false,data=false,smpl=false;
 for(size_t p=12;p<b.size();){require(b.size()-p>=8);auto n=u(p+4);size_t a=p+8;require(n<=b.size()-a);
  if(eq(p,"fmt ")){require(!fmt&&(n==16||n==18));auto tag=u(a)&65535;w.channels=u(a)>>16;w.rate=u(a+4);auto block=u(a+12)&65535,bits=u(a+12)>>16;require(tag==1&&w.channels>=1&&w.channels<=8&&w.rate>=8000&&w.rate<=192000&&bits==16&&block==w.channels*2&&u(a+8)==w.rate*block);if(n==18)require(!b[a+16]&&!b[a+17]);fmt=true;}
  else if(eq(p,"data")){require(!data&&n>0);w.dataAt=uint32_t(a);w.dataSize=n;data=true;}
  else if(eq(p,"smpl")){require(!smpl&&n>=36);smpl=true;auto loops=u(a+28),extra=u(a+32);require(loops<=1&&uint64_t(36)+24*loops+extra==n);if(loops){require(u(a+40)==0&&u(a+52)==0&&u(a+56)==0);w.loopBegin=u(a+44);auto last=u(a+48);require(last!=UINT32_MAX);w.loopEnd=last+1;require(w.loopBegin<w.loopEnd);}}
  p=a+n+(n&1);require(p<=b.size());
 }
 require(fmt&&data&&w.dataSize%(w.channels*2)==0&&w.loopEnd<=w.dataSize/(w.channels*2));return w;
}
}
