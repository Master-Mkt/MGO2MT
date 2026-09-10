#include "character_model.h"
#include <fstream>
#include <iterator>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
static void check(bool ok){if(!ok)throw std::runtime_error("GWM test failure");}
static void put(std::vector<char>&b,size_t p,uint32_t n){for(int i=0;i<4;++i)b[p+i]=char(n>>(8*i));}
static void reject(const std::vector<char>&b){bool threw=false;try{CharacterModel m(b);}catch(const std::runtime_error&){threw=true;}check(threw);}
int main(int argc,char**argv){try{
 std::vector<char>b(196,0);std::memcpy(b.data(),"GWM1",4);put(b,4,1);put(b,8,3);put(b,12,3);put(b,16,1);put(b,20,1);put(b,36,0x3f800000);put(b,40,0x3f800000);
 float v[24]={0,0,0,0,0,1,0,0, 1,0,0,0,0,1,1,0, 0,1,0,0,0,1,0,1};std::memcpy(b.data()+48,v,sizeof(v));
 put(b,148,1);put(b,152,2);put(b,160,3);put(b,172,4);put(b,176,4);put(b,180,9);put(b,184,8);
 CharacterModel m(b);check(m.vertices.size()==3&&m.parts.size()==1&&m.textures[0].pixels.size()==8);
 for(size_t n=0;n<b.size();++n)reject(std::vector<char>(b.begin(),b.begin()+n));
 auto bad=[&](size_t p,uint32_t v){auto c=b;put(c,p,v);reject(c);};
 bad(0,0);bad(4,2);bad(8,0xffffffff);bad(12,4);bad(16,0);bad(20,257);bad(40,0);bad(48,0x7fc00000);bad(52,0x40000000);bad(68,0);bad(144,3);bad(156,3);bad(160,6);bad(164,1);bad(168,1);bad(172,0);bad(176,8193);bad(180,3);bad(184,9);
 auto trailing=b;trailing.push_back(0);reject(trailing);
 if(argc>1){std::ifstream f(argv[1],std::ios::binary);check(bool(f));std::vector<char>original((std::istreambuf_iterator<char>(f)),{});CharacterModel actual(original);check(actual.vertices.size()==3517&&actual.indices.size()==15312&&actual.parts.size()==23&&actual.textures.size()==7);std::cout<<"original geometry, bounds, 5104 triangles and 7 embedded textures validated\n";}
 std::cout<<"GWM finite geometry, truncation, count, bounds, indices, material and BC extents passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
