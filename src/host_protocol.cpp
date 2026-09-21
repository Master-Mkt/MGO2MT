#include <windows.h>
#include <bcrypt.h>
#include "host_protocol.h"
#include <algorithm>
namespace mgo2mt::host {
namespace {
constexpr uint32_t lcg=1566083941;
uint32_t read(std::span<const uint8_t>b,size_t at,unsigned n){if(at>b.size()||n>b.size()-at)throw Invalid(Error::extent);uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(b[at+i])<<(8*i);return v;}
void append(std::vector<uint8_t>&b,uint32_t v,unsigned n){while(n--){b.push_back(uint8_t(v));v>>=8;}}
std::array<uint8_t,16> md5(std::span<const uint8_t>a,std::span<const uint8_t>b){
 BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;std::array<uint8_t,16> out{};
 auto check=[](NTSTATUS s){if(s<0)throw std::runtime_error("host checksum provider");};
 try{check(BCryptOpenAlgorithmProvider(&alg,BCRYPT_MD5_ALGORITHM,nullptr,0));check(BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0));for(auto data:{a,b})if(!data.empty())check(BCryptHashData(hash,const_cast<PUCHAR>(data.data()),ULONG(data.size()),0));check(BCryptFinishHash(hash,out.data(),ULONG(out.size()),0));}
 catch(...){if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);throw;}
 BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);return out;
}
std::array<uint8_t,10> checksum(std::span<const uint8_t>b,uint32_t key){std::array<uint8_t,4> k{uint8_t(key),uint8_t(key>>8),uint8_t(key>>16),uint8_t(key>>24)};auto a=md5(k,b),d=md5(k,a);std::array<uint8_t,10> out;std::copy_n(d.begin(),10,out.begin());for(unsigned i=0;i<6;++i)out[i]^=d[i+10];return out;}
void crypt(std::span<uint8_t>b,uint16_t sequence,uint32_t key,bool encrypt){uint32_t rolling=(lcg*(sequence&0x7fff)+1)^key;for(size_t i=0;i<b.size();i+=4){unsigned n=unsigned(std::min(size_t(4),b.size()-i));auto word=read(b,i,n);auto value=word^rolling;auto plain=encrypt?word:value;if(n<4)plain&=(uint32_t(1)<<(n*8))-1;for(unsigned j=0;j<n;++j)b[i+j]=uint8_t(value>>(8*j));rolling+=plain;}}
void scramble(std::vector<uint8_t>&b,bool encode){auto n=uint32_t(b.size());auto q=lcg*n+1;auto ix=(q>>16)%(n-2)+2;q=lcg*q+1;auto iy=(q>>16)%(n-2)+2;q=lcg*q+1;auto sx=(q>>16)%n;q=lcg*q+1;auto sy=(q>>16)%n;
 if(encode){b[0]^=b[ix];b[1]^=b[iy];std::swap(b[0],b[sx]);std::swap(b[1],b[sy]);}else{std::swap(b[1],b[sy]);std::swap(b[0],b[sx]);b[1]^=b[iy];b[0]^=b[ix];}}
}
bool valid_endpoint(const Endpoint&e,bool loopback){auto a=e.address;return e.port&&a[0]&&a[0]<224&&(a[0]!=127||loopback)&&!(a[0]==169&&a[1]==254)&&a!=std::array<uint8_t,4>{255,255,255,255};}
std::vector<uint8_t> encode_hello(const Hello&h){if(!h.character||h.endpoints.empty()||h.endpoints.size()>2||(h.flags&~7))throw Invalid(Error::identity);std::vector<uint8_t>b;append(b,h.character,4);append(b,h.seed,4);append(b,version,4);append(b,h.flags,1);append(b,h.extra,2);append(b,uint32_t(h.endpoints.size()),1);for(auto&e:h.endpoints){if(!valid_endpoint(e))throw Invalid(Error::identity);b.insert(b.end(),e.address.begin(),e.address.end());append(b,e.port,2);}return b;}
Hello decode_hello(std::span<const uint8_t>b,uint32_t expected){if(b.size()<16||read(b,0,4)!=expected||!expected)throw Invalid(Error::identity);if(read(b,8,4)!=version)throw Invalid(Error::version_mismatch);Hello h;h.character=expected;h.seed=read(b,4,4);h.flags=b[12];h.extra=uint16_t(read(b,13,2));if(h.flags&~7||!b[15]||b[15]>2||b.size()!=16+size_t(b[15])*6)throw Invalid(Error::extent);for(size_t at=16;at<b.size();at+=6){Endpoint e;std::copy_n(b.begin()+at,4,e.address.begin());e.port=uint16_t(read(b,at+4,2));if(!valid_endpoint(e))throw Invalid(Error::identity);h.endpoints.push_back(e);}return h;}
std::vector<uint8_t> encode_messages(std::span<const Message>ms){std::vector<uint8_t>b;if(ms.empty()||ms.size()>128)throw Invalid(Error::message);for(auto&m:ms){if(m.channel>4095||m.payload.size()>max_datagram||(m.ack&&(!m.reliable||m.payload.size()>(m.timing?1u:0u))))throw Invalid(Error::message);auto tag=uint16_t(m.channel|(m.reliable?0x1000:0)|(m.ack?0x4000:0)|(m.timing?0x8000:0)|(m.payload.size()>255?0x2000:0));append(b,tag,2);append(b,uint32_t(m.payload.size()),m.payload.size()>255?2:1);if(m.reliable)b.push_back(m.serial);b.insert(b.end(),m.payload.begin(),m.payload.end());if(b.size()>max_datagram-12)throw Invalid(Error::extent);}return b;}
std::vector<Message> decode_messages(std::span<const uint8_t>b){std::vector<Message>out;size_t at=0;while(at<b.size()){if(out.size()>=128)throw Invalid(Error::message);auto tag=read(b,at,2);at+=2;unsigned lenBytes=tag&0x2000?2:1;auto n=read(b,at,lenBytes);at+=lenBytes;Message m;m.channel=uint16_t(tag&0xfff);m.reliable=(tag&0x1000)!=0;m.ack=(tag&0x4000)!=0;m.timing=(tag&0x8000)!=0;if(m.reliable)m.serial=uint8_t(read(b,at++,1));if(n>b.size()-at||(m.ack&&(!m.reliable||n>(m.timing?1u:0u))))throw Invalid(Error::message);m.payload.assign(b.begin()+at,b.begin()+at+n);at+=n;out.push_back(std::move(m));}if(out.empty())throw Invalid(Error::message);return out;}
std::vector<uint8_t> inflate(std::span<const uint8_t>b){size_t bit=0,pos=1;std::array<uint8_t,512> ring{};std::array<bool,512>written{};std::vector<uint8_t>out;
 auto get=[&](unsigned n){if(bit+n>b.size()*8)throw Invalid(Error::compression);unsigned v=0;while(n--){v=(v<<1)|((b[bit/8]>>(7-bit%8))&1);++bit;}return v;};
 for(;;){bool literal=get(1)!=0;unsigned index=0,count=1,value=0;if(literal)value=get(8);else{index=get(9);if(!index)return out;count=get(4)+2;}while(count--){if(out.size()>=max_datagram)throw Invalid(Error::compression);if(!literal){if(!written[index])throw Invalid(Error::compression);value=ring[index];index=(index+1)&511;}out.push_back(uint8_t(value));ring[pos]=uint8_t(value);written[pos]=true;pos=(pos+1)&511;}}
}
std::vector<uint8_t> encode(const Packet&p,Keys k){auto data=encode_messages(p.messages);std::vector<uint8_t>b;append(b,p.sequence,2);crypt(data,p.sequence,k.cipher,true);b.insert(b.end(),data.begin(),data.end());auto mac=checksum(b,k.mac);b.insert(b.end(),mac.begin(),mac.end());scramble(b,true);return b;}
Packet decode(std::span<const uint8_t>wire,Keys k,uint16_t expected){if(wire.size()<15||wire.size()>max_datagram)throw Invalid(Error::extent);std::vector<uint8_t>b(wire.begin(),wire.end());scramble(b,false);auto mac=checksum(std::span(b).first(b.size()-10),k.mac);unsigned diff=0;for(unsigned i=0;i<10;++i)diff|=mac[i]^b[b.size()-10+i];if(diff)throw Invalid(Error::integrity);uint16_t raw=uint16_t(read(b,0,2));
 // Reconstruct the nearest sequence modulo 32768, then interpret the toggle.
 uint16_t seq=uint16_t((expected&0x8000)|(raw&0x7fff));int delta=int(int16_t(seq-expected));if(delta>16383||delta< -16384)seq^=0x8000;
 bool compressed=((raw^seq)&0x8000)!=0;auto body=std::span(b).subspan(2,b.size()-12);crypt(body,raw,k.cipher,false);auto plain=compressed?inflate(body):std::vector<uint8_t>(body.begin(),body.end());return {seq,decode_messages(plain)};
}
}
