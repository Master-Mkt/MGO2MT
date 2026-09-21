#include "item_drop_physics.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace mgo2mt::items {
namespace {
using V=stage::Vec3;
V add(V a,V b){for(int i=0;i<3;++i)a[i]+=b[i];return a;}
V sub(V a,V b){for(int i=0;i<3;++i)a[i]-=b[i];return a;}
V mul(V a,float b){for(auto&x:a)x*=b;return a;}
float dot(V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
V cross(V a,V b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
V center(Position p,V h){return {p.x,p.y+h[1],p.z};}
bool finite(V a){return std::all_of(a.begin(),a.end(),[](float x){return std::isfinite(x)&&std::abs(x)<1000000;});}
struct Hit{float t=1;V normal{};bool hit=false,budget=false;};
// Continuous separating-axis theorem: box axes, triangle normal and nine
// edge-cross-box axes. Double-sided; no names/material-derived solid guesses.
Hit triangle(V c,V h,V d,V a,V b,V z){
 const V edges[3]{sub(b,a),sub(z,b),sub(a,z)},basis[3]{{1,0,0},{0,1,0},{0,0,1}};
 V axes[13];size_t n=0;for(auto v:basis)axes[n++]=v;axes[n++]=cross(edges[0],edges[1]);for(auto e:edges)for(auto v:basis)axes[n++]=cross(e,v);
 float enter=-1e30f,leave=1e30f;V normal{};
 for(auto axis:axes){float length=std::sqrt(dot(axis,axis));if(length<1e-7f)continue;axis=mul(axis,1/length);
  const float pa=dot(sub(a,c),axis),pb=dot(sub(b,c),axis),pz=dot(sub(z,c),axis),r=std::abs(axis[0])*h[0]+std::abs(axis[1])*h[1]+std::abs(axis[2])*h[2];
  const float lo=(std::min)({pa,pb,pz})-r,hi=(std::max)({pa,pb,pz})+r,v=dot(d,axis);
  if(std::abs(v)<1e-9f){if(lo>0||hi<0)return {};continue;}
  float first=lo/v,last=hi/v;V entering=mul(axis,v>0?-1.f:1.f);if(first>last)std::swap(first,last);
  if(first>enter){enter=first;normal=entering;}leave=(std::min)(leave,last);if(enter>leave)return {};
 }
 if(leave<=0||enter>1||enter>leave)return {};
 if(enter<0&&dot(d,d)<1e-10f)return {0,{},true,false};
 return {(std::max)(0.f,enter),normal,true,false};
}
Hit sweep(const stage::Collision* world,V c,V h,V delta,size_t& budget){
 if(!world)return {};V lo{},hi{};for(int i=0;i<3;++i){lo[i]=(std::min)(c[i],c[i]+delta[i])-h[i]-1;hi[i]=(std::max)(c[i],c[i]+delta[i])+h[i]+1;}
 auto candidates=world->bounds_candidates(lo,hi);if(candidates.size()>budget)return {0,{},true,true};budget-=candidates.size();Hit best;
 for(auto index:candidates){if(index>=world->triangles.size())return {0,{},true,true};const auto&t=world->triangles[index];auto hit=triangle(c,h,delta,world->vertices[t.vertices[0]],world->vertices[t.vertices[1]],world->vertices[t.vertices[2]]);if(hit.hit&&(!best.hit||hit.t<best.t))best=hit;}
 return best;
}
Hit sweep(const stage::Collision* world,const stage::Collision* objects,V c,V h,V delta,size_t&budget){auto a=sweep(world,c,h,delta,budget),b=sweep(objects,c,h,delta,budget);if(a.budget||b.budget)return {0,{},true,true};return b.hit&&(!a.hit||b.t<a.t)?b:a;}
}
void DropPhysics::reset(){scope_={};clock_=0;clocked_=false;world_=objects_=nullptr;bodies_.clear();}
stage::Vec3 DropPhysics::extent(const Contents& c)const{auto h=item_box::profile(c,&catalog_).halfExtent;h[0]=h[2]=std::hypot(h[0],h[2]);return h;}
bool DropPhysics::clear(Position p,const Contents& c,const stage::Collision*w,const stage::Collision*o)const{auto h=extent(c);if(!finite(center(p,h)))return false;size_t budget=4096;return !sweep(w,o,center(p,h),h,{},budget).hit;}
std::vector<Movement> DropPhysics::advance(const SnapshotState& state,const stage::Collision*w,const stage::Collision*o,uint64_t now,bool active){
 if(state.scope!=scope_){reset();scope_=state.scope;}
 std::vector<Movement> moves;if(!scope_.epoch||!scope_.generation||state.entities.size()>8192)return moves;
 const bool changedWorld=world_!=w||objects_!=o;world_=w;objects_=o;
 std::set<uint64_t> present;for(const auto&e:state.entities)if(e.kind!=PlacementKind::installed&&e.key.scope==scope_){present.insert(e.key.id);if(bodies_.size()<4096&&!bodies_.contains(e.key.id))bodies_.emplace(e.key.id,Body{e.position,{},now,0,e.owner,e.kind==PlacementKind::round});}
 std::erase_if(bodies_,[&](const auto& p){return !present.contains(p.first);});
 if(clocked_&&now<clock_)return moves;
 const uint64_t elapsed=clocked_?(std::min)(now-clock_,uint64_t(100)):0;clock_=now;clocked_=true;
 if(!active||!w||!elapsed)return moves;size_t budget=65536;
 for(const auto&e:state.entities){auto found=bodies_.find(e.key.id);if(found==bodies_.end())continue;auto&b=found->second;
  // Registry mutation is authoritative, including correction after rejected move.
  if(b.position!=e.position){b.position=e.position;b.velocity={};b.grounded=false;}
  if(changedWorld){b.grounded=false;b.stopped=false;b.velocity={};b.simulatedMs=0;}
  if(b.stopped||b.grounded)continue;
  if(b.simulatedMs>=30000){b.stopped=true;b.velocity={};continue;}b.simulatedMs+=elapsed;
  auto h=extent(e.contents);uint64_t remaining=elapsed;
  while(remaining){const uint64_t step=(std::min)(remaining,uint64_t(16));remaining-=step;const float dt=float(step)*.001f;b.velocity[1]=(std::max)(-20000.f,b.velocity[1]-9800.f*dt);V delta=mul(b.velocity,dt);
   auto hit=sweep(w,o,center(b.position,h),h,delta,budget);if(hit.budget){b.velocity={};break;}
   float fraction=hit.hit?(std::max)(0.f,hit.t-.0001f):1.f;V p=add(V{b.position.x,b.position.y,b.position.z},mul(delta,fraction));if(!finite(p)){b.stopped=true;b.velocity={};break;}
   b.position.x=p[0];b.position.y=p[1];b.position.z=p[2];
   if(hit.hit){const float into=dot(b.velocity,hit.normal);if(into<0)b.velocity=sub(b.velocity,mul(hit.normal,1.2f*into));else b.velocity={};
    if(hit.normal[1]>.7f&&std::abs(into)<500){b.grounded=true;b.velocity={};break;}
   }
  }
  if(b.position!=e.position)moves.push_back({e.key,e.revision,b.position});
 }
 return moves;
}
bool DropPhysics::contact(const Entity&e,Actor actor,V feet,stage::Capsule capsule,uint64_t now){
 auto found=bodies_.find(e.key.id);if((clocked_&&now<clock_)||e.key.scope!=scope_||found==bodies_.end()||e.kind==PlacementKind::installed||!finite(feet)||!std::isfinite(capsule.radius)||!std::isfinite(capsule.height)||capsule.radius<=0||capsule.height<2*capsule.radius)return false;
 auto&b=found->second;auto h=extent(e.contents);V c=center(e.position,h);const float lower=feet[1]+capsule.radius,upper=feet[1]+capsule.height-capsule.radius;
 float d2=0;for(int i:{0,2}){const float d=(std::max)(0.f,std::abs(feet[i]-c[i])-h[i]);d2+=d*d;}
 const float dy=(std::max)({0.f,c[1]-h[1]-upper,lower-c[1]-h[1]});d2+=dy*dy;const bool overlap=d2<=capsule.radius*capsule.radius;
 if(actor==b.owner&&!overlap)b.ownerExited=true;
 const bool grace=e.kind==PlacementKind::round||(now>=b.born&&now-b.born>=750);
 return overlap&&grace&&(actor!=b.owner||b.ownerExited);
}
bool DropPhysics::grounded(EntityKey key)const{auto it=bodies_.find(key.id);return key.scope==scope_&&it!=bodies_.end()&&it->second.grounded;}
}
