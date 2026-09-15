#include "character_renderer.h"
#include "stage_assets.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
using namespace mgo2win;using Microsoft::WRL::ComPtr;
namespace {
void check(bool x,const char* m){if(!x)throw std::runtime_error(m);}
void ok(HRESULT h){check(SUCCEEDED(h),"BB D3D11 WARP");}
stage::Result wait(stage::Assets& a){auto until=std::chrono::steady_clock::now()+std::chrono::seconds(40);while(a.result().status==stage::Status::loading&&std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds(5));auto r=a.result();check(r.status==stage::Status::preview_ready&&r.model&&r.authoredLighting&&r.objectBindings,"BB authored scene loads");return r;}
constexpr unsigned width=1000,height=650;using Pixels=std::vector<uint8_t>;
Pixels render(ID3D11Device* d,ID3D11DeviceContext* c,const CharacterModel& model,const WorldView& camera){
 CharacterRenderer renderer(d,model);renderer.resize_target(d,width,height);renderer.render(c,0,false,&camera);
 ComPtr<ID3D11Resource> resource;renderer.view()->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;ok(resource.As(&texture));D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
 ComPtr<ID3D11Texture2D> staging;ok(d->CreateTexture2D(&desc,nullptr,&staging));c->CopyResource(staging.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE mapped{};ok(c->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));Pixels pixels(size_t(width)*height*4);
 for(unsigned y=0;y<height;++y)std::copy_n(static_cast<uint8_t*>(mapped.pData)+size_t(y)*mapped.RowPitch,width*4,pixels.data()+size_t(y)*width*4);c->Unmap(staging.Get(),0);return pixels;
}
void bitmap(const std::filesystem::path& path,Pixels pixels){for(size_t i=0;i<pixels.size();i+=4)std::swap(pixels[i],pixels[i+2]);BITMAPFILEHEADER f{};BITMAPINFOHEADER b{};b.biSize=40;b.biWidth=width;b.biHeight=-LONG(height);b.biPlanes=1;b.biBitCount=32;b.biSizeImage=DWORD(pixels.size());f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(b);f.bfSize=f.bfOffBits+b.biSizeImage;std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&f),sizeof(f));out.write(reinterpret_cast<const char*>(&b),sizeof(b));out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());check(bool(out),"BB BMP write");}
size_t changed(const Pixels& a,const Pixels& b){size_t n=0;for(size_t i=0;i<a.size();i+=4)if(a[i]!=b[i]||a[i+1]!=b[i+1]||a[i+2]!=b[i+2])++n;return n;}
}
int main(int argc,char** argv){try{
 check(argc==3,"BB stage directory and capture directory required");std::filesystem::path root=argv[1],out=argv[2];std::filesystem::create_directories(out);
 stage::Assets assets(root),late(root);host::LoadRequest q{1,7,0,0,{7,1,0},host::MatchTransition::initial};assets.select(q);late.select(q);auto authored=wait(assets);wait(late);
 auto registry=stage::load_combat_object_registry(root/"n007a.objects.cfg");stage::SceneAuthority authority(registry);stage::SceneReceiver receiver(registry,0),lateReceiver(registry,1);authority.begin(q);receiver.begin(q);lateReceiver.begin(q);
 check(receiver.receive(q,*authority.snapshot(0))&&assets.object_states(*receiver.snapshot()),"intact full wire scene reaches renderer");const auto intact=assets.result();
 check(intact.objectModel&&intact.objectHitCollision&&intact.objectBindings->size()==15,"actual BB original models and targets");
 auto anchor=intact.objectBindings->front().position;WorldView camera{{anchor[0]+1800,anchor[1]-1400,anchor[2]},{-1,.3f,0},float(width)/height};
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context));
 const auto before=render(device.Get(),context.Get(),*intact.objectModel,camera);bitmap(out/"bb-lights-on.bmp",before);
 auto delta=authority.update(registry.entries.front().bindingId,1);check(delta&&receiver.receive(q,*delta)&&assets.object_states(*receiver.snapshot()),"first lamp delta changes rendered scene");const auto broken=assets.result();const auto after=render(device.Get(),context.Get(),*broken.objectModel,camera);bitmap(out/"bb-light-broken.bmp",after);
 size_t rebaked=0;for(size_t i=0;i<authored.model->vertices.size();++i){const auto&a=intact.objectModel->vertices[i];const auto&b=broken.objectModel->vertices[i];if(a.lr!=b.lr||a.lg!=b.lg||a.lb!=b.lb)++rebaked;}
 check(rebaked>10,"lamp state changes actual original stage illumination");check(changed(before,after)>50,"lamp destruction changes actual BB render pixels");
 check(lateReceiver.receive(q,*authority.snapshot(1))&&late.object_states(*lateReceiver.snapshot()),"late join broken snapshot accepted");const auto lateImage=render(device.Get(),context.Get(),*late.result().objectModel,camera);check(lateImage==after,"late join light and mesh render equals existing client");bitmap(out/"bb-late-join.bmp",lateImage);
 for(size_t i=1;i<registry.entries.size();++i){auto d=authority.update(registry.entries[i].bindingId,1);check(d&&receiver.receive(q,*d),"every lamp has an independent one-bit delta");}check(assets.object_states(*receiver.snapshot()),"all broken full scene");const auto all=assets.result();check(!all.objectHitCollision,"all destroyed lamp hit volumes removed");bitmap(out/"bb-all-lights-broken.bmp",render(device.Get(),context.Get(),*all.objectModel,camera));
 const auto stale=*receiver.snapshot();++q.sequence;++q.generation;++q.round;q.transition=host::MatchTransition::next_round;assets.select(q);wait(assets);authority.begin(q);receiver.begin(q);check(!assets.object_states(stale),"old round cannot restore old damage");check(receiver.receive(q,*authority.snapshot(0))&&assets.object_states(*receiver.snapshot()),"next round full intact scene");const auto restored=render(device.Get(),context.Get(),*assets.result().objectModel,camera);check(restored==before,"next round restores exact original model lighting pixels");bitmap(out/"bb-next-round-restored.bmp",restored);
 std::ofstream report(out/"render.json");report<<"{\"map\":7,\"lamps\":15,\"warp\":true,\"synthetic_image\":false,\"original_geometry\":true,\"recovered_original_lighting_shader\":false,\"rebakedStageVertices\":"<<rebaked<<",\"changedPixels\":"<<changed(before,after)<<",\"lateJoinPixelsEqual\":true,\"nextRoundPixelsEqual\":true,\"camera\":["<<camera.eye[0]<<','<<camera.eye[1]<<','<<camera.eye[2]<<"]}\n";
 std::cout<<"BB actual stage WARP: light geometry, illumination, late join, next-round restoration PASS; rebaked="<<rebaked<<" pixels="<<changed(before,after)<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
