#include "product_identity.h"
#include "stage_collision.h"
#include <cmath>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <numeric>
#include <set>
namespace mgo2mt::stage {
static Vec3 sub(Vec3 a,Vec3 b){for(int i=0;i<3;++i)a[i]-=b[i];return a;}
static Vec3 cross(Vec3 a,Vec3 b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
static float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static Vec3 add(Vec3 a,Vec3 b){for(int i=0;i<3;++i)a[i]+=b[i];return a;}
static Vec3 mul(Vec3 a,float s){for(auto&v:a)v*=s;return a;}
static Vec3 unit(Vec3 a){float n=std::sqrt(dot(a,a));return n>1e-8f?mul(a,1/n):Vec3{};}
static bool finite(Vec3 a){for(auto x:a)if(!std::isfinite(x)||std::abs(x)>=1e7f)return false;return true;}
static bool valid(Capsule c){return std::isfinite(c.radius)&&std::isfinite(c.height)&&std::isfinite(c.skin)&&c.radius>0&&c.radius<=10000&&c.height>=2*c.radius&&c.height<=100000&&c.skin>=.001f&&c.skin<c.radius*.25f;}
Collision Collision::read(std::istream&in){
 auto require=[](bool ok){if(!ok)throw std::runtime_error("Invalid native stage collision");};
 std::string magic;unsigned version,nv,nt;require(bool(in>>magic>>version>>nv>>nt));require(magic==mgo2mt::brand::Format{"MGO2MT.STAGE_COLLISION"}&&(version>=1&&version<=3)&&nv<=500000&&nt<=500000);
 Collision c;c.vertices.resize(nv);c.triangles.resize(nt);
 if(version>=2){unsigned count;require(bool(in>>count)&&count<=65536);c.materials.resize(count);for(auto&m:c.materials){unsigned verified;require(bool(in>>m.id>>m.friction>>m.restitution>>verified)&&verified<=1);m.verified=verified!=0;if(version>=3){unsigned resistanceVerified;require(bool(in>>m.resistance>>resistanceVerified)&&resistanceVerified<=1);m.resistanceVerified=resistanceVerified!=0;if(!m.resistanceVerified)require(m.resistance==1000);}}}
 for(auto&v:c.vertices)for(auto&x:v)require(bool(in>>x)&&std::isfinite(x)&&std::abs(x)<1e7);
 for(auto&t:c.triangles){for(auto&i:t.vertices)require(bool(in>>i)&&i<nv);require(bool(in>>t.attribute>>t.polygonAttribute)&&t.polygonAttribute<=65535);if(version>=2)require(bool(in>>t.material)&&(t.material==~0u||t.material<c.materials.size()));}
 std::string tail;require(!(in>>tail));return make(std::move(c.vertices),std::move(c.triangles),std::move(c.materials));
}
Collision Collision::make(std::vector<Vec3>v,std::vector<CollisionTriangle>t,std::vector<CollisionMaterial>m){
 if(v.size()>2000000||t.size()>2000000||m.size()>65536)throw std::runtime_error("Collision assembly extent");
 for(auto p:v)if(!finite(p))throw std::runtime_error("Collision assembly vertex");
 for(auto&f:t){for(auto i:f.vertices)if(i>=v.size())throw std::runtime_error("Collision assembly index");if(f.material!=~0u&&f.material>=m.size())throw std::runtime_error("Collision material index");}
 for(auto a:m)if(!std::isfinite(a.friction)||!std::isfinite(a.restitution)||a.friction<0||a.friction>10||a.restitution<0||a.restitution>1)throw std::runtime_error("Collision material value");
 Collision c;c.vertices=std::move(v);c.triangles=std::move(t);c.materials=std::move(m);c.build();return c;
}
CollisionMaterial Collision::material(size_t triangle)const{if(triangle<triangles.size()&&triangles[triangle].material<materials.size())return materials[triangles[triangle].material];return {};}
Collision Collision::combine(const Collision&base,std::span<const CollisionInstance>instances){
 if(instances.size()>512)throw std::runtime_error("Collision instance extent");auto vertices=base.vertices;auto triangles=base.triangles;auto materials=base.materials;std::set<uint32_t>ids;
 for(auto&i:instances){if(!i.id||!ids.insert(i.id).second||!i.geometry||!finite(i.position)||!finite(i.degrees))throw std::runtime_error("Collision instance invalid");auto&g=*i.geometry;
  if(vertices.size()+g.vertices.size()>2000000||triangles.size()+g.triangles.size()>2000000||materials.size()+g.materials.size()>65536)throw std::runtime_error("Collision instance size");
  unsigned vb=unsigned(vertices.size()),mb=unsigned(materials.size());
  auto rotate=[&](Vec3 p){for(int axis:{2,0,1}){float a=i.degrees[axis]*3.14159265359f/180,c=std::cos(a),s=std::sin(a);if(axis==2)p={c*p[0]-s*p[1],s*p[0]+c*p[1],p[2]};else if(axis==0)p={p[0],c*p[1]-s*p[2],s*p[1]+c*p[2]};else p={c*p[0]+s*p[2],p[1],-s*p[0]+c*p[2]};}return add(p,i.position);};
  for(auto p:g.vertices)vertices.push_back(rotate(p));for(auto t:g.triangles){for(auto&v:t.vertices)v+=vb;if(t.material!=~0u)t.material+=mb;t.object=i.id;triangles.push_back(t);}materials.insert(materials.end(),g.materials.begin(),g.materials.end());
 }return make(std::move(vertices),std::move(triangles),std::move(materials));
}
void Collision::build(){order_.resize(triangles.size());std::iota(order_.begin(),order_.end(),0u);nodes_.clear();if(!order_.empty())build_node(0,unsigned(order_.size()));}
unsigned Collision::build_node(unsigned first,unsigned count){
 unsigned index=unsigned(nodes_.size());nodes_.push_back({});Node n;n.first=first;n.count=count;n.lo={1e30f,1e30f,1e30f};n.hi={-1e30f,-1e30f,-1e30f};
 for(unsigned i=first;i<first+count;++i)for(auto vi:triangles[order_[i]].vertices)for(int j=0;j<3;++j){n.lo[j]=std::min(n.lo[j],vertices[vi][j]);n.hi[j]=std::max(n.hi[j],vertices[vi][j]);}
 if(count>8){int axis=0;for(int j=1;j<3;++j)if(n.hi[j]-n.lo[j]>n.hi[axis]-n.lo[axis])axis=j;
  auto center=[&](unsigned t){float x=0;for(auto v:triangles[t].vertices)x+=vertices[v][axis];return x;};unsigned half=count/2;
  std::nth_element(order_.begin()+first,order_.begin()+first+half,order_.begin()+first+count,[&](unsigned a,unsigned b){float ca=center(a),cb=center(b);return ca==cb?a<b:ca<cb;});
  n.count=0;n.left=build_node(first,half);n.right=build_node(first+half,count-half);
 }nodes_[index]=n;return index;
}
std::vector<unsigned> Collision::candidates(Vec3 lo,Vec3 hi)const{
 std::vector<unsigned> result;if(nodes_.empty())return result;std::vector<unsigned> stack{0};
 while(!stack.empty()){auto index=stack.back();stack.pop_back();auto&n=nodes_[index];bool overlaps=true;for(int j=0;j<3;++j)overlaps&=lo[j]<=n.hi[j]+.01f&&hi[j]>=n.lo[j]-.01f;if(!overlaps)continue;
  if(n.count)for(unsigned i=0;i<n.count;++i)result.push_back(order_[n.first+i]);else{stack.push_back(n.left);stack.push_back(n.right);}
 }return result;
}
// Intersect the actual ray with each BVH box, not the large enclosing box
// of a diagonal segment. Padding preserves the former broad-phase tolerance.
std::vector<unsigned> Collision::ray_candidates(Vec3 o,Vec3 d,float limit)const{
 std::vector<unsigned> result;if(nodes_.empty())return result;
 std::array<unsigned,64> stack{};size_t size=1;
 while(size){const auto&n=nodes_[stack[--size]];double near=0,far=limit;bool hit=true;
  for(unsigned j=0;j<3;++j){const double lo=double(n.lo[j])-.01,hi=double(n.hi[j])+.01;
   if(d[j]==0){if(o[j]<lo||o[j]>hi){hit=false;break;}continue;}
   double a=(lo-o[j])/d[j],b=(hi-o[j])/d[j];if(a>b)std::swap(a,b);near=std::max(near,a);far=std::min(far,b);if(near>far){hit=false;break;}
  }
  if(!hit)continue;if(n.count)for(unsigned i=0;i<n.count;++i)result.push_back(order_[n.first+i]);else{stack[size++]=n.left;stack[size++]=n.right;}
 }return result;
}
std::optional<CollisionHit> Collision::ray(Vec3 o,Vec3 d,float maximum,CollisionQuery filter)const{
 if(!std::isfinite(maximum)||maximum<=0)return {};
 for(auto x:o)if(!std::isfinite(x))return {};
 float length=std::sqrt(dot(d,d));if(!std::isfinite(length)||length<1e-8f)return {};for(auto&x:d)x/=length;
 std::optional<CollisionHit> hit;
 auto end=add(o,mul(d,maximum));if(!finite(end))return {};Vec3 lo,hi;for(int j=0;j<3;++j){lo[j]=std::min(o[j],end[j]);hi[j]=std::max(o[j],end[j]);}
 for(auto i:ray_candidates(o,d,maximum)){const auto&t=triangles[i];if(!filter.matches(t.attribute))continue;auto a=vertices[t.vertices[0]],e1=sub(vertices[t.vertices[1]],a),e2=sub(vertices[t.vertices[2]],a);auto p=cross(d,e2);float determinant=dot(e1,p);if(std::abs(determinant)<1e-8f)continue;float inv=1/determinant;auto s=sub(o,a);float u=dot(s,p)*inv;if(u<0||u>1)continue;auto q=cross(s,e1);float v=dot(d,q)*inv;if(v<0||u+v>1)continue;float distance=dot(e2,q)*inv;if(distance<0||distance>maximum)continue;if(hit&&distance==maximum&&i>hit->triangle)continue;maximum=distance;CollisionHit h{distance,o,i,unit(cross(e1,e2))};if(dot(h.normal,d)>0)h.normal=mul(h.normal,-1);for(int j=0;j<3;++j)h.position[j]+=d[j]*distance;hit=h;}
 return hit;
}
std::vector<CollisionRayHit> Collision::ray_all(Vec3 o,Vec3 d,float maximum,CollisionQuery filter)const{
 std::vector<CollisionRayHit> hits;
 if(!std::isfinite(maximum)||maximum<=0||!finite(o))return hits;
 float length=std::sqrt(dot(d,d));if(!std::isfinite(length)||length<1e-8f)return hits;d=mul(d,1/length);
 auto end=add(o,mul(d,maximum));if(!finite(end))return hits;Vec3 lo,hi;for(int j=0;j<3;++j){lo[j]=std::min(o[j],end[j]);hi[j]=std::max(o[j],end[j]);}
 for(auto i:ray_candidates(o,d,maximum)){
  const auto&t=triangles[i];if(!filter.matches(t.attribute))continue;auto a=vertices[t.vertices[0]],e1=sub(vertices[t.vertices[1]],a),e2=sub(vertices[t.vertices[2]],a);
  auto p=cross(d,e2);float determinant=dot(e1,p);if(!std::isfinite(determinant)||std::abs(determinant)<1e-8f)continue;
  float inv=1/determinant;auto s=sub(o,a);float u=dot(s,p)*inv;if(!std::isfinite(u)||u<0||u>1)continue;
  auto q=cross(s,e1);float v=dot(d,q)*inv;if(!std::isfinite(v)||v<0||u+v>1)continue;
  float distance=dot(e2,q)*inv;if(!std::isfinite(distance)||distance<0||distance>maximum)continue;
  auto normal=unit(cross(e1,e2));if(!finite(normal))continue;
  hits.push_back({distance,add(o,mul(d,distance)),i,normal,dot(normal,d)<0,t.attribute,t.polygonAttribute,t.object,material(i)});
 }
 std::sort(hits.begin(),hits.end(),[](const auto&a,const auto&b){return a.distance==b.distance?a.triangle<b.triangle:a.distance<b.distance;});return hits;
}
// Closest point on a nondegenerate triangle; edges cover degenerate input.
static Vec3 on_segment(Vec3 p,Vec3 a,Vec3 b){auto d=sub(b,a);float n=dot(d,d);return add(a,mul(d,n>1e-12f?std::clamp(dot(sub(p,a),d)/n,0.f,1.f):0.f));}
static Vec3 on_triangle(Vec3 p,Vec3 a,Vec3 b,Vec3 c){
 auto ab=sub(b,a),ac=sub(c,a),n=cross(ab,ac);float nn=dot(n,n);
 if(nn>1e-12f){auto q=sub(p,mul(n,dot(sub(p,a),n)/nn));if(dot(cross(ab,sub(q,a)),n)>=0&&dot(cross(sub(c,b),sub(q,b)),n)>=0&&dot(cross(sub(a,c),sub(q,c)),n)>=0)return q;}
 Vec3 q=on_segment(p,a,b);float best=dot(sub(p,q),sub(p,q));for(auto e:{on_segment(p,b,c),on_segment(p,c,a)}){float d=dot(sub(p,e),sub(p,e));if(d<best){best=d;q=e;}}return q;
}
struct Pair {Vec3 segment{},triangle{};float squared=1e30f;};
static void offer(Pair&best,Vec3 a,Vec3 b){float d=dot(sub(a,b),sub(a,b));if(d<best.squared)best={a,b,d};}
static void segment_pair(Pair&best,Vec3 p,Vec3 q,Vec3 a,Vec3 b){
 auto d=sub(q,p),e=sub(b,a),r=sub(p,a);float dd=dot(d,d),ee=dot(e,e),de=dot(d,e),dr=dot(d,r),er=dot(e,r),s=0,t=0;
 if(dd<1e-12f){offer(best,p,on_segment(p,a,b));return;}if(ee<1e-12f){offer(best,on_segment(a,p,q),a);return;}
 float den=dd*ee-de*de;if(den>1e-12f)s=std::clamp((de*er-dr*ee)/den,0.f,1.f);t=(de*s+er)/ee;
 if(t<0){t=0;s=std::clamp(-dr/dd,0.f,1.f);}else if(t>1){t=1;s=std::clamp((de-dr)/dd,0.f,1.f);}offer(best,add(p,mul(d,s)),add(a,mul(e,t)));
}
static Pair closest(Vec3 p,Vec3 q,Vec3 a,Vec3 b,Vec3 c){
 Pair best;offer(best,p,on_triangle(p,a,b,c));offer(best,q,on_triangle(q,a,b,c));
 auto n=cross(sub(b,a),sub(c,a)),d=sub(q,p);float den=dot(n,d);
 if(std::abs(den)>1e-10f){float t=dot(n,sub(a,p))/den;if(t>=0&&t<=1){auto x=add(p,mul(d,t));offer(best,x,on_triangle(x,a,b,c));}}
 segment_pair(best,p,q,a,b);segment_pair(best,p,q,b,c);segment_pair(best,p,q,c,a);return best;
}
static std::pair<Vec3,Vec3> capsule_bounds(Vec3 p,Vec3 d,Capsule c){Vec3 lo{},hi{};for(int j=0;j<3;++j){lo[j]=std::min(p[j],p[j]+d[j]);hi[j]=std::max(p[j],p[j]+d[j]);if(j==1){lo[j]-=c.skin;hi[j]+=c.height+c.skin;}else{lo[j]-=c.radius+c.skin;hi[j]+=c.radius+c.skin;}}return {lo,hi};}
bool Collision::clear(Vec3 feet,Capsule shape,CollisionQuery filter)const{
 if(!finite(feet)||!valid(shape))return false;auto [lo,hi]=capsule_bounds(feet,{},shape);auto p=feet,q=feet;p[1]+=shape.radius;q[1]+=shape.height-shape.radius;
 for(auto i:candidates(lo,hi)){auto&t=triangles[i];if(!filter.matches(t.attribute))continue;auto h=closest(p,q,vertices[t.vertices[0]],vertices[t.vertices[1]],vertices[t.vertices[2]]);if(h.squared<(shape.radius-shape.skin)*(shape.radius-shape.skin))return false;}return true;
}
std::optional<CapsuleHit> Collision::sweep(Vec3 feet,Vec3 delta,Capsule shape,CollisionQuery filter)const{
 if(!finite(feet)||!valid(shape))return {};auto a=feet,b=feet;a[1]+=shape.radius;b[1]+=shape.height-shape.radius;return sweep_segment(a,b,delta,shape.radius,shape.skin,filter);
}
// A triangle cannot intersect a swept capsule wholly outside its supporting
// plane. Test all four translated segment endpoints on the same side. This
// strict, double-precision guard removes conservative-advancement false hits
// at coplanar internal edges without weakening wall/ground contact tolerances.
static bool outside_swept_plane(Vec3 startA,Vec3 startB,Vec3 delta,float radius,float skin,Vec3 a,Vec3 b,Vec3 c){
 double u[3],v[3],n[3];for(unsigned k=0;k<3;++k){u[k]=double(b[k])-a[k];v[k]=double(c[k])-a[k];}
 n[0]=u[1]*v[2]-u[2]*v[1];n[1]=u[2]*v[0]-u[0]*v[2];n[2]=u[0]*v[1]-u[1]*v[0];
 const double length=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);if(length<=1e-12)return false;
 const auto distance=[&](Vec3 p,bool end){double d=0;for(unsigned k=0;k<3;++k)d+=(double(p[k])+(end?double(delta[k]):0)-a[k])*n[k];return d/length;};
 const double limit=double(radius)+skin,da=distance(startA,false),db=distance(startB,false),ea=distance(startA,true),eb=distance(startB,true);
 return (da>=limit&&db>=limit&&ea>=limit&&eb>=limit)||(da<=-limit&&db<=-limit&&ea<=-limit&&eb<=-limit);
}
std::optional<CapsuleHit> Collision::sweep_segment(Vec3 startA,Vec3 startB,Vec3 delta,float radius,float skin,CollisionQuery filter)const{
 if(!finite(startA)||!finite(startB)||!finite(delta)||!finite(add(startA,delta))||!finite(add(startB,delta))||!std::isfinite(radius)||!std::isfinite(skin)||radius<=0||radius>10000||skin<0||skin>=radius*.25f)return {};float speed=std::sqrt(dot(delta,delta));if(speed<1e-8f)return {};
 Vec3 lo,hi;for(int j=0;j<3;++j){lo[j]=std::min({startA[j],startB[j],startA[j]+delta[j],startB[j]+delta[j]})-radius-skin;hi[j]=std::max({startA[j],startB[j],startA[j]+delta[j],startB[j]+delta[j]})+radius+skin;}std::optional<CapsuleHit> result;
 for(auto i:candidates(lo,hi)){auto&t=triangles[i];if(!filter.matches(t.attribute))continue;auto a=vertices[t.vertices[0]],b=vertices[t.vertices[1]],c=vertices[t.vertices[2]];float time=0;
  if(outside_swept_plane(startA,startB,delta,radius,skin,a,b,c))continue;
  for(unsigned step=0;step<32;++step){auto p=add(startA,mul(delta,time)),q=add(startB,mul(delta,time));auto h=closest(p,q,a,b,c);float distance=std::sqrt(h.squared);auto n=distance>1e-5f?mul(sub(h.segment,h.triangle),1/distance):unit(cross(sub(b,a),sub(c,a)));
   if(distance<=1e-5f&&dot(delta,n)>0)n=mul(n,-1);float closing=-dot(delta,n);if(closing<=1e-6f)break;
   float separation=distance-radius-skin;
   if(separation<=.01f||step==31){if(!result||time<result->fraction||(time==result->fraction&&i<result->triangle))result=CapsuleHit{time,n,i};break;}
   float advance=separation/closing;time+=std::max(advance,1e-7f);if(time>1||(result&&time>result->fraction))break;
  }
 }return result;
}
std::vector<CollisionContact> Collision::contacts(Vec3 a,Vec3 b,float radius,float margin,size_t limit,CollisionQuery filter)const{
 std::vector<CollisionContact> result;if(!finite(a)||!finite(b)||!std::isfinite(radius)||!std::isfinite(margin)||radius<=0||radius>10000||margin<0||margin>radius||!limit||limit>256)return result;
 Vec3 lo,hi;for(int j=0;j<3;++j){lo[j]=std::min(a[j],b[j])-radius-margin;hi[j]=std::max(a[j],b[j])+radius+margin;}
 for(auto i:candidates(lo,hi)){auto&t=triangles[i];if(!filter.matches(t.attribute))continue;auto x=vertices[t.vertices[0]],y=vertices[t.vertices[1]],z=vertices[t.vertices[2]];auto h=closest(a,b,x,y,z);float distance=std::sqrt(h.squared);if(distance>radius+margin)continue;
  auto n=distance>1e-5f?mul(sub(h.segment,h.triangle),1/distance):unit(cross(sub(y,x),sub(z,x)));if(dot(n,n)<.5f)continue;if(distance<=1e-5f&&dot(n,sub(mul(add(a,b),.5f),x))<0)n=mul(n,-1);
  result.push_back({h.triangle,n,radius-distance,i});
 }
 std::sort(result.begin(),result.end(),[](auto&a,auto&b){return a.penetration!=b.penetration?a.penetration>b.penetration:a.triangle<b.triangle;});if(result.size()>limit)result.resize(limit);return result;
}
float Collision::fall_prevention_fraction(Vec3 feet,Vec3 delta,Capsule shape)const{
 if(!finite(feet)||!finite(delta)||!valid(shape))return 0;
 const float length=std::hypot(delta[0],delta[2]);if(length<.001f)return 1;
 // Don't infer a fall barrier from Cliff, Rail or a lack of support. Only an
 // explicit Don't Fall surface can retain a grounded player at its edge.
 const auto contact=sweep(feet,{0,-4*shape.skin,0},shape,query::floor);
 if(!contact||contact->normal[1]<.70710678f)return 1;
 auto origin=feet;origin[1]+=8;
 auto support=ray(origin,{0,-1,0},48,query::floor);
 if(!support||support->normal[1]<.70710678f)return 1;
 const unsigned count=unsigned(std::ceil(length/std::max(10.f,shape.radius*.125f)));
 if(count>4096)return 0;
 const float stepLength=length/count;
 bool guarded=attribute::has(triangles[support->triangle].attribute,attribute::dont_fall);
 float previous=0,height=support->position[1];
 for(unsigned i=1;i<=count;++i){
  const float fraction=float(i)/count;
  auto probe=add(feet,mul(delta,fraction));probe[1]=height+stepLength+8;
  const auto floorAt=[&](float f){auto p=probe;p[0]=feet[0]+delta[0]*f;p[2]=feet[2]+delta[2]*f;
   auto hit=ray(p,{0,-1,0},2*stepLength+48,query::floor);
   if(hit&&hit->normal[1]>=.70710678f)return hit;return std::optional<CollisionHit>{};};
  auto next=floorAt(fraction);
  if(!next){
   if(!guarded)return 1;
   float lo=previous,hi=fraction;
   for(unsigned j=0;j<16;++j){const float mid=(lo+hi)*.5f;if(floorAt(mid))lo=mid;else hi=mid;}
   return std::max(0.f,lo-shape.skin/length);
  }
  height=next->position[1];guarded=attribute::has(triangles[next->triangle].attribute,attribute::dont_fall);previous=fraction;
 }
 return 1;
}
}
