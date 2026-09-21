#include "render_profiler.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <sstream>

namespace mgo2mt::render_profiler {
namespace {
using Microsoft::WRL::ComPtr;
constexpr uint32_t bit(Stage s){return 1u<<unsigned(s);}
bool valid_ms(double value){return std::isfinite(value)&&value>=0&&value<=60000;}
double sample_value(const Sample&s,Series series){
 if(series==Series::CpuFrame)return s.cpu.frameMs;
 if(series==Series::CpuSubmit)return s.cpu.submitMs;
 if(series==Series::Present)return s.cpu.presentMs;
 if(s.state!=SampleState::Valid)return Missing;
 constexpr Stage stages[]={Stage::Total,Stage::Shadow,Stage::Opaque,Stage::AmbientReflection,Stage::TransparentFx,Stage::Post,Stage::Ui};
 const auto index=size_t(series)-size_t(Series::GpuTotal);return index<std::size(stages)?s.gpuMs[size_t(stages[index])]:Missing;
}
std::string hresult(const char*what,HRESULT hr){char text[96];std::snprintf(text,sizeof(text),"%s (0x%08lx)",what,static_cast<unsigned long>(hr));return text;}
std::string narrow(const wchar_t*text){const int length=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);if(length<=1)return "Unknown adapter";std::string out(size_t(length),0);WideCharToMultiByte(CP_UTF8,0,text,-1,out.data(),length,nullptr,nullptr);out.pop_back();return out;}
}

std::string_view series_name(Series value){
 constexpr std::string_view names[]={"CPU frame","CPU submit","Present wait","GPU total","GPU shadow","GPU world/actors","GPU AO/reflection","GPU alpha + FX","GPU post","GPU UI"};
 return size_t(value)<std::size(names)?names[size_t(value)]:"Unknown";
}
std::string_view state_name(SampleState value){
 switch(value){case SampleState::Pending:return "measuring";case SampleState::Valid:return "valid";case SampleState::Disjoint:return "disjoint";case SampleState::Dropped:return "ring busy / dropped";case SampleState::Unavailable:return "unavailable";case SampleState::Invalid:return "invalid";}return "invalid";
}
Summary summarize(std::span<const Sample> samples,Series series){
 Summary result;std::vector<double> values;values.reserve(samples.size());
 for(const auto&sample:samples){const double value=sample_value(sample,series);if(valid_ms(value)){values.push_back(value);result.latest=value;}}
 if(values.empty())return result;result.count=values.size();result.average=std::accumulate(values.begin(),values.end(),0.0)/double(values.size());
 std::sort(values.begin(),values.end());result.maximum=values.back();result.p95=values[size_t(std::ceil(.95*double(values.size())))-1];return result;
}
Sample evaluate_timestamps(const TimestampData&data){
 Sample result;result.state=SampleState::Invalid;result.invalidStages=data.invalid;
 if(data.disjoint){result.state=SampleState::Disjoint;return result;}
 const auto total=size_t(Stage::Total);if(!data.frequency||!(data.issued&bit(Stage::Total))||(data.invalid&bit(Stage::Total))||data.ticks[total][1]<data.ticks[total][0])return result;
 for(size_t index=0;index<StageCount;++index){
  if(!(data.issued&(1u<<index)))continue;
  const auto&tick=data.ticks[index];
  if((data.invalid&(1u<<index))||tick[1]<tick[0]||tick[0]<data.ticks[total][0]||tick[1]>data.ticks[total][1]){result.invalidStages|=1u<<index;continue;}
  const double ms=double(tick[1]-tick[0])*1000.0/double(data.frequency);
  if(valid_ms(ms))result.gpuMs[index]=ms;else result.invalidStages|=1u<<index;
 }
 if(valid_ms(result.gpuMs[total]))result.state=SampleState::Valid;
 return result;
}

