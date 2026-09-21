#include "product_identity.h"
#include "stage_normals.h"
#include "stage_floor_blend.h"
#include "stage_surface_alpha.h"
#include "stage_assets.h"
#include "stage_profiles.h"
#include "stage_lighting.h"
#include "stage_chunks.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <set>
#include <sstream>
#include <limits>
namespace mgo2mt::stage {
namespace {
void chunk_require(bool value){if(!value)throw std::runtime_error("Invalid stage chunks");}
void chunk_cancel(std::stop_token stop){if(stop.stop_requested())throw std::runtime_error("Stage chunks cancelled");}
size_t decoded_size(const CharacterModel&m){
 size_t n=m.vertices.size()*sizeof(ModelVertex)+m.indices.size()*4+m.parts.size()*sizeof(ModelPart)+m.textures.size()*sizeof(ModelTexture);
 for(const auto&t:m.textures)n+=t.pixels.size();
 for(const auto&p:m.parts){const auto&o=p.original;n+=o.textures.size()*sizeof(OriginalTexture);
  for(const auto*s:{&o.mdnPath,&o.mdnSha256,&o.packagePath,&o.packageSha256,&o.vertexProgramSha256,&o.fragmentProgramSha256,&o.ruleId,&o.fallbackReason})n+=s->size();
  for(const auto&t:o.textures)n+=t.provenance.size();
 }return n;
}
}
void append_stage_chunk(CharacterModel&target,CharacterModel chunk,std::stop_token stop){
 chunk_cancel(stop);
 const auto max=uint64_t(std::numeric_limits<uint32_t>::max());
 chunk_require(target.vertices.size()+uint64_t(chunk.vertices.size())<=max&&target.indices.size()+uint64_t(chunk.indices.size())<=max&&target.textures.size()+uint64_t(chunk.textures.size())<max);
 const auto vb=uint32_t(target.vertices.size()),ib=uint32_t(target.indices.size()),tb=uint32_t(target.textures.size());
 auto texture=[&](uint32_t&v){if(v!=noMaterialTexture){chunk_require(v<chunk.textures.size());v+=tb;}};
 for(size_t j=0;j<chunk.indices.size();++j){if(!(j&16383))chunk_cancel(stop);auto&i=chunk.indices[j];chunk_require(i<chunk.vertices.size());i+=vb;}
 for(auto&p:chunk.parts){chunk_require(uint64_t(p.first)+p.count<=chunk.indices.size());p.first+=ib;texture(p.texture);texture(p.floorNormal);texture(p.floorBlend);if(p.flags)texture(p.flags);for(auto&t:p.original.textures)texture(t.image);}
 if(target.vertices.empty())target.bounds=chunk.bounds;
 else for(int k=0;k<3;++k){target.bounds[k]=std::min(target.bounds[k],chunk.bounds[k]);target.bounds[k+3]=std::max(target.bounds[k+3],chunk.bounds[k+3]);}
 target.overviewBounds=target.bounds;target.hasOverviewBounds=true;
 target.vertices.insert(target.vertices.end(),chunk.vertices.begin(),chunk.vertices.end());
 target.indices.insert(target.indices.end(),chunk.indices.begin(),chunk.indices.end());
 target.parts.insert(target.parts.end(),std::make_move_iterator(chunk.parts.begin()),std::make_move_iterator(chunk.parts.end()));
 target.textures.insert(target.textures.end(),std::make_move_iterator(chunk.textures.begin()),std::make_move_iterator(chunk.textures.end()));
 chunk_cancel(stop);
}
std::shared_ptr<CharacterModel> load_stage_chunks(const std::filesystem::path&manifest,std::stop_token stop,ChunkLimits limits){
 chunk_cancel(stop);chunk_require(limits.count&&limits.count<=32&&limits.fileBytes<=64u*1024*1024&&limits.totalBytes<=512u*1024*1024&&limits.decodedBytes<=1024u*1024*1024);
 const auto root=std::filesystem::canonical(manifest.parent_path());
 chunk_require(std::filesystem::canonical(manifest).parent_path()==root&&std::filesystem::is_regular_file(manifest)&&std::filesystem::file_size(manifest)<=16384);
 auto base=manifest.filename().string();constexpr std::string_view suffix=".chunks.cfg";
 chunk_require(base.ends_with(suffix));base.resize(base.size()-suffix.size());
 chunk_require(!base.empty()&&base.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")==std::string::npos);
 std::ifstream input(manifest);std::string word;unsigned version=0;size_t count=0;
 chunk_require(bool(input>>word>>version)&&word==mgo2mt::brand::Format{"MGO2MT.STAGE_CHUNKS"}&&version==1);
 chunk_require(bool(input>>word>>count)&&word=="COUNT"&&count&&count<=limits.count);
 struct Row{std::filesystem::path path;std::string sha;std::array<float,6> bounds;size_t bytes;};
 std::vector<Row>rows;std::set<std::string>seen;size_t total=std::filesystem::file_size(manifest);chunk_require(total<=limits.totalBytes);
 auto account=[&](const std::filesystem::path&p,size_t maximum,bool optional){
  if(optional&&!std::filesystem::exists(p))return size_t(0);
  chunk_require(std::filesystem::is_regular_file(p)&&std::filesystem::canonical(p).parent_path()==root);
  auto n=std::filesystem::file_size(p);chunk_require(n<=maximum&&n<=limits.totalBytes-total);total+=size_t(n);return size_t(n);
 };
 for(size_t i=0;i<count;++i){chunk_cancel(stop);std::string file;Row row;
  chunk_require(bool(input>>word>>file>>row.sha)&&word=="CHUNK");
  chunk_require(file.size()<=96&&file.starts_with(base)&&file.ends_with(".gwm")&&file.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.-")==std::string::npos&&file.find("..") == std::string::npos&&seen.insert(file).second);
  chunk_require(row.sha.size()==64&&row.sha.find_first_not_of("0123456789abcdef")==std::string::npos);
  for(auto&v:row.bounds)chunk_require(bool(input>>v)&&std::isfinite(v)&&std::abs(v)<=1000000);
  for(int k=0;k<3;++k)chunk_require(row.bounds[k]<=row.bounds[k+3]);
  row.path=root/file;row.bytes=account(row.path,limits.fileBytes,false);chunk_require(row.bytes>=48);
  for(auto ext:{".gwn",".gfb",".gsa"}){auto p=row.path;p.replace_extension(ext);account(p,ext==std::string_view(".gwn")?8000044:ext==std::string_view(".gsa")?48+4096*24:64u*1024*1024,true);}
  rows.push_back(std::move(row));
 }
 chunk_require(bool(input>>word)&&word=="END");chunk_require(!(input>>word));
 auto result=std::make_shared<CharacterModel>();size_t decoded=0;
 for(const auto&row:rows){chunk_cancel(stop);std::ifstream f(row.path,std::ios::binary|std::ios::ate);chunk_require(f&&f.tellg()==std::streamoff(row.bytes));
  std::vector<char>bytes(row.bytes);f.seekg(0);for(size_t at=0;at<bytes.size();){chunk_cancel(stop);auto n=std::min<size_t>(65536,bytes.size()-at);chunk_require(bool(f.read(bytes.data()+at,n)));at+=n;}
  std::array<unsigned char,32>sha{};chunk_require(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,reinterpret_cast<PUCHAR>(bytes.data()),ULONG(bytes.size()),sha.data(),32)>=0);
  constexpr char hex[]="0123456789abcdef";std::string digest;for(auto c:sha){digest+=hex[c>>4];digest+=hex[c&15];}chunk_require(digest==row.sha);chunk_cancel(stop);
  CharacterModel model(bytes);for(int k=0;k<6;++k)chunk_require(std::abs(model.bounds[k]-row.bounds[k])<=std::max(.001f,std::abs(model.bounds[k])*1e-6f));
  auto sidecar=row.path;sidecar.replace_extension(".gwn");load_original_normals(model,bytes,sidecar);chunk_cancel(stop);
  sidecar.replace_extension(".gfb");load_floor_blend(model,bytes,sidecar);chunk_cancel(stop);
  sidecar.replace_extension(".gsa");load_surface_alpha(model,bytes,sidecar);chunk_cancel(stop);
  auto n=decoded_size(model);chunk_require(n<=limits.decodedBytes-decoded);decoded+=n;append_stage_chunk(*result,std::move(model),stop);
 }return result;
}
// Reconstruct from authored lights on each complete snapshot. A broken lamp
// wins when original light spheres overlap, independent of registry order.
static void apply_light_changes(Lighting& lighting,const std::vector<LightChange>& changes){
 for(bool enabled:{true,false})for(const auto& change:changes)if(change.enabled==enabled){
  if(change.key)lighting.enable(change.key,change.id,enabled);
  if(change.radius>0)lighting.enable_sphere(change.center,change.radius,enabled);
 }
}
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
 for(auto part:m.parts){part.first+=indexBase;part.texture+=textureBase;if(part.floorNormal!=noMaterialTexture)part.floorNormal+=textureBase;if(part.floorBlend!=noMaterialTexture)part.floorBlend+=textureBase;if(part.flags)part.flags+=textureBase;for(auto&t:part.original.textures)if(t.image!=noMaterialTexture)t.image+=textureBase;result.parts.push_back(std::move(part));}result.textures.insert(result.textures.end(),m.textures.begin(),m.textures.end());result.hasOverviewBounds=true;
}
static std::shared_ptr<const CharacterModel> placement_preview(const Result&r){
 if(r.objectSnapshot)return {}; // never overlay speculative intact props on received destruction
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
  if(!model)model=std::make_shared<CharacterModel>(r.objectModel?*r.objectModel:*r.model);append_model(*model,*found->second,item.position(),item.degrees(),r.lighting.get());
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
 if(old.received||old.objectSnapshot){old.round.transientLights.clear();received_preview(old);old.debugModel=placement_preview(old);std::lock_guard lock(mutex_);result_=std::move(old);return;}
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
 auto old=result();if(old.objectSnapshot||old.request!=request||!old.model||!old.authoredLighting||old.status!=Status::preview_ready||changes.size()>224)return false;
 for(const auto&change:changes){if(!std::isfinite(change.radius)||change.radius<0||change.radius>1000000)return false;for(auto c:change.center)if(!std::isfinite(c)||std::abs(c)>1000000)return false;}
 auto light=std::make_shared<Lighting>(*old.authoredLighting);
 apply_light_changes(*light,changes);
 auto model=std::make_shared<CharacterModel>(*old.model);light->sample_vertices(model->vertices);
 old.model=std::move(model);old.lighting=std::move(light);received_preview(old);old.debugModel=placement_preview(old);std::lock_guard lock(mutex_);result_=std::move(old);return true;
}
bool Assets::object_lights(const host::LoadRequest&request,const host::ObjectStates&states,const std::vector<ObjectLightBinding>&bindings){
 if(!states.complete()||bindings.size()>224)return false;
 std::vector<LightChange>changes;for(const auto&binding:bindings){if(binding.index>=states.values().size()||states.widths()[binding.index]!=1)return false;
  // 5A1AF8/5A2868 one-bit destructibles: nonzero means already broken.
  changes.push_back({binding.key,~0u,states.values()[binding.index]==0,binding.center,binding.radius});
 }return light_states(request,changes);
}
bool Assets::object_states(const SceneSnapshot&snapshot,std::span<const ObjectBinding>bindings){
 auto old=result();if(old.request!=snapshot.request||old.status!=Status::preview_ready||!old.model||!snapshot.revision||snapshot.objects.size()>224||bindings.size()>224)return false;
 if(old.objectSnapshot){if(snapshot.revision<old.objectSnapshot->revision)return false;if(snapshot.revision==old.objectSnapshot->revision)return snapshot==*old.objectSnapshot;}
 try{
  std::vector<ObjectBinding> resolved(bindings.begin(),bindings.end());for(auto&b:resolved)if(b.cboxOrdinal>=0){if(size_t(b.cboxOrdinal)>=old.cboxes.size())return false;auto&placement=old.cboxes[size_t(b.cboxOrdinal)];b.position=placement.anchor.position;b.degrees={0,placement.rotationRadians*180.f/3.14159265359f,0};}bindings=resolved;
  std::map<uint32_t,SceneObjectState> states;for(auto s:snapshot.objects)if(!s.bindingId||!states.emplace(s.bindingId,s).second)return false;
  if(old.objectSnapshot){if(snapshot.objects.size()!=old.objectSnapshot->objects.size())return false;for(auto s:old.objectSnapshot->objects){auto f=states.find(s.bindingId);if(f==states.end()||f->second.initial!=s.initial)return false;}}
  if(states.size()!=bindings.size())return false;
  std::set<uint32_t>seen,components;std::vector<CollisionInstance>colliders,hits;std::vector<std::pair<const ObjectBinding*,const ObjectPartBinding*>>parts;
  auto lighting=old.authoredLighting?std::make_shared<Lighting>(*old.authoredLighting):nullptr;std::vector<LightChange> lightChanges;
  auto validPosition=[](Vec3 p){for(float x:p)if(!std::isfinite(x)||std::abs(x)>=1000000)return false;return true;};
  for(const auto&b:bindings){auto found=states.find(b.bindingId);if(!b.bindingId||!seen.insert(b.bindingId).second||found==states.end()||!b.width||b.width>8||!validPosition(b.position)||!validPosition(b.degrees)||b.parts.size()>16||b.lights.size()>16)return false;
   unsigned maximum=(1u<<b.width)-1;auto state=found->second;if(state.current>maximum||state.initial>maximum)return false;
   for(const auto&p:b.parts){if(!p.componentId||!components.insert(p.componentId).second||p.mask>maximum||(p.value&~p.mask)||(!p.model&&!p.collision)||(p.placement&&(!validPosition(p.placement->position)||!validPosition(p.placement->degrees))))return false;
    if((state.current&p.mask)==p.value){if(p.model)parts.push_back({&b,&p});if(p.collision)(p.hitOnly?hits:colliders).push_back({p.componentId,p.collision,p.placement?p.placement->position:b.position,p.placement?p.placement->degrees:b.degrees});}}
   for(const auto&r:b.lights){auto&l=r.light;if(!r.mask||r.mask>maximum||(r.value&~r.mask)||!lighting||!std::isfinite(l.radius)||l.radius<0||l.radius>1000000||!validPosition(l.center))return false;
    bool enabled=(state.current&r.mask)==r.value?l.enabled:!l.enabled;
    auto change=l;change.enabled=enabled;lightChanges.push_back(change);}
  }
  if(lighting)apply_light_changes(*lighting,lightChanges);
  auto model=std::make_shared<CharacterModel>(*old.model);
  if(lighting)lighting->sample_vertices(model->vertices);
  for(auto[b,p]:parts)append_model(*model,*p->model,p->placement?p->placement->position:b->position,p->placement?p->placement->degrees:b->degrees,lighting.get());
  auto base=old.authoredCollision?old.authoredCollision:old.collision;
  if(!colliders.empty()&&!base)return false;
  auto collision=base?(colliders.empty()?base:std::make_shared<const Collision>(Collision::combine(*base,colliders))):nullptr;
  old.objectHitCollision=hits.empty()?nullptr:std::make_shared<const Collision>(Collision::combine(Collision::make({},{}),hits));
  old.objectModel=std::move(model);old.collision=std::move(collision);if(lighting)old.lighting=std::move(lighting);old.objectSnapshot=snapshot;old.activeObjectComponents=parts.size();received_preview(old);old.debugModel.reset();
  std::lock_guard lock(mutex_);if(result_.request!=snapshot.request||result_.generation!=old.generation)return false;result_=std::move(old);return true;
 }catch(...){return false;}
}
bool Assets::object_states(const SceneSnapshot&snapshot){auto current=result();return current.objectBindings&&object_states(snapshot,*current.objectBindings);}
void Assets::select(std::optional<host::LoadRequest> request){
 auto old=result();if(old.request==request)return;
 worker_.request_stop();if(worker_.joinable())worker_.join();
 Result next;next.request=request;next.generation=request?++generation_:0;
 if(request){
  if(name(request->rotation.map).empty())next.status=Status::unknown_map;
  else if(root_.empty()||!runtime_stage_supported(request->rotation.map))next.status=Status::unavailable;
  else if(old.model&&old.request&&old.request->rotation.map==request->rotation.map&&old.lighting==old.authoredLighting){next.status=Status::preview_ready;next.model=old.model;next.surfaceLayers=old.surfaceLayers;next.skyModel=old.skyModel;next.skySettings=old.skySettings;next.collision=old.authoredCollision?old.authoredCollision:old.collision;next.authoredCollision=next.collision;next.round=old.round.reset(next.generation);next.props=old.props;next.lighting=old.lighting;next.authoredLighting=old.authoredLighting;next.itemModels=old.itemModels;next.objectBindings=old.objectBindings;next.cboxLayout=old.cboxLayout;if(next.cboxLayout)next.cboxes=next.cboxLayout->select(request->generation);next.debugModel=placement_preview(next);}
  else next.status=Status::loading;
 }
 {std::lock_guard lock(mutex_);result_=next;}
 if(next.status!=Status::loading)return;
 // The filename is selected locally, never constructed from host-controlled text.
 auto path=asset_path(root_,request->rotation.map,".gwm");
 auto generation=next.generation;
 try{worker_=std::jthread([this,path,request,generation](std::stop_token stop){
  Result finished;finished.request=request;finished.status=Status::invalid;finished.generation=generation;
  try{
   auto chunks=path;chunks.replace_extension(".chunks.cfg");auto scene=path;scene.replace_extension(".scene.json");
   std::ifstream in(path,std::ios::binary|std::ios::ate);
   if(!in&&std::filesystem::exists(path))throw std::runtime_error("Stage preview open");
   if(!in&&!std::filesystem::exists(chunks)&&!std::filesystem::exists(scene))finished.status=Status::unavailable;
   else{
    std::shared_ptr<CharacterModel>model;
    if(!in)model=load_stage_chunks(chunks,stop);
    else{
    auto size=in.tellg();if(size<48||size>64*1024*1024)throw std::runtime_error("Stage preview extent");
    std::vector<char>bytes(static_cast<size_t>(size));in.seekg(0);
    for(size_t at=0;at<bytes.size();){if(stop.stop_requested())return;auto n=std::min<size_t>(65536,bytes.size()-at);if(!in.read(bytes.data()+at,n))throw std::runtime_error("Stage preview read");at+=n;}
    if(stop.stop_requested())return;
    model=std::make_shared<CharacterModel>(bytes);
    load_original_normals(*model,bytes,asset_path(path.parent_path(),request->rotation.map,".gwn"));load_floor_blend(*model,bytes,asset_path(path.parent_path(),request->rotation.map,".gfb"));load_surface_alpha(*model,bytes,asset_path(path.parent_path(),request->rotation.map,".gsa"));
    }
    auto lightPath=asset_path(path.parent_path(),request->rotation.map,".lighting.cfg");
    if(std::filesystem::exists(lightPath)){
     if(std::filesystem::file_size(lightPath)>4*1024*1024)throw std::runtime_error("Stage lighting extent");
     std::ifstream input(lightPath);auto lighting=Lighting::read(input);
     lighting.sample_vertices(model->vertices,stop);if(stop.stop_requested())return;
     model->overviewBounds=lighting.cameraBounds;model->hasOverviewBounds=true;
     finished.lighting=std::make_shared<const Lighting>(std::move(lighting));finished.authoredLighting=finished.lighting;
    }
    // Optional until the active GCX preset has an evidenced model binding.
    // A present but malformed sky invalidates this load; it must not leak the
    // previous stage's sky or become a silently accepted partial asset.
    auto skyPath=asset_path(path.parent_path(),request->rotation.map,".sky.gwm");
    if(std::filesystem::exists(skyPath)){
     std::ifstream input(skyPath,std::ios::binary|std::ios::ate);auto n=input.tellg();if(n<48||n>8*1024*1024)throw std::runtime_error("Stage sky extent");
     std::vector<char>b(static_cast<size_t>(n));input.seekg(0);if(!input.read(b.data(),n))throw std::runtime_error("Stage sky read");
     auto sky=std::make_shared<CharacterModel>(b,ModelExtent::sky);
     const auto settingsPath=asset_path(path.parent_path(),request->rotation.map,".sky.cfg");
     if(std::filesystem::exists(settingsPath)){
      if(std::filesystem::file_size(settingsPath)>2*1024*1024)throw std::runtime_error("Stage sky settings extent");
      std::ifstream settingsInput(settingsPath);auto settings=SkySettings::read(settingsInput);settings.prepare(*sky);
      finished.skySettings=std::make_shared<const SkySettings>(std::move(settings));
     }
     finished.skyModel=std::move(sky);
    }
    auto collisionPath=asset_path(path.parent_path(),request->rotation.map,".collision.cfg");
    if(std::filesystem::exists(collisionPath)){
     if(std::filesystem::file_size(collisionPath)>32*1024*1024)throw std::runtime_error("Stage collision extent");
     std::ifstream input(collisionPath);finished.collision=std::make_shared<const Collision>(Collision::read(input));finished.authoredCollision=finished.collision;
    }
    auto placementPath=asset_path(path.parent_path(),request->rotation.map,".placements.cfg");
    if(std::filesystem::exists(placementPath)){if(std::filesystem::file_size(placementPath)>1024*1024)throw std::runtime_error("Placement extent");std::ifstream input(placementPath);finished.round=Round::read(input).reset(generation);}
    auto cboxPath=asset_path(path.parent_path(),request->rotation.map,".cbox.cfg");
    if(std::filesystem::exists(cboxPath)){
     if(std::filesystem::file_size(cboxPath)>16384)throw std::runtime_error("CBOX layout extent");
     std::ifstream input(cboxPath);finished.cboxLayout=std::make_shared<const CboxLayout>(CboxLayout::read(input));
     finished.cboxes=finished.cboxLayout->select(request->generation);
    }
    if(std::filesystem::exists(asset_path(path.parent_path(),request->rotation.map,".bindings.cfg")))finished.objectBindings=std::make_shared<const std::vector<ObjectBinding>>(read_object_bindings(path.parent_path(),true,request->rotation.map));
    // Prepare only the immutable architecture copy, before appending dynamic
    // actors/items. Original bytes and separate collision triangles stay intact.
    finished.surfaceLayers=separate_stage_surfaces(*model);
    if(stop.stop_requested())return;
    finished.model=std::move(model);
    if(!finished.round.objects.empty())for(unsigned i=0;i<6;++i){if(stop.stop_requested())return;auto propPath=path.parent_path()/"props"/(std::to_string(i)+".gwm");std::ifstream prop(propPath,std::ios::binary|std::ios::ate);if(!prop)throw std::runtime_error("Missing stage prop");auto n=prop.tellg();if(n<48||n>4*1024*1024)throw std::runtime_error("Stage prop extent");std::vector<char>b(static_cast<size_t>(n));prop.seekg(0);if(!prop.read(b.data(),n))throw std::runtime_error("Stage prop read");finished.props.push_back(std::make_shared<const CharacterModel>(b));}
    // Original B06E20 model switch: 113=ibox_item_large, 140=ibox_item_small.
    // Converted files are fixed local IDs, never paths supplied by a peer.
    for(unsigned id:{113,140}){auto itemPath=path.parent_path()/"items"/(std::to_string(id)+".gwm");if(!std::filesystem::exists(itemPath))continue;
     std::ifstream input(itemPath,std::ios::binary|std::ios::ate);auto n=input.tellg();if(n<48||n>4*1024*1024)throw std::runtime_error("Stage item extent");std::vector<char>b(static_cast<size_t>(n));input.seekg(0);if(!input.read(b.data(),n))throw std::runtime_error("Stage item read");finished.itemModels.emplace(uint8_t(id),std::make_shared<const CharacterModel>(b));}
    finished.debugModel=placement_preview(finished);finished.status=Status::preview_ready;
   }
  }catch(...){finished.skySettings.reset();finished.skyModel.reset();finished.model.reset();finished.debugModel.reset();finished.collision.reset();finished.cboxLayout.reset();finished.cboxes.clear();finished.status=Status::invalid;}
  if(stop.stop_requested())return;
  std::lock_guard lock(mutex_);result_=std::move(finished);
 });}catch(...){std::lock_guard lock(mutex_);result_.status=Status::unavailable;}
}
}
