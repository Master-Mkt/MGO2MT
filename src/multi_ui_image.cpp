#include "multi_ui.h"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
namespace mgo2mt::multi_ui {
namespace {
void require(bool b,const char*s="Invalid or unsupported UI image"){if(!b)throw std::runtime_error(s);}
void hr(HRESULT v){require(SUCCEEDED(v),"Windows image decoder failed");}
uint32_t word(std::span<const uint8_t>b,size_t at){require(at+4<=b.size());return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void size(unsigned w,unsigned h){require(w&&h&&w<=4096&&h<=4096&&uint64_t(w)*h*4<=64*1024*1024,"UI image dimensions exceed budget");}
using Color=std::array<uint8_t,4>;
Color rgb565(unsigned c){unsigned r=(c>>11)&31,g=(c>>5)&63,b=c&31;return {uint8_t((r<<3)|(r>>2)),uint8_t((g<<2)|(g>>4)),uint8_t((b<<3)|(b>>2)),255};}
}
Image decode_dds(std::span<const uint8_t>b){
 require(b.size()>=128&&b.size()<=64*1024*1024+148&&word(b,0)==0x20534444&&word(b,4)==124&&word(b,76)==32);
 const auto flags=word(b,8),h=word(b,12),w=word(b,16),depth=word(b,24),mips=std::max(1u,word(b,28)),pf=word(b,80),fourcc=word(b,84);
 require((flags&0x1007)==0x1007&&depth<=1&&(word(b,112)&0x200200)==0&&word(b,112)==0&&mips<=13);size(w,h);
 unsigned maximum=1;for(unsigned d=std::max(w,h);d>1;d>>=1)++maximum;require(mips<=maximum);
 enum Codec{rgba,bgra,bc1,bc2,bc3};Codec codec;size_t offset=128;
 if(pf&4){if(fourcc==0x31545844)codec=bc1;else if(fourcc==0x33545844)codec=bc2;else if(fourcc==0x35545844)codec=bc3;else if(fourcc==0x30315844){
   require(b.size()>=148&&word(b,132)==3&&word(b,136)==0&&word(b,140)==1&&(word(b,144)==0||word(b,144)==1));offset=148;
   switch(word(b,128)){case 28:case 29:codec=rgba;break;case 87:case 91:codec=bgra;break;case 71:case 72:codec=bc1;break;case 74:case 75:codec=bc2;break;case 77:case 78:codec=bc3;break;default:require(false);}
  }else require(false);
 }else{require((pf&0x41)==0x41&&word(b,88)==32&&word(b,96)==0xff00&&word(b,104)==0xff000000);if(word(b,92)==0xff&&word(b,100)==0xff0000)codec=rgba;else if(word(b,92)==0xff0000&&word(b,100)==0xff)codec=bgra;else require(false);}
 auto bytes=[&](unsigned x,unsigned y){return codec<=bgra?uint64_t(x)*y*4:uint64_t((x+3)/4)*((y+3)/4)*(codec==bc1?8:16);};
 uint64_t extent=offset;for(unsigned i=0,x=w,y=h;i<mips;++i,x=std::max(1u,x/2),y=std::max(1u,y/2))extent+=bytes(x,y);require(extent==b.size(),"DDS mip payload extent mismatch");
 Image result{w,h,std::vector<uint8_t>(size_t(w)*h*4)};
 if(codec<=bgra){std::copy_n(b.data()+offset,result.rgba.size(),result.rgba.data());if(codec==bgra)for(size_t i=0;i<result.rgba.size();i+=4)std::swap(result.rgba[i],result.rgba[i+2]);return result;}
 for(unsigned by=0;by<(h+3)/4;++by)for(unsigned bx=0;bx<(w+3)/4;++bx){const auto*block=b.data()+offset;offset+=codec==bc1?8:16;const auto*c=block+(codec==bc1?0:8);unsigned c0=c[0]|unsigned(c[1])<<8,c1=c[2]|unsigned(c[3])<<8;std::array<Color,4>palette{rgb565(c0),rgb565(c1),Color{},Color{}};
  if(c0>c1||codec!=bc1){for(int k=0;k<3;++k){palette[2][k]=uint8_t((2*palette[0][k]+palette[1][k])/3);palette[3][k]=uint8_t((palette[0][k]+2*palette[1][k])/3);}palette[2][3]=palette[3][3]=255;}
  else{for(int k=0;k<3;++k)palette[2][k]=uint8_t((palette[0][k]+palette[1][k])/2);palette[2][3]=255;}
  uint32_t bits=uint32_t(c[4])|uint32_t(c[5])<<8|uint32_t(c[6])<<16|uint32_t(c[7])<<24;uint64_t alphaBits=0;std::array<unsigned,8>alpha{};
  if(codec==bc3){alpha[0]=block[0];alpha[1]=block[1];if(alpha[0]>alpha[1])for(unsigned i=1;i<=6;++i)alpha[i+1]=((7-i)*alpha[0]+i*alpha[1])/7;else{for(unsigned i=1;i<=4;++i)alpha[i+1]=((5-i)*alpha[0]+i*alpha[1])/5;alpha[6]=0;alpha[7]=255;}for(unsigned i=0;i<6;++i)alphaBits|=uint64_t(block[i+2])<<(i*8);}
  for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x){unsigned p=y*4+x;auto color=palette[(bits>>(2*p))&3];if(codec==bc2)color[3]=uint8_t(((block[p/2]>>(4*(p&1)))&15)*17);if(codec==bc3)color[3]=uint8_t(alpha[(alphaBits>>(3*p))&7]);if(bx*4+x<w&&by*4+y<h)std::copy(color.begin(),color.end(),result.rgba.data()+(size_t(by*4+y)*w+bx*4+x)*4);}
 }return result;
}
Image decode_image(const std::filesystem::path&p){
 require(std::filesystem::is_regular_file(p)&&std::filesystem::file_size(p)<=64*1024*1024+148,"Missing or oversized UI image");
 std::ifstream in(p,std::ios::binary|std::ios::ate);const auto count=in.tellg();require(count>=4);std::vector<uint8_t>bytes(static_cast<size_t>(count));in.seekg(0);require(bool(in.read(reinterpret_cast<char*>(bytes.data()),count)));
 if(word(bytes,0)==0x20534444)return decode_dds(bytes);
 // WIC decodes only the reviewed raster containers to straight RGBA.
 // GIF animation, TIFF pages and ICO resolutions deliberately use frame zero.
 auto init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);struct Co{bool release;~Co(){if(release)CoUninitialize();}}co{SUCCEEDED(init)};require(SUCCEEDED(init)||init==RPC_E_CHANGED_MODE);
 using Microsoft::WRL::ComPtr;ComPtr<IWICImagingFactory>factory;hr(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));ComPtr<IWICStream>stream;hr(factory->CreateStream(&stream));hr(stream->InitializeFromMemory(bytes.data(),DWORD(bytes.size())));ComPtr<IWICBitmapDecoder>decoder;
 require(SUCCEEDED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder)),"Unsupported or invalid UI raster image (Windows codec unavailable or malformed data)");
 GUID container{};hr(decoder->GetContainerFormat(&container));require(container==GUID_ContainerFormatPng||container==GUID_ContainerFormatJpeg||container==GUID_ContainerFormatBmp||container==GUID_ContainerFormatGif||container==GUID_ContainerFormatTiff||container==GUID_ContainerFormatIco,"Only DDS, PNG, JPEG, BMP, GIF, TIFF, ICO are supported");
 UINT frames=0;hr(decoder->GetFrameCount(&frames));require(frames>0&&frames<=4096,"UI image frame count exceeds budget");ComPtr<IWICBitmapFrameDecode>frame;hr(decoder->GetFrame(0,&frame));UINT w=0,h=0;hr(frame->GetSize(&w,&h));size(w,h);ComPtr<IWICFormatConverter>converter;hr(factory->CreateFormatConverter(&converter));hr(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));Image result{w,h,std::vector<uint8_t>(size_t(w)*h*4)};hr(converter->CopyPixels(nullptr,w*4,UINT(result.rgba.size()),result.rgba.data()));return result;
}
std::vector<uint8_t> encode_dds(const Image&i){size(i.width,i.height);require(i.rgba.size()==size_t(i.width)*i.height*4);std::vector<uint8_t>b(128);auto put=[&](size_t at,uint32_t n){for(unsigned k=0;k<4;++k)b[at+k]=uint8_t(n>>(8*k));};put(0,0x20534444);put(4,124);put(8,0x100f);put(12,i.height);put(16,i.width);put(20,i.width*4);put(76,32);put(80,0x41);put(88,32);put(92,0xff);put(96,0xff00);put(100,0xff0000);put(104,0xff000000);put(108,0x1000);b.insert(b.end(),i.rgba.begin(),i.rgba.end());return b;}
void export_dds(const std::filesystem::path&source,const std::filesystem::path&destination){auto parent=destination.parent_path().empty()?std::filesystem::current_path():destination.parent_path();auto target=std::filesystem::exists(destination)?std::filesystem::canonical(destination):std::filesystem::canonical(parent)/destination.filename();require(std::filesystem::canonical(source)!=target,"DDS export would replace its input");auto b=encode_dds(decode_image(source));std::ofstream out(destination,std::ios::binary|std::ios::trunc);require(bool(out)&&bool(out.write(reinterpret_cast<const char*>(b.data()),b.size())),"DDS export write failed");}
}
