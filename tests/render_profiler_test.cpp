#include "render_profiler.h"
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace mgo2mt::render_profiler;
using Microsoft::WRL::ComPtr;
namespace {
unsigned checks=0;
void check(bool value,const char*message){++checks;if(!value)throw std::runtime_error(message);}
void ok(HRESULT result,const char*message){if(FAILED(result))throw std::runtime_error(message);}
double elapsed(std::chrono::steady_clock::time_point from,std::chrono::steady_clock::time_point to){return std::chrono::duration<double,std::milli>(to-from).count();}
void png(const std::filesystem::path&path,const std::vector<uint32_t>&argb){
 ComPtr<IWICImagingFactory> factory;ok(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"WIC factory");
 ComPtr<IWICStream> stream;ok(factory->CreateStream(&stream),"WIC stream");ok(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE),"WIC destination");
 ComPtr<IWICBitmapEncoder> encoder;ok(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder),"WIC PNG");ok(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache),"WIC initialize");
 ComPtr<IWICBitmapFrameEncode> frame;ok(encoder->CreateNewFrame(&frame,nullptr),"WIC frame");ok(frame->Initialize(nullptr),"WIC frame initialize");ok(frame->SetSize(1280,720),"WIC extent");WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;ok(frame->SetPixelFormat(&format),"WIC BGRA");check(format==GUID_WICPixelFormat32bppBGRA,"PNG preserves ARGB memory BGRA bytes");ok(frame->WritePixels(720,1280*4,UINT(argb.size()*4),reinterpret_cast<BYTE*>(const_cast<uint32_t*>(argb.data()))),"WIC rows");ok(frame->Commit(),"WIC frame commit");ok(encoder->Commit(),"WIC commit");
}
void pure(const std::filesystem::path&output){
 TimestampData data;data.frequency=1000000;data.issued=(1u<<unsigned(Stage::Total))|(1u<<unsigned(Stage::Opaque));data.ticks[size_t(Stage::Total)]={100,20100};data.ticks[size_t(Stage::Opaque)]={2100,7100};
 auto sample=evaluate_timestamps(data);check(sample.state==SampleState::Valid&&sample.gpuMs[size_t(Stage::Total)]==20&&sample.gpuMs[size_t(Stage::Opaque)]==5,"frequency converts actual tick differences to ms");check(std::isnan(sample.gpuMs[size_t(Stage::Shadow)]),"unmeasured stage remains NaN");
 auto invalid=data;invalid.frequency=0;check(evaluate_timestamps(invalid).state==SampleState::Invalid,"zero frequency rejected");invalid=data;invalid.disjoint=true;sample=evaluate_timestamps(invalid);check(sample.state==SampleState::Disjoint&&std::all_of(sample.gpuMs.begin(),sample.gpuMs.end(),[](double v){return std::isnan(v);}),"disjoint cannot expose any GPU time");
 invalid=data;invalid.ticks[size_t(Stage::Total)]={20100,100};check(evaluate_timestamps(invalid).state==SampleState::Invalid,"backward total counter rejected");invalid=data;invalid.ticks[size_t(Stage::Opaque)]={99,7100};sample=evaluate_timestamps(invalid);check(sample.state==SampleState::Valid&&std::isnan(sample.gpuMs[size_t(Stage::Opaque)])&&sample.invalidStages,"stage outside total is marked invalid without destroying actual total");
 invalid=data;invalid.ticks[size_t(Stage::Opaque)]={2100,2100};check(evaluate_timestamps(invalid).gpuMs[size_t(Stage::Opaque)]==0,"measured zero is distinct from missing");invalid=data;invalid.invalid=1u<<unsigned(Stage::Opaque);check(std::isnan(evaluate_timestamps(invalid).gpuMs[size_t(Stage::Opaque)]),"duplicate stage markers rejected");
 invalid=data;invalid.ticks[size_t(Stage::Total)]={0,UINT64_MAX};check(evaluate_timestamps(invalid).state==SampleState::Invalid,"unreasonable counter interval rejected");
 invalid=data;invalid.issued=1u<<unsigned(Stage::Total);invalid.ticks[size_t(Stage::Total)]={UINT64_MAX-20000,UINT64_MAX};check(evaluate_timestamps(invalid).gpuMs[size_t(Stage::Total)]==20,"subtract64bit timestamps before floating point conversion");
 std::vector<Sample> history;for(unsigned n=0;n<240;++n){auto s=evaluate_timestamps(data);s.frame=n+1;s.cpu={10+double(n%7),3+double(n%3),2};s.gpuMs[size_t(Stage::Total)]=8+double(n%4);s.gpuMs[size_t(Stage::Opaque)]=4;s.gpuMs[size_t(Stage::Shadow)]=1;s.gpuMs[size_t(Stage::AmbientReflection)]=.75;s.gpuMs[size_t(Stage::TransparentFx)]=.5;s.gpuMs[size_t(Stage::Post)]=.6;s.gpuMs[size_t(Stage::Ui)]=.1;history.push_back(s);}
 history[100].cpu.frameMs=93;history[180].gpuMs[size_t(Stage::Total)]=75;history[220].state=SampleState::Disjoint;history[239].state=SampleState::Pending;
 auto summary=summarize(history,Series::GpuTotal);check(summary.count==238&&summary.latest==10&&summary.maximum==75&&summary.p95==11,"summary excludes invalid samples and uses nearest-rank p95");
 Status status;status.supported=true;status.adapter="SYNTHETIC TEST FIXTURE - not a hardware measurement";status.reason="Graph validation only";status.disjoint=1;status.pending=1;
 std::vector<uint32_t> pixels(1280*720,0xff172435);auto graph=paint(pixels,1280,720,history,status,{16,240,1248,336});check(graph.painted&&graph.axisMaximumMs>=93&&graph.spikes>=2&&graph.lineSegments>1000,"spikes remain visible under autoscale and valid series drawn");check(pixels[0]==0xff172435&&pixels[719*1280+1279]==0xff172435,"paint does not touch outside requested rect");png(output/"graph-synthetic-validation.png",pixels);
 pixels.assign(pixels.size(),0xff172435);Status noGpu;noGpu.reason="Timestamp queries unavailable";check(paint(pixels,1280,720,{},noGpu,{16,240,1248,288}).painted,"unavailable status has a visible graph panel");png(output/"graph-unavailable.png",pixels);
 std::array<Sample,2> gap{evaluate_timestamps(data),evaluate_timestamps(data)};gap[0].frame=1;gap[1].frame=3;check(paint(pixels,1280,720,gap,status).lineSegments==0,"skipped frame IDs break rather than interpolate graph lines");gap[1].frame=2;check(paint(pixels,1280,720,gap,status).lineSegments==2,"two measured GPU series connect consecutive frames");gap[1].state=SampleState::Pending;check(paint(pixels,1280,720,gap,status).lineSegments==0,"pending GPU sample cannot reuse old values in graph");
 check(!paint(pixels,1280,720,history,status,{0,0,-1,288}).painted,"invalid rectangle rejected");std::span<uint32_t> shortBuffer(pixels.data(),100);check(!paint(shortBuffer,1280,720,history,status).painted,"short output buffer rejected");
 Profiler profiler;check(!profiler.initialize(nullptr),"null device gracefully unavailable");for(uint64_t n=1;n<=250;++n)profiler.record_cpu(n,{10,3,2});check(profiler.history().size()==240&&profiler.history().front().frame==11,"CPU history bounded to240 chronological samples");profiler.record_cpu(250,{Missing,-1,std::numeric_limits<double>::infinity()});check(std::isnan(profiler.history().back().cpu.frameMs)&&std::isnan(profiler.history().back().cpu.submitMs)&&std::isnan(profiler.history().back().cpu.presentMs),"invalid CPU clocks do not fabricate timings");check(profiler.history().back().state==SampleState::Unavailable,"CPU-only sample never claims GPU pending");
}
void gpu(D3D_DRIVER_TYPE driver,const std::string&name,const std::filesystem::path&output){
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};const HRESULT created=D3D11CreateDevice(nullptr,driver,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);
 std::ofstream report(output/(name+".txt"));if(FAILED(created)){report<<"Device unavailable: HRESULT "<<std::hex<<uint32_t(created)<<'\n';std::cout<<name<<" device unavailable; no GPU values invented\n";return;}
 Profiler profiler;if(!profiler.initialize(device.Get())){report<<"Queries unavailable: "<<profiler.status().reason<<'\n';check(!profiler.begin_frame(context.Get(),1),"unsupported timer declines frame");profiler.record_cpu(1,{Missing,Missing,Missing});std::vector<uint32_t> pixels(1280*720,0xff172435);paint(pixels,1280,720,profiler,{16,240,1248,336});png(output/("graph-"+name+"-unavailable.png"),pixels);std::cout<<name<<" timer unavailable; no GPU values invented\n";return;}
 D3D11_TEXTURE2D_DESC desc{};desc.Width=1024;desc.Height=1024;desc.MipLevels=1;desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
 ComPtr<ID3D11Texture2D> texture;ComPtr<ID3D11RenderTargetView> target;ok(device->CreateTexture2D(&desc,nullptr,&texture),"timer test target");ok(device->CreateRenderTargetView(texture.Get(),nullptr,&target),"timer test RTV");
 uint64_t frame=0;
 // Intentionally do not poll: filling8 slots must drop the ninth sample,
 // never overwrite live queries or flush inside the profiler.
 for(unsigned n=0;n<QueryRingCapacity+1;++n){const bool begun=profiler.begin_frame(context.Get(),++frame);check(begun==(n<QueryRingCapacity),"GPU ring capacity without implicit poll/reuse");if(begun)profiler.end_frame(context.Get());}
 check(profiler.status().pending==QueryRingCapacity&&profiler.status().dropped==1&&profiler.history().back().state==SampleState::Dropped,"saturated ring drops newest sample explicitly");
 auto drain=[&](){context->Flush();const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(profiler.status().pending&&std::chrono::steady_clock::now()<deadline){profiler.poll(context.Get());std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(!profiler.status().pending,"headless test submission completes within bounded deadline");};
 // Only the headless harness flushes to submit work without a swap-chain.
 // The production profiler itself never flushes or waits.
 drain();auto previous=std::chrono::steady_clock::now();
 for(unsigned n=0;n<240;++n){profiler.poll(context.Get());const auto begin=std::chrono::steady_clock::now();const bool begun=profiler.begin_frame(context.Get(),++frame);
  for(unsigned stage=0;stage<unsigned(Stage::Total);++stage){if(begun)profiler.begin_stage(context.Get(),Stage(stage));for(unsigned clear=0;clear<(stage+1)*3;++clear){const float color[]={float((clear+n)%17)/17,float(stage)/6,.2f,1};context->ClearRenderTargetView(target.Get(),color);}if(begun)profiler.end_stage(context.Get(),Stage(stage));}
  if(begun)profiler.end_frame(context.Get());const auto submitted=std::chrono::steady_clock::now();context->Flush();std::this_thread::sleep_for(std::chrono::milliseconds(1));const auto finish=std::chrono::steady_clock::now();profiler.record_cpu(frame,{elapsed(previous,finish),elapsed(begin,submitted),Missing});previous=finish;
 }
 drain();const auto total=summarize(profiler.history(),Series::GpuTotal);const auto status=profiler.status();report<<"Adapter: "<<status.adapter<<"\nsoftware="<<status.software<<"\nsubmitted="<<status.submitted<<" valid="<<status.resolved<<" disjoint="<<status.disjoint<<" invalid="<<status.invalid<<" dropped="<<status.dropped<<"\nGPU total valid-history="<<total.count<<" latest-ms="<<total.latest<<" mean-ms="<<total.average<<" p95-ms="<<total.p95<<"\n";
 check(status.software==(driver==D3D_DRIVER_TYPE_WARP),"adapter software classification");check(total.count>0||status.disjoint>0,"real queries report valid or explicit disjoint results");
 if(total.count){check(total.latest>=0&&total.average>=0,"real measured GPU interval finite");for(unsigned stage=unsigned(Series::Shadow);stage<unsigned(Series::Count);++stage)check(summarize(profiler.history(),Series(stage)).count>0,"each submitted stage resolves independently");}
 std::vector<uint32_t> pixels(1280*720,0xff172435);auto graph=paint(pixels,1280,720,profiler,{16,240,1248,336});check(graph.painted,"real GPU history graph");png(output/("graph-"+name+".png"),pixels);
 check(profiler.begin_frame(context.Get(),++frame),"ring reusable after drain");profiler.begin_stage(context.Get(),Stage::Opaque);profiler.end_frame(context.Get());drain();const auto&last=profiler.history().back();check(last.state!=SampleState::Valid||(last.invalidStages&(1u<<unsigned(Stage::Opaque))),"unfinished stage marked unavailable");
 check(profiler.begin_frame(context.Get(),++frame),"abandon setup");profiler.abandon_frame(context.Get());drain();check(profiler.history().back().state==SampleState::Invalid,"abandoned frame cannot expose GPU time");
 report<<"Post checks complete\n";std::cout<<name<<" adapter="<<status.adapter<<" real GPU samples="<<total.count<<" mean="<<total.average<<" ms\n";
}
}
int main(int argc,char**argv){try{const auto path=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/"mgo2mt-render-profiler";std::filesystem::create_directories(path);const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);ok(com,"COM initialization");pure(path);gpu(D3D_DRIVER_TYPE_HARDWARE,"hardware",path);gpu(D3D_DRIVER_TYPE_WARP,"warp",path);CoUninitialize();std::cout<<"PASS "<<checks<<" render profiler checks: CPU/GPU separation, nonblocking ring, disjoint/invalid/dropped, stage queries, history, statistics and graph PNG\n";return 0;}catch(const std::exception&error){std::cerr<<error.what()<<'\n';return 1;}}
