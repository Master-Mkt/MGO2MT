#include "stage_assets.h"
#include "stage_lighting.h"
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cmath>
namespace mgo2win::stage {
static void append_model(CharacterModel&result,const CharacterModel&m,Vec3 position,Vec3 degrees,const Lighting*lighting){
 auto vertexBase=uint32_t(result.vertices.size()),indexBase=uint32_t(result.indices.size()),textureBase=uint32_t(result.textures.size());
 // Native Euler composition; quantized original angles remain retained in the
 // received state. Yaw uses the existing stage/model coordinate convention.
 auto rotate=[&](Vec3 v){float a=degrees[2]*3.14159265359f/180,c=std::cos(a),s=std::sin(a);v={c*v[0]-s*v[1],s*v[0]+c*v[1],v[2]};a=degrees[0]*3.14159265359f/180;c=std::cos(a);s=std::sin(a);v={v[0],c*v[1]-s*v[2],s*v[1]+c*v[2]};a=degrees[1]*3.14159265359f/180;c=std::cos(a);s=std::sin(a);return Vec3{c*v[0]+s*v[2],v[1],-s*v[0]+c*v[2]};};
 for(auto v:m.vertices){auto p=rotate({v.x,v.y,v.z}),n=rotate({v.nx,v.ny,v.nz});v.x=p[0]+position[0];v.y=p[1]+position[1];v.z=p[2]+position[2];v.nx=n[0];v.ny=n[1];v.nz=n[2];
  if(lighting){auto light=lighting->sample({v.x,v.y,v.z},n);v.lr=light.color[0];v.lg=light.color[1];v.lb=light.color[2];v.lit=1;}
  result.vertices.push_back(v);float xyz[]={v.x,v.y,v.z};for(int k=0;k<3;++k){result.overviewBounds[k]=std::min(result.overviewBounds[k],xyz[k]);result.overviewBounds[k+3]=std::max(result.overviewBounds[k+3],xyz[k]);}
 }
 for(auto i:m.indices)result.indices.push_back(i+vertexBase);
 for(auto part:m.parts){part.first+=indexBase;part.texture+=textureBase;if(part.flags)part.flags+=textureBase;result.parts.push_back(part);}result.textures.insert(result.textures.end(),m.textures.begin(),m.textures.end());result.hasOverviewBounds=true;
}
static std::shared_ptr<const CharacterModel> placement_preview(const Result&r){
 if(!r.model||r.props.size()!=6||r.round.objects.empty())return {};
 auto result=std::make_shared<CharacterModel>(r.receivedModel?*r.receivedModel:*r.model);
 // GCX activation of these reference background actors is not yet proven.
 for(const auto&p:r.round.objects)append_model(*result,*r.props.at(p.model),p.position,{0,p.yaw,0},r.lighting.get());
 return result;
}
static void received_preview(Result&r){
 r.receivedModel.reset();r.missingItemModels=0;if(!r.model||!r.received)return;
 std::shared_ptr<CharacterModel> model;
 for(const auto&[id,item]:r.received->items){if(item.state!=2&&item.state!=3)continue;auto found=r.itemModels.find(item.type);if(found==r.itemModels.end()){++r.missingItemModels;continue;}
  if(!model)model=std::make_shared<CharacterModel>(*r.model);append_model(*model,*found->second,item.position(),item.degrees(),r.lighting.get());
 }r.receivedModel=std::move(model);
}
std::string_view name(uint8_t map){
 // Updated lobby GCX proc11 switch; verified against raw command arguments.
 constexpr std::string_view names[]={"","n001a","n002a","n003a","n004a","n005a","n006a","n007a","n008a","n009a","n010a","n018a","n012a","n013a","n014a","n015a","","sm_dd","sm_ll","n024a","n022a","n023a","n020a"};
 return map<std::size(names)?names[map]:std::string_view{};
}
Assets::~Assets(){worker_.request_stop();if(worker_.joinable())worker_.join();}
Result Assets::result()const{std::lock_guard lock(mutex_);return result_;}
void Assets::reset(){auto old=result();if(!old.request)return;worker_.request_stop();if(worker_.joinable())worker_.join();
 if(old.received){old.round.transientLights.clear();received_preview(old);old.debugModel=placement_preview(old);std::lock_guard lock(mutex_);result_=std::move(old);return;}
 {std::lock_guard lock(mutex_);result_={};}select(old.request);
}
void Assets::receive(const std::optional<host::Placements>&received){
 auto old=result();if(!old.request||!old.model||old.status!=Status::preview_ready)return;
 if(received&&received->generation!=old.request->generation)return;
 if(old.received==received)return;
 old.received=received;received_preview(old);old.debugModel=placement_preview(old);
 std::lock_guard lock(mutex_);result_=std::move(old);
}
bool Assets::light_states(const host::LoadRequest&request,const std::vector<LightChange>&changes){
 auto old=result();if(old.request!=request||!old.model||!old.authoredLighting||old.status!=Status::preview_ready||changes.size()>224)return false;
 for(const auto&change:changes){if(!std::isfinite(change.radius)||change.radius<0||change.radius>1000000)return false;for(auto c:change.center)if(!std::isfinite(c)||std::abs(c)>1000000)return false;}
 auto light=std::make_shared<Lighting>(*old.authoredLighting);
 for(const auto&change:changes){if(change.key)light->enable(change.key,change.id,change.enabled);if(change.radius>0)light->enable_sphere(change.center,change.radius,change.enabled);}
 auto model=std::make_shared<CharacterModel>(*old.model);for(auto&v:model->vertices){auto c=light->sample({v.x,v.y,v.z},{v.nx,v.ny,v.nz}).color;v.lr=c[0];v.lg=c[1];v.lb=c[2];v.lit=1;}
 old.model=std::move(model);old.lighting=std::move(light);received_preview(old);old.debugModel=placement_preview(old);std::lock_guard lock(mutex_);result_=std::move(old);return true;
}
bool Assets::object_lights(const host::LoadRequest&request,const host::ObjectStates&states,const std::vector<ObjectLightBinding>&bindings){
 if(!states.complete()||bindings.size()>224)return false;
 std::vector<LightChange>changes;for(const auto&binding:bindings){if(binding.index>=states.values().size()||states.widths()[binding.index]!=1)return false;
  // 5A1AF8/5A2868 one-bit destructibles: nonzero means already broken.
  changes.push_back({binding.key,~0u,states.values()[binding.index]==0,binding.center,binding.radius});
 }return light_states(request,changes);
}
void Assets::select(std::optional<host::LoadRequest> request){
 auto old=result();if(old.request==request)return;
 worker_.request_stop();if(worker_.joinable())worker_.join();
 Result next;next.request=request;next.generation=request?++generation_:0;
 if(request){
  if(name(request->rotation.map).empty())next.status=Status::unknown_map;
  else if(root_.empty()||request->rotation.map!=20)next.status=Status::unavailable;
  else if(old.model&&old.request&&old.request->rotation.map==request->rotation.map&&old.lighting==old.authoredLighting){next.status=Status::preview_ready;next.model=old.model;next.collision=old.collision;next.round=old.round.reset(next.generation);next.props=old.props;next.lighting=old.lighting;next.authoredLighting=old.authoredLighting;next.itemModels=old.itemModels;next.cboxLayout=old.cboxLayout;if(next.cboxLayout)next.cboxes=next.cboxLayout->select(request->generation);next.debugModel=placement_preview(next);}
  else next.status=Status::loading;
 }
 {std::lock_guard lock(mutex_);result_=next;}
 if(next.status!=Status::loading)return;
 // The filename is selected locally, never constructed from host-controlled text.
 auto path=root_/"n022a.gwm";
 auto generation=next.generation;
 try{worker_=std::jthread([this,path,request,generation](std::stop_token stop){
  Result finished;finished.request=request;finished.status=Status::invalid;finished.generation=generation;
  try{
   std::ifstream in(path,std::ios::binary|std::ios::ate);
   if(!in)finished.status=Status::unavailable;
   else{
    auto size=in.tellg();if(size<48||size>64*1024*1024)throw std::runtime_error("Stage preview extent");
    std::vector<char>bytes(static_cast<size_t>(size));in.seekg(0);
    for(size_t at=0;at<bytes.size();){if(stop.stop_requested())return;auto n=std::min<size_t>(65536,bytes.size()-at);if(!in.read(bytes.data()+at,n))throw std::runtime_error("Stage preview read");at+=n;}
    if(stop.stop_requested())return;
    auto model=std::make_shared<CharacterModel>(bytes);
    auto lightPath=path.parent_path()/"n022a.lighting.cfg";
    if(std::filesystem::exists(lightPath)){
     if(std::filesystem::file_size(lightPath)>4*1024*1024)throw std::runtime_error("Stage lighting extent");
     std::ifstream input(lightPath);auto lighting=Lighting::read(input);
     size_t index=0;for(auto&v:model->vertices){if((index++&511)==0&&stop.stop_requested())return;auto sample=lighting.sample({v.x,v.y,v.z},{v.nx,v.ny,v.nz});v.lr=sample.color[0];v.lg=sample.color[1];v.lb=sample.color[2];v.lit=1;}
     model->overviewBounds=lighting.cameraBounds;model->hasOverviewBounds=true;
     finished.lighting=std::make_shared<const Lighting>(std::move(lighting));finished.authoredLighting=finished.lighting;
    }
    auto collisionPath=path.parent_path()/"n022a.collision.cfg";
    if(std::filesystem::exists(collisionPath)){
     if(std::filesystem::file_size(collisionPath)>32*1024*1024)throw std::runtime_error("Stage collision extent");
     std::ifstream input(collisionPath);finished.collision=std::make_shared<const Collision>(Collision::read(input));
    }
    auto placementPath=path.parent_path()/"n022a.placements.cfg";
    if(std::filesystem::exists(placementPath)){if(std::filesystem::file_size(placementPath)>1024*1024)throw std::runtime_error("Placement extent");std::ifstream input(placementPath);finished.round=Round::read(input).reset(generation);}
    auto cboxPath=path.parent_path()/"n022a.cbox.cfg";
    if(std::filesystem::exists(cboxPath)){
     if(std::filesystem::file_size(cboxPath)>16384)throw std::runtime_error("CBOX layout extent");
     std::ifstream input(cboxPath);finished.cboxLayout=std::make_shared<const CboxLayout>(CboxLayout::read(input));
     finished.cboxes=finished.cboxLayout->select(request->generation);
    }
    finished.model=std::move(model);
    if(!finished.round.objects.empty())for(unsigned i=0;i<6;++i){if(stop.stop_requested())return;auto propPath=path.parent_path()/"props"/(std::to_string(i)+".gwm");std::ifstream prop(propPath,std::ios::binary|std::ios::ate);if(!prop)throw std::runtime_error("Missing stage prop");auto n=prop.tellg();if(n<48||n>4*1024*1024)throw std::runtime_error("Stage prop extent");std::vector<char>b(static_cast<size_t>(n));prop.seekg(0);if(!prop.read(b.data(),n))throw std::runtime_error("Stage prop read");finished.props.push_back(std::make_shared<const CharacterModel>(b));}
    // Original B06E20 model switch: 113=ibox_item_large, 140=ibox_item_small.
    // Converted files are fixed local IDs, never paths supplied by a peer.
    for(unsigned id:{113,140}){auto itemPath=path.parent_path()/"items"/(std::to_string(id)+".gwm");if(!std::filesystem::exists(itemPath))continue;
     std::ifstream input(itemPath,std::ios::binary|std::ios::ate);auto n=input.tellg();if(n<48||n>4*1024*1024)throw std::runtime_error("Stage item extent");std::vector<char>b(static_cast<size_t>(n));input.seekg(0);if(!input.read(b.data(),n))throw std::runtime_error("Stage item read");finished.itemModels.emplace(uint8_t(id),std::make_shared<const CharacterModel>(b));}
    finished.debugModel=placement_preview(finished);finished.status=Status::preview_ready;
   }
  }catch(...){finished.model.reset();finished.debugModel.reset();finished.collision.reset();finished.cboxLayout.reset();finished.cboxes.clear();finished.status=Status::invalid;}
  if(stop.stop_requested())return;
  std::lock_guard lock(mutex_);result_=std::move(finished);
 });}catch(...){std::lock_guard lock(mutex_);result_.status=Status::unavailable;}
}
}
