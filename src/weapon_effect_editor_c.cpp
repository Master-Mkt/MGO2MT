#define MGO2WPN_EXPORTS
#include "weapon_effect_editor_c.h"
#include "gameplay_config.h"
#include "weapon_effect_config.h"
#include "multi_ui.h"
#include "pcm_wave.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
namespace {
thread_local std::string lastError;
thread_local std::string sampleSource;
thread_local mgo2mt::weapon_effect::Config sampleConfig;
void message(char* out,int capacity,const std::string& text){if(out&&capacity>0){auto n=(std::min)(size_t(capacity-1),text.size());std::memcpy(out,text.data(),n);out[n]=0;}}
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
std::vector<uint8_t> read(const std::filesystem::path& path,size_t maximum){std::ifstream in(path,std::ios::binary|std::ios::ate);require(bool(in),"Cannot open file");auto n=in.tellg();require(n>0&&uint64_t(n)<=maximum,"File size is outside the supported range");std::vector<uint8_t>b(static_cast<size_t>(n));in.seekg(0);require(bool(in.read(reinterpret_cast<char*>(b.data()),n)),"Cannot read file");return b;}
struct Image{uint32_t width,height,codec;std::vector<uint8_t> bytes;};
std::map<uint32_t,Image> bundle(const std::filesystem::path& path){
 auto b=read(path,16*1024*1024);size_t at=4;
 auto word=[&](){require(at<=b.size()&&b.size()-at>=4,"Truncated GWFX image");uint32_t v;std::memcpy(&v,b.data()+at,4);at+=4;return v;};
 require(b.size()>=12&&!std::memcmp(b.data(),"GWFX",4)&&word()==1,"Unsupported GWFX format");auto count=word();require(count>0&&count<=128,"Invalid GWFX image count");std::map<uint32_t,Image> out;
 for(uint32_t i=0;i<count;++i){auto key=word(),w=word(),h=word(),codec=word(),size=word();require(key&&!out.contains(key)&&w&&h&&w<=2048&&h<=2048&&w%4==0&&h%4==0&&(codec==9||codec==11)&&size==w/4*(h/4)*(codec==9?8:16)&&size<=b.size()-at,"Invalid GWFX image");out.emplace(key,Image{w,h,codec,{b.begin()+at,b.begin()+at+size}});at+=size;}require(at==b.size(),"Trailing GWFX data");return out;
}
mgo2mt::multi_ui::Image decode(const wchar_t* path,uint32_t key){require(path&&*path,"Missing image path");if(!key)return mgo2mt::multi_ui::decode_image(path);auto images=bundle(path);auto it=images.find(key);require(it!=images.end(),"Texture key is not present in this bundle");const auto&i=it->second;std::vector<uint8_t>dds(128);auto put=[&](size_t at,uint32_t n){std::memcpy(dds.data()+at,&n,4);};put(0,0x20534444);put(4,124);put(8,0x81007);put(12,i.height);put(16,i.width);put(20,uint32_t(i.bytes.size()));put(76,32);put(80,4);put(84,i.codec==9?0x31545844:0x35545844);put(108,0x1000);dds.insert(dds.end(),i.bytes.begin(),i.bytes.end());return mgo2mt::multi_ui::decode_dds(dds);}
}
int mw_validate(const wchar_t* gameplayPath,const wchar_t* effectsPath,char* error,int capacity){
 try{require(gameplayPath&&*gameplayPath,"Missing gameplay JSON path");mgo2mt::gameplay::Config game;std::string e;if(!game.load(gameplayPath,e))throw std::runtime_error(e);if(effectsPath&&*effectsPath){mgo2mt::weapon_effect::Config effects;if(!effects.load(effectsPath,e))throw std::runtime_error(e);for(const auto&[id,definition]:effects.weapons()){(void)definition;require(game.find(id)!=nullptr,"Effects refer to an unknown gameplay weapon ID");}}lastError.clear();message(error,capacity,{});return 1;}catch(const std::exception&e){lastError=e.what();message(error,capacity,lastError);return 0;}catch(...){lastError="Unexpected validation failure";message(error,capacity,lastError);return 0;}
}
int mw_image(const wchar_t* file,uint32_t key,uint8_t* rgba,uint32_t* width,uint32_t* height,int capacity){
 try{require(width&&height&&capacity>=0,"Missing image dimensions");auto result=decode(file,key);*width=result.width;*height=result.height;lastError.clear();if(!rgba)return 2;require(size_t(capacity)>=result.rgba.size(),"Image buffer too small");std::memcpy(rgba,result.rgba.data(),result.rgba.size());return 1;}catch(const std::exception&e){lastError=e.what();if(width)*width=0;if(height)*height=0;return 0;}catch(...){lastError="Unexpected image failure";return 0;}
}
int mw_texture_keys(const wchar_t* file,uint32_t* keys,int capacity){try{require(file&&*file&&capacity>=0,"Missing bundle path");auto images=bundle(file);if(keys){require(size_t(capacity)>=images.size(),"Texture key buffer too small");size_t n=0;for(const auto&[key,image]:images){(void)image;keys[n++]=key;}}lastError.clear();return int(images.size());}catch(const std::exception&e){lastError=e.what();return -1;}catch(...){lastError="Unexpected bundle failure";return -1;}}
int mw_wave(const wchar_t* file,char* error,int capacity){try{require(file&&*file,"Missing WAV path");auto b=read(file,16*1024*1024);auto w=mgo2mt::read_pcm_wave(b);require(w.channels<=2&&!w.loopEnd&&double(w.dataSize)/(w.rate*w.channels*2)<=30,"Effect WAV must be mono/stereo 16-bit PCM, at most 30 seconds, without a loop");lastError.clear();message(error,capacity,{});return 1;}catch(const std::exception&e){lastError=e.what();message(error,capacity,lastError);return 0;}catch(...){lastError="Unexpected WAV failure";message(error,capacity,lastError);return 0;}}
int mw_last_error(char* error,int capacity){message(error,capacity,lastError);return int(lastError.size());}
int mw_sample(const char* json,int length,uint16_t weapon,const char* channel,uint64_t ageMs,uint64_t seed,MWParticle* particles,int capacity){
 try{static_assert(sizeof(MWParticle)==68);require(json&&length>0&&length<=1024*1024&&weapon<=511&&channel&&capacity>=0,"Invalid sample request");
  std::string_view incoming(json,size_t(length));if(sampleSource!=incoming){std::string error;if(!sampleConfig.load_text(incoming,error))throw std::runtime_error(error);sampleSource=incoming;}
  auto emitters=sampleConfig.particles(weapon,channel);std::vector<mgo2mt::weapon_effect::ParticleSample> samples;if(emitters)samples=mgo2mt::weapon_effect::sample(*emitters,{0,0,0},{0,0,1},seed,ageMs);
  if(particles){require(size_t(capacity)>=samples.size(),"Sample buffer too small");size_t n=0;for(const auto&s:samples){auto&p=particles[n++];std::copy(s.position.begin(),s.position.end(),p.position);p.radius=s.radius;p.rotation=s.rotation;std::copy(s.stretch.begin(),s.stretch.end(),p.stretch);std::copy(s.rgba.begin(),s.rgba.end(),p.rgba);std::copy(s.uv.begin(),s.uv.end(),p.uv);p.texture=s.texture;p.additive=s.additive?1:0;}}
  lastError.clear();return int(samples.size());
 }catch(const std::exception&e){lastError=e.what();return -1;}catch(...){lastError="Unexpected sample failure";return -1;}
}
int mw_texture_path(uint32_t key,char* utf8,int capacity){for(const auto&[path,texture]:sampleConfig.textures())if(key==texture){message(utf8,capacity,path);return int(path.size());}message(utf8,capacity,{});return 0;}
