// Partial title preview with optional native GCX subset; full game boot is pending.
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <syncstream>
#include "agreement_screen.h"
#include "login_screen.h"
#include "character_renderer.h"
#include "character_catalog.h"
#include <stdexcept>
#include <thread>
#include <vector>
#include <memory>
#include "title_animation.h"
#include "title_gcx.h"
#include "audio_control.h"
#include "audio_fade.h"
using Microsoft::WRL::ComPtr;
int run_audio_probe(int,wchar_t**,const std::atomic_bool*,const mgo2win::AudioControl*);
static void check(HRESULT hr) { if(FAILED(hr)) throw std::runtime_error("D3D HRESULT " + std::to_string(static_cast<unsigned long>(hr))); }
static std::vector<char> file(const std::filesystem::path& path) {
 std::ifstream f(path,std::ios::binary|std::ios::ate); if(!f) throw std::runtime_error("Cannot open asset");
 auto n=f.tellg();if(n<0||n>128*1024*1024)throw std::runtime_error("Asset size limit");
 std::vector<char>b(static_cast<size_t>(n));f.seekg(0);f.read(b.data(),n);if(!f)throw std::runtime_error("Asset read");return b;
}
static uint32_t u32(const std::vector<char>& b,size_t p){if(p+4>b.size())throw std::runtime_error("Truncated asset");uint32_t v;std::memcpy(&v,b.data()+p,4);return v;}
using mgo2win::Vertex;
using mgo2win::Quad;
static_assert(sizeof(Quad)==140);
struct Window {
 static inline uint32_t pressed=0;
 static inline unsigned agreementInput=0;
 static inline mgo2win::LoginScreen* login=nullptr;
 static inline mgo2win::ControllerInput* input=nullptr;
 static inline float viewX=0,viewY=0,viewW=1,viewH=1;
 HWND handle=nullptr;
 ~Window(){login=nullptr;input=nullptr;if(handle)DestroyWindow(handle);}
 static LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
  if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(r.right&&r.bottom){float x=float(short(LOWORD(lp)))/r.right,y=float(short(HIWORD(lp)))/r.bottom;if(x<viewX||y<viewY||x>=viewX+viewW||y>=viewY+viewH)return 0;lp=MAKELPARAM(int((x-viewX)/viewW*r.right),int((y-viewY)/viewH*r.bottom));}}
  if(!login&&input&&msg==WM_KEYDOWN&&!(lp&(1LL<<25))&&wp!=VK_ESCAPE&&wp!=VK_RETURN&&!(wp>=VK_LEFT&&wp<=VK_DOWN)){auto mapped=input->keyboard_menu(unsigned(wp));if(mapped)wp=mapped;}
  if(login&&login->message(hwnd,msg,wp,lp))return 0;
  if(msg==WM_CLOSE || (msg==WM_KEYDOWN&&wp==VK_ESCAPE)){PostQuitMessage(0);return 0;}
  if(msg==WM_MOUSEWHEEL){agreementInput |= static_cast<short>(HIWORD(wp))>0?8:16;return 0;}
  if(msg==WM_KEYDOWN&&!(lp&(1LL<<30))){switch(wp){case VK_LEFT:agreementInput|=1;break;case VK_RIGHT:agreementInput|=2;break;case VK_RETURN:agreementInput|=4;break;case VK_UP:agreementInput|=8;break;case VK_DOWN:agreementInput|=16;break;case VK_PRIOR:agreementInput|=32;break;case VK_NEXT:agreementInput|=64;break;case VK_HOME:agreementInput|=128;break;case VK_END:agreementInput|=256;break;case 'R':agreementInput|=512;break;}}
  if(msg==WM_KEYDOWN&&wp==VK_RETURN&&!(lp&(1LL<<30))){pressed|=8;return 0;}
  return DefWindowProcW(hwnd,msg,wp,lp);
 }
 void create(){WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"MGO2WIN_AssetPreview";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
  if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("Register window");
  RECT rect{0,0,1280,720};AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
  handle=CreateWindowW(wc.lpszClassName,L"MGO2WIN - Partial title asset preview (not game boot)",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,wc.hInstance,nullptr);
  if(!handle)throw std::runtime_error("Create window");ShowWindow(handle,SW_SHOW);
 }
};
struct AudioThread {
 std::atomic_bool stop{false}; std::atomic_int result{-1}; std::thread thread;
 mgo2win::AudioControl control;
 ~AudioThread(){stop=true;if(thread.joinable())thread.join();}
 void start(const std::filesystem::path& wav,const std::wstring& seconds,unsigned cue=0,const char* stream=nullptr){control.cue=cue;control.stream=stream?stream:(wav.stem()==L"lobby"||wav.stem()==L"bgm_mgo_lobby01")?"lobby_bgm":"other";thread=std::thread([this,wav,seconds]{
  std::vector<std::wstring> args{L"audio",wav.wstring(),seconds};if(wav.extension()!=L".gwa")args.insert(args.end(),{L"369920",L"3028480",L"0"});std::vector<wchar_t*> pointers;for(auto& a:args)pointers.push_back(a.data());result=run_audio_probe(static_cast<int>(args.size()),pointers.data(),&stop,&control);
 });}
};
static void capture(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* source,const std::filesystem::path& output){
 D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
 ComPtr<ID3D11Texture2D> staging;check(device->CreateTexture2D(&desc,nullptr,&staging));context->CopyResource(staging.Get(),source);
 D3D11_MAPPED_SUBRESOURCE map{};check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map));
 std::vector<unsigned char> pixels(desc.Width*desc.Height*4);
 for(UINT y=0;y<desc.Height;y++)for(UINT x=0;x<desc.Width;x++){
  auto p=static_cast<const unsigned char*>(map.pData)+y*map.RowPitch+4*x;auto q=pixels.data()+(y*desc.Width+x)*4;q[0]=p[2];q[1]=p[1];q[2]=p[0];q[3]=255;
 }
 context->Unmap(staging.Get(),0);BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+static_cast<DWORD>(pixels.size());
 BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=desc.Width;info.biHeight=-static_cast<LONG>(desc.Height);info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
 std::ofstream f(output,std::ios::binary);f.write(reinterpret_cast<char*>(&header),sizeof(header));f.write(reinterpret_cast<char*>(&info),sizeof(info));f.write(reinterpret_cast<char*>(pixels.data()),pixels.size());if(!f)throw std::runtime_error("Capture write");
}
int run_title_preview(int argc,wchar_t**argv){try{
 if(argc<4)throw std::runtime_error("Usage: mgo2win_title_preview scene seconds capture.bmp [--audio] [--scripted-input] [--gcx file --entry procedure --wav file]");
 double seconds=std::stod(argv[2]);if(!(seconds>0&&seconds<=600))throw std::runtime_error("Duration limit");
 bool sound=false,scripted=false,scriptedNo=false,scriptedLogin=false,scriptedPorts=false,scriptedStun=false,scriptedControls=false,scriptedGraphics=false,scriptedCharacters=false,scriptedSlots=false,scriptedAppearance=false,scriptedCreation=false,safeGraphics=false,saveCaptures=true;std::filesystem::path gcxPath,wavPath,sePath,loadingPath,menuConfirmPath,menuMovePath,agreementBackgroundPath,lobbyMusicPath,motionPath,loginPath,networkKeys,modelPath,catalogPath,voiceDirectory;std::wstring policyUrl;uint32_t entry=18;
 for(int i=4;i<argc;++i){std::wstring arg=argv[i];if(arg==L"--audio")sound=true;else if(arg==L"--scripted-input")scripted=true;else if(arg==L"--scripted-no"){scripted=true;scriptedNo=true;}
  else if(arg==L"--scripted-login"){scripted=true;scriptedLogin=true;}
  else if(arg==L"--scripted-ports"){scripted=true;scriptedPorts=true;}
  else if(arg==L"--scripted-stun"){scripted=true;scriptedStun=true;}
  else if(arg==L"--scripted-slots"){scripted=true;scriptedSlots=true;}
  else if(arg==L"--scripted-creation"){scripted=true;scriptedCreation=true;}
  else if(arg==L"--scripted-appearance"){scripted=true;scriptedAppearance=true;}
  else if(arg==L"--scripted-characters"){scripted=true;scriptedCharacters=true;}
  else if(arg==L"--character-catalog"&&i+1<argc)catalogPath=argv[++i];
  else if(arg==L"--voice-directory"&&i+1<argc)voiceDirectory=argv[++i];
  else if(arg==L"--character-model"&&i+1<argc)modelPath=argv[++i];
  else if(arg==L"--network-keys"&&i+1<argc)networkKeys=argv[++i];
  else if(arg==L"--scripted-controls"){scripted=true;scriptedControls=true;}
  else if(arg==L"--scripted-graphics"){scripted=true;scriptedGraphics=true;}
  else if(arg==L"--safe-graphics")safeGraphics=true;
  else if(arg==L"--login-background"&&i+1<argc)loginPath=argv[++i];
  else if(arg==L"--no-capture")saveCaptures=false;
  else if(arg==L"--se"&&i+1<argc)sePath=argv[++i];
  else if(arg==L"--policy-url"&&i+1<argc)policyUrl=argv[++i];
  else if(arg==L"--menu-confirm"&&i+1<argc)menuConfirmPath=argv[++i];
  else if(arg==L"--menu-move"&&i+1<argc)menuMovePath=argv[++i];
  else if(arg==L"--agreement-motion"&&i+1<argc)motionPath=argv[++i];
  else if(arg==L"--agreement-background"&&i+1<argc)agreementBackgroundPath=argv[++i];
  else if(arg==L"--lobby-music"&&i+1<argc)lobbyMusicPath=argv[++i];
  else if(arg==L"--loading"&&i+1<argc)loadingPath=argv[++i];
  else if((arg==L"--gcx"||arg==L"--entry"||arg==L"--wav")&&i+1<argc){++i;if(arg==L"--gcx")gcxPath=argv[i];else if(arg==L"--wav")wavPath=argv[i];else {auto n=std::stoul(argv[i]);if(!n||n>32767)throw std::runtime_error("Entry range");entry=static_cast<uint32_t>(n);}}
  else throw std::runtime_error("Unknown/missing option");}
 auto path=std::filesystem::absolute(argv[1]);auto bytes=file(path);
 std::unique_ptr<mgo2win::TitleGcx> gcx;
 if(!gcxPath.empty()){gcx=std::make_unique<mgo2win::TitleGcx>(file(gcxPath),std::cout);gcx->start(entry);}
 std::unique_ptr<mgo2win::TitleAnimation> animation;uint32_t textureCount=0,count=0;std::vector<Quad>quads;
 if(bytes.size()>=4&&!std::memcmp(bytes.data(),"M2AN",4)){animation=std::make_unique<mgo2win::TitleAnimation>(bytes,gcx?gcx->timeout():18000);textureCount=animation->texture_count();quads=animation->geometry();count=static_cast<uint32_t>(quads.size());}
 else{if(scripted)throw std::runtime_error("Scripted input requires animation");if(bytes.size()<16||std::memcmp(bytes.data(),"M2PV",4)||u32(bytes,4)!=1||!u32(bytes,12)||u32(bytes,12)>64)throw std::runtime_error("Preview header");textureCount=u32(bytes,12);count=u32(bytes,8);if(count>10000||bytes.size()!=16+size_t(count)*sizeof(Quad))throw std::runtime_error("Preview extent");quads.resize(count);std::memcpy(quads.data(),bytes.data()+16,count*sizeof(Quad));}
 std::unique_ptr<mgo2win::AgreementScreen> agreement;
 if(!policyUrl.empty()){if(loadingPath.empty()||menuConfirmPath.empty()||menuMovePath.empty())throw std::runtime_error("Agreement needs loading and menu sounds");agreement=std::make_unique<mgo2win::AgreementScreen>(policyUrl);}
 std::vector<Quad> agreementBackground;unsigned backgroundTextures=0;
 if(!agreementBackgroundPath.empty()){
  auto b=file(agreementBackgroundPath);if(b.size()<16||std::memcmp(b.data(),"M2PV",4)||u32(b,4)!=1||!u32(b,12)||u32(b,12)>64||u32(b,8)>=10000||b.size()!=16+size_t(u32(b,8))*sizeof(Quad))throw std::runtime_error("Agreement background extent");
  backgroundTextures=u32(b,12);agreementBackground.resize(u32(b,8));std::memcpy(agreementBackground.data(),b.data()+16,agreementBackground.size()*sizeof(Quad));
  for(const auto&q:agreementBackground){if(q.atlas< -1||q.atlas>=static_cast<int>(backgroundTextures)||q.blend<0||q.blend>1)throw std::runtime_error("Background draw range");for(const auto&v:q.vertices){float f[8];std::memcpy(f,&v,sizeof(v));for(float x:f)if(!std::isfinite(x))throw std::runtime_error("Background vertex");}}
 }
 std::vector<Quad> loginBackground;unsigned loginTextures=0;
 if(!loginPath.empty()){
  auto b=file(loginPath);if(b.size()<16||std::memcmp(b.data(),"M2PV",4)||u32(b,4)!=1||!u32(b,12)||u32(b,12)>64||u32(b,8)>=10000||b.size()!=16+size_t(u32(b,8))*sizeof(Quad))throw std::runtime_error("Agreement background extent");
  loginTextures=u32(b,12);loginBackground.resize(u32(b,8));std::memcpy(loginBackground.data(),b.data()+16,loginBackground.size()*sizeof(Quad));
  for(const auto&q:loginBackground){if(q.atlas< -1||q.atlas>=static_cast<int>(loginTextures)||q.blend<0||q.blend>1)throw std::runtime_error("Background draw range");for(const auto&v:q.vertices){float f[8];std::memcpy(f,&v,sizeof(v));for(float x:f)if(!std::isfinite(x))throw std::runtime_error("Background vertex");}}
 }
 std::unique_ptr<mgo2win::TitleAnimation> motionBack,motionFront;
 if(!motionPath.empty()){auto b=file(motionPath);motionBack=std::make_unique<mgo2win::TitleAnimation>(b);motionFront=std::make_unique<mgo2win::TitleAnimation>(b,18000,true);if(motionBack->texture_count()!=8)throw std::runtime_error("Background motion textures");}
 std::vector<Vertex>vertices;
 if(gcx&&!animation)throw std::runtime_error("GCX requires animated title asset");
 std::unique_ptr<mgo2win::TitleAnimation> loading;
 if(!loadingPath.empty()){
  if(!gcx)throw std::runtime_error("Loading requires GCX");
  loading=std::make_unique<mgo2win::TitleAnimation>(file(loadingPath));
  if(loading->texture_count()!=2)throw std::runtime_error("Loading textures");
 }
 if(animation)animation->set_callbacks([&](uint32_t result){std::osyncstream(std::cout)<<"{\"host_callback\":\"selected\",\"result\":"<<result<<",\"tick\":"<<animation->ticks()<<"}"<<std::endl;if(gcx)gcx->callback(false,result);},[&](uint32_t result){std::osyncstream(std::cout)<<"{\"host_callback\":\"completed\",\"result\":"<<result<<",\"tick\":"<<animation->ticks()<<"}"<<std::endl;if(gcx)gcx->callback(true,result);});
 for(const auto&q:quads){if(q.atlas< -1||q.atlas>=static_cast<int>(textureCount)||q.blend<0||q.blend>1)throw std::runtime_error("Preview draw range");for(const auto&v:q.vertices){float values[8];std::memcpy(values,&v,sizeof(v));for(float value:values)if(!std::isfinite(value))throw std::runtime_error("Nonfinite vertex");}for(int i:{0,1,2,0,2,3})vertices.push_back(q.vertices[i]);}
 if(vertices.empty())throw std::runtime_error("Empty scene");
 std::filesystem::path inputPath;if(scripted)inputPath=std::filesystem::path(argv[3]).parent_path()/L"input.cfg";else{wchar_t local[32768];DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);if(!n||n>=32768)throw std::runtime_error("Input settings directory unavailable");inputPath=std::filesystem::path(local)/L"MGO2WIN/input.cfg";}
 auto controllerInput=std::make_shared<mgo2win::ControllerInput>(inputPath);
 Window window;window.create();Window::input=controllerInput.get();if(animation)SetWindowTextW(window.handle,L"MGO2WIN - Title actor preview | Enter: START | Esc: close | No GCX boot");ComPtr<ID3D11Device>device;ComPtr<ID3D11DeviceContext>context;ComPtr<IDXGISwapChain>swap;
 if(gcx)SetWindowTextW(window.handle,L"MGO2WIN - Title | Enter: START | Esc: close");
 DXGI_SWAP_CHAIN_DESC sd{};sd.BufferDesc.Width=1280;sd.BufferDesc.Height=720;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=2;sd.OutputWindow=window.handle;sd.Windowed=TRUE;sd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;sd.Flags=DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
 D3D_FEATURE_LEVEL level{};HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&sd,&swap,&device,&level,&context);const bool warp=FAILED(hr);
 if(warp)check(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&sd,&swap,&device,&level,&context));
 struct FullscreenExit{IDXGISwapChain* s;~FullscreenExit(){s->SetFullscreenState(FALSE,nullptr);}} fullscreenExit{swap.Get()};
 ComPtr<IDXGIFactory> factory;if(SUCCEEDED(swap->GetParent(IID_PPV_ARGS(&factory))))factory->MakeWindowAssociation(window.handle,DXGI_MWA_NO_ALT_ENTER);
 ComPtr<ID3D11Texture2D>back;check(swap->GetBuffer(0,IID_PPV_ARGS(&back)));ComPtr<ID3D11RenderTargetView>rt;check(device->CreateRenderTargetView(back.Get(),nullptr,&rt));
 const char*shader=R"(struct V{float2 p:POSITION;float2 uv:TEXCOORD;float4 c:COLOR;};struct P{float4 p:SV_POSITION;float2 uv:TEXCOORD;float4 c:COLOR;};P vs(V v){P o;o.p=float4(v.p.x/640-1,1-v.p.y/360,0,1);o.uv=v.uv;o.c=v.c;return o;}Texture2D tex:register(t0);SamplerState smp:register(s0);float4 ps(P p):SV_TARGET{return tex.Sample(smp,p.uv)*p.c;})";
 ComPtr<ID3DBlob>vsb,psb,errors;check(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,"vs","vs_4_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&vsb,&errors));check(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,"ps","ps_4_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&psb,&errors));
 ComPtr<ID3D11VertexShader>vs;ComPtr<ID3D11PixelShader>ps;check(device->CreateVertexShader(vsb->GetBufferPointer(),vsb->GetBufferSize(),nullptr,&vs));check(device->CreatePixelShader(psb->GetBufferPointer(),psb->GetBufferSize(),nullptr,&ps));
 D3D11_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0}};
 ComPtr<ID3D11InputLayout>input;check(device->CreateInputLayout(elements,3,vsb->GetBufferPointer(),vsb->GetBufferSize(),&input));
 D3D11_BUFFER_DESC bd{};bd.ByteWidth=10000*6*sizeof(Vertex);bd.Usage=D3D11_USAGE_DYNAMIC;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;ComPtr<ID3D11Buffer>vb;check(device->CreateBuffer(&bd,nullptr,&vb));
 std::unique_ptr<mgo2win::CharacterCatalog> characterCatalog; mgo2win::PreparedCharacter prepared;
 std::optional<std::array<uint8_t,28>> preparedAppearance;uint32_t preparedId=0;ULONGLONG modelBegan=0;unsigned appearanceChanges=0;
 if(!catalogPath.empty()){characterCatalog=std::make_unique<mgo2win::CharacterCatalog>(file(catalogPath));std::osyncstream(std::cout)<<"{\"character_catalog_loaded\":true,\"meshes\":"<<characterCatalog->mesh_count()<<",\"motion_clip\":"<<characterCatalog->clip()<<"}"<<std::endl;}
 std::unique_ptr<mgo2win::CharacterRenderer> characterRenderer;unsigned modelFrames=0,emptyModelFrames=0;bool lastModelVisible=false;
 if(!modelPath.empty()){mgo2win::CharacterModel model(file(modelPath));characterRenderer=std::make_unique<mgo2win::CharacterRenderer>(device.Get(),model);std::osyncstream(std::cout)<<"{\"character_model_loaded\":true,\"vertices\":"<<model.vertices.size()<<",\"triangles\":"<<model.indices.size()/3<<",\"textures\":"<<model.textures.size()<<"}"<<std::endl;}
 auto titleTextures=textureCount;if(loading)textureCount+=loading->texture_count();auto backgroundOffset=textureCount;textureCount+=backgroundTextures;auto motionOffset=textureCount;if(motionBack)textureCount+=motionBack->texture_count();auto loginOffset=textureCount;textureCount+=loginTextures;
 for(auto&q:agreementBackground)if(q.atlas>=0)q.atlas+=backgroundOffset;
 for(auto&q:loginBackground)if(q.atlas>=0)q.atlas+=loginOffset;
 std::vector<ComPtr<ID3D11ShaderResourceView>>textures(textureCount+1);
 for(UINT i=0;i<=textureCount;i++){
  D3D11_TEXTURE2D_DESC td{};td.MipLevels=1;td.ArraySize=1;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{};uint32_t white=0xffffffff;std::vector<char>dds;
  if(i==textureCount){td.Width=1;td.Height=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;data.pSysMem=&white;data.SysMemPitch=4;}
  else{dds=file(i>=loginOffset?loginPath.parent_path()/L"images"/(std::to_wstring(i-loginOffset)+L".dds"):i>=motionOffset?motionPath.parent_path()/L"images"/(std::to_wstring(i-motionOffset)+L".dds"):i>=backgroundOffset?agreementBackgroundPath.parent_path()/L"images"/(std::to_wstring(i-backgroundOffset)+L".dds"):loading&&i>=titleTextures?loadingPath.parent_path()/L"images"/(std::to_wstring(i-titleTextures)+L".dds"):path.parent_path()/L"images"/(std::to_wstring(i)+L".dds"));if(dds.size()<128||std::memcmp(dds.data(),"DDS ",4)||u32(dds,4)!=124)throw std::runtime_error("DDS header");td.Height=u32(dds,12);td.Width=u32(dds,16);auto code=u32(dds,84);if(code!=0x31545844&&code!=0x35545844)throw std::runtime_error("DDS codec");td.Format=code==0x31545844?DXGI_FORMAT_BC1_UNORM:DXGI_FORMAT_BC3_UNORM;auto block=code==0x31545844?8u:16u;if(!td.Width||!td.Height||td.Width>8192||td.Height>8192||dds.size()!=128+size_t((td.Width+3)/4)*((td.Height+3)/4)*block)throw std::runtime_error("DDS extent");data.pSysMem=dds.data()+128;data.SysMemPitch=((td.Width+3)/4)*block;}
  ComPtr<ID3D11Texture2D>texture;check(device->CreateTexture2D(&td,&data,&texture));check(device->CreateShaderResourceView(texture.Get(),nullptr,&textures[i]));
 }
 D3D11_SAMPLER_DESC samp{};samp.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;samp.AddressU=samp.AddressV=samp.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;samp.MaxLOD=D3D11_FLOAT32_MAX;ComPtr<ID3D11SamplerState>sampler;check(device->CreateSamplerState(&samp,&sampler));
 ComPtr<ID3D11BlendState>blend[2];for(int i=0;i<2;i++){D3D11_BLEND_DESC b{};auto&r=b.RenderTarget[0];r.BlendEnable=TRUE;r.SrcBlend=D3D11_BLEND_SRC_ALPHA;r.DestBlend=i?D3D11_BLEND_ONE:D3D11_BLEND_INV_SRC_ALPHA;r.BlendOp=D3D11_BLEND_OP_ADD;r.SrcBlendAlpha=D3D11_BLEND_ONE;r.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;r.BlendOpAlpha=D3D11_BLEND_OP_ADD;r.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;check(device->CreateBlendState(&b,&blend[i]));}
 D3D11_RASTERIZER_DESC rs{};rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_NONE;rs.DepthClipEnable=TRUE;ComPtr<ID3D11RasterizerState>raster;check(device->CreateRasterizerState(&rs,&raster));
 context->IASetInputLayout(input.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);UINT stride=sizeof(Vertex),offset=0;ID3D11Buffer*vptr=vb.Get();context->IASetVertexBuffers(0,1,&vptr,&stride,&offset);context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);ID3D11SamplerState*sp=sampler.Get();context->PSSetSamplers(0,1,&sp);context->RSSetState(raster.Get());
 D3D11_VIEWPORT viewport{0,0,1280,720,0,1};context->RSSetViewports(1,&viewport);ID3D11RenderTargetView*rp=rt.Get();context->OMSetRenderTargets(1,&rp,nullptr);
 auto graphics=std::make_shared<mgo2win::GraphicsSettings>(inputPath.parent_path()/L"graphics.cfg");
 ComPtr<IDXGIOutput> output;std::vector<DXGI_MODE_DESC> displayModes;
 if(SUCCEEDED(swap->GetContainingOutput(&output))){UINT n=0;if(SUCCEEDED(output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM,0,&n,nullptr))&&n<=4096){displayModes.resize(n);if(SUCCEEDED(output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM,0,&n,displayModes.data()))){displayModes.resize(n);for(auto m:displayModes){mgo2win::GraphicsConfig test;test.width=m.Width;test.height=m.Height;test.refresh_num=m.RefreshRate.Numerator;test.refresh_den=m.RefreshRate.Denominator;if(mgo2win::valid_graphics(test)&&m.ScanlineOrdering!=DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST&&m.ScanlineOrdering!=DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST)graphics->modes.push_back({m.Width,m.Height,m.RefreshRate.Numerator,m.RefreshRate.Denominator});}}}}
 std::osyncstream(std::cout)<<"{\"graphics_display_modes\":"<<graphics->modes.size()<<"}"<<std::endl;
 graphics->apply=[&](const mgo2win::GraphicsConfig& cfg){
  DXGI_MODE_DESC target{};target.Width=cfg.width;target.Height=cfg.height;target.Format=DXGI_FORMAT_R8G8B8A8_UNORM;target.RefreshRate={cfg.refresh_num,cfg.refresh_den};
  if(cfg.fullscreen){if(!output||warp)return false;DXGI_MODE_DESC found{};if(FAILED(output->FindClosestMatchingMode(&target,&found,device.Get()))||found.Width!=cfg.width||found.Height!=cfg.height)return false;if(cfg.refresh_num&&uint64_t(found.RefreshRate.Numerator)*cfg.refresh_den!=uint64_t(cfg.refresh_num)*found.RefreshRate.Denominator)return false;target=found;}
  context->OMSetRenderTargets(0,nullptr,nullptr);rt.Reset();back.Reset();context->Flush();
  HRESULT changed=swap->SetFullscreenState(cfg.fullscreen?TRUE:FALSE,cfg.fullscreen?output.Get():nullptr);
  if(changed!=S_OK)return false;
  if(cfg.fullscreen){if(swap->ResizeTarget(&target)!=S_OK)return false;}
  else{RECT bounds{0,0,LONG(cfg.width),LONG(cfg.height)};AdjustWindowRect(&bounds,WS_OVERLAPPEDWINDOW,FALSE);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(window.handle,MONITOR_DEFAULTTONEAREST),&mi);int w=bounds.right-bounds.left,h=bounds.bottom-bounds.top;double scale=std::min({1.0,double(mi.rcWork.right-mi.rcWork.left)/w,double(mi.rcWork.bottom-mi.rcWork.top)/h});w=int(w*scale);h=int(h*scale);SetWindowPos(window.handle,nullptr,mi.rcWork.left+(mi.rcWork.right-mi.rcWork.left-w)/2,mi.rcWork.top+(mi.rcWork.bottom-mi.rcWork.top-h)/2,w,h,SWP_NOZORDER|SWP_NOACTIVATE);}
  if(FAILED(swap->ResizeBuffers(2,cfg.width,cfg.height,DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH)))return false;
  if(FAILED(swap->GetBuffer(0,IID_PPV_ARGS(&back)))||FAILED(device->CreateRenderTargetView(back.Get(),nullptr,&rt)))return false;
  float scale=std::min(float(cfg.width)/1280,float(cfg.height)/720);viewport={ (cfg.width-1280*scale)/2,(cfg.height-720*scale)/2,1280*scale,720*scale,0,1};context->RSSetViewports(1,&viewport);rp=rt.Get();context->OMSetRenderTargets(1,&rp,nullptr);
  Window::viewX=viewport.TopLeftX/cfg.width;Window::viewY=viewport.TopLeftY/cfg.height;Window::viewW=viewport.Width/cfg.width;Window::viewH=viewport.Height/cfg.height;
  BOOL actualFull=FALSE;if(FAILED(swap->GetFullscreenState(&actualFull,nullptr))||bool(actualFull)!=bool(cfg.fullscreen))return false;
  D3D11_TEXTURE2D_DESC actualBuffer{};back->GetDesc(&actualBuffer);
  std::osyncstream(std::cout)<<"{\"graphics_renderer\":true,\"fullscreen\":"<<(actualFull?"true":"false")<<",\"buffer_width\":"<<actualBuffer.Width<<",\"buffer_height\":"<<actualBuffer.Height<<",\"target_refresh_num\":"<<(cfg.fullscreen?target.RefreshRate.Numerator:0)<<",\"target_refresh_den\":"<<(cfg.fullscreen?target.RefreshRate.Denominator:1)<<"}"<<std::endl;
  return true;
 };
 if(safeGraphics)graphics->draft=graphics->active;
 if(!scripted&&!safeGraphics&&graphics->draft!=graphics->active){if(graphics->apply(graphics->draft)){graphics->active=graphics->draft;std::osyncstream(std::cout)<<"{\"graphics_restored\":true}"<<std::endl;}else{check(graphics->apply(graphics->active)?S_OK:E_FAIL);graphics->draft=graphics->active;}}
 AudioThread audio;if(sound&&(!gcx||gcx->bgm_requested()))audio.start(wavPath.empty()?path.parent_path().parent_path()/L"audio/bgm_mgo_title01.wav":wavPath,argv[2],23);
 AudioThread effect,lobbyAudio;bool seStarted=false,lobbyMusicStarted=false;
 if(gcx&&sound&&!sePath.empty())gcx->set_se_handler([&](uint32_t cue){
  if(seStarted)throw std::runtime_error("Duplicate START sound");seStarted=true;
  std::osyncstream(std::cout)<<"{\"start_sound\":true,\"cue\":"<<cue<<",\"tick\":"<<animation->ticks()<<"}"<<std::endl;
  effect.start(sePath,L"600",18999);
 });
 std::vector<std::unique_ptr<AudioThread>> menuAudio;unsigned menuFailures=0;
 std::unique_ptr<AudioThread> voiceAudio;
 auto menuSound=[&](unsigned cue){if(!sound)return;for(auto it=menuAudio.begin();it!=menuAudio.end();){if((*it)->result.load()!=-1){if((*it)->result.load())++menuFailures;it=menuAudio.erase(it);}else ++it;}
  if(menuAudio.size()>=4)return;auto a=std::make_unique<AudioThread>();a->start(cue==93?menuConfirmPath:menuMovePath,L"10",cue);menuAudio.push_back(std::move(a));
  std::osyncstream(std::cout)<<"{\"menu_sound_requested\":"<<cue<<"}"<<std::endl;
 };
 ComPtr<ID3D11Texture2D> agreementTexture;ComPtr<ID3D11ShaderResourceView> agreementView;
 if(agreement){D3D11_TEXTURE2D_DESC td{};td.Width=1280;td.Height=720;td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;check(device->CreateTexture2D(&td,nullptr,&agreementTexture));check(device->CreateShaderResourceView(agreementTexture.Get(),nullptr,&agreementView));}
 bool agreementStarted=false,agreementVisible=false,agreementCaptured=false,agreementScrolledCaptured=false,agreementChoiceCaptured=false;
 std::unique_ptr<mgo2win::LoginScreen> login;unsigned loginFrames=0,loginVisits=0,returnFrames=0;
 bool portTitle=false,charTitle=false;ULONGLONG slotBegan=0;unsigned slotStep=0,slotCaptured=0;bool modelRotated=false;
 struct LoginReset{~LoginReset(){Window::login=nullptr;}} loginReset;
 unsigned agreementFrames=0;ULONGLONG closeAt=0;
 mgo2win::AudioFade fade;
 if(gcx&&sound)gcx->set_fade_handler([&](int duration){fade.stop(duration);audio.control.gain=fade.gain();std::osyncstream(std::cout)<<"{\"bgm_fade\":true,\"tick\":"<<animation->ticks()<<",\"argument\":"<<duration<<",\"remaining_frames\":"<<fade.remaining()<<",\"gain\":"<<fade.gain()<<"}"<<std::endl;});
 auto began=std::chrono::steady_clock::now();auto deadline=GetTickCount64()+static_cast<ULONGLONG>(seconds*1000);bool running=true,loadingVisible=false,loadingCaptured=false,loadingReady=false;unsigned frames=0,occluded=0,lastState=0,captureIndex=0,loadingFrames=0;const uint32_t captureTicks[]={5,300,625,800,1000,1200};
 while(running&&GetTickCount64()<deadline){MSG msg{};while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)running=false;TranslateMessage(&msg);DispatchMessage(&msg);}if(!running)break;
  graphics->tick(GetTickCount64(),GetForegroundWindow()==window.handle);
  bool inputActive=!scripted&&GetForegroundWindow()==window.handle;
  auto pad=controllerInput->poll(inputActive,login?login->input_slot():controllerInput->config.slot);
  if(inputActive&&!(login&&login->controller_sample(pad))){auto actions=controllerInput->actions(pad);for(unsigned a=0;a<mgo2win::input_actions;++a)if(actions[a]){auto k=mgo2win::menu_key(a);if(k)Window::proc(window.handle,WM_KEYDOWN,k,1LL<<25);}}
  if(closeAt&&GetTickCount64()>=closeAt)break;
  if(animation){auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();auto target=static_cast<uint32_t>(elapsed*300000.0/1001.0);
   while(animation->ticks()+5<=target&&animation->state()!=4){fade.advance(1);audio.control.gain=fade.gain();if(scripted&&(animation->ticks()+5==100||animation->ticks()+5==900))SendMessageW(window.handle,WM_KEYDOWN,VK_RETURN,0);uint32_t input=Window::pressed;Window::pressed=0;animation->tick(5,input);
    if(animation->state()!=lastState){lastState=animation->state();std::osyncstream(std::cout)<<"{\"actor_state\":"<<lastState<<",\"tick\":"<<animation->ticks()<<"}"<<std::endl;}
   }
   if(animation->state()==4&&loading&&gcx->loading_requested()){
    if(!loadingVisible){loadingVisible=true;std::osyncstream(std::cout)<<"{\"loading_entered\":true,\"layout\":\"loading_MGO\",\"event\":\"0x9ca2fa\"}"<<std::endl;SetWindowTextW(window.handle,L"MGO2WIN - Loading | Esc: close");}
    loading->tick(5,0);fade.advance(1);audio.control.gain=fade.gain();
    quads=loading->geometry();for(auto&q:quads)if(q.atlas>=0)q.atlas+=titleTextures;
    if(loadingFrames>=3&&!loadingReady){gcx->loading_ready();loadingReady=true;if(agreement){agreement->start();agreementStarted=true;if(sound&&!lobbyMusicPath.empty()){lobbyAudio.start(lobbyMusicPath,argv[2],23);lobbyMusicStarted=true;}}}
   }else quads=animation->geometry();
   count=static_cast<uint32_t>(quads.size());vertices.clear();for(const auto&q:quads)for(int i:{0,1,2,0,2,3})vertices.push_back(q.vertices[i]);
  }
  if(agreementStarted&&(agreementVisible||(agreement->ready()&&loadingFrames>=30))){
   agreement->report();if(!agreementVisible){agreementVisible=true;Window::agreementInput=0;SetWindowTextW(window.handle,L"OpenMGO2 - Agreement | Enter: select | Esc: close");menuSound(93);std::osyncstream(std::cout)<<"{\"agreement_visible\":true}"<<std::endl;}
   auto input=Window::agreementInput;Window::agreementInput=0;
   if(scripted){if(agreementFrames==60)input=mgo2win::AgreementScreen::pageDown;if(agreementFrames==90)input=mgo2win::AgreementScreen::home;if(agreementFrames==100&&!scriptedNo)input=mgo2win::AgreementScreen::left;if(agreementFrames==140)input=mgo2win::AgreementScreen::confirm;}
   if(scriptedLogin&&!login&&loginVisits==1){++returnFrames;if(returnFrames==30)input=mgo2win::AgreementScreen::left;if(returnFrames==60)input=mgo2win::AgreementScreen::confirm;}
   bool close=false;int cue=closeAt||login?-1:agreement->input(input,close);if(cue>=0)menuSound(static_cast<unsigned>(cue));if(close)closeAt=GetTickCount64()+1250;
   if(!login&&agreement->accepted()&&!loginPath.empty()){
    std::filesystem::path store;
    if(scripted)store=std::filesystem::path(argv[3]).parent_path()/L"login-test.dat";
    else{wchar_t local[32768];DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);if(!n||n>=32768)throw std::runtime_error("Login settings directory unavailable");store=std::filesystem::path(local)/L"MGO2WIN/login.dat";}
    login=std::make_unique<mgo2win::LoginScreen>(store,!scripted,mgo2win::authenticate,true,controllerInput,graphics,networkKeys);Window::login=login.get();loginFrames=0;++loginVisits;
    login->character_catalog(characterCatalog.get());
    if(scriptedSlots||scriptedCreation)login->show_character_preview(true);
    if(scriptedCharacters||scriptedAppearance)login->show_character_preview();
    if(scriptedPorts)login->show_port_preview();
    if(scriptedControls||scriptedGraphics)login->show_port_preview();
    if(scriptedStun)login->show_port_preview(true);
    SetWindowTextW(window.handle,L"OpenMGO2 - Login | Tab: select | Esc: back");
    std::osyncstream(std::cout)<<"{\"login_visible\":true,\"authentication_started\":false}"<<std::endl;
   }
   if(login){
    if(scriptedSlots){
     if(!slotBegan)slotBegan=GetTickCount64();auto elapsed=GetTickCount64()-slotBegan;
     auto key=[&](WPARAM k){SendMessageW(window.handle,WM_KEYDOWN,k,0);};auto release=[&]{SendMessageW(window.handle,WM_KEYUP,VK_BACK,0);};
     unsigned previous=slotStep;
     if(!modelRotated&&elapsed>=4100){for(int turn=0;turn<5;++turn)key(VK_RIGHT);modelRotated=true;std::osyncstream(std::cout)<<"{\"character_model_rotation_test\":true,\"yaw\":"<<login->model_yaw()<<"}"<<std::endl;}
     if(slotStep==0&&elapsed>=1500){key(VK_DOWN);++slotStep;}
     else if(slotStep==1&&elapsed>=2200){key(VK_UP);++slotStep;}
     else if(slotStep==2&&elapsed>=2500){key(VK_BACK);++slotStep;}
     else if(slotStep==3&&elapsed>=4000){release();++slotStep;}
     else if(slotStep==4&&elapsed>=4300){key(VK_BACK);++slotStep;}
     else if(slotStep==5&&elapsed>=7500){release();++slotStep;}
     else if(slotStep==6&&elapsed>=8500){key(VK_RETURN);++slotStep;}
     else if(slotStep==7&&elapsed>=8800){key(VK_BACK);++slotStep;}
     else if(slotStep==8&&elapsed>=12000){release();++slotStep;}
     else if(slotStep==9&&elapsed>=12700){key(VK_LEFT);key(VK_RETURN);++slotStep;}
     else if(slotStep==10&&elapsed>=13300){for(int i=0;i<4;++i)key(VK_DOWN);key(VK_RETURN);++slotStep;}
     else if(slotStep==11&&elapsed>=14100){key(VK_ESCAPE);++slotStep;}
     if(previous!=slotStep){login->report();std::osyncstream(std::cout)<<"{\"slot_script_step\":"<<slotStep<<",\"elapsed_ms\":"<<elapsed<<"}"<<std::endl;}
    }
    if(scriptedCreation){
     auto key=[&](WPARAM k){SendMessageW(window.handle,WM_KEYDOWN,k,0);};
     auto chars=[&](const wchar_t*s){for(;*s;++s)SendMessageW(window.handle,WM_CHAR,*s,0);};
     if(loginFrames==110){key(VK_DOWN);key(VK_RETURN);}
     if(loginFrames==140)chars(L"試作兵士");
     if(loginFrames==155){key(VK_DELETE);chars(L"한글이름");}
     if(loginFrames==160){key(VK_DOWN);key(VK_RIGHT);key(VK_DOWN);key(VK_RIGHT);key(VK_DOWN);key(VK_RIGHT);}
     if(loginFrames==170){key(VK_DOWN);for(int i=0;i<10;++i)key(VK_LEFT);}
     if(loginFrames==182){for(int i=0;i<20;++i)key(VK_RIGHT);}
     if(loginFrames==200)key(VK_F4);
     if(loginFrames==190){key(VK_F2);key(VK_DOWN);key(VK_RIGHT);key(VK_SPACE);key(VK_RIGHT);}
     if(loginFrames==230){key(VK_F3);key(VK_DOWN);key(VK_DOWN);key(VK_RIGHT);}
     if(loginFrames==270){key(VK_END);key(VK_UP);key(VK_RETURN);}
     if(loginFrames==310)key(VK_RETURN);
     if(loginFrames==340)key(VK_ESCAPE);
     if(loginFrames==365)key(VK_RETURN); // Default NO retains the draft.
     if(loginFrames==385){key(VK_ESCAPE);key(VK_LEFT);key(VK_RETURN);}
     if(loginFrames==410)key(VK_ESCAPE);
    }
    if(scriptedAppearance){auto key=[&](WPARAM k){SendMessageW(window.handle,WM_KEYDOWN,k,0);};if(loginFrames==150||loginFrames==240||loginFrames==330)key(VK_DOWN);if(loginFrames==430)key(VK_ESCAPE);}
    if(scriptedCharacters){auto key=[&](WPARAM k){SendMessageW(window.handle,WM_KEYDOWN,k,0);};if(loginFrames==110){key(VK_DOWN);key(VK_DOWN);}if(loginFrames==145)key(VK_END);if(loginFrames==160)key(VK_UP);if(loginFrames==230)key(VK_RETURN);if(loginFrames==350)key(VK_ESCAPE);}
    if(scriptedGraphics){
     auto key=[&](WPARAM k){SendMessageW(window.handle,WM_KEYDOWN,k,0);};
     auto click=[&](int x,int y){RECT r{};GetClientRect(window.handle,&r);SendMessageW(window.handle,WM_LBUTTONUP,0,MAKELPARAM(int((Window::viewX+float(x)/1280*Window::viewW)*r.right),int((Window::viewY+float(y)/720*Window::viewH)*r.bottom)));};
     if(loginFrames==10)key(VK_F3);
     if(loginFrames==20){key(VK_DOWN);key(VK_DOWN);key(VK_DOWN);key(VK_RIGHT);graphics->draft.width=1600;graphics->draft.height=900;}
     if(loginFrames==30)click(250,620);
     if(loginFrames==50)key(VK_RETURN);
     if(loginFrames==70){graphics->draft.fullscreen=1;if(!graphics->modes.empty()){auto m=graphics->modes.front();for(auto candidate:graphics->modes)if(candidate.width==1280&&candidate.height==720){m=candidate;break;}graphics->draft.width=m.width;graphics->draft.height=m.height;graphics->draft.refresh_num=m.num;graphics->draft.refresh_den=m.den;}click(250,620);}
     if(loginFrames==100)key(VK_ESCAPE);
     if(loginFrames==120){graphics->draft.width=1280;graphics->draft.height=720;click(250,620);}
    }
    if(scriptedControls){
     auto key=[&](WPARAM k){SendMessageW(window.handle,WM_KEYDOWN,k,0);};
     auto click=[&](int x,int y){RECT r{};GetClientRect(window.handle,&r);SendMessageW(window.handle,WM_LBUTTONUP,0,MAKELPARAM(x*r.right/1280,y*r.bottom/720));};
     if(loginFrames==10)key(VK_F2);
     if(loginFrames==20)click(800,440);
     if(loginFrames==25)key('F');
     if(loginFrames==30)click(220,620);
     if(loginFrames==40)click(520,235);
     if(loginFrames==50)click(800,440);
     if(loginFrames==55)login->controller_sample({true,0,0});
     if(loginFrames==56)login->controller_sample({true,1u<<5,1u<<5});
     if(loginFrames==65)click(220,620);
     if(loginFrames==75)key(VK_NEXT);
     if(loginFrames==85)key(VK_NEXT);
     if(loginFrames==95)key(VK_F1);
     if(loginFrames==105)key(VK_F2);
     if(loginFrames==115){login->show_port_preview();key(VK_F2);}
     if(loginFrames==130){auto actions=controllerInput->actions({true,1u<<8,1u<<8});for(unsigned a=0;a<mgo2win::input_actions;++a)if(actions[a]){auto k=mgo2win::menu_key(a);if(k)Window::proc(window.handle,WM_KEYDOWN,k,1LL<<25);}std::osyncstream(std::cout)<<"{\"controller_scripted_pad_tab\":true,\"physical_device_test\":false}"<<std::endl;}
     if(loginFrames==140)key(VK_F2);
    }
    if(scriptedStun&&loginFrames==20)SendMessageW(window.handle,WM_KEYDOWN,VK_RETURN,0);
    if(scriptedPorts){
     auto key=[&](WPARAM k){SendMessageW(window.handle,WM_KEYDOWN,k,0);};
     auto chars=[&](const wchar_t*s){for(;*s;++s)SendMessageW(window.handle,WM_CHAR,*s,0);};
     if(loginFrames==10){for(int i=0;i<3;++i)key(VK_UP);key(VK_RIGHT);for(int i=0;i<3;++i)key(VK_DOWN);}
     if(loginFrames==20)key(VK_RETURN);
     if(loginFrames==35){key(VK_DOWN);key(VK_RETURN);}
     if(loginFrames==50){for(int i=0;i<3;++i)key(VK_UP);key(VK_DELETE);chars(L"80");key(VK_RETURN);key(VK_RETURN);}
     if(loginFrames==75){key(VK_DELETE);chars(L"5730");key(VK_RETURN);key(VK_RETURN);}
     if(loginFrames==80){key(VK_UP);key(VK_RETURN);key(VK_END);}
     if(loginFrames==90){key(VK_RETURN);key(VK_DOWN);}
     if(loginFrames==100){key(VK_DOWN);key(VK_RETURN);}
     if(loginFrames==130)key(VK_ESCAPE);
     if(loginFrames==150)login->show_port_preview();
     if(loginFrames==170)key(VK_RETURN);
    }
    if(scriptedLogin&&loginVisits==1){
     auto key=[&](WPARAM k){SendMessageW(window.handle,WM_KEYDOWN,k,0);};
     auto chars=[&](const wchar_t*s){for(;*s;++s)SendMessageW(window.handle,WM_CHAR,*s,0);};
     if(loginFrames==20){key(VK_DOWN);key(VK_DOWN);key(VK_RETURN);} // Empty form.
     if(loginFrames==50){chars(L"preview_user");key(VK_TAB);chars(L"local-test-only");key(VK_TAB);}
     if(loginFrames==75){for(int i=0;i<4;++i)key(VK_TAB);key(VK_RETURN);for(int i=0;i<3;++i)key(VK_TAB);}
     if(loginFrames==100)key(VK_RETURN); // Local unavailable notice, no request.
     if(loginFrames==180)key(VK_ESCAPE);
    }
    for(auto c:login->cues())menuSound(c);
    if(voiceAudio&&(!login->creation_visible()||(!scripted&&GetForegroundWindow()!=window.handle)||voiceAudio->result.load()!=-1)){
     if(voiceAudio->result.load()>0)++menuFailures;voiceAudio.reset();
    }
    if(auto request=login->take_audition();request&&sound&&!voiceDirectory.empty()&&(scripted||GetForegroundWindow()==window.handle)){
     if(request->gender>1||request->voice>7)throw std::runtime_error("Character voice range");
     voiceAudio.reset();voiceAudio=std::make_unique<AudioThread>();voiceAudio->control.frequencyRatio=mgo2win::character_voice_ratio(request->pitch);
     auto name=std::to_wstring(request->gender)+L"_"+std::to_wstring(request->voice)+L".gwa";
     voiceAudio->start(voiceDirectory/name,L"10",0,"character_voice");
     std::osyncstream(std::cout)<<"{\"voice_audition_started\":true,\"gender\":"<<request->gender<<",\"voice\":"<<request->voice<<",\"pitch\":"<<request->pitch<<"}"<<std::endl;
    }
    if(login->back()){login->report();Window::login=nullptr;login.reset();agreement->return_from_login();Window::agreementInput=0;
     SetWindowTextW(window.handle,L"OpenMGO2 - Agreement | Enter: select | Esc: close");
    }
   }
   if(characterCatalog){
    auto appearance=login?login->preview_appearance():std::nullopt;auto id=login?login->preview_character_id():0;
    if(appearance!=preparedAppearance||id!=preparedId){preparedAppearance=appearance;preparedId=id;characterRenderer.reset();prepared={};
     if(appearance){prepared=characterCatalog->assemble(*appearance);if(prepared.ready())characterRenderer=std::make_unique<mgo2win::CharacterRenderer>(device.Get(),prepared.model);modelBegan=GetTickCount64();++appearanceChanges;
      std::osyncstream(std::cout)<<"{\"appearance_assembled\":true,\"parts\":"<<prepared.selectedParts<<",\"missing_models\":"<<prepared.missingModels<<",\"missing_colors\":"<<prepared.missingColors<<",\"defaulted_lower\":"<<(prepared.defaultedLower?"true":"false")<<"}"<<std::endl;
      for(const auto&problem:prepared.issues)std::osyncstream(std::cout)<<"{\"appearance_issue\":true,\"gender\":"<<prepared.gender<<",\"slot\":"<<problem.slot<<",\"item\":"<<problem.id<<",\"color\":"<<problem.color<<",\"texture\":"<<problem.texture<<",\"reason\":\""<<problem.reason<<"\"}"<<std::endl;}
    }
   }
   if(login){login->model_available(bool(characterRenderer));login->model_partial(prepared.missingModels||prepared.missingColors);}
   context->UpdateSubresource(agreementTexture.Get(),0,nullptr,login?login->draw():agreement->draw(),1280*4,0);
   bool portNow=login&&login->port_visible();if(portNow!=portTitle){portTitle=portNow;SetWindowTextW(window.handle,portNow?L"OpenMGO2 - Port settings | Esc: back":L"OpenMGO2 - Login | Enter: select | Esc: back");}
   bool charNow=login&&login->character_visible();if(charNow!=charTitle){charTitle=charNow;SetWindowTextW(window.handle,charNow?L"OpenMGO2 - Characters | Esc: settings":L"OpenMGO2 - Settings | Esc: back");}
   quads.clear();
   auto appendMotion=[&](mgo2win::TitleAnimation& m){m.tick(5,0);auto v=m.geometry();for(auto&q:v)if(q.atlas>=0)q.atlas+=motionOffset;quads.insert(quads.end(),v.begin(),v.end());};
   if(motionBack)appendMotion(*motionBack);
   const auto&frame=login&&!login->port_visible()?loginBackground:agreementBackground;quads.insert(quads.end(),frame.begin(),frame.end());
   if(motionFront)appendMotion(*motionFront);
   if(characterRenderer&&login&&login->model_preview_visible()){
    // A translucent black panel separates the character from the animated backdrop.
    quads.emplace_back();auto& panel=quads.back();panel.atlas=-1;panel.blend=0;
    const float panelXY[4][2]={{720,196},{1160,196},{1160,588},{720,588}};
    for(int i=0;i<4;++i){float values[]={panelXY[i][0],panelXY[i][1],0,0,0,0,0,.38f};std::memcpy(&panel.vertices[i],values,sizeof(Vertex));}
    quads.emplace_back();auto& model=quads.back();model.atlas=-3;model.blend=0;
    // 140% of the former 440x280 preview; preserve its camera/aspect ratio.
    const float xy[4][2]={{632,196},{1248,196},{1248,588},{632,588}},uv[4][2]={{0,0},{1,0},{1,1},{0,1}};
    for(int i=0;i<4;++i){float values[]={xy[i][0],xy[i][1],uv[i][0],uv[i][1],1,1,1,1};std::memcpy(&model.vertices[i],values,sizeof(Vertex));}
   }
   quads.emplace_back();auto& q=quads.back();q.atlas=-2;q.blend=0;
   const float xy[4][2]={{0,0},{1280,0},{1280,720},{0,720}};const float uv[4][2]={{0,0},{1,0},{1,1},{0,1}};
   for(int i=0;i<4;++i){float values[]={xy[i][0],xy[i][1],uv[i][0],uv[i][1],1,1,1,1};std::memcpy(&q.vertices[i],values,sizeof(Vertex));}
   vertices.clear();for(const auto& draw:quads)for(int i:{0,1,2,0,2,3})vertices.push_back(draw.vertices[i]);count=static_cast<unsigned>(quads.size());
  }
  D3D11_MAPPED_SUBRESOURCE mapped{};check(context->Map(vb.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped));if(!vertices.empty())std::memcpy(mapped.pData,vertices.data(),vertices.size()*sizeof(Vertex));context->Unmap(vb.Get(),0);
  bool modelVisible=characterRenderer&&agreementVisible&&login&&login->model_preview_visible();
  if(modelVisible){if(characterCatalog){characterCatalog->pose(prepared,(GetTickCount64()-modelBegan)/1000.);characterRenderer->update_vertices(context.Get(),prepared.model.vertices);}characterRenderer->render(context.Get(),login->model_yaw());++modelFrames;login->model_rendered();
   context->IASetInputLayout(input.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->IASetVertexBuffers(0,1,&vptr,&stride,&offset);context->IASetIndexBuffer(nullptr,DXGI_FORMAT_R32_UINT,0);context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);context->PSSetSamplers(0,1,&sp);context->RSSetState(raster.Get());context->RSSetViewports(1,&viewport);context->OMSetRenderTargets(1,&rp,nullptr);context->OMSetDepthStencilState(nullptr,0);
  }else if(login&&login->character_visible())++emptyModelFrames;
  if(modelVisible!=lastModelVisible){lastModelVisible=modelVisible;std::osyncstream(std::cout)<<"{\"character_model_visible\":"<<(modelVisible?"true":"false")<<"}"<<std::endl;}
  const float clear[4]={0,0,0,1};context->ClearRenderTargetView(rt.Get(),clear);
  for(UINT i=0;i<count;i++){const auto&q=quads[i];ID3D11ShaderResourceView*tex=q.atlas==-3?characterRenderer->view():q.atlas==-2?agreementView.Get():textures[q.atlas<0?textureCount:q.atlas].Get();context->PSSetShaderResources(0,1,&tex);context->OMSetBlendState(blend[q.blend].Get(),nullptr,0xffffffff);context->Draw(6,i*6);}
  if(saveCaptures&&frames==0)capture(device.Get(),context.Get(),back.Get(),argv[3]);
  if(saveCaptures&&animation&&captureIndex<6&&animation->ticks()>=captureTicks[captureIndex]){auto out=std::filesystem::path(argv[3]);out.replace_filename(out.stem().wstring()+L"_"+std::to_wstring(captureTicks[captureIndex])+L".bmp");capture(device.Get(),context.Get(),back.Get(),out);std::osyncstream(std::cout)<<"{\"capture_requested_tick\":"<<captureTicks[captureIndex]<<",\"capture_actual_tick\":"<<animation->ticks()<<"}"<<std::endl;++captureIndex;}
  if(saveCaptures&&loadingVisible&&loadingFrames>=3&&!loadingCaptured){auto out=std::filesystem::path(argv[3]);out.replace_filename(L"loading.bmp");capture(device.Get(),context.Get(),back.Get(),out);loadingCaptured=true;}
  if(saveCaptures&&agreementVisible){
   auto shot=[&](const wchar_t* name){auto p=std::filesystem::path(argv[3]);p.replace_filename(name);capture(device.Get(),context.Get(),back.Get(),p);};
   if(motionBack&&(agreementFrames==35||agreementFrames==95))shot(agreementFrames==35?L"agreement_motion_35.bmp":L"agreement_motion_95.bmp");
   if(!agreementCaptured){shot(L"agreement.bmp");agreementCaptured=true;}
   if(scripted&&agreementFrames==61&&!agreementScrolledCaptured){shot(L"agreement_scrolled.bmp");agreementScrolledCaptured=true;}
   if(agreement->accepted()&&!agreementChoiceCaptured){shot(L"agreement_yes.bmp");agreementChoiceCaptured=true;}
  }
  if(saveCaptures&&login&&(loginFrames==0||loginFrames==21||loginFrames==51||loginFrames==101))capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"login_"+std::to_wstring(loginVisits)+L"_"+std::to_wstring(loginFrames)+L".bmp"));
  if(saveCaptures&&scriptedSlots&&slotStep!=slotCaptured&&(slotStep==1||slotStep==2||slotStep==4||slotStep==6||slotStep==10||slotStep==11||slotStep==12)){capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"slots_"+std::to_wstring(slotStep)+L".bmp"));slotCaptured=slotStep;}
  if(saveCaptures&&scriptedCreation&&login&&(loginFrames==120||loginFrames==150||loginFrames==180||loginFrames==210||loginFrames==250||loginFrames==290||loginFrames==350||loginFrames==375||loginFrames==400))capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"creation_"+std::to_wstring(loginFrames)+L".bmp"));
  if(saveCaptures&&scriptedAppearance&&login&&(loginFrames==90||loginFrames==120||loginFrames==180||loginFrames==270||loginFrames==360||loginFrames==440))capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"appearance_"+std::to_wstring(loginFrames)+L".bmp"));
  if(saveCaptures&&scriptedCharacters&&login&&(loginFrames==10||loginFrames==100||loginFrames==120||loginFrames==240||loginFrames==320||loginFrames==360))capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"characters_"+std::to_wstring(loginFrames)+L".bmp"));
  if(saveCaptures&&scriptedPorts&&login&&(loginFrames==21||loginFrames==36||loginFrames==51||loginFrames==81||loginFrames==101||loginFrames==131||loginFrames==151))capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"ports_"+std::to_wstring(loginFrames)+L".bmp"));
  if(saveCaptures&&scriptedStun&&login&&(loginFrames==21||loginFrames==220))capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"stun_"+std::to_wstring(loginFrames)+L".bmp"));
  if(saveCaptures&&scriptedControls&&login&&(loginFrames==11||loginFrames==21||loginFrames==31||loginFrames==51||loginFrames==66||loginFrames==76||loginFrames==86||loginFrames==116||loginFrames==131))capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"controls_"+std::to_wstring(loginFrames)+L".bmp"));
  if(saveCaptures&&scriptedGraphics&&login&&(loginFrames==11||loginFrames==35||loginFrames==51||loginFrames==85||loginFrames==105||loginFrames==125||loginFrames==1100))capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"graphics_"+std::to_wstring(loginFrames)+L".bmp"));
  if(saveCaptures&&scriptedLogin&&!login&&loginVisits==1&&loginFrames==180){capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/L"login_back.bmp");++loginFrames;}
  if(login)++loginFrames;
  if(agreementVisible)++agreementFrames;
  const auto presented=swap->Present(graphics->active.vsync?1:0,0);check(presented);if(presented==DXGI_STATUS_OCCLUDED)++occluded;++frames;if(loadingVisible&&!agreementVisible)++loadingFrames;if(animation&&animation->state()==4&&!loadingVisible)running=false;Sleep(1);
 }
 // Preview shutdown policy: allow the final GCX fade request to finish within
 // the existing duration bound. This does not advance an unimplemented actor.
 unsigned drained=0;
 if(animation&&animation->state()==4&&sound)while(fade.remaining()&&GetTickCount64()<deadline){Sleep(17);fade.advance(1);audio.control.gain=fade.gain();++drained;}
 if(drained)std::osyncstream(std::cout)<<"{\"preview_audio_drain_frames\":"<<drained<<",\"gain\":"<<fade.gain()<<"}"<<std::endl;
 for(auto& a:menuAudio)a->stop=true;for(auto& a:menuAudio){if(a->thread.joinable())a->thread.join();if(a->result.load())++menuFailures;}
 if(motionBack)std::osyncstream(std::cout)<<"{\"background_motion_ticks\":"<<motionBack->ticks()<<",\"background_loop_restarts\":"<<(motionBack->loop_restarts()+motionFront->loop_restarts())<<"}"<<std::endl;
 std::osyncstream(std::cout)<<"{\"character_model_frames\":"<<modelFrames<<",\"character_frames_without_model\":"<<emptyModelFrames<<",\"account_appearance_applied\":"<<(characterCatalog&&appearanceChanges?"true":"false")<<",\"appearance_changes\":"<<appearanceChanges<<",\"motion_playing\":"<<(characterCatalog&&modelFrames?"true":"false")<<"}"<<std::endl;
 if(login)login->report();
 std::osyncstream(std::cout)<<"{\"login_visits\":"<<loginVisits<<",\"login_frames\":"<<loginFrames<<",\"original_login_quads\":"<<loginBackground.size()<<"}"<<std::endl;
 if(agreement)std::osyncstream(std::cout)<<"{\"agreement_frames\":"<<agreementFrames<<",\"agreement_accepted\":"<<(agreement->accepted()?"true":"false")<<",\"menu_audio_failures\":"<<menuFailures<<"}"<<std::endl;
 lobbyAudio.stop=true;if(lobbyAudio.thread.joinable())lobbyAudio.thread.join();
 std::osyncstream(std::cout)<<"{\"lobby_music_started\":"<<(lobbyMusicStarted?"true":"false")<<",\"lobby_music_exit\":"<<lobbyAudio.result.load()<<",\"original_background_quads\":"<<agreementBackground.size()<<"}"<<std::endl;
 audio.stop=true;effect.stop=true;if(audio.thread.joinable())audio.thread.join();if(effect.thread.joinable())effect.thread.join();check(device->GetDeviceRemovedReason());
 std::osyncstream(std::cout)<<"{\"start_sound_started\":"<<(seStarted?"true":"false")<<",\"start_sound_exit\":"<<effect.result.load()<<",\"loading_frames\":"<<loadingFrames<<",\"loading_ready\":"<<(loadingReady?"true":"false")<<"}"<<std::endl;
 if(animation)std::osyncstream(std::cout)<<"{\"scope\":\""<<(gcx?"gcx_title_subset_no_full_boot":"title_actor_preview_no_gcx_boot")<<"\",\"ticks\":"<<animation->ticks()<<",\"state\":"<<animation->state()<<",\"start_accepted\":"<<animation->accepted()<<",\"start_rejected\":"<<animation->rejected()<<",\"completion_callbacks\":"<<animation->callbacks()<<",\"accepted_tick\":"<<animation->accepted_tick()<<",\"callback_tick\":"<<animation->callback_tick()<<",\"scripted_input\":"<<(scripted?"true":"false")<<"}"<<std::endl;
 std::osyncstream(std::cout)<<"{\"status\":\"partial_asset_preview\",\"quads\":"<<count<<",\"frames\":"<<frames<<",\"occluded_presents\":"<<occluded<<",\"warp\":"<<(warp?"true":"false")<<",\"audio_exit\":"<<audio.result.load()<<"}"<<std::endl;
 return sound&&((lobbyMusicStarted&&lobbyAudio.result.load()!=0)||menuFailures||audio.result.load()!=0||(seStarted&&effect.result.load()!=0))?1:0;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}

