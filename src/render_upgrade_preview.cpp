#include "render_upgrade_preview.h"
#include "character_renderer.h"
#include "render_effects.h"
#include "render_profiler.h"
#include "combat_particle_renderer.h"
#include "stage_normals.h"
#include "stage_floor_blend.h"
#include "stage_surface_alpha.h"
#include "stage_lighting.h"
#include "stage_sky.h"
#include "menu_font.h"
#include "world_depth.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <cmath>
namespace mgo2mt {namespace {
using Microsoft::WRL::ComPtr;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void ok(HRESULT value){require(SUCCEEDED(value),"Graphics capture D3D11 call");}
std::vector<char> read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);require(bool(f),"Graphics capture resource missing");return {std::istreambuf_iterator<char>(f),{}};}
std::vector<uint32_t> pixels(ID3D11Device*d,ID3D11DeviceContext*c,ID3D11ShaderResourceView*view){
 ComPtr<ID3D11Resource> resource;view->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;ok(resource.As(&texture));D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);require(desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM,"Capture requires resolved SDR output");desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> copy;ok(d->CreateTexture2D(&desc,nullptr,&copy));c->CopyResource(copy.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE map{};ok(c->Map(copy.Get(),0,D3D11_MAP_READ,0,&map));std::vector<uint32_t> out(size_t(desc.Width)*desc.Height);
 for(unsigned y=0;y<desc.Height;++y)for(unsigned x=0;x<desc.Width;++x){auto p=static_cast<const uint8_t*>(map.pData)+y*map.RowPitch+x*4;out[size_t(y)*desc.Width+x]=0xff000000u|(uint32_t(p[0])<<16)|(uint32_t(p[1])<<8)|p[2];}c->Unmap(copy.Get(),0);return out;
}
void bitmap(const std::filesystem::path&path,std::vector<uint32_t> values,const wchar_t* label){
 constexpr unsigned w=1280,h=720;HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-int(h);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* raw=nullptr;auto image=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&raw,nullptr,0);require(image&&raw,"Capture DIB allocation");auto old=SelectObject(dc,image);std::memcpy(raw,values.data(),values.size()*4);auto font=create_menu_font(24,FW_BOLD);auto oldFont=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);RECT rect{20,18,1260,90};SetTextColor(dc,RGB(0,0,0));OffsetRect(&rect,2,2);DrawTextW(dc,label,-1,&rect,DT_LEFT|DT_NOPREFIX);OffsetRect(&rect,-2,-2);SetTextColor(dc,RGB(255,223,120));DrawTextW(dc,label,-1,&rect,DT_LEFT|DT_NOPREFIX);GdiFlush();BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+w*h*4;std::ofstream output(path,std::ios::binary);output.write(reinterpret_cast<char*>(&file),sizeof(file));output.write(reinterpret_cast<char*>(&info.bmiHeader),sizeof(info.bmiHeader));output.write(static_cast<char*>(raw),w*h*4);require(bool(output),"Capture write");SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(image);DeleteDC(dc);
}
}
int run_render_upgrade_preview(const std::filesystem::path&data,const std::filesystem::path&output){try{
 std::filesystem::create_directories(output);menu_font_resources().load(data/"fonts");
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL feature;bool software=false;
 auto created=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&feature,&context);
 if(FAILED(created)){software=true;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&feature,&context));}
 auto bytes=read(data/"stage/n022a.gwm");CharacterModel model(bytes);stage::load_original_normals(model,bytes,data/"stage/n022a.gwn");stage::load_floor_blend(model,bytes,data/"stage/n022a.gfb");stage::load_surface_alpha(model,bytes,data/"stage/n022a.gsa");std::ifstream lf(data/"stage/n022a.lighting.cfg");auto light=stage::Lighting::read(lf);
 for(auto&v:model.vertices){auto color=light.sample({v.x,v.y,v.z},{v.nx,v.ny,v.nz}).color;v.lr=color[0];v.lg=color[1];v.lb=color[2];v.lit=1;}
 CharacterRenderer world(device.Get(),model);world.prepare_lod(device.Get(),model);effects::Renderer effects(device.Get());combat::particles::Renderer particles(device.Get(),data/"fx/original.gwfx");
 auto skyModel=CharacterModel(read(data/"stage/n022a.sky.gwm"),ModelExtent::sky);std::ifstream skyFile(data/"stage/n022a.sky.cfg");auto skySettings=stage::SkySettings::read(skyFile);skySettings.prepare(skyModel);CharacterRenderer sky(device.Get(),skyModel,true);auto skyPose=skySettings.sample(12);SkyFrame skyFrame{skyPose.position,skyPose.degrees,skySettings.color,skySettings.fogColor,skySettings.fog,skyPose.cloudU};
 WorldView camera{{-40000,2800,29500},{-.75f,-.2f,.65f},16.f/9,1.f};effects::Camera lens;DirectX::XMFLOAT4X4 matrix;auto projection=world_projection(camera.aspect,camera.verticalFov);DirectX::XMStoreFloat4x4(&matrix,projection);std::memcpy(lens.projection.data(),&matrix,64);DirectX::XMStoreFloat4x4(&matrix,DirectX::XMMatrixInverse(nullptr,projection));std::memcpy(lens.inverseProjection.data(),&matrix,64);
 const auto reflectionMaterials=render_reflections::load(data/"render_materials.json");
 auto presentation=render_backend::Device(device.Get()).target(1280,720);render_profiler::Profiler profiler;profiler.initialize(device.Get());
 // Original smoke images; positions/lifetime here are an offline render fixture.
 std::array<combat::particles::Sprite,3> smoke;
 for(unsigned i=0;i<smoke.size();++i)smoke[i]={{-43000.f-float(i)*170,850.f+float(i)*270,31800.f},900.f+float(i)*150,.2f*float(i),{.65f,.65f,.65f,.36f},0xcaa2b5,{0,0,1,1},false};
 std::array<DynamicPointLight,1> lights{{{{-42200,1700,30900},{1,.7f,.3f},2500,4}}};
 std::vector<uint32_t> legacy,enhanced;size_t differences=0,reflectionDifferences=0;uint64_t id=0;
 auto draw=[&](effects::Settings settings,bool soft,bool lod,bool timed){
  render_backend::Options options;options.hdr=settings.hdr;options.softParticles=soft;options.lod=lod;options.reflections=settings.ssr;options.reflectionMaterials=reflectionMaterials;render_backend::Device(device.Get()).configure(options);world.resize_target(device.Get(),1280,720,settings.hdr,settings.ssr);
  const auto start=std::chrono::steady_clock::now();if(timed){profiler.poll(context.Get());profiler.begin_frame(context.Get(),id);profiler.begin_stage(context.Get(),render_profiler::Stage::Opaque);}
  world.render(context.Get(),0,false,&camera,nullptr,nullptr,lights,nullptr,nullptr,nullptr,1,CharacterPass::opaque);
  sky.render(context.Get(),0,false,&camera,&world,nullptr,{},nullptr,nullptr,nullptr,1,CharacterPass::all,&skyFrame);
  if(timed){profiler.end_stage(context.Get(),render_profiler::Stage::Opaque);profiler.begin_stage(context.Get(),render_profiler::Stage::AmbientReflection);}
  auto lit=effects.opaque(context.Get(),{world.view(),world.depth_view(),world.reflection_view(),lens},settings);if(lit!=world.view())effects.copy_to(context.Get(),lit,world.target_view());
  if(timed){profiler.end_stage(context.Get(),render_profiler::Stage::AmbientReflection);profiler.begin_stage(context.Get(),render_profiler::Stage::TransparentFx);}
  world.render(context.Get(),0,false,&camera,&world,nullptr,lights,nullptr,nullptr,nullptr,1,CharacterPass::alpha);particles.render(context.Get(),world,camera,smoke);
  if(timed){profiler.end_stage(context.Get(),render_profiler::Stage::TransparentFx);profiler.begin_stage(context.Get(),render_profiler::Stage::Post);}
  auto final=effects.finish(context.Get(),world.view(),settings,world.hdr());effects.copy_to(context.Get(),final,presentation.target.Get());
  if(timed){profiler.end_stage(context.Get(),render_profiler::Stage::Post);profiler.end_frame(context.Get());const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();profiler.record_cpu(id,{render_profiler::Missing,ms,render_profiler::Missing});++id;context->Flush();}
 };
 draw({},false,false,false);legacy=pixels(device.Get(),context.Get(),presentation.view.Get());bitmap(output/"01-original.bmp",legacy,L"QQ / 追加描画効果 OFF");
 effects::Settings all;all.hdr=all.fxaa=all.ssao=all.ssr=all.bloom=true;draw(all,true,true,false);enhanced=pixels(device.Get(),context.Get(),presentation.view.Get());bitmap(output/"02-enhanced.bmp",enhanced,L"QQ / HDR + FXAA + SSAO + Bloom + SSR + Soft particles + LOD");
 for(size_t i=0;i<legacy.size();++i)differences+=legacy[i]!=enhanced[i];require(differences>1000,"Enhanced rendering changes real stage");
 auto noReflection=all;noReflection.ssr=false;draw(noReflection,true,true,false);auto withoutReflection=pixels(device.Get(),context.Get(),presentation.view.Get());for(size_t i=0;i<enhanced.size();++i)reflectionDifferences+=enhanced[i]!=withoutReflection[i];require(reflectionDifferences>0,"Native material profile produces real SSR pixels");
 draw({},false,false,false);require(legacy==pixels(device.Get(),context.Get(),presentation.view.Get()),"OFF restores exact original image");
 for(unsigned n=0;n<100;++n){draw(all,true,true,true);Sleep(2);}
 for(unsigned n=0;n<200&&profiler.status().pending;++n){profiler.poll(context.Get());Sleep(2);}profiler.poll(context.Get());
 auto graph=pixels(device.Get(),context.Get(),presentation.view.Get());auto painted=render_profiler::paint(graph,1280,720,profiler,{16,355,1248,340});require(painted.painted,"Actual GPU graph output");bitmap(output/"03-gpu-graph.bmp",graph,L"QQ / 実GPU処理時間 [ms] — オフライン描画試験（画面更新待ちは未測定）");
 const auto summary=render_profiler::summarize(profiler.history(),render_profiler::Series::GpuTotal);const auto status=profiler.status();require(summary.count>0,"No valid GPU measurements from capture device");size_t reflective=0,nativeReflective=0;for(const auto&p:model.parts)nativeReflective+=render_reflections::material(reflectionMaterials,p.materialShader)[0]>0;for(const auto&p:model.parts)reflective+=p.original.present&&p.original.reflectionSlot<p.original.textures.size();
 std::ofstream report(output/"capture.json");report<<"{\"passed\":true,\"offlineRenderFixture\":true,\"hardware\":"<<(software?"false":"true")<<",\"adapter\":"<<std::quoted(status.adapter)<<",\"changedPixels\":"<<differences<<",\"exactOffRestoration\":true,\"sourceReflectionMaterials\":"<<reflective<<",\"nativeReflectionMaterials\":"<<nativeReflective<<",\"ssrChangedPixels\":"<<reflectionDifferences<<",\"lodTriangles\":"<<world.lod_triangles()<<",\"sourceTriangles\":"<<world.original_triangles()<<",\"simplifiedParts\":"<<world.simplified_parts()<<",\"gpuSamples\":"<<summary.count<<",\"gpuMeanMs\":"<<(summary.count?summary.average:0)<<",\"gpuP95Ms\":"<<(summary.count?summary.p95:0)<<",\"gpuPending\":"<<status.pending<<",\"gpuDropped\":"<<status.dropped<<",\"effectsBytes\":"<<effects.allocated_bytes()<<"}\n";
 std::ofstream history(output/"gpu-times.csv");history<<"frame,state,cpu_submit_ms,gpu_total_ms,opaque_ms,ao_ssr_ms,transparent_ms,post_ms\n";for(const auto&s:profiler.history()){history<<s.frame<<','<<render_profiler::state_name(s.state)<<','<<s.cpu.submitMs;for(auto stage:{render_profiler::Stage::Total,render_profiler::Stage::Opaque,render_profiler::Stage::AmbientReflection,render_profiler::Stage::TransparentFx,render_profiler::Stage::Post}){history<<',';auto ms=s.gpuMs[size_t(stage)];if(std::isfinite(ms))history<<ms;}history<<'\n';}
 std::cout<<"Graphics pipeline/off restoration/real GPU graph PASS, samples="<<summary.count<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
}
