#include "item_box_presentation.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>
namespace mgo2mt::item_box {
namespace {
bool finite(stage::Vec3 p){for(float v:p)if(!std::isfinite(v)||std::abs(v)>=1000000)return false;return true;}
bool valid(items::Position p){return finite({p.x,p.y,p.z})&&std::isfinite(p.yaw);}
float distance2(stage::Vec3 a,stage::Vec3 b){float v=0;for(unsigned i=0;i<3;++i)v+=(a[i]-b[i])*(a[i]-b[i]);return v;}
}
Asset asset(const items::Contents& contents,const weapons::Catalog* catalog){
 if(contents.domain==items::Domain::equipment){
  // Proved namespace joins: equipment10 -> world113 (large), equipment22 ->
  // world140 (small). Other equipment uses an explicit native small-box rule.
  if(!contents.item||contents.item>65535)return Asset::unavailable;
  return contents.item==10?Asset::large:Asset::smallBox;
 }
 switch(profile(contents,catalog).size){
  case Size::primary:return Asset::large;
  case Size::secondary:return Asset::medium;
  case Size::reserve:return Asset::smallBox;
  default:return Asset::unavailable;
 }
}
bool grounded(const items::Entity&e,Profile p,const stage::Collision&w){
 if(!valid(e.position)||!finite(p.halfExtent)||p.halfExtent[0]<=0||p.halfExtent[1]<=0||p.halfExtent[2]<=0)return false;const auto& pos=e.position;float c=std::cos(pos.yaw),s=std::sin(pos.yaw);
 for(auto corner:std::array<std::array<float,2>,5>{{{0,0},{-1,-1},{1,-1},{1,1},{-1,1}}}){
  float x=corner[0]*p.halfExtent[0]*.8f,z=corner[1]*p.halfExtent[2]*.8f;
  auto hit=w.ray({pos.x+x*c+z*s,pos.y+8,pos.z-x*s+z*c},{0,-1,0},16);
  if(hit&&hit->normal[1]>.7f&&std::abs(hit->position[1]-pos.y)<=4.01f){const auto attr=w.triangles[hit->triangle].attribute;if(!attr||(attr&0x10))return true;}
 }return false;
}
bool Presentation::load_catalog(const std::filesystem::path&p,std::string& error){if(!catalog_.load(p,error))return false;catalogReady_=true;tracks_.clear();return true;}
void Presentation::clear(){connection_=scene_=now_=revision_=0;scope_={};actor_={};tracks_.clear();}
std::vector<Box> Presentation::update(const items::ClientState&s,const stage::Collision&w,uint64_t scene,uint64_t now){
 if(!s.context.active||s.status!=items::ClientStatus::ready||!s.connection||!scene||!s.context.scope.epoch||!s.context.scope.generation||s.context.actor.slot>=24||!s.context.actor.instance||!s.context.actor.character||!s.context.actor.life||!s.world||s.world->scope!=s.context.scope||s.world->entities.size()>8192||!valid(s.context.position)){clear();return {};}
 if(connection_!=s.connection||scene_!=scene||scope_!=s.context.scope||actor_!=s.context.actor){clear();connection_=s.connection;scene_=scene;scope_=s.context.scope;actor_=s.context.actor;}
 if(now<now_||s.world->revision<revision_)return {};now_=now;revision_=s.world->revision;
 std::vector<const items::Entity*> visible;std::set<uint64_t> ids;
 for(const auto&e:s.world->entities){if(e.key.scope!=scope_||!e.key.id||!ids.insert(e.key.id).second||!valid(e.position)||!e.contents.item){clear();return {};}
  if(e.kind!=items::PlacementKind::installed&&distance2({e.position.x,e.position.y,e.position.z},{s.context.position.x,s.context.position.y,s.context.position.z})<=40000.f*40000.f)visible.push_back(&e);
 }
 std::sort(visible.begin(),visible.end(),[&](auto a,auto b){auto origin=stage::Vec3{s.context.position.x,s.context.position.y,s.context.position.z};auto ad=distance2({a->position.x,a->position.y,a->position.z},origin),bd=distance2({b->position.x,b->position.y,b->position.z},origin);return ad==bd?a->key.id<b->key.id:ad<bd;});
 if(visible.size()>maximum_visible)visible.resize(maximum_visible);
 std::map<uint64_t,Track> next;std::vector<Box> out;out.reserve(visible.size());
 for(auto e:visible){auto p=profile(e->contents,catalogReady_?&catalog_:nullptr);const bool ground=grounded(*e,p,w);auto at=tracks_.find(e->key.id);
  Track t{e->position,p.size,now,ground};if(at!=tracks_.end()&&ground&&at->second.grounded&&at->second.size==p.size)t.groundAt=at->second.groundAt;
  const float yaw=ground?std::remainder(e->position.yaw-float((now-t.groundAt)%1000)*.00628318530718f,6.28318530718f):e->position.yaw;
  out.push_back({e->key,{e->position.x,e->position.y,e->position.z},yaw,p.size,ground,asset(e->contents,catalogReady_?&catalog_:nullptr)});next.emplace(e->key.id,t);
 }tracks_=std::move(next);return out;
}
std::array<CharacterModel,3> original_models(const std::filesystem::path& root){
 std::array<CharacterModel,3> result;
 constexpr const char* names[]={"113.gwm","ibox_item_mid.gwm","140.gwm"};
 for(size_t i=0;i<result.size();++i){
  const auto path=root/names[i];std::ifstream in(path,std::ios::binary|std::ios::ate);const auto length=in.tellg();
  if(!in||length<48||length>4*1024*1024)throw std::runtime_error(std::string("Original item box missing or invalid extent: ")+names[i]);
  std::vector<char> bytes(static_cast<size_t>(length));in.seekg(0);
  if(!in.read(bytes.data(),length))throw std::runtime_error(std::string("Original item box read failed: ")+names[i]);
  try{result[i]=CharacterModel(bytes);}catch(const std::exception& error){throw std::runtime_error(std::string("Original item box invalid: ")+names[i]+": "+error.what());}
 }
 return result;
}
CharacterModel model(const CharacterModel& original,Size size){
 if(size>=Size::unknown||original.vertices.empty()||original.indices.empty()||original.parts.empty()||original.textures.empty())throw std::invalid_argument("Original item box model/category");
 const auto h=profile(size).halfExtent;std::array<float,3> dimensions{};
 for(size_t axis=0;axis<3;++axis){
  if(!std::isfinite(original.bounds[axis])||!std::isfinite(original.bounds[axis+3]))throw std::invalid_argument("Original item box bounds");
  dimensions[axis]=original.bounds[axis+3]-original.bounds[axis];
  if(!std::isfinite(dimensions[axis])||dimensions[axis]<=.001f||dimensions[axis]>1000000)throw std::invalid_argument("Original item box extent");
 }
 const float scale=(std::min)({2*h[0]/dimensions[0],2*h[1]/dimensions[1],2*h[2]/dimensions[2]});
 const float centerX=(original.bounds[0]+original.bounds[3])*.5f,centerZ=(original.bounds[2]+original.bounds[5])*.5f;
 CharacterModel result=original;
 for(auto& vertex:result.vertices){vertex.x=(vertex.x-centerX)*scale;vertex.y=(vertex.y-original.bounds[1])*scale;vertex.z=(vertex.z-centerZ)*scale;}
 result.bounds={-dimensions[0]*scale*.5f,0,-dimensions[2]*scale*.5f,dimensions[0]*scale*.5f,dimensions[1]*scale,dimensions[2]*scale*.5f};
 result.hasOverviewBounds=false;result.overviewBounds={};return result;
}
Renderer::Renderer(ID3D11Device*d,const std::filesystem::path& root){
 if(!d)throw std::invalid_argument("Item box device");const auto originals=original_models(root);
 for(const auto& pair:std::array<std::pair<Size,Asset>,5>{{{Size::primary,Asset::large},{Size::secondary,Asset::medium},{Size::reserve,Asset::smallBox},{Size::equipment,Asset::large},{Size::equipment,Asset::smallBox}}})
  models_[unsigned(pair.first)][unsigned(pair.second)]=std::make_unique<CharacterRenderer>(d,model(originals[unsigned(pair.second)],pair.first));
}
void Renderer::draw(ID3D11DeviceContext*c,std::span<const Box> boxes,CharacterRenderer&surface,const WorldView&view,std::span<const DynamicPointLight>lights,const shadows::Renderer* shadow,const stage::Lighting* environment){
 if(!c||boxes.size()>maximum_visible||!finite(view.eye)||!finite(view.direction)||distance2(view.direction,{0,0,0})<.000001f||!std::isfinite(view.aspect)||view.aspect<=0||view.aspect>32)throw std::invalid_argument("Item box view/capacity");
 validate_dynamic_lights(lights);for(const auto&b:boxes){
  if(b.size>Size::unknown||b.asset>Asset::unavailable||!finite(b.position)||!std::isfinite(b.yaw))throw std::invalid_argument("Item box geometry");
  if(b.asset!=Asset::unavailable&&(b.size==Size::unknown||!models_[unsigned(b.size)][unsigned(b.asset)]))throw std::invalid_argument("Item box original model/category mismatch");
 }
 for(const auto&b:boxes)if(b.asset!=Asset::unavailable){auto resolved=environment?environment->environment(b.position):EnvironmentLight{};models_[unsigned(b.size)][unsigned(b.asset)]->render(c,b.yaw,false,&view,&surface,&b.position,lights,nullptr,shadow,environment?&resolved:nullptr);}
}
void Renderer::shadow_casters(std::vector<shadows::Caster>& out,std::span<const Box> boxes)const{
 if(boxes.size()>maximum_visible)throw std::invalid_argument("Shadow item count");for(const auto&b:boxes){if(b.size>=Size::unknown||b.asset>=Asset::unavailable)continue;if(!finite(b.position)||!std::isfinite(b.yaw))throw std::invalid_argument("Shadow item position");if(auto*r=models_[unsigned(b.size)][unsigned(b.asset)].get())out.push_back({r,b.yaw,b.position});}
}

}
