#include "physics_debug_render.h"
#include "source_coordinates.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace mgo2mt::physics_debug {
namespace {
Vec3 add(Vec3 a,Vec3 b){for(unsigned k=0;k<3;++k)a[k]+=b[k];return a;}
Vec3 sub(Vec3 a,Vec3 b){for(unsigned k=0;k<3;++k)a[k]-=b[k];return a;}
Vec3 mul(Vec3 a,float b){for(auto&v:a)v*=b;return a;}
float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vec3 cross(Vec3 a,Vec3 b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
bool finite(Vec3 a){for(auto v:a)if(!std::isfinite(v)||std::abs(v)>=1e7f)return false;return true;}
Vec3 unit(Vec3 a){return mul(a,1/std::sqrt(dot(a,a)));}
constexpr unsigned segments=12;
constexpr float pi=3.14159265358979323846f;
uint32_t color(Kind k){switch(k){case Kind::terrain:return 0x9050e080;case Kind::body:return 0xd040dfff;case Kind::rigid:return 0xd0ff9b36;case Kind::bone:return 0xd0ffff50;case Kind::visual:return 0xa0ec70f0;case Kind::attachment:return 0xd0ffffff;case Kind::hit:return 0xb0ff6060;case Kind::trigger:return 0x906680ff;case Kind::fallBarrier:return 0xd0ff9933;}return 0xffffffff;}
}
Frame::Frame(Budget b):budget_(b){if(!b.lines||b.lines>32768||!b.terrainTriangles||b.terrainTriangles>4096||!b.pixels||b.pixels>2000000||!std::isfinite(b.terrainRange)||b.terrainRange<=0||b.terrainRange>50000)throw std::invalid_argument("Physics debug budget");lines_.reserve(b.lines);}
void Frame::line(Vec3 a,Vec3 b,Kind k){if(!finite(a)||!finite(b)||lines_.size()>=budget_.lines){++stats_.omitted;return;}lines_.push_back({a,b,k});stats_.lines=lines_.size();}
void Frame::sphere(Vec3 center,float r,Kind k){if(!finite(center)||!std::isfinite(r)||r<=0||r>50000){++stats_.omitted;return;}for(unsigned axis=0;axis<3;++axis)for(unsigned i=0;i<segments;++i){Vec3 a=center,b=center;float t=float(i)*2*pi/segments,u=float(i+1)*2*pi/segments;a[(axis+1)%3]+=r*std::cos(t);a[(axis+2)%3]+=r*std::sin(t);b[(axis+1)%3]+=r*std::cos(u);b[(axis+2)%3]+=r*std::sin(u);line(a,b,k);}}
void Frame::capsule(Vec3 a,Vec3 b,float r,Kind k){if(!finite(a)||!finite(b)||!std::isfinite(r)||r<=0||r>50000){++stats_.omitted;return;}auto d=sub(b,a);if(dot(d,d)<1e-6f){sphere(a,r,k);return;}d=unit(d);auto x=unit(cross(d,std::abs(d[1])<.9f?Vec3{0,1,0}:Vec3{1,0,0})),y=cross(d,x);
 for(unsigned i=0;i<segments;++i){auto radial=[&](float t){return mul(add(mul(x,std::cos(t)),mul(y,std::sin(t))),r);};auto p=radial(float(i)*2*pi/segments),q=radial(float(i+1)*2*pi/segments);line(add(a,p),add(a,q),k);line(add(b,p),add(b,q),k);if(i%3==0)line(add(a,p),add(b,p),k);}
 for(auto axis:{x,y})for(unsigned i=0;i<segments;++i){float t=float(i)*pi/segments,u=float(i+1)*pi/segments;auto arc=[&](Vec3 end,float angle,float sign){return add(end,add(mul(axis,r*std::cos(angle)),mul(d,sign*r*std::sin(angle))));};line(arc(a,t,-1),arc(a,u,-1),k);line(arc(b,t,1),arc(b,u,1),k);}
}
void Frame::standing(Vec3 feet,stage::Capsule c){if(!std::isfinite(c.height)||!std::isfinite(c.radius)||c.height<2*c.radius||c.height>10000){++stats_.omitted;return;}auto a=feet,b=feet;a[1]+=c.radius;b[1]+=c.height-c.radius;capsule(a,b,c.radius,Kind::body);}
void Frame::hit_regions(Vec3 feet,float yaw,host_hit::Stance stance,Vec3 coverOffset){
 if(!finite(feet)||!finite(coverOffset)||!std::isfinite(yaw)){++stats_.omitted;return;}const auto bones=host_hit::pose(0,stance);const float c=std::cos(yaw),s=std::sin(yaw);auto rotate=[&](Vec3 p){return Vec3{p[0]*c+p[2]*s,p[1],-p[0]*s+p[2]*c};};
 for(const auto& region:original_hit_regions::boxes){if(region.bone>=bones.size())continue;const auto& bone=bones[region.bone];
  auto draw=[&](Vec3 origin){std::array<Vec3,8> corners;for(unsigned i=0;i<8;++i){auto p=bone.origin;for(unsigned axis=0;axis<3;++axis)p=add(p,mul(bone.axes[axis],region.offset[axis]+((i&(1u<<axis))?region.halfExtent[axis]:-region.halfExtent[axis])));corners[i]=add(origin,rotate(p));}for(unsigned i=0;i<8;++i)for(unsigned axis=0;axis<3;++axis)if(!(i&(1u<<axis)))line(corners[i],corners[i|(1u<<axis)],Kind::hit);};
  draw(feet);if(region.bone>0&&region.bone<13&&dot(coverOffset,coverOffset)>0)draw(add(feet,coverOffset));
 }
}
void Frame::box(Vec3 c,Vec3 h,Kind k){if(!finite(c)||!finite(h)||std::any_of(h.begin(),h.end(),[](float v){return v<=0||v>50000;})){++stats_.omitted;return;}std::array<Vec3,8>p;for(unsigned i=0;i<8;++i)for(unsigned j=0;j<3;++j)p[i][j]=c[j]+((i&(1u<<j))?h[j]:-h[j]);for(unsigned i=0;i<8;++i)for(unsigned j=0;j<3;++j)if(!(i&(1u<<j)))line(p[i],p[i|(1u<<j)],k);}
void Frame::rigid(const physics::RigidBody&b){if(!b.valid()){++stats_.omitted;return;}auto [a,z]=b.segment();capsule(a,z,b.radius,Kind::rigid);}
void Frame::skeleton(const PreparedCharacter&p,std::span<const CatalogBone>bones,Vec3 origin,float yaw){if(!finite(origin)||!std::isfinite(yaw)||bones.size()>4096){++stats_.omitted;return;}auto world=[&](Vec3 v){return add(origin,{v[0]*std::cos(yaw)+v[2]*std::sin(yaw),v[1],-v[0]*std::sin(yaw)+v[2]*std::cos(yaw)});};for(const auto&b:bones){if(b.parent<0||size_t(b.parent)>=bones.size())continue;auto a=p.bone_position(b.key),z=p.bone_position(bones[b.parent].key);if(a&&z)line(world(*a),world(*z),Kind::bone);}}
void Frame::terrain(const stage::Collision&w,Vec3 eye){if(!finite(eye)){++stats_.omitted;return;}Vec3 lo=eye,hi=eye;for(unsigned j=0;j<3;++j){lo[j]-=budget_.terrainRange;hi[j]+=budget_.terrainRange;}auto ids=w.bounds_candidates(lo,hi);std::erase_if(ids,[&](unsigned id){const auto&t=w.triangles.at(id);for(unsigned j=0;j<3;++j){float a=1e30f,b=-1e30f;for(auto index:t.vertices){a=(std::min)(a,w.vertices.at(index)[j]);b=(std::max)(b,w.vertices.at(index)[j]);}if(b<lo[j]||a>hi[j])return true;}return false;});const size_t available=(std::min)(budget_.terrainTriangles,(budget_.lines-lines_.size())/3);if(!available){stats_.omitted+=ids.size();return;}const size_t step=(std::max)(size_t(1),(ids.size()+available-1)/available);size_t taken=0;for(size_t i=0;i<ids.size()&&taken<available;i+=step){const auto&t=w.triangles.at(ids[i]);for(unsigned e=0;e<3;++e)line(w.vertices.at(t.vertices[e]),w.vertices.at(t.vertices[(e+1)%3]),(t.attribute&stage::attribute::dont_fall)?Kind::fallBarrier:stage::query::player.matches(t.attribute)?Kind::terrain:Kind::trigger);++taken;}stats_.triangles+=taken;stats_.omitted+=ids.size()-taken;}
Stats paint(std::span<uint32_t>pixels,unsigned width,unsigned height,const Frame&frame,const View&v){auto stats=frame.stats();if(!width||!height||width>8192||height>8192||pixels.size()!=size_t(width)*height||v.left<0||v.top<0||v.width<=0||v.height<=0||v.left>int(width)-v.width||v.top>int(height)-v.height||!finite(v.eye)||!finite(v.direction)||dot(v.direction,v.direction)<1e-6f||!std::isfinite(v.aspect)||v.aspect<=0||v.aspect>32||!valid_vertical_fov(v.verticalFov))return stats;
 auto z=unit(v.direction),right=cross(Vec3{0,1,0},z);if(dot(right,right)<1e-8f)return stats;right=unit(right);auto up=cross(z,right);const float tangent=std::tan(v.verticalFov*.5f);
 for(const auto&line:frame.lines()){auto camera=[&](Vec3 p){auto d=sub(p,v.eye);return Vec3{source_screen_x*dot(d,right)/(tangent*v.aspect),dot(d,up)/tangent,dot(d,z)};};auto a=camera(line.a),b=camera(line.b);if(!std::all_of(a.begin(),a.end(),[](float f){return std::isfinite(f);})||!std::all_of(b.begin(),b.end(),[](float f){return std::isfinite(f);}))continue;float t0=0,t1=1;bool admitted=true;
  // Clip in homogeneous camera space, including near/far and screen sides.
  const std::array<float,6> fa{a[2]-10,500000-a[2],a[0]+a[2],a[2]-a[0],a[1]+a[2],a[2]-a[1]},fb{b[2]-10,500000-b[2],b[0]+b[2],b[2]-b[0],b[1]+b[2],b[2]-b[1]};
  for(unsigned j=0;j<6;++j){if(fa[j]<0&&fb[j]<0){admitted=false;break;}if(fa[j]<0)t0=(std::max)(t0,fa[j]/(fa[j]-fb[j]));else if(fb[j]<0)t1=(std::min)(t1,fa[j]/(fa[j]-fb[j]));}if(!admitted||t0>t1)continue;auto delta=sub(b,a);b=add(a,mul(delta,t1));a=add(a,mul(delta,t0));auto screen=[&](Vec3 p){return std::array<int,2>{std::clamp(v.left+int((p[0]/p[2]+1)*.5f*v.width),v.left,v.left+v.width-1),std::clamp(v.top+int((1-p[1]/p[2])*.5f*v.height),v.top,v.top+v.height-1)};};auto aa=screen(a),bb=screen(b);const int steps=(std::max)(std::abs(bb[0]-aa[0]),std::abs(bb[1]-aa[1]));const uint32_t src=color(line.kind),alpha=src>>24;
  for(int j=0;j<=steps;++j){if(stats.pixels>=frame.budget().pixels)return stats;const int x=steps?aa[0]+(bb[0]-aa[0])*j/steps:aa[0],y=steps?aa[1]+(bb[1]-aa[1])*j/steps:aa[1];auto&dst=pixels[size_t(y)*width+x];const uint32_t old=dst>>24,den=alpha*255+old*(255-alpha);uint32_t out=((den+127)/255)<<24;for(unsigned shift:{0u,8u,16u})out|=((((src>>shift)&255)*alpha*255+((dst>>shift)&255)*old*(255-alpha)+den/2)/den)<<shift;dst=out;++stats.pixels;}
 }return stats;
}
}
