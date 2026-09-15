#include "weapon_icons.h"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <charconv>
#include <stdexcept>
namespace mgo2win::weapons {
std::map<uint16_t,std::string> read_icon_index(const std::filesystem::path& path){
 std::ifstream f(path,std::ios::binary|std::ios::ate);
 if(!f||f.tellg()<1||f.tellg()>16384)throw std::runtime_error("Weapon icon index extent");
 f.seekg(0);std::string line;auto get=[&]{if(!std::getline(f,line))return false;if(!line.empty()&&line.back()=='\r')line.pop_back();return true;};
 if(!get()||line!="MGO2WIN_WEAPON_ICONS\t1")throw std::runtime_error("Weapon icon index version");
 std::map<uint16_t,std::string> result;
 while(get()){
  auto tab=line.find('\t',5);unsigned id=0;
  if(!line.starts_with("ICON\t")||tab==line.npos)throw std::runtime_error("Weapon icon row");
  auto number=std::string_view(line).substr(5,tab-5);auto [end,ec]=std::from_chars(number.data(),number.data()+number.size(),id);
  auto name=line.substr(tab+1);
  if(ec!=std::errc{}||end!=number.data()+number.size()||id>255||name.size()<5||name.size()>80||!name.ends_with(".png")||name.find("..")!=name.npos||name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-. ")!=name.npos||name.find(' ')!=name.npos)
   throw std::runtime_error("Weapon icon ID or filename");
  if(!result.emplace(uint16_t(id),name).second||result.size()>128)throw std::runtime_error("Weapon icon duplicate or count");
 }
 if(!f.eof()||result.empty())throw std::runtime_error("Weapon icon index incomplete");return result;
}
bool Icons::load(const std::filesystem::path& path,std::string& error){
 try{
  auto index=read_icon_index(path);
  struct Com {HRESULT status=CoInitializeEx(nullptr,COINIT_MULTITHREADED);~Com(){if(SUCCEEDED(status))CoUninitialize();}} com;
  if(FAILED(com.status)&&com.status!=RPC_E_CHANGED_MODE)throw std::runtime_error("Weapon icon COM initialization");
  using Microsoft::WRL::ComPtr;ComPtr<IWICImagingFactory> factory;
  auto check=[](HRESULT h){if(FAILED(h))throw std::runtime_error("Weapon icon PNG decode");};
  check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
  std::map<uint16_t,Icon> draft;size_t total=0;
  for(const auto&[id,name]:index){
   auto source=path.parent_path()/name;
   if(std::filesystem::file_size(source)>4*1024*1024)throw std::runtime_error("Weapon icon file extent");
   ComPtr<IWICBitmapDecoder> decoder;check(factory->CreateDecoderFromFilename(source.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder));
   GUID format{};check(decoder->GetContainerFormat(&format));UINT frames=0;check(decoder->GetFrameCount(&frames));
   if(format!=GUID_ContainerFormatPng||frames!=1)throw std::runtime_error("Weapon icon must be single PNG");
   ComPtr<IWICBitmapFrameDecode> frame;check(decoder->GetFrame(0,&frame));Icon icon;check(frame->GetSize(&icon.width,&icon.height));
   if(!icon.width||!icon.height||icon.width>1024||icon.height>512||(total+=size_t(icon.width)*icon.height)>8*1024*1024)throw std::runtime_error("Weapon icon dimensions");
   ComPtr<IWICFormatConverter> converter;check(factory->CreateFormatConverter(&converter));
   check(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
   icon.bgra.resize(size_t(icon.width)*icon.height);check(converter->CopyPixels(nullptr,icon.width*4,UINT(icon.bgra.size()*4),reinterpret_cast<BYTE*>(icon.bgra.data())));
   draft.emplace(id,std::move(icon));
  }
  images_=std::move(draft);error.clear();return true;
 }catch(const std::exception&e){images_.clear();error=e.what();return false;}
}
const Icon* Icons::find(uint16_t id)const{auto it=images_.find(id);return it==images_.end()?nullptr:&it->second;}
void paint_icon(const Icon&icon,std::span<uint32_t> dst,int width,int height,int x,int y,int w,int h,bool muted,double displayWidth,double displayHeight){
 if(width<=0||height<=0||dst.size()<size_t(width)*height||w<=0||h<=0||!icon.width||!icon.height||icon.bgra.size()!=size_t(icon.width)*icon.height)return;
 if(!displayWidth&&!displayHeight){displayWidth=icon.width;displayHeight=icon.height;}
 if(!std::isfinite(displayWidth)||!std::isfinite(displayHeight)||displayWidth<=0||displayHeight<=0)return;
 double scale=std::min(double(w)/displayWidth,double(h)/displayHeight);
 int dw=std::max(1,int(std::round(displayWidth*scale))),dh=std::max(1,int(std::round(displayHeight*scale)));x+=(w-dw)/2;y+=(h-dh)/2;
 for(int j=0;j<dh;++j)for(int i=0;i<dw;++i){
  if(x+i<0||x+i>=width||y+j<0||y+j>=height)continue;
  double sx=std::clamp((i+.5)*icon.width/dw-.5,0.,double(icon.width-1)),sy=std::clamp((j+.5)*icon.height/dh-.5,0.,double(icon.height-1));
  unsigned ix=unsigned(sx),iy=unsigned(sy);double fx=sx-ix,fy=sy-iy,a=0,c[3]{};
  for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx){auto p=icon.bgra[std::min(iy+dy,icon.height-1)*icon.width+std::min(ix+dx,icon.width-1)];double weight=(dx?fx:1-fx)*(dy?fy:1-fy);double alpha=double(p>>24)/255.;a+=alpha*weight;for(unsigned k=0;k<3;++k)c[k]+=double((p>>(8*k))&255)*alpha*weight;}
  if(a<=0)continue;
  if(muted){double gray=c[0]*.114+c[1]*.587+c[2]*.299;for(auto&v:c)v=gray*.65;a*=.65;}
  auto& pixel=dst[size_t(y+j)*width+x+i];double da=double(pixel>>24)/255.,oa=a+da*(1-a);uint32_t out=uint32_t(std::round(oa*255))<<24;
  for(unsigned k=0;k<3;++k){double value=(c[k]+double((pixel>>(8*k))&255)*da*(1-a))/oa;out|=uint32_t(std::clamp(std::round(value),0.,255.))<<(8*k);}pixel=out;
 }
}
}
