#include "mortar_shell_renderer.h"
#include "character_renderer.h"
#include "shadow_renderer.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace mgo2mt::mortar_shells {
namespace {
Vec3 add(Vec3 a,Vec3 b){for(unsigned j=0;j<3;++j)a[j]+=b[j];return a;}
Vec3 sub(Vec3 a,Vec3 b){for(unsigned j=0;j<3;++j)a[j]-=b[j];return a;}
Vec3 mul(Vec3 a,float s){for(auto& v:a)v*=s;return a;}
Vec3 cross(Vec3 a,Vec3 b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
float length(Vec3 a){return std::hypot(a[0],a[1],a[2]);}
bool finite(Vec3 a){for(float v:a)if(!std::isfinite(v)||std::abs(v)>1.e8f)return false;return true;}
bool unit(Vec3 a){return finite(a)&&std::abs(length(a)-1)<.001f;}
bool valid_type(const mounted::Type& t){return t.kind==mounted::Kind::mortar&&t.weapon==103&&
 std::isfinite(t.launchSpeed)&&t.launchSpeed>=100&&t.launchSpeed<=100000&&
 std::isfinite(t.gravity)&&t.gravity>=100&&t.gravity<=50000&&
 std::isfinite(t.blastRadius)&&t.blastRadius>=100&&t.blastRadius<=20000&&
 t.maxFlightMs>=100&&t.maxFlightMs<=30000;}
bool same_flight(const mounted::Type& a,const mounted::Type& b){return a.launchSpeed==b.launchSpeed&&a.gravity==b.gravity&&a.maxFlightMs==b.maxFlightMs&&a.blastRadius==b.blastRadius;}
const combat::Player* owner(const combat::Snapshot& s,combat::Identity id,uint32_t life){
 if(id.slot>=s.players.size()||!id.instance||!id.character||!life)return nullptr;
 const auto& p=s.players[id.slot];return p&&p->identity==id&&p->life==life?&*p:nullptr;
}
const mounted::Type* find_type(const mounted::Registry& registry,const combat::Event& e,const combat::Player& player,uint8_t map){
 auto type_for=[&](uint32_t id)->const mounted::Type*{
  if(id>UINT16_MAX)return nullptr;
  for(const auto& i:registry.placements)if(i.map==map&&i.id==id)
   for(const auto& t:registry.types)if(t.id==i.type)return valid_type(t)?&t:nullptr;
  return nullptr;
 };
 // New accepted events carry their emplacement ID. A nonzero but invalid ID
 // must not silently select an unrelated device after a configuration change.
 if(e.object)return type_for(e.object);
 if(player.mountedId)return type_for(player.mountedId);
 // Legacy event compatibility: only an unambiguous common flight policy is
 // allowed after the operator dismounts. Never guess the nearest emplacement.
 const mounted::Type* candidate=nullptr;
 for(const auto& i:registry.placements)if(i.map==map)for(const auto& t:registry.types)if(t.id==i.type&&t.kind==mounted::Kind::mortar){
  if(!valid_type(t)||(candidate&&!same_flight(*candidate,t)))return nullptr;
  candidate=&t;
 }
 return candidate;
}
CharacterModel read_model(const std::filesystem::path& path){
 std::ifstream file(path,std::ios::binary|std::ios::ate);
 if(!file||file.tellg()<=0||uint64_t(file.tellg())>16*1024*1024)throw std::runtime_error("Mortar shell model missing or too large");
 std::vector<char> bytes(size_t(file.tellg()));file.seekg(0);
 if(!file.read(bytes.data(),std::streamsize(bytes.size())))throw std::runtime_error("Mortar shell model read failed");
 return CharacterModel(bytes);
}
}
Simulation::Simulation(float range):range_(range){if(!std::isfinite(range)||range<=0||range>1000000)throw std::invalid_argument("Mortar shell range");}
void Simulation::clear(){flights_.fill({});epoch_=scene_=now_=eventFloor_=0;map_=0;initialized_=false;++generation_;}
size_t Simulation::size()const{return size_t(std::count_if(flights_.begin(),flights_.end(),[](const auto& f){return bool(f);}));}
void Simulation::advance(const stage::Collision& world,uint64_t now){
 for(auto& slot:flights_)if(slot){auto& f=*slot;
  if(now<f.at||now-f.at>1000||now<f.born||now-f.born>=f.ttlMs){slot.reset();continue;}
  bool done=false;
  // Match the HOST's bounded 20 ms ballistic point segments. A delayed local
  // render starts at receipt time; no untrusted client clock correction occurs.
  while(f.at<now){const auto ms=std::min(uint64_t(20),now-f.at);const float dt=float(ms)*.001f;
   Vec3 displacement=mul(f.velocity,dt);displacement[1]-=.5f*f.gravity*dt*dt;
   const float distance=length(displacement),remaining=range_-f.traveled;
   if(!finite(displacement)||!std::isfinite(distance)||remaining<=0){done=true;break;}
   const float travel=std::min(distance,remaining);const auto direction=distance>0?mul(displacement,1/distance):Vec3{0,0,1};
   f.traceFrom=f.position;f.traceTo=add(f.position,mul(direction,travel));
   if(travel>0&&world.ray(f.position,direction,travel,stage::query::bomb)){done=true;break;}
   f.position=f.traceTo;f.velocity[1]-=f.gravity*dt;f.traveled+=travel;f.at+=ms;
   if(!finite(f.position)||!finite(f.velocity)||travel<distance||f.traveled>=range_){done=true;break;}
  }
  if(done)slot.reset();
 }
}
void Simulation::update(std::span<const combat::Event> events,const combat::Snapshot* snapshot,const mounted::Registry& registry,
                        uint8_t map,const std::shared_ptr<const stage::Collision>& world,uint64_t now,uint64_t scene){
 if(!snapshot||!snapshot->epoch||!scene||!map||!world){clear();return;}
 const bool replacement=initialized_&&(epoch_!=snapshot->epoch||scene_!=scene||map_!=map||now<now_);
 if(replacement){clear();epoch_=snapshot->epoch;scene_=scene;map_=map;now_=now;initialized_=true;eventFloor_=snapshot->eventWatermark;return;}
 if(!initialized_){epoch_=snapshot->epoch;scene_=scene;map_=map;initialized_=true;}
 now_=now;
 for(auto& f:flights_)if(f&&!owner(*snapshot,f->source,f->life))f.reset();
 advance(*world,now);
 // Events can arrive in a batch with explosion before shot in caller storage.
 // Sort only the bounded relevant IDs; old/replayed events never resurrect a
 // shell. Excessive batches are discarded rather than unbounded allocation.
 if(events.size()>4096){eventFloor_=std::max(eventFloor_,snapshot->eventWatermark);return;}
 std::vector<const combat::Event*> ordered;ordered.reserve(events.size());
 for(const auto& e:events)if(e.epoch==epoch_&&e.id>eventFloor_&&e.id<=snapshot->eventWatermark&&e.weapon==103&&
   (e.kind==combat::EventKind::shot||e.kind==combat::EventKind::explosion))ordered.push_back(&e);
 std::sort(ordered.begin(),ordered.end(),[](const auto* a,const auto* b){return a->id<b->id;});
 for(const auto* event:ordered){const auto& e=*event;if(e.id<=eventFloor_)continue;eventFloor_=e.id;
  if(!finite(e.position))continue;
  if(e.kind==combat::EventKind::explosion){
   // The HOST does not yet transmit acceptedShotId on an explosion. Remove
   // nearby shells of this exact firing incarnation, using the configured
   // blast radius. Never let another player's explosion erase their shots.
   for(auto& f:flights_)if(f&&f->source==e.source&&f->life==e.sourceLife&&length(sub(f->position,e.position))<=f->blastRadius)f.reset();
   continue;
  }
  const auto* player=owner(*snapshot,e.source,e.sourceLife);if(!player||!unit(e.normal))continue;
  const auto* type=find_type(registry,e,*player,map);if(!type||now>UINT64_MAX-type->maxFlightMs)continue;
  const auto free=std::find_if(flights_.begin(),flights_.end(),[](const auto& f){return !f;});if(free==flights_.end())continue;
  *free=Flight{e.id,now,now,e.source,e.sourceLife,type->maxFlightMs,e.position,mul(e.normal,type->launchSpeed),e.position,e.position,type->gravity,0,type->blastRadius};
 }
}
void orient_vertices(std::span<const ModelVertex> input,Vec3 velocity,std::span<ModelVertex> output){
 const float n=length(velocity);if(input.size()!=output.size()||!finite(velocity)||!std::isfinite(n)||n<.000001f)throw std::invalid_argument("Mortar shell orientation");
 const auto d=mul(velocity,1/n);Vec3 q{d[2],0,-d[0]};float w=1+d[1];const float qn=std::hypot(length(q),w);
 if(qn<.000000000001f){q={1,0,0};w=0;}else{q=mul(q,1/qn);w/=qn;}
 const auto turn=[&](Vec3 p){return add(p,mul(cross(q,add(cross(q,p),mul(p,w))),2));};
 for(size_t i=0;i<input.size();++i){auto v=input[i];const auto p=turn({v.x,v.y,v.z}),normal=turn({v.nx,v.ny,v.nz});
  v.x=p[0];v.y=p[1];v.z=p[2];v.nx=normal[0];v.ny=normal[1];v.nz=normal[2];output[i]=v;
 }
}
struct Renderer::Impl {
 Microsoft::WRL::ComPtr<ID3D11Device> device;
 CharacterModel model;
 Simulation simulation;
 struct Draw {std::unique_ptr<CharacterRenderer> renderer;std::vector<ModelVertex> vertices;};
 std::array<Draw,capacity> draws;
 uint64_t generation=0;
 Impl(ID3D11Device* d,const std::filesystem::path& path,float range):device(d),model(read_model(path)),simulation(range){if(!d)throw std::invalid_argument("Mortar shell device");}
};
Renderer::Renderer(ID3D11Device* device,const std::filesystem::path& path,float range):impl_(std::make_unique<Impl>(device,path,range)){}
Renderer::~Renderer()=default;
void Renderer::clear(){impl_->simulation.clear();for(auto& d:impl_->draws)d=Impl::Draw{};impl_->generation=impl_->simulation.generation();}
size_t Renderer::size()const{return impl_->simulation.size();}
const Simulation& Renderer::simulation()const{return impl_->simulation;}
void Renderer::update(ID3D11DeviceContext* context,std::span<const combat::Event> events,const combat::Snapshot* snapshot,const mounted::Registry& registry,
                      uint8_t map,const std::shared_ptr<const stage::Collision>& world,uint64_t now,uint64_t scene){
 if(!context){clear();return;}
 auto& p=*impl_;p.simulation.update(events,snapshot,registry,map,world,now,scene);
 if(p.generation!=p.simulation.generation()){for(auto& d:p.draws)d=Impl::Draw{};p.generation=p.simulation.generation();}
 for(size_t i=0;i<capacity;++i)if(const auto& flight=p.simulation.flights()[i]){auto& draw=p.draws[i];
  if(!draw.renderer){draw.renderer=std::make_unique<CharacterRenderer>(p.device.Get(),p.model);draw.renderer->resize_target(p.device.Get(),1,1);draw.vertices=p.model.vertices;}
  // At the exact apex a vertical shot may have zero velocity. Its previous
  // orientation remains valid until the next nonzero downward velocity.
  if(length(flight->velocity)>.000001f)orient_vertices(p.model.vertices,flight->velocity,draw.vertices);
  draw.renderer->update_vertices(context,draw.vertices);
 }
}
void Renderer::shadow_casters(std::vector<shadows::Caster>& out)const{
 const auto& p=*impl_;for(size_t i=0;i<capacity;++i)if(const auto& f=p.simulation.flights()[i])if(p.draws[i].renderer)out.push_back({p.draws[i].renderer.get(),0,f->position});
}
}