struct Profiler::Impl {
 struct Slot {
  ComPtr<ID3D11Query> disjoint;std::array<std::array<ComPtr<ID3D11Query>,2>,StageCount> time;
  enum State {Free,Active,Pending} state=Free;uint64_t frame=0;uint32_t began=0,ended=0,invalid=0;bool abandoned=false;
 };
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
 std::array<Slot,QueryRingCapacity> slots;size_t active=QueryRingCapacity;Status info;std::vector<Sample> history;
 uint64_t lastFrame=0;bool hasFrame=false;
 Impl(){history.reserve(HistoryCapacity);}
 Sample* find(uint64_t frame){auto found=std::find_if(history.begin(),history.end(),[&](const auto&s){return s.frame==frame;});return found==history.end()?nullptr:&*found;}
 Sample* append(uint64_t frame){if(auto*found=find(frame))return found;if(!history.empty()&&frame<=history.back().frame)return nullptr;if(history.size()==HistoryCapacity)history.erase(history.begin());history.push_back({});history.back().frame=frame;return &history.back();}
 bool same(ID3D11DeviceContext*c)const{return c&&c==context.Get();}
 void unavailable(const std::string&reason){info.supported=false;info.reason=reason;active=QueryRingCapacity;for(auto&slot:slots)if(slot.state!=Slot::Free){if(auto*s=find(slot.frame))s->state=SampleState::Unavailable;slot.state=Slot::Free;}info.pending=0;}
};
Profiler::Profiler():impl_(std::make_unique<Impl>()){}Profiler::~Profiler(){if(impl_&&impl_->active!=QueryRingCapacity)abandon_frame(impl_->context.Get());}Profiler::Profiler(Profiler&&)noexcept=default;
Profiler&Profiler::operator=(Profiler&&other)noexcept{if(this!=&other){if(impl_&&impl_->active!=QueryRingCapacity)abandon_frame(impl_->context.Get());impl_=std::move(other.impl_);}return *this;}
bool Profiler::initialize(ID3D11Device*device){
 if(impl_&&impl_->active!=QueryRingCapacity)abandon_frame(impl_->context.Get());impl_=std::make_unique<Impl>();auto&i=*impl_;if(!device){i.info.reason="No D3D11 device";return false;}i.device=device;device->GetImmediateContext(&i.context);
 ComPtr<IDXGIDevice> dxgi;if(SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dxgi)))){ComPtr<IDXGIAdapter> adapter;if(SUCCEEDED(dxgi->GetAdapter(&adapter))){DXGI_ADAPTER_DESC desc{};if(SUCCEEDED(adapter->GetDesc(&desc)))i.info.adapter=narrow(desc.Description);ComPtr<IDXGIAdapter1> adapter1;if(SUCCEEDED(adapter.As(&adapter1))){DXGI_ADAPTER_DESC1 d{};if(SUCCEEDED(adapter1->GetDesc1(&d)))i.info.software=(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)!=0;}}}
 for(auto&slot:i.slots){D3D11_QUERY_DESC desc{D3D11_QUERY_TIMESTAMP_DISJOINT,0};HRESULT hr=device->CreateQuery(&desc,&slot.disjoint);if(FAILED(hr)){i.unavailable(hresult("Timestamp-disjoint unsupported",hr));return false;}desc.Query=D3D11_QUERY_TIMESTAMP;for(auto&pair:slot.time)for(auto&query:pair){hr=device->CreateQuery(&desc,&query);if(FAILED(hr)){i.unavailable(hresult("Timestamp unsupported",hr));return false;}}}
 i.info.supported=true;i.info.reason="Awaiting a completed GPU sample";return true;
}
bool Profiler::begin_frame(ID3D11DeviceContext*context,uint64_t frame){
 auto&i=*impl_;if(i.active!=QueryRingCapacity)abandon_frame(i.context.Get());
 if(i.hasFrame&&frame<=i.lastFrame){i.info.reason="Frame IDs must increase";return false;}i.hasFrame=true;i.lastFrame=frame;
 auto*sample=i.append(frame);if(!sample)return false;
 if(!i.info.supported||!i.same(context)){sample->state=SampleState::Unavailable;if(i.info.supported)i.info.reason="An immediate context from the initialized device is required";return false;}
 auto free=std::find_if(i.slots.begin(),i.slots.end(),[](const auto&s){return s.state==Impl::Slot::Free;});
 if(free==i.slots.end()){sample->state=SampleState::Dropped;++i.info.dropped;i.info.reason="GPU query ring busy; newest measurement dropped";return false;}
 i.active=size_t(free-i.slots.begin());auto&slot=*free;slot.state=Impl::Slot::Active;slot.frame=frame;slot.began=bit(Stage::Total);slot.ended=0;slot.invalid=0;slot.abandoned=false;
 context->Begin(slot.disjoint.Get());context->End(slot.time[size_t(Stage::Total)][0].Get());++i.info.submitted;sample->state=SampleState::Pending;return true;
}
void Profiler::begin_stage(ID3D11DeviceContext*context,Stage stage){
 auto&i=*impl_;if(i.active==QueryRingCapacity||!i.same(context)||stage==Stage::Total||size_t(stage)>=StageCount)return;auto&slot=i.slots[i.active];
 if(slot.began&bit(stage)){slot.invalid|=bit(stage);return;}slot.began|=bit(stage);context->End(slot.time[size_t(stage)][0].Get());
}
void Profiler::end_stage(ID3D11DeviceContext*context,Stage stage){
 auto&i=*impl_;if(i.active==QueryRingCapacity||!i.same(context)||stage==Stage::Total||size_t(stage)>=StageCount)return;auto&slot=i.slots[i.active];
 if(!(slot.began&bit(stage))||(slot.ended&bit(stage))){slot.invalid|=bit(stage);return;}slot.ended|=bit(stage);context->End(slot.time[size_t(stage)][1].Get());
}
void Profiler::end_frame(ID3D11DeviceContext*context){
 auto&i=*impl_;if(i.active==QueryRingCapacity||!i.same(context))return;auto&slot=i.slots[i.active];
 slot.invalid|=slot.began&~slot.ended&~bit(Stage::Total);context->End(slot.time[size_t(Stage::Total)][1].Get());slot.ended|=bit(Stage::Total);context->End(slot.disjoint.Get());slot.state=Impl::Slot::Pending;i.active=QueryRingCapacity;++i.info.pending;
}
void Profiler::abandon_frame(ID3D11DeviceContext*context){
 auto&i=*impl_;if(i.active==QueryRingCapacity||!i.same(context))return;auto&slot=i.slots[i.active];slot.abandoned=true;slot.invalid|=bit(Stage::Total);end_frame(context);
}
void Profiler::poll(ID3D11DeviceContext*context){
 auto&i=*impl_;if(!i.info.supported||!i.same(context))return;
 for(auto&slot:i.slots){
  if(slot.state!=Impl::Slot::Pending)continue;D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
  HRESULT hr=context->GetData(slot.disjoint.Get(),&disjoint,sizeof(disjoint),D3D11_ASYNC_GETDATA_DONOTFLUSH);
  if(hr==S_FALSE)continue;if(FAILED(hr)){i.unavailable(hresult("GPU timer GetData failed",hr));return;}
  TimestampData data;data.frequency=disjoint.Frequency;data.disjoint=disjoint.Disjoint!=FALSE;data.issued=slot.began&slot.ended;data.invalid=slot.invalid;
  bool ready=true;
  if(!data.disjoint&&!slot.abandoned)for(size_t n=0;n<StageCount&&ready;++n)if(data.issued&(1u<<n))for(size_t endpoint=0;endpoint<2;++endpoint){
   hr=context->GetData(slot.time[n][endpoint].Get(),&data.ticks[n][endpoint],sizeof(uint64_t),D3D11_ASYNC_GETDATA_DONOTFLUSH);
   if(hr==S_FALSE){ready=false;break;}if(FAILED(hr)){i.unavailable(hresult("GPU timestamp GetData failed",hr));return;}
  }
  if(!ready)continue;auto result=evaluate_timestamps(data);if(slot.abandoned)result.state=SampleState::Invalid;
  if(auto*sample=i.find(slot.frame)){sample->gpuMs=result.gpuMs;sample->state=result.state;sample->invalidStages=result.invalidStages;}
  if(result.state==SampleState::Valid)++i.info.resolved;else if(result.state==SampleState::Disjoint)++i.info.disjoint;else ++i.info.invalid;
  slot.state=Impl::Slot::Free;--i.info.pending;i.info.reason=result.state==SampleState::Valid?"GPU timestamps measured":std::string(state_name(result.state));
 }
}
void Profiler::record_cpu(uint64_t frame,CpuTimes times){
 const bool existed=impl_->find(frame)!=nullptr;if(auto*sample=impl_->append(frame)){if(!existed)sample->state=SampleState::Unavailable;sample->cpu.frameMs=valid_ms(times.frameMs)&&times.frameMs>0?times.frameMs:Missing;sample->cpu.submitMs=valid_ms(times.submitMs)?times.submitMs:Missing;sample->cpu.presentMs=valid_ms(times.presentMs)?times.presentMs:Missing;}
}
std::span<const Sample> Profiler::history()const{return impl_->history;}
Status Profiler::status()const{return impl_->info;}

