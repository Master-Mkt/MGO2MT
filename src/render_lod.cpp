#include "render_lod.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <stdexcept>
#include <unordered_map>
namespace mgo2mt::render_lod {
namespace {
struct P {double x,y,z;};
P point(const ModelVertex&v){return {v.x,v.y,v.z};}P sub(P a,P b){return {a.x-b.x,a.y-b.y,a.z-b.z};}P cross(P a,P b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}double dot(P a,P b){return a.x*b.x+a.y*b.y+a.z*b.z;}double length(P a){return std::sqrt(dot(a,a));}
struct Local {std::vector<uint32_t> used,tri,component;std::vector<bool> locked;};
Local topology(const CharacterModel&m,std::span<const uint32_t>indices){
 // MDN conversion preserves triangle-strip duplicate vertices. Reconstruct
 // connectivity only for exactly matching complete attributes inside one part.
 // A UV/normal/color seam therefore remains a locked boundary. No source vertex
 // is changed: canonical IDs continue to reference the immutable original VB.
 Local l;std::vector<uint32_t>all(indices.begin(),indices.end());std::sort(all.begin(),all.end());all.erase(std::unique(all.begin(),all.end()),all.end());std::vector<uint32_t>canonical;canonical.reserve(all.size());
 std::map<std::array<float,20>,uint32_t>identical;
 for(auto id:all){const auto&v=m.vertices[id];std::array<float,20>key{v.x,v.y,v.z,v.nx,v.ny,v.nz,v.u,v.v,v.u1,v.v1,v.lr,v.lg,v.lb,v.lit,v.ar,v.ag,v.ab,v.aa,v.u2,v.v2};auto [it,inserted]=identical.emplace(key,uint32_t(l.used.size()));canonical.push_back(it->second);if(inserted)l.used.push_back(id);}
 l.locked.resize(l.used.size());l.component.resize(l.used.size());std::iota(l.component.begin(),l.component.end(),0u);
 auto find=[&](uint32_t a){while(l.component[a]!=a){l.component[a]=l.component[l.component[a]];a=l.component[a];}return a;};
 l.tri.reserve(indices.size());for(auto v:indices)l.tri.push_back(canonical[std::lower_bound(all.begin(),all.end(),v)-all.begin()]);
 std::unordered_map<uint64_t,uint8_t>edges;edges.reserve(indices.size());
 for(size_t t=0;t<l.tri.size();t+=3)for(unsigned k=0;k<3;++k){auto a=l.tri[t+k],b=l.tri[t+(k+1)%3];auto ra=find(a),rb=find(b);if(ra!=rb)l.component[std::max(ra,rb)]=std::min(ra,rb);if(a>b)std::swap(a,b);auto&count=edges[(uint64_t(a)<<32)|b];count=std::min(uint8_t(3),uint8_t(count+1));}
 for(const auto&[edge,count]:edges)if(count!=2){l.locked[uint32_t(edge>>32)]=true;l.locked[uint32_t(edge)]=true;}
 for(uint32_t i=0;i<l.component.size();++i)l.component[i]=find(i);return l;
}
bool compatible(const ModelVertex&a,const ModelVertex&b,float uv){
 P na{a.nx,a.ny,a.nz},nb{b.nx,b.ny,b.nz};double la=length(na),lb=length(nb);if((la>1e-8)!=(lb>1e-8))return false;if(la>1e-8&&dot(na,nb)<.995*la*lb)return false;
 if(std::abs(a.u-b.u)>uv||std::abs(a.v-b.v)>uv||std::abs(a.u1-b.u1)>uv||std::abs(a.v1-b.v1)>uv||std::abs(a.u2-b.u2)>uv||std::abs(a.v2-b.v2)>uv)return false;
 for(auto d:{a.lr-b.lr,a.lg-b.lg,a.lb-b.lb,a.lit-b.lit,a.ar-b.ar,a.ag-b.ag,a.ab-b.ab,a.aa-b.aa})if(std::abs(d)>.04f)return false;return true;
}
struct Key {int64_t x,y,z;uint32_t component;bool operator==(const Key&)const=default;};
struct Hash {size_t operator()(const Key&k)const{size_t h=std::hash<int64_t>{}(k.x);for(auto v:{k.y,k.z,int64_t(k.component)})h^=std::hash<int64_t>{}(v)+size_t(0x9e3779b9)+(h<<6)+(h>>2);return h;}};
Variant simplify(const CharacterModel&m,const Local&l,float cell,float uv){
 Variant result;std::vector<uint32_t>map(l.used.size());std::iota(map.begin(),map.end(),0u);std::unordered_map<Key,std::vector<uint32_t>,Hash>cells;cells.reserve(l.used.size());
 for(uint32_t i=0;i<l.used.size();++i){if(l.locked[i])continue;const auto&v=m.vertices[l.used[i]];Key key{int64_t(std::floor(v.x/cell)),int64_t(std::floor(v.y/cell)),int64_t(std::floor(v.z/cell)),l.component[i]};auto&bucket=cells[key];bool found=false;for(auto candidate:bucket)if(compatible(v,m.vertices[l.used[candidate]],uv)){map[i]=candidate;found=true;break;}if(!found&&bucket.size()<32)bucket.push_back(i);}
 // Pin original vertices whenever a proposed collapse reverses a surviving
 // triangle. Bounded passes; refuse this variant if the repair is not stable.
 bool stable=false;for(unsigned pass=0;pass<5;++pass){bool changed=false;for(size_t t=0;t<l.tri.size();t+=3){auto a=l.tri[t],b=l.tri[t+1],c=l.tri[t+2],ma=map[a],mb=map[b],mc=map[c];if(ma==mb||mb==mc||mc==ma)continue;auto pa=point(m.vertices[l.used[a]]),pb=point(m.vertices[l.used[b]]),pc=point(m.vertices[l.used[c]]);auto qa=point(m.vertices[l.used[ma]]),qb=point(m.vertices[l.used[mb]]),qc=point(m.vertices[l.used[mc]]);auto n=cross(sub(pb,pa),sub(pc,pa)),nn=cross(sub(qb,qa),sub(qc,qa));if(dot(n,n)>1e-16&&(dot(n,nn)<=1e-12||dot(nn,nn)<1e-16)){if(ma!=a||mb!=b||mc!=c){map[a]=a;map[b]=b;map[c]=c;changed=true;}}}if(!changed){stable=true;break;}}
 if(!stable)return result;
 std::vector<bool>survived(l.used.size());result.indices.reserve(l.tri.size());
 for(size_t t=0;t<l.tri.size();t+=3){auto a=map[l.tri[t]],b=map[l.tri[t+1]],c=map[l.tri[t+2]];if(a==b||b==c||c==a)continue;result.indices.insert(result.indices.end(),{l.used[a],l.used[b],l.used[c]});survived[l.component[l.tri[t]]]=true;}
 // A disconnected closed component may fit within one cell. Keep its original
 // triangles rather than silently deleting that object from the stage.
 for(size_t t=0;t<l.tri.size();t+=3)if(!survived[l.component[l.tri[t]]])for(unsigned k=0;k<3;++k)result.indices.push_back(l.used[l.tri[t+k]]);
 if(result.indices.size()>=l.tri.size()||result.indices.empty()){result.indices.clear();return result;}
 for(size_t i=0;i<l.used.size();++i)result.maxError=std::max(result.maxError,float(length(sub(point(m.vertices[l.used[i]]),point(m.vertices[l.used[map[i]]])))));
 return result;
}
}
Mesh build(const CharacterModel&m,BuildOptions options){
 if(!std::isfinite(options.midCell)||!std::isfinite(options.farCell)||options.midCell<.001f||options.farCell<options.midCell||options.farCell>1e6f||!std::isfinite(options.maxUvDelta)||options.maxUvDelta<0||options.maxUvDelta>1||options.maxVertices>1000000||options.maxIndices>3000000||options.maxParts>65536)throw std::invalid_argument("LOD options");
 Mesh result;result.originalTriangles=m.indices.size()/3;if(m.parts.size()>options.maxParts||m.vertices.size()>options.maxVertices||m.indices.size()>options.maxIndices){result.budgetExceeded=true;return result;}result.parts.resize(m.parts.size());
 for(const auto&v:m.vertices){for(float value:{v.x,v.y,v.z,v.nx,v.ny,v.nz,v.u,v.v,v.u1,v.v1,v.u2,v.v2,v.lr,v.lg,v.lb,v.lit,v.ar,v.ag,v.ab,v.aa})if(!std::isfinite(value)||std::abs(value)>1e9f)throw std::invalid_argument("LOD vertex extent");}
 size_t work=0;for(size_t n=0;n<m.parts.size();++n){const auto&p=m.parts[n];if(p.count%3||size_t(p.first)+p.count>m.indices.size())throw std::invalid_argument("LOD part indices");work+=p.count;if(work>options.maxIndices){result.budgetExceeded=true;result.parts.clear();return result;}auto&out=result.parts[n];out.originalCount=p.count;out.protectedPart=p.surfaceAlpha!=0||p.count<96;auto span=std::span(m.indices).subspan(p.first,p.count);
  std::array<float,3>low{1e30f,1e30f,1e30f},high{-1e30f,-1e30f,-1e30f};for(auto index:span){if(index>=m.vertices.size())throw std::invalid_argument("LOD vertex index");const auto&v=m.vertices[index];std::array<float,3>a{v.x,v.y,v.z};for(unsigned k=0;k<3;++k){low[k]=std::min(low[k],a[k]);high[k]=std::max(high[k],a[k]);}if(v.aa<.999f)out.protectedPart=true;}
  if(p.count){for(unsigned k=0;k<3;++k)out.center[k]=(low[k]+high[k])*.5f;out.radius=float(length({(high[0]-low[0])*.5,(high[1]-low[1])*.5,(high[2]-low[2])*.5}));}
  if(!out.protectedPart){auto local=topology(m,span);out.mid=simplify(m,local,options.midCell,options.maxUvDelta);out.coarse=simplify(m,local,options.farCell,options.maxUvDelta);if(!out.mid.indices.empty()&&(!out.coarse.indices.empty()&&out.coarse.indices.size()>out.mid.indices.size()))out.coarse={};}
  result.midTriangles+=(out.mid.indices.empty()?p.count:out.mid.indices.size())/3;result.farTriangles+=(out.coarse.indices.empty()?(out.mid.indices.empty()?p.count:out.mid.indices.size()):out.coarse.indices.size())/3;
 }return result;
}
float projected_error(const Part&p,const Variant&v,float distance,float fov,unsigned height,float aspect){
 if(!std::isfinite(distance)||!std::isfinite(fov)||fov<=.01f||fov>=3.13f||!height||height>16384||!std::isfinite(aspect)||aspect<.1f||aspect>16||!std::isfinite(p.radius)||p.radius<0||!std::isfinite(v.maxError)||v.maxError<0)return std::numeric_limits<float>::infinity();float depth=distance-p.radius-v.maxError;if(depth<=.001f)return std::numeric_limits<float>::infinity();const float tangent=std::tan(fov*.5f);return v.maxError*float(height)*(1+tangent*std::max(1.f,aspect))*1.414213563f/(2*tangent*depth);
}
unsigned select(const Part&p,float distance,float fov,unsigned height,unsigned previous,float budget,float aspect){
 if(p.protectedPart||!std::isfinite(budget)||budget<=0||budget>16)return 0;if(previous>2)previous=0;for(unsigned level:{2u,1u}){const auto&v=level==2?p.coarse:p.mid;if(v.indices.empty())continue;float threshold=budget*(previous>=level?1.f:.8f);if(projected_error(p,v,distance,fov,height,aspect)<=threshold)return level;}return 0;
}
}

