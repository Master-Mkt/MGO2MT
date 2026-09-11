#include "host_protocol.h"
#include "host_session.h"
#include <algorithm>
#include <fstream>
#include <iostream>
using namespace mgo2win::host;
void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
template<class F>void invalid(F f){bool failed=false;try{f();}catch(const Invalid&){failed=true;}check(failed,"malformed host data accepted");}
int main(int argc,char**argv){try{
 Hello hello{123,0x12345678,2,2,{{{192,0,2,1},5730},{{10,0,0,2},5730}}};auto b=encode_hello(hello);auto h=decode_hello(b,123);check(h.seed==hello.seed&&h.endpoints==hello.endpoints,"hello layout");invalid([&]{decode_hello(b,124);});b[8]^=1;invalid([&]{decode_hello(b,123);});
 for(unsigned len:{0u,1u,2u,3u,4u,254u,255u,256u,1900u})for(uint16_t seq:{uint16_t(0),uint16_t(32767),uint16_t(32768),uint16_t(65535)}){
  Message m;m.channel=1;m.serial=255;m.payload.resize(len);for(unsigned i=0;i<len;++i)m.payload[i]=uint8_t(i*17+9);Packet p{seq,{m}};Keys k{0x12431234,0xfedcab98};auto raw=encode(p,k);auto d=decode(raw,k,seq);check(d.sequence==seq&&d.messages.size()==1&&d.messages[0].payload==m.payload&&d.messages[0].serial==255,"cipher/message round trip");
  for(size_t at=0;at<raw.size();at+=std::max(size_t(1),raw.size()/17)){auto corrupt=raw;corrupt[at]^=1;invalid([&]{decode(corrupt,k,seq);});}
 }
 invalid([]{decode({});});invalid([]{decode_messages(std::array<uint8_t,3>{0,0x30,255});});invalid([]{inflate(std::array<uint8_t,1>{0x80});});invalid([]{inflate(std::array<uint8_t,2>{0,0x40});});
 // Literal A followed by an overlapping ring copy, then the zero terminator.
 auto bits=[](const char*s){std::vector<uint8_t>v;size_t n=0;for(;*s;++s){if(*s!='0'&&*s!='1')continue;if(n%8==0)v.push_back(0);v.back()|=(*s-'0')<<(7-n%8);++n;}return v;};
 check(inflate(bits("1 01000001 0 000000001 0010 0 000000000"))==std::vector<uint8_t>(5,'A'),"original overlap decompression");
 std::string overflow;for(unsigned i=0;i<2049;++i)overflow+="100000000";overflow+="0000000000";invalid([&]{inflate(bits(overflow.c_str()));});
 check(valid_endpoint({{192,0,2,1},5730})&&!valid_endpoint({{127,0,0,1},5730})&&!valid_endpoint({{224,0,0,1},5730})&&!valid_endpoint({{0,0,0,0},5730}),"endpoint boundaries");
 unsigned verified=0;if(argc>1){std::ifstream in(argv[1],std::ios::binary);auto read=[&](unsigned n){uint32_t v=0;for(unsigned i=0;i<n;++i){int c=in.get();if(c<0)throw std::runtime_error("fixture truncated");v|=uint32_t(c)<<(8*i);}return v;};auto count=read(4);check(count<=10000,"fixture bound");for(unsigned i=0;i<count;++i){auto len=read(2);Keys k{read(4),read(4)};auto seq=uint16_t(read(2));auto size=read(2);check(len<=2048&&size<=2048,"fixture extent");std::vector<uint8_t>raw(len),plain(size);in.read(reinterpret_cast<char*>(raw.data()),len);in.read(reinterpret_cast<char*>(plain.data()),size);check(bool(in),"fixture read");auto d=decode(raw,k,seq);check(d.sequence==seq&&encode_messages(d.messages)==plain,"retail packet oracle");for(auto&m:d.messages)if(m.channel==1&&!m.ack&&!m.payload.empty()&&m.payload[0]==11)global_generation(m.payload);++verified;}check(in.peek()==EOF,"fixture trailing");}
 std::cout<<"host protocol checks passed; private reference frames="<<verified<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