namespace {
constexpr std::array<uint32_t,size_t(Series::Count)> Colors={0xffffffff,0xff4fe5ff,0xffffff55,0xffd28bff,0xffffa34f,0xff6cff87,0xff60a5ff,0xffff668b,0xff54ffff,0xff91ffcf};
void blend(uint32_t&target,uint32_t source){const unsigned sa=source>>24,da=target>>24,a=sa*255+da*(255-sa);if(!a)return;uint32_t out=((a+127)/255)<<24;for(unsigned shift:{0u,8u,16u}){unsigned sc=(source>>shift)&255,dc=(target>>shift)&255;out|=((sc*sa*255+dc*da*(255-sa)+a/2)/a)<<shift;}target=out;}
struct Canvas {
 std::span<uint32_t> pixels;int width,height,left,top,right,bottom;
 void point(int x,int y,uint32_t color){if(x>=left&&x<right&&y>=top&&y<bottom)blend(pixels[size_t(y)*width+x],color);}
 void fill(int x,int y,int w,int h,uint32_t color){for(int py=std::max(y,top);py<std::min(y+h,bottom);++py)for(int px=std::max(x,left);px<std::min(x+w,right);++px)point(px,py,color);}
 void line(int x0,int y0,int x1,int y1,uint32_t color){int dx=std::abs(x1-x0),sx=x0<x1?1:-1,dy=-std::abs(y1-y0),sy=y0<y1?1:-1,error=dx+dy;for(int guard=0;guard<8192;++guard){point(x0,y0,color);if(x0==x1&&y0==y1)break;const int twice=2*error;if(twice>=dy){error+=dy;x0+=sx;}if(twice<=dx){error+=dx;y0+=sy;}}}
};
struct Text {
 HDC dc=nullptr;HBITMAP bitmap=nullptr;HFONT font=nullptr;HGDIOBJ oldBitmap=nullptr,oldFont=nullptr;uint32_t*data=nullptr;int width=0,height=0;
 Text(int w,int h):width(w),height(h){dc=CreateCompatibleDC(nullptr);if(!dc)return;BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&data),nullptr,0);font=CreateFontW(-14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Consolas");if(bitmap&&font){oldBitmap=SelectObject(dc,bitmap);oldFont=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);}}
 ~Text(){if(oldBitmap)SelectObject(dc,oldBitmap);if(oldFont)SelectObject(dc,oldFont);if(font)DeleteObject(font);if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);}
 void draw(Canvas&canvas,int x,int y,int maxWidth,std::string_view value,uint32_t color){
  if(!oldBitmap||!oldFont||maxWidth<=0)return;std::memset(data,0,size_t(width)*height*4);SetTextColor(dc,RGB(255,255,255));
  int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);if(length<=0)return;std::wstring text(size_t(length),0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),text.data(),length);
  RECT rect{0,0,std::min(maxWidth,width),height};DrawTextW(dc,text.data(),length,&rect,DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);GdiFlush();
  for(int py=0;py<height;++py)for(int px=0;px<rect.right;++px){const unsigned alpha=data[size_t(py)*width+px]&255;if(alpha)canvas.point(x+px,y+py,(color&0xffffff)|(alpha<<24));}
 }
};
std::string number(double value){if(!valid_ms(value))return "   --";char text[48];std::snprintf(text,sizeof(text),value<100?"%6.2f":"%6.1f",value);return text;}
double axis_step(double target){if(!std::isfinite(target)||target<=20)return 20;const double power=std::pow(10.0,std::floor(std::log10(target))),normalized=target/power;for(double step:{1.,2.,2.5,5.,10.})if(normalized<=step)return power*step;return power*10;}
}
GraphResult paint(std::span<uint32_t>pixels,unsigned width,unsigned height,std::span<const Sample>samples,const Status&status,GraphOptions options){
 GraphResult result;if(!width||!height||width>8192||height>8192||pixels.size()<size_t(width)*height||options.width<600||options.height<240||options.width>4096||options.height>2048)return result;
 const int left=std::clamp(options.left,0,int(width)),top=std::clamp(options.top,0,int(height)),right=int(std::clamp<int64_t>(int64_t(options.left)+options.width,0,width)),bottom=int(std::clamp<int64_t>(int64_t(options.top)+options.height,0,height));if(right<=left||bottom<=top)return result;
 Canvas c{pixels,int(width),int(height),left,top,right,bottom};c.fill(options.left,options.top,options.width,options.height,0xe807101b);Text text(std::min(options.width,2048),18);
 auto label=[&](int x,int y,int w,std::string value,uint32_t color=0xffd7e5ff){text.draw(c,x,y,w,value,color);};
 const int x=options.left,y=options.top,legendWidth=440,plotLeft=x+52,plotRight=x+options.width-legendWidth-20,plotTop=y+54,plotBottom=y+options.height-34;
 const int plotWidth=plotRight-plotLeft,plotHeight=plotBottom-plotTop;if(plotWidth<=0||plotHeight<=0)return result;
 if(samples.size()>HistoryCapacity)samples=samples.last(HistoryCapacity);
 std::array<Summary,size_t(Series::Count)> summaries;double maximum=0;for(size_t series=0;series<summaries.size();++series){summaries[series]=summarize(samples,Series(series));if(valid_ms(summaries[series].maximum))maximum=std::max(maximum,summaries[series].maximum);}
 result.axisMaximumMs=axis_step(std::max(20.,maximum*1.05));const auto fps=summaries[size_t(Series::CpuFrame)].average;char fpsText[48];if(valid_ms(fps)&&fps>0)std::snprintf(fpsText,sizeof(fpsText),"FPS %6.1f",1000./fps);else std::snprintf(fpsText,sizeof(fpsText),"FPS --");
 label(x+12,y+8,options.width-24,std::string("F12 FRAME TIMES  |  ")+fpsText+"  |  last "+std::to_string(samples.size())+" sampled frames  |  CPU wall time / GPU timestamp",0xffffffff);
 label(x+12,y+28,options.width-24,(status.software?"WARP / software: ":"Adapter: ")+status.adapter+"  |  "+(status.supported?status.reason:"GPU unavailable: "+status.reason),status.supported?0xff9dceff:0xffffaa77);
 auto py=[&](double value){return plotBottom-int(std::lround(std::clamp(value/result.axisMaximumMs,0.,1.)*plotHeight));};
 for(int n=0;n<=4;++n){double value=result.axisMaximumMs*n/4.;const int gy=py(value);c.line(plotLeft,gy,plotRight,gy,0xff2c3f51);label(x+2,gy-7,48,number(value),0xff93a9c0);}
 const int budgetY=py(1000./60.);for(int px=plotLeft;px<plotRight;px+=8)c.line(px,budgetY,std::min(px+3,plotRight),budgetY,0xffcfaa65);label(plotLeft+5,std::max(plotTop,budgetY-17),170,"16.67 ms / 60 FPS",0xffffcf7d);
 const double dx=double(plotWidth)/double(HistoryCapacity-1);const size_t offset=HistoryCapacity-samples.size();
 for(size_t series=0;series<summaries.size();++series){bool previous=false;int oldX=0,oldY=0;uint64_t oldFrame=0;
  for(size_t n=0;n<samples.size();++n){const double value=sample_value(samples[n],Series(series));if(!valid_ms(value)){previous=false;continue;}const int px=plotLeft+int(std::lround(double(offset+n)*dx)),gy=py(value);
   if(previous&&samples[n].frame==oldFrame+1){c.line(oldX,oldY,px,gy,Colors[series]);++result.lineSegments;}else c.point(px,gy,Colors[series]);
   if((series==size_t(Series::CpuFrame)||series==size_t(Series::GpuTotal))&&value>1000./60.){c.line(px,gy-2,px,gy+2,Colors[series]);++result.spikes;}
   previous=true;oldX=px;oldY=gy;oldFrame=samples[n].frame;
  }
 }
 const int legendX=plotRight+18,legendStep=std::min(18,(options.height-84)/int(Series::Count));label(legendX,plotTop-19,legendWidth,"                     last valid    mean     p95",0xffb9cce3);
 for(size_t series=0;series<summaries.size();++series){const int ly=plotTop+int(series)*legendStep;c.fill(legendX,ly+4,12,3,Colors[series]);const auto&s=summaries[series];label(legendX+18,ly,legendWidth-18,std::string(series_name(Series(series)))+std::string(20-series_name(Series(series)).size(),' ')+number(s.latest)+" "+number(s.average)+" "+number(s.p95),Colors[series]);}
 const size_t valid=std::count_if(samples.begin(),samples.end(),[](const auto&s){return s.state==SampleState::Valid;}),pending=std::max(status.pending,size_t(std::count_if(samples.begin(),samples.end(),[](const auto&s){return s.state==SampleState::Pending;})));
 label(x+12,y+options.height-23,options.width-24,"ms  |  GPU valid "+std::to_string(valid)+" / pending "+std::to_string(pending)+"  |  disjoint "+std::to_string(status.disjoint)+" / dropped "+std::to_string(status.dropped)+" / invalid "+std::to_string(status.invalid)+"  |  -- = missing, never zero-filled",0xffb9cce3);
 if(!valid)label(plotLeft+12,plotTop+plotHeight/2,plotWidth-24,status.supported?"GPU: measuring / no completed valid sample":"GPU: unavailable (CPU times remain independent)",0xffffcf7d);
 result.painted=true;return result;
}
GraphResult paint(std::span<uint32_t>pixels,unsigned width,unsigned height,const Profiler&profiler,GraphOptions options){return paint(pixels,width,height,profiler.history(),profiler.status(),options);}
}
