#include "stage_collision.h"
#include <cmath>
#include <stdexcept>
#include <string>
namespace mgo2win::stage {
static Vec3 sub(Vec3 a,Vec3 b){for(int i=0;i<3;++i)a[i]-=b[i];return a;}
static Vec3 cross(Vec3 a,Vec3 b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
static float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Collision Collision::read(std::istream&in){
 auto require=[](bool ok){if(!ok)throw std::runtime_error("Invalid native stage collision");};
 std::string magic;unsigned version,nv,nt;require(bool(in>>magic>>version>>nv>>nt));require(magic=="MGO2WIN.STAGE_COLLISION"&&version==1&&nv<=500000&&nt<=500000);
 Collision c;c.vertices.resize(nv);c.triangles.resize(nt);
 for(auto&v:c.vertices)for(auto&x:v)require(bool(in>>x)&&std::isfinite(x)&&std::abs(x)<1e7);
 for(auto&t:c.triangles){for(auto&i:t.vertices)require(bool(in>>i)&&i<nv);require(bool(in>>t.attribute>>t.polygonAttribute)&&t.polygonAttribute<=65535);}
 std::string tail;require(!(in>>tail));return c;
}
std::optional<CollisionHit> Collision::ray(Vec3 o,Vec3 d,float maximum)const{
 if(!std::isfinite(maximum)||maximum<=0)return {};
 for(auto x:o)if(!std::isfinite(x))return {};
 float length=std::sqrt(dot(d,d));if(!std::isfinite(length)||length<1e-8f)return {};for(auto&x:d)x/=length;
 std::optional<CollisionHit> hit;
 for(size_t i=0;i<triangles.size();++i){const auto&t=triangles[i];auto a=vertices[t.vertices[0]],e1=sub(vertices[t.vertices[1]],a),e2=sub(vertices[t.vertices[2]],a);auto p=cross(d,e2);float determinant=dot(e1,p);if(std::abs(determinant)<1e-8f)continue;float inv=1/determinant;auto s=sub(o,a);float u=dot(s,p)*inv;if(u<0||u>1)continue;auto q=cross(s,e1);float v=dot(d,q)*inv;if(v<0||u+v>1)continue;float distance=dot(e2,q)*inv;if(distance<0||distance>maximum)continue;maximum=distance;CollisionHit h{distance,o,i};for(int j=0;j<3;++j)h.position[j]+=d[j]*distance;hit=h;}
 return hit;
}
}
