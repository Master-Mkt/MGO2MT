#include "character_catalog.h"
#include "character_renderer.h"
#include "motion_blend_presentation.h"
#include "selection_model.h"
#include "special_action_motion.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
using Microsoft::WRL::ComPtr;
namespace {
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
void ok(HRESULT v){check(SUCCEEDED(v),"Motion transition WARP failure");}
std::vector<char> read(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary);check(bool(in),"Motion render asset missing");return {(std::istreambuf_iterator<char>(in)),{}};}
struct Image{unsigned w=0,h=0;std::vector<unsigned char> pixels;};
Image capture(ID3D11Device*d,ID3D11DeviceContext*c,const CharacterRenderer&r){
 ComPtr<ID3D11Resource> resource;r.view()->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;ok(resource.As(&texture));D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);check(desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM,"RGBA capture required");desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
 ComPtr<ID3D11Texture2D> copy;ok(d->CreateTexture2D(&desc,nullptr,&copy));c->CopyResource(copy.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE mapped{};ok(c->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped));Image out{desc.Width,desc.Height,std::vector<unsigned char>(size_t(desc.Width)*desc.Height*4)};
 for(unsigned y=0;y<out.h;++y)std::copy_n(static_cast<const unsigned char*>(mapped.pData)+size_t(y)*mapped.RowPitch,size_t(out.w)*4,out.pixels.data()+size_t(y)*out.w*4);c->Unmap(copy.Get(),0);return out;
}
void write(const std::filesystem::path&p,const Image&im){
 auto data=im.pixels;for(size_t i=0;i<data.size();i+=4)std::swap(data[i],data[i+2]);BITMAPFILEHEADER f{};BITMAPINFOHEADER h{};h.biSize=sizeof(h);h.biWidth=LONG(im.w);h.biHeight=-LONG(im.h);h.biPlanes=1;h.biBitCount=32;h.biSizeImage=DWORD(data.size());f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(h);f.bfSize=f.bfOffBits+h.biSizeImage;
 std::ofstream out(p,std::ios::binary);out.write(reinterpret_cast<const char*>(&f),sizeof(f));out.write(reinterpret_cast<const char*>(&h),sizeof(h));out.write(reinterpret_cast<const char*>(data.data()),std::streamsize(data.size()));check(bool(out),"Motion image write failed");
}
size_t visible(const Image&im){size_t n=0;for(unsigned y=0;y<im.h;++y)for(unsigned x=0;x<im.w;++x)if(im.pixels[(size_t(y)*im.w+x)*4+3]){++n;check(x&&y&&x+1<im.w&&y+1<im.h,"Motion body touches capture edge");}return n;}
size_t changed(const Image&a,const Image&b){check(a.pixels.size()==b.pixels.size(),"Capture extent");size_t n=0;for(size_t i=0;i<a.pixels.size();i+=4)if(!std::equal(a.pixels.begin()+i,a.pixels.begin()+i+4,b.pixels.begin()+i))++n;return n;}
std::array<float,3> world(const ModelVertex&v,std::array<float,3>origin,float yaw){float s=std::sin(yaw),c=std::cos(yaw);return {origin[0]+c*v.x+s*v.z,origin[1]+v.y,origin[2]-s*v.x+c*v.z};}
void coordinate_checks(const CharacterCatalog&catalog,PreparedCharacter body,const MotionPose&pose){
 motion_blend::Lane lane;auto complete=catalog.complete_pose(body.gender,pose);catalog.pose(body,lane.sample({1,2,3,4,5},1,0,complete,0,5));auto before=body;
 const std::array<float,3> a{-42878,1200,29781},b{-42700,1300,29850};const float ay=2.2f,by=-1.1f;
 lane.rebase(a,ay,b,by,catalog.skeleton(body.gender).front().position);catalog.pose(body,*lane.pose());
 for(size_t i=0;i<body.model.vertices.size();++i){auto x=world(before.model.vertices[i],a,ay),y=world(body.model.vertices[i],b,by);for(unsigned axis=0;axis<3;++axis)check(std::abs(x[axis]-y[axis])<.05f,"Rebased bone skin must retain world coordinates");}
 // Emulate the ragdoll's world-root orientation (root X/Z are zero there),
 // then verify removal of actor yaw restores ordinary local rendering.
 auto worldPose=complete;auto&q=worldPose.rotations.at(worldPose.rootBone);float s=std::sin(ay*.5f),c=std::cos(ay*.5f);q={c*q[0]+s*q[2],c*q[1]+s*q[3],c*q[2]-s*q[0],c*q[3]-s*q[1]};
 catalog.pose(body,motion_blend::local_physics_pose(worldPose,ay));catalog.pose(before,complete);
 for(size_t i=0;i<body.model.vertices.size();++i){const auto&x=body.model.vertices[i];const auto&y=before.model.vertices[i];check(std::abs(x.x-y.x)<.02f&&std::abs(x.y-y.y)<.02f&&std::abs(x.z-y.z)<.02f,"Physics world yaw removal preserves local skin");}
}
void series(ID3D11Device*d,ID3D11DeviceContext*c,const CharacterCatalog&catalog,PreparedCharacter body,const MotionPose&from,const MotionPose&to,const std::filesystem::path&output,const std::string&name){
 MotionBlend blend;auto a=catalog.complete_pose(body.gender,from),b=catalog.complete_pose(body.gender,to);catalog.pose(body,blend.update(1,a,0));CharacterRenderer renderer(d,body.model);
 // Fixed world camera and original vertices: no preview lift or frame-wise
 // auto fitting can hide a root/scale discontinuity. Box props are not included.
 const WorldView camera{{-2300,1600,3500},{2300,-750,-3500}};
 auto draw=[&](){renderer.update_vertices(c,body.model.vertices);renderer.render(c,0,false,&camera);return capture(d,c,renderer);};
 const auto original=draw();catalog.pose(body,blend.update(2,b,1));auto zero=draw();check(original.pixels==zero.pixels,"Transition alpha zero must preserve actual rendered pixels");
 Image first,last;for(unsigned i=0;i<=4;++i){if(i)catalog.pose(body,blend.update(2,b,.05));auto shown=draw();auto n=visible(shown);check(n>1000,"Original body absent from blend capture");write(output/(name+"-"+std::to_string(i*25)+".bmp"),shown);std::cout<<name<<" progress="<<blend.progress()<<" visible_pixels="<<n<<'\n';if(!i)first=shown;if(i==4)last=shown;}
 check(blend.progress()==1,"Render series must reach endpoint");MotionBlend endpoint;catalog.pose(body,endpoint.update(1,b,0));check(draw().pixels==last.pixels,"Final rendered pose equals normalized target");
 std::cout<<name<<" endpoint_changed_pixels="<<changed(first,last)<<'\n';
}
}
int main(int argc,char**argv){try{
 check(argc==7,"usage: motion_blend_render_test appearance.gwc player.gwmot special_male.gwmot selection0.gwmot selection1.gwmot output-directory");std::filesystem::path output=argv[6];std::filesystem::create_directories(output);
 CharacterCatalog catalog(read(argv[1]));PlayerMotionBank player(read(argv[2]));player::SpecialMotionBank special(read(argv[3]));
 ComPtr<ID3D11Device>d;ComPtr<ID3D11DeviceContext>c;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c));
 for(unsigned gender=0;gender<2;++gender){std::array<uint8_t,28>appearance{};appearance[0]=uint8_t(gender);appearance[2]=11;appearance[3]=22;appearance[15]=46;appearance[17]=57;auto body=catalog.assemble(appearance);check(body.ready()&&!body.missingModels&&!body.missingColors,"Original render appearance incomplete");const std::string prefix=gender?"female":"male";
  auto idle=*player.sample(PlayerMotion::Idle,.1);coordinate_checks(catalog,body,idle);
  series(d.Get(),c.Get(),catalog,body,idle,*player.sample(PlayerMotion::CrouchIdle,.3),output,prefix+"-standing-crouch");
  if(!gender)series(d.Get(),c.Get(),catalog,body,idle,*special.sample(player::SpecialPhase::hold,.2),output,prefix+"-normal-salute");
  PlayerMotionBank selection(read(argv[4+gender]));auto from=selection_pose(selection,PlayerMotion::SelectionBoxEnter,100),to=selection_pose(selection,PlayerMotion::SelectionBox,0);check(from&&to,"Original box transition clips missing");series(d.Get(),c.Get(),catalog,body,*from,*to,output,prefix+"-box-enter-loop");
 }
 std::cout<<"WARP 25 captures, original rigs, exact alpha0/endpoint pixels, world rebase/local physics quaternion PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
