#include "character_catalog.h"
#include "character_renderer.h"
#include "player_motion.h"
#include "selection_model.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
using namespace mgo2mt;
using Microsoft::WRL::ComPtr;
namespace {
void check(bool condition,const char* reason){if(!condition)throw std::runtime_error(reason);}
void ok(HRESULT result){check(SUCCEEDED(result),"PC selection D3D11 operation failed");}
std::vector<char> read(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);check(bool(in),"PC selection input unavailable");return {(std::istreambuf_iterator<char>(in)),{}};}
struct Frame{unsigned width=0,height=0;std::vector<unsigned char> rgba;};
Frame capture(ID3D11Device* device,ID3D11DeviceContext* context,const CharacterRenderer& renderer){
 ComPtr<ID3D11Resource> resource;renderer.view()->GetResource(&resource);
 ComPtr<ID3D11Texture2D> texture;ok(resource.As(&texture));D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
 check(desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM,"PC selection capture requires RGBA8");
 desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
 ComPtr<ID3D11Texture2D> copy;ok(device->CreateTexture2D(&desc,nullptr,&copy));context->CopyResource(copy.Get(),texture.Get());
 D3D11_MAPPED_SUBRESOURCE mapped{};ok(context->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped));
 Frame result{desc.Width,desc.Height,std::vector<unsigned char>(size_t(desc.Width)*desc.Height*4)};
 for(unsigned y=0;y<desc.Height;++y)std::copy_n(static_cast<const unsigned char*>(mapped.pData)+size_t(y)*mapped.RowPitch,size_t(desc.Width)*4,result.rgba.data()+size_t(y)*desc.Width*4);
 context->Unmap(copy.Get(),0);return result;
}
void write(const std::filesystem::path& path,const Frame& frame){
 auto bgra=frame.rgba;for(size_t i=0;i<bgra.size();i+=4)std::swap(bgra[i],bgra[i+2]);
 BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=LONG(frame.width);info.biHeight=-LONG(frame.height);info.biPlanes=1;info.biBitCount=32;info.biSizeImage=DWORD(bgra.size());
 file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+info.biSizeImage;
 std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&file),sizeof(file));out.write(reinterpret_cast<const char*>(&info),sizeof(info));out.write(reinterpret_cast<const char*>(bgra.data()),std::streamsize(bgra.size()));check(bool(out),"PC selection capture write failed");
}
size_t visible(const Frame& frame){size_t result=0;for(size_t i=3;i<frame.rgba.size();i+=4)if(frame.rgba[i])++result;return result;}
size_t edge_pixels(const Frame& image){
 size_t edge=0;
 for(unsigned x=0;x<image.width;++x){edge+=image.rgba[size_t(x)*4+3]!=0;edge+=image.rgba[(size_t(image.height-1)*image.width+x)*4+3]!=0;}
 for(unsigned y=1;y+1<image.height;++y){edge+=image.rgba[size_t(y)*image.width*4+3]!=0;edge+=image.rgba[(size_t(y)*image.width+image.width-1)*4+3]!=0;}
 return edge;
}
void fully_framed(const Frame& image){check(edge_pixels(image)==0,"Original selection avatar is clipped by preview frame edge");}
size_t changed(const Frame& a,const Frame& b){check(a.width==b.width&&a.height==b.height,"PC selection surface resized");size_t result=0;for(size_t i=0;i<a.rgba.size();i+=4)if(!std::equal(a.rgba.begin()+i,a.rgba.begin()+i+4,b.rgba.begin()+i))++result;return result;}
bool same(const ModelVertex& a,const ModelVertex& b){return a.x==b.x&&a.y==b.y&&a.z==b.z&&a.nx==b.nx&&a.ny==b.ny&&a.nz==b.nz;}
void valid_vertices(const PreparedCharacter& body){
 check(body.model.vertices.size()==body.bind.size()&&body.bind.size()==body.skin.size(),"Skin bindings lost original vertex correspondence");
 for(const auto& v:body.model.vertices){
  check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.nx)&&std::isfinite(v.ny)&&std::isfinite(v.nz),"Nonfinite selection deformation");
  check(std::abs(v.x)<10000&&std::abs(v.y)<10000&&std::abs(v.z)<10000,"Selection deformation escaped avatar bounds");
 }
}
struct Result{size_t bones=0,tracks=0,vertices=0,moved=0,pixels0=0,pixels120=0,pixels400=0,changedPixels=0,boxVisible=0,boxExposed=0,boxEdge=0;double displacement=0;uint32_t sourceKey=0;};
Result original(ID3D11Device* device,ID3D11DeviceContext* context,const CharacterCatalog& catalog,const PlayerMotionBank& bank,unsigned gender,const std::filesystem::path& output,const CharacterModel* box){
 const auto* clip=bank.find(PlayerMotion::SelectionSalute);
 check(clip&&clip->sourceKey&&clip->sourceIndex==33&&clip->frames==400&&clip->fps==60&&!clip->loop,"Original selection salute metadata changed");
 auto skeleton=catalog.skeleton(gender);check(!skeleton.empty(),"Original gender skeleton unavailable");std::set<uint32_t> boneKeys;
 for(const auto& bone:skeleton)check(boneKeys.insert(bone.key).second,"Duplicate original skeleton key");
 check(boneKeys.contains(clip->rootBone),"Salute root is absent from original skeleton");
 check(clip->roots.size()==401&&!clip->tracks.empty(),"Selection clip samples incomplete");
 for(const auto& [key,samples]:clip->tracks){check(boneKeys.contains(key),"Selection rotation track does not fit original gender skeleton");check(samples.size()==401,"Selection rotation track is truncated");}
 constexpr double seconds=400.0/60.0;
 check(selection_origin_y(bank)==1081.f,"Selection origin no longer matches the reviewed catalog source root");
 auto first=selection_pose(bank,PlayerMotion::SelectionSalute,0),middle=selection_pose(bank,PlayerMotion::SelectionSalute,2),last=selection_pose(bank,PlayerMotion::SelectionSalute,seconds),held=selection_pose(bank,PlayerMotion::SelectionSalute,seconds+30);
 check(first&&middle&&last&&held,"Selection salute could not be sampled");
 check(last->root==held->root&&last->rootBone==held->rootBone&&last->rotations==held->rotations,"Nonloop salute did not hold its final sample after 400/60 seconds");
 check(first->rotations!=middle->rotations&&last->rotations!=middle->rotations,"Selection sample timing does not reach changing motion");
 for(const auto* pose:{&*first,&*middle,&*last}){
  check(pose->root[0]==0&&pose->root[2]==0,"Selection preview retained world X/Z translation");
  for(const auto& [key,q]:pose->rotations){double norm=0;for(float value:q){check(std::isfinite(value),"Nonfinite salute quaternion");norm+=double(value)*value;}check(std::abs(norm-1)<.001,"Salute quaternion not normalized");}
 }
 std::array<uint8_t,28> appearance{};appearance[0]=uint8_t(gender);appearance[2]=11;appearance[3]=22;appearance[15]=46;appearance[17]=57;
 auto body=catalog.assemble(appearance);check(body.ready()&&body.selectedParts==5&&!body.missingModels&&!body.missingColors&&body.issues.empty(),"Original appearance is incomplete for selected gender");
 for(const auto& binding:body.skin)for(unsigned i=0;i<4;++i)if(binding.weights[i]>0)check(binding.bones[i]<skeleton.size(),"Original skin references absent bone");

 auto logBounds=[&](const char*label){float low=1e9f,high=-1e9f;for(const auto&v:body.model.vertices){low=std::min(low,v.y);high=std::max(high,v.y);}std::cout<<"gender="<<gender<<" "<<label<<" ymin="<<low<<" ymax="<<high<<" original_bounds="<<body.model.bounds[1]<<","<<body.model.bounds[4]<<" root_y="<<first->root[1]<<"\n";};
 logBounds("original_idle");
 catalog.pose(body,*first);valid_vertices(body);logBounds("salute_frame0");auto initial=body.model.vertices;
 CharacterRenderer renderer(device,body.model);renderer.render(context,.15f);auto frame0=capture(device,context,renderer);
 catalog.pose(body,*middle);valid_vertices(body);Result result;result.bones=skeleton.size();result.tracks=clip->tracks.size();result.vertices=initial.size();result.sourceKey=clip->sourceKey;
 for(size_t i=0;i<initial.size();++i){const auto& a=initial[i];const auto& b=body.model.vertices[i];double delta=std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z);result.displacement+=delta;if(delta>.01)++result.moved;}
 check(result.moved>100&&result.displacement>100,"Original salute did not deform the skinned avatar");
 renderer.update_vertices(context,body.model.vertices);renderer.render(context,.15f);auto frame120=capture(device,context,renderer);
 catalog.pose(body,*last);valid_vertices(body);auto final=body.model.vertices;
 renderer.update_vertices(context,body.model.vertices);renderer.render(context,.15f);auto frame400=capture(device,context,renderer);
 catalog.pose(body,*held);check(std::equal(final.begin(),final.end(),body.model.vertices.begin(),same),"Held salute changed final CPU deformation");
 renderer.update_vertices(context,body.model.vertices);renderer.render(context,.15f);check(capture(device,context,renderer).rgba==frame400.rgba,"Held salute changed final rendered frame");
 catalog.pose(body,*first);check(std::equal(initial.begin(),initial.end(),body.model.vertices.begin(),same),"Repeated salute accumulated deformation instead of using original bind vertices");
 renderer.update_vertices(context,body.model.vertices);renderer.render(context,.15f);check(capture(device,context,renderer).rgba==frame0.rgba,"Repeated salute changed initial rendered frame");

 result.pixels0=visible(frame0);result.pixels120=visible(frame120);result.pixels400=visible(frame400);result.changedPixels=changed(frame0,frame120);
 check(result.pixels0>1000&&result.pixels120>1000&&result.pixels400>1000,"Original selection avatar was not visible");check(result.changedPixels>100,"Selection salute animation did not alter visible pixels");
 const std::string prefix=gender?"female":"male";
 write(output/(prefix+"-frame000.bmp"),frame0);write(output/(prefix+"-frame120.bmp"),frame120);write(output/(prefix+"-frame400.bmp"),frame400);
 std::cout<<prefix<<" frame0 framing\n";fully_framed(frame0);std::cout<<prefix<<" frame120 framing\n";fully_framed(frame120);std::cout<<prefix<<" frame400 framing\n";fully_framed(frame400);
 if(box){
  const auto* idle=bank.find(PlayerMotion::SelectionBox);const auto* enter=bank.find(PlayerMotion::SelectionBoxEnter);
  check(idle&&idle->sourceIndex==24&&idle->loop&&enter&&enter->sourceIndex==3&&!enter->loop,"Original box selection actions unavailable");
  auto boxPose=selection_pose(bank,PlayerMotion::SelectionBox,.5);check(bool(boxPose),"Box idle pose unavailable");catalog.pose(body,*boxPose);valid_vertices(body);logBounds("box_idle");std::cout<<"box root_y="<<boxPose->root[1]<<" source_prop_bounds="<<box->bounds[1]<<","<<box->bounds[4]<<"\n";
  auto combined=selection_with_prop(body.model,*box,-selection_origin_y(bank));check(combined.bounds==body.model.bounds,"Box prop changed standing preview camera bounds");
  check(combined.vertices.size()==body.model.vertices.size()+box->vertices.size(),"Box merge lost model vertices");
  for(size_t i=0;i<box->vertices.size();++i)check(([&]{auto expected=box->vertices[i];expected.y-=selection_origin_y(bank);return same(combined.vertices[body.model.vertices.size()+i],expected);}()),"Box moved away from actor origin");
  CharacterRenderer boxRenderer(device,combined);boxRenderer.preview_vertical_offset(.12f);boxRenderer.render(context,.15f);auto boxFrame=capture(device,context,boxRenderer);
  renderer.preview_vertical_offset(.12f);renderer.update_vertices(context,body.model.vertices);renderer.render(context,.15f);auto bodyUnderBox=capture(device,context,renderer);
  auto propOnly=*box;for(auto&v:propOnly.vertices)v.y-=selection_origin_y(bank);propOnly.bounds=body.model.bounds;CharacterRenderer propRenderer(device,propOnly);propRenderer.preview_vertical_offset(.12f);propRenderer.render(context,.15f);auto propFrame=capture(device,context,propRenderer);
  size_t exposed=0;for(size_t i=0;i<boxFrame.rgba.size();i+=4)if(boxFrame.rgba[i+3]&&!propFrame.rgba[i+3])++exposed;
  write(output/(prefix+"-box-idle.bmp"),boxFrame);write(output/(prefix+"-box-body.bmp"),bodyUnderBox);write(output/(prefix+"-box-prop.bmp"),propFrame);
  result.boxVisible=visible(boxFrame);result.boxExposed=exposed;result.boxEdge=edge_pixels(boxFrame);
  fully_framed(boxFrame);

  check(visible(boxFrame)>1000&&changed(bodyUnderBox,boxFrame)>1000,"Textured box prop did not reach selection rendering");
  std::cout<<prefix<<" box_body_pixels="<<visible(bodyUnderBox)<<" box_exposed_body_pixels="<<exposed<<" box_prop_pixels="<<visible(propFrame)<<'\n';
  check(exposed<visible(bodyUnderBox)/4,"Box idle body is not mostly covered by original box at shared actor origin");
  // Rotation is user-controlled: inspect each five-degree direction, including
  // face-on and diagonal corners, at the same standing camera distance/scale.
  for(unsigned step=0;step<72;++step){float yaw=float(step)*6.28318530718f/72;
   boxRenderer.render(context,yaw);auto rotated=capture(device,context,boxRenderer);fully_framed(rotated);check(visible(rotated)>1000,"Rotated box disappeared");
   if(step%18==0)write(output/(prefix+"-box-yaw"+std::to_string(step*5)+".bmp"),rotated);
  }
  // Match the native entry duration: raise the framing gradually while the
  // original nonloop pose crouches, keeping its first eight-frame blend.
  for(unsigned tick=0;tick<=enter->frames;tick+=2){double time=double(tick)/enter->fps;auto pose=selection_pose(bank,PlayerMotion::SelectionBoxEnter,time);catalog.pose(body,*pose);auto entryVertices=body.model.vertices;
   if(tick<8){float t=float(tick)/8;t=t*t*(3-2*t);for(size_t i=0;i<entryVertices.size();++i){entryVertices[i].x=initial[i].x+(entryVertices[i].x-initial[i].x)*t;entryVertices[i].y=initial[i].y+(entryVertices[i].y-initial[i].y)*t;entryVertices[i].z=initial[i].z+(entryVertices[i].z-initial[i].z)*t;}}
   float progress=float(tick)/enter->frames;progress=progress*progress*(3-2*progress);renderer.preview_vertical_offset(.12f*progress);renderer.update_vertices(context,entryVertices);renderer.render(context,.15f);fully_framed(capture(device,context,renderer));
  }
  catalog.pose(body,*boxPose);
  // Returning to a standing pose lowers the framing over the same short
  // interpolation as the body, so its head never jumps above the surface.
  auto crouched=body.model.vertices;
  for(unsigned step=0;step<=20;++step){float t=float(step)/20;t=t*t*(3-2*t);auto blended=crouched;
   for(size_t i=0;i<blended.size();++i){blended[i].x+=t*(initial[i].x-blended[i].x);blended[i].y+=t*(initial[i].y-blended[i].y);blended[i].z+=t*(initial[i].z-blended[i].z);}
   renderer.preview_vertical_offset(.12f*(1-t));renderer.update_vertices(context,blended);renderer.render(context,.15f);fully_framed(capture(device,context,renderer));
  }
  // A preview-only adjustment cannot shift the game camera or stage overview.
  WorldView worldCamera{{0,0,3500},{0,0,-1}};
  for(bool overview:{false,true}){
   boxRenderer.preview_vertical_offset(0);boxRenderer.render(context,.15f,overview,overview?nullptr:&worldCamera);auto control=capture(device,context,boxRenderer);
   boxRenderer.preview_vertical_offset(.12f);boxRenderer.render(context,.15f,overview,overview?nullptr:&worldCamera);check(control.rgba==capture(device,context,boxRenderer).rgba,"Preview offset leaked into world/overview rendering");
  }
  // Match the runtime update path: only replace the body prefix; retain prop vertices.
  auto nextBoxPose=selection_pose(bank,PlayerMotion::SelectionBox,1);catalog.pose(body,*nextBoxPose);
  std::copy(body.model.vertices.begin(),body.model.vertices.end(),combined.vertices.begin());
  for(size_t i=0;i<box->vertices.size();++i)check(([&]{auto expected=box->vertices[i];expected.y-=selection_origin_y(bank);return same(combined.vertices[body.model.vertices.size()+i],expected);}()),"Animated body update altered static box prop");
  boxRenderer.update_vertices(context,combined.vertices);boxRenderer.render(context,.15f);check(visible(capture(device,context,boxRenderer))>1000,"Animated combined box model disappeared");
 }

 std::cout<<prefix<<" bones="<<result.bones<<" tracks="<<result.tracks<<" vertices="<<result.vertices<<" moved_vertices="<<result.moved<<" visible_pixels="<<result.pixels120<<" changed_pixels="<<result.changedPixels<<'\n';return result;
}
}
int main(int argc,char**argv){try{
 check(argc==5||argc==6,"Usage: pc_selection_render_test appearance.gwc selection0.gwmot selection1.gwmot output-dir [box.gwm]");
 CharacterCatalog catalog(read(argv[1]));PlayerMotionBank male(read(argv[2])),female(read(argv[3]));std::filesystem::path output=argv[4];std::filesystem::create_directories(output);
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context));
 std::optional<CharacterModel> box;if(argc==6)box.emplace(read(argv[5]));
 std::array<Result,2> results{original(device.Get(),context.Get(),catalog,male,0,output,box?&*box:nullptr),original(device.Get(),context.Get(),catalog,female,1,output,box?&*box:nullptr)};
 std::ofstream report(output/"validation.json");report<<"{\n  \"test\": \"original_pc_selection_salute\",\n  \"renderer\": \"D3D11 WARP / CharacterRenderer\",\n  \"source_index\": 33,\n  \"frames\": 400,\n  \"native_fps\": 60,\n  \"duration_seconds\": "<<400.0/60<<",\n  \"loop\": false,\n  \"skeleton_compatible\": true,\n  \"final_frame_held\": true,\n  \"repeat_deterministic\": true,\n  \"genders\": [\n";
 for(unsigned gender=0;gender<2;++gender){const auto&r=results[gender];report<<"    {\"gender\": "<<gender<<", \"source_key\": "<<r.sourceKey<<", \"bones\": "<<r.bones<<", \"tracks\": "<<r.tracks<<", \"vertices\": "<<r.vertices<<", \"moved_vertices\": "<<r.moved<<", \"visible_pixels_frame120\": "<<r.pixels120<<", \"changed_pixels\": "<<r.changedPixels<<", \"box_visible_pixels\": "<<r.boxVisible<<", \"box_exposed_body_pixels\": "<<r.boxExposed<<", \"box_edge_pixels\": "<<r.boxEdge<<"}"<<(gender?"\n":",\n");}
 report<<"  ]\n}\n";check(bool(report),"PC selection validation report write failed");std::cout<<"Original male/female selection salute CPU and D3D11 render checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
