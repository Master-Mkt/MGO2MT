#include "character_renderer.h"
#include "stage_lighting.h"
#include "enemy_name_tag.h"
#include "menu_font.h"
#include <DirectXPackedVector.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
using namespace mgo2mt;using Microsoft::WRL::ComPtr;
namespace {
void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
void ok(HRESULT h){check(SUCCEEDED(h),"D3D11 quality test");}
std::vector<uint8_t> pixels(ID3D11Device*d,ID3D11DeviceContext*c,ID3D11ShaderResourceView*view,unsigned mip=0){
 ComPtr<ID3D11Resource> resource;view->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;ok(resource.As(&texture));D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);unsigned w=std::max(1u,desc.Width>>mip),h=std::max(1u,desc.Height>>mip);desc.BindFlags=desc.MiscFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.Usage=D3D11_USAGE_STAGING;
 ComPtr<ID3D11Texture2D> staging;ok(d->CreateTexture2D(&desc,nullptr,&staging));c->CopyResource(staging.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE mapped{};ok(c->Map(staging.Get(),mip,D3D11_MAP_READ,0,&mapped));std::vector<uint8_t> result(size_t(w)*h*4);
 for(unsigned y=0;y<h;++y)std::copy_n(static_cast<uint8_t*>(mapped.pData)+y*mapped.RowPitch,w*4,result.data()+size_t(y)*w*4);c->Unmap(staging.Get(),mip);return result;
}
void bitmap(const std::filesystem::path& path,std::vector<uint8_t> p,unsigned w,unsigned h){
 for(size_t i=0;i<p.size();i+=4)std::swap(p[i],p[i+2]);BITMAPFILEHEADER f{};BITMAPINFOHEADER b{};f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(b);f.bfSize=f.bfOffBits+DWORD(p.size());b.biSize=sizeof(b);b.biWidth=LONG(w);b.biHeight=-LONG(h);b.biPlanes=1;b.biBitCount=32;b.biSizeImage=DWORD(p.size());std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<char*>(&f),sizeof(f));out.write(reinterpret_cast<char*>(&b),sizeof(b));out.write(reinterpret_cast<char*>(p.data()),p.size());check(bool(out),"quality screenshot");
}
void contracts(ID3D11Device*d,ID3D11DeviceContext*c){
 render_backend::Device backend(d);check(backend.options()==render_backend::Options{},"Legacy default");
 for(unsigned n:{0u,2u,4u,8u,16u}){D3D11_SAMPLER_DESC desc{};backend.sampler(n)->GetDesc(&desc);std::cout<<"sampler requested="<<n<<" actual="<<desc.MaxAnisotropy<<" filter="<<unsigned(desc.Filter)<<std::endl;check((!n||desc.MaxAnisotropy==n)&&desc.Filter==(n?D3D11_FILTER_ANISOTROPIC:D3D11_FILTER_MIN_MAG_MIP_LINEAR),"real anisotropic sampler");}
 for(unsigned p:{50u,75u,100u,125u,150u,175u,200u}){auto extent=render_backend::internal_extent(3840,2160,p);check(uint64_t(extent.width)*extent.height<=16777216&&std::abs(double(extent.width)/extent.height-16.0/9)<.002,"internal resolution budget and aspect");}
 auto native=render_backend::internal_extent(3840,2160,100);check(native.width==3840&&native.height==2160&&!native.reduced,"native4K");check(render_backend::internal_extent(7680,4320,200).reduced,"extreme setting bounded");
 // Half white/half black in each 2x2 block. Color mip must average in linear light.
 std::vector<uint32_t> checker(16*16);for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x)checker[y*16+x]=(x+y)%2?0xffffffff:0xff000000;
 D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=16;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA initial{checker.data(),16*4,0};ComPtr<ID3D11Texture2D> texture;ok(d->CreateTexture2D(&desc,&initial,&texture));
 for(bool color:{false,true}){D3D11_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=color?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;sd.Texture2D.MipLevels=1;ComPtr<ID3D11ShaderResourceView> source;ok(d->CreateShaderResourceView(texture.Get(),&sd,&source));
  D3D11_VIEWPORT before{17,29,45,67,0,1};c->RSSetViewports(1,&before);auto mip=backend.mip_chain(c,source.Get(),color);D3D11_VIEWPORT after{};UINT count=1;c->RSGetViewports(&count,&after);check(after.TopLeftX==17&&after.Height==67,"mip generation preserves caller context");auto tiny=pixels(d,c,mip.Get(),4);check(tiny.size()==4&&std::abs(int(tiny[0])-(color?188:128))<=1&&tiny[3]==255,"correct sRGB/data mip averages");}
 // A grey BC1 diffuse under half illumination: decode once, light, encode once.
 CharacterModel model;model.bounds={-5000,-5000,1500,5000,5000,1500};for(auto p:std::array<std::array<float,2>,4>{{{-5000,-5000},{5000,-5000},{5000,5000},{-5000,5000}}}){ModelVertex v{};v.x=p[0];v.y=p[1];v.z=1500;v.nz=-1;v.lr=v.lg=v.lb=.5f;v.lit=1;model.vertices.push_back(v);}model.indices={0,1,2,0,2,3};model.parts.push_back({0,6,0,0});model.textures.push_back({4,4,9,{0x10,0x84,0x10,0x84,0,0,0,0}});
 CharacterRenderer renderer(d,model);WorldView camera{{0,0,0},{0,0,1}};renderer.render(c,0,false,&camera);auto legacy=pixels(d,c,renderer.view());backend.configure({16,true,true});renderer.render(c,0,false,&camera);auto linear=pixels(d,c,renderer.view());auto at=(196*616+308)*4;check(legacy[at]>=65&&legacy[at]<=67&&linear[at]>=94&&linear[at]<=97,"linear lighting and output transfer");backend.configure({});renderer.render(c,0,false,&camera);check(pixels(d,c,renderer.view())==legacy,"Legacy exact restoration after enhanced path");
 // HDR retains radiance above1 rather than clipping in an8bit scene target.
 for(auto&v:model.vertices)v.lr=v.lg=v.lb=8;
 renderer.update_vertices(c,model.vertices);renderer.resize_target(d,64,64,true,true);camera.aspect=1;renderer.render(c,0,false,&camera);
 ComPtr<ID3D11Resource> resource;renderer.view()->GetResource(&resource);ComPtr<ID3D11Texture2D> hdrTexture;ok(resource.As(&hdrTexture));D3D11_TEXTURE2D_DESC hd{};hdrTexture->GetDesc(&hd);check(hd.Format==DXGI_FORMAT_R16G16B16A16_FLOAT,"HDR16float scene allocation");
 hd.Usage=D3D11_USAGE_STAGING;hd.BindFlags=0;hd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> readback;ok(d->CreateTexture2D(&hd,nullptr,&readback));c->CopyResource(readback.Get(),hdrTexture.Get());D3D11_MAPPED_SUBRESOURCE mapping{};ok(c->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapping));auto pixel=reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(mapping.pData)+32*mapping.RowPitch)+32*4;float radiance=DirectX::PackedVector::XMConvertHalfToFloat(pixel[0]);c->Unmap(readback.Get(),0);check(radiance>1&&radiance<8,"unclipped linear HDR radiance");
 check(renderer.depth_view()&&renderer.reflection_view(),"readable reverse-depth and reflection mask");renderer.resize_target(d,64,64);check(!renderer.hdr()&&!renderer.reflection_view(),"HDR OFF releases extra targets");
}
}
int main(int argc,char**argv){try{
 ComPtr<ID3D11Device>d;ComPtr<ID3D11DeviceContext>c;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c));contracts(d.Get(),c.Get());
 if(argc==3){const std::filesystem::path root=argv[1],out=argv[2];std::filesystem::create_directories(out);std::ifstream f(root/"n022a.gwm",std::ios::binary),lf(root/"n022a.lighting.cfg");check(bool(f)&&bool(lf),"QQ source assets");std::vector<char> bytes{std::istreambuf_iterator<char>(f),{}};CharacterModel model(bytes);auto lighting=stage::Lighting::read(lf);check(lighting.hemispheres.size()==104&&lighting.points.size()==31,"QQ LT3 source light counts");auto ambient=lighting;ambient.hemispheres.clear();ambient.points.clear();ambient.authored.clear();ambient.direct={};size_t influenced=0;
  for(auto&v:model.vertices){auto value=lighting.sample({v.x,v.y,v.z},{v.nx,v.ny,v.nz}).color;auto flat=ambient.sample({v.x,v.y,v.z},{v.nx,v.ny,v.nz}).color;if(value!=flat)++influenced;v.lr=value[0];v.lg=value[1];v.lb=value[2];v.lit=1;}check(influenced>100,"QQ actual vertices differ from ambient-only lighting");
  CharacterRenderer renderer(d.Get(),model);WorldView camera{{-40000,2800,29500},{-.75f,-.2f,.65f},16.f/9};std::vector<uint8_t> baseline;size_t changed=0;
  for(unsigned step=0;step<3;++step){unsigned w=step==2?3840:1280,h=step==2?2160:720;render_backend::Device(d.Get()).configure(step==1?render_backend::Options{16,true,true}:render_backend::Options{});renderer.resize_target(d.Get(),w,h);renderer.render(c.Get(),0,false,&camera);auto p=pixels(d.Get(),c.Get(),renderer.view());if(step==0)baseline=p;if(step==1){for(size_t i=0;i<p.size();i+=4)changed+=p[i]!=baseline[i]||p[i+1]!=baseline[i+1]||p[i+2]!=baseline[i+2];check(changed>1000,"QQ enhanced pipeline visibly active");}bitmap(out/(step==0?"qq-legacy.bmp":step==1?"qq-linear-mips-16x.bmp":"qq-legacy-4k.bmp"),p,w,h);}
  std::ofstream report(out/"render.json");report<<"{\"offlineRenderFixture\":true,\"stage\":\"n022a\",\"hemispheres\":104,\"points\":31,\"verticesAffectedBeyondAmbient\":"<<influenced<<",\"linearChangedPixels\":"<<changed<<",\"native4K\":true,\"originalShaderParity\":false}\n";
 }
 std::cout<<"D3D11 Forward quality PASS: sRGB mip average, shader transfer, 16x sampler, bounded internal4K, state/Legacy restoration\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
