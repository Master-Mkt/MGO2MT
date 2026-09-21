#include "mounted_weapons.h"
#include "multi_ui_json.h"
#include "gameplay_config.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>
namespace mgo2mt::mounted {
namespace {
constexpr float pi=3.14159265359f;
bool finite(Vec3 v,float bound=1000000){return std::all_of(v.begin(),v.end(),[&](float f){return std::isfinite(f)&&std::abs(f)<bound;});}
Vec3 add(Vec3 a,Vec3 b){for(int i=0;i<3;++i)a[i]+=b[i];return a;}
bool asset(const std::string& s){return gameplay::relative_resource_path(s);}
using J=multi_ui::json::Value;
void require(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void keys(const J& v,std::initializer_list<const char*> names){require(v.type==J::object,"Mounted JSON object required");for(const auto& [k,x]:v.o)require(std::any_of(names.begin(),names.end(),[&](const char* n){return k==n;}),"Unknown mounted JSON field");}
const J& field(const J&v,const char*n){auto i=v.o.find(n);require(i!=v.o.end(),"Missing mounted JSON field");return i->second;}
std::string text(const J& v){require(v.type==J::string,"Mounted JSON text required");return v.s;}
float number(const J& v){require(v.type==J::number,"Mounted JSON number required");return float(v.n);}
unsigned integer(const J&v,unsigned max){require(v.type==J::number&&v.n>=0&&v.n<=max&&std::floor(v.n)==v.n,"Mounted JSON integer out of range");return unsigned(v.n);}
Vec3 vector(const J&v){require(v.type==J::array&&v.a.size()==3,"Mounted JSON vec3 required");return {number(v.a[0]),number(v.a[1]),number(v.a[2])};}
}
bool valid(const Type&t){
 const bool launch=unsigned(t.kind)<=unsigned(Kind::catapult)&&std::isfinite(t.bindPitch)&&std::abs(t.bindPitch)<=pi&&finite(t.launchDirection,2)&&std::abs(std::hypot(t.launchDirection[0],t.launchDirection[1],t.launchDirection[2])-1)<.01f&&std::isfinite(t.launchSpeed)&&t.launchSpeed>=100&&t.launchSpeed<=100000&&std::isfinite(t.gravity)&&t.gravity>=100&&t.gravity<=50000&&std::isfinite(t.blastRadius)&&t.blastRadius>=100&&t.blastRadius<=20000&&t.cooldownMs>=100&&t.cooldownMs<=60000&&t.maxFlightMs>=100&&t.maxFlightMs<=30000;
 return launch&&(t.projectileModel.empty()||asset(t.projectileModel))&&!t.id.empty()&&t.id.size()<=64&&!t.name.empty()&&t.name.size()<=128&&asset(t.model)&&(t.rig.empty()||asset(t.rig))&&(t.maleMotion.empty()||asset(t.maleMotion))&&(t.femaleMotion.empty()||asset(t.femaleMotion))&&(t.kind==Kind::catapult||t.weapon)&&finite(t.collisionCenter,10000)&&finite(t.collisionHalfExtents,10000)&&(t.collisionHalfExtents==Vec3{}||std::all_of(t.collisionHalfExtents.begin(),t.collisionHalfExtents.end(),[](float f){return f>=1;}))&&finite(t.ejection,10000)&&finite(t.ejectionDirection,2)&&(t.ejectionDirection==Vec3{}||(std::hypot(t.ejectionDirection[0],t.ejectionDirection[1],t.ejectionDirection[2])>=.99f&&std::hypot(t.ejectionDirection[0],t.ejectionDirection[1],t.ejectionDirection[2])<=1.01f))&&finite(t.eye,10000)&&finite(t.pivot,10000)&&finite(t.muzzle,10000)&&finite(t.operatorOffset,10000)&&std::isfinite(t.yawMin)&&std::isfinite(t.yawMax)&&t.yawMin>=-pi&&t.yawMax<=pi&&t.yawMin<=0&&t.yawMax>=0&&std::isfinite(t.pitchMin)&&std::isfinite(t.pitchMax)&&t.pitchMin>=-1.4f&&t.pitchMax<=1.4f&&t.pitchMin<=t.pitchMax&&std::isfinite(t.initialPitch)&&t.initialPitch>=t.pitchMin&&t.initialPitch<=t.pitchMax&&std::isfinite(t.useRadius)&&t.useRadius>=100&&t.useRadius<=5000;}
bool valid(const Instance&i){return i.map&&i.id&&i.id<=32767&&!i.type.empty()&&finite(i.origin)&&std::isfinite(i.yaw)&&std::abs(i.yaw)<=pi;}
const Type* Registry::find(const std::string&id)const{for(const auto&t:types)if(t.id==id)return &t;return nullptr;}
const Instance* Registry::find(uint8_t map,uint16_t id)const{for(const auto&i:placements)if(i.map==map&&i.id==id)return &i;return nullptr;}
std::vector<Instance> Registry::scene(uint8_t map)const{std::vector<Instance> out;for(const auto&i:placements)if(i.map==map)out.push_back(i);return out;}
bool Registry::load(const std::filesystem::path&path,std::string& error){try{
 std::ifstream file(path,std::ios::binary|std::ios::ate);require(bool(file),"Mounted JSON cannot be opened");auto size=file.tellg();require(size>0&&size<=1024*1024,"Mounted JSON size limit");file.seekg(0);std::string bytes(size_t(size),'\0');require(bool(file.read(bytes.data(),std::streamsize(bytes.size()))),"Mounted JSON read failed");auto root=multi_ui::json::parse(bytes);keys(root,{"version","types","placements"});require(integer(field(root,"version"),1)==1,"Mounted JSON version");
 Registry next;const auto& ts=field(root,"types");require(ts.type==J::array&&ts.a.size()<=64,"Mounted type capacity");std::set<std::string> names;
 for(const auto&v:ts.a){keys(v,{"id","name","weapon","model","pivot","muzzle","operatorOffset","yawMin","yawMax","pitchMin","pitchMax","useRadius","infiniteAmmo","rig","maleMotion","femaleMotion","collisionCenter","collisionHalfExtents","eye","operatorOrbit","ejection","ejectionDirection","projectileModel","kind","bindPitch","initialPitch","launchDirection","launchSpeed","gravity","blastRadius","cooldownMs","maxFlightMs"});Type t;
 if(auto it=v.o.find("kind");it!=v.o.end()){auto value=text(it->second);require(value=="gun"||value=="mortar"||value=="catapult","Unknown mounted kind");t.kind=value=="mortar"?Kind::mortar:value=="catapult"?Kind::catapult:Kind::gun;}
 for(auto [key,value]:{std::pair{"bindPitch",&t.bindPitch},std::pair{"initialPitch",&t.initialPitch},std::pair{"launchSpeed",&t.launchSpeed},std::pair{"gravity",&t.gravity},std::pair{"blastRadius",&t.blastRadius}})if(auto it=v.o.find(key);it!=v.o.end())*value=number(it->second);
 if(auto it=v.o.find("launchDirection");it!=v.o.end())t.launchDirection=vector(it->second);
 if(auto it=v.o.find("cooldownMs");it!=v.o.end())t.cooldownMs=integer(it->second,60000);
 if(auto it=v.o.find("maxFlightMs");it!=v.o.end())t.maxFlightMs=integer(it->second,30000);
 t.id=text(field(v,"id"));t.name=text(field(v,"name"));t.model=text(field(v,"model"));t.weapon=uint16_t(integer(field(v,"weapon"),65535));t.pivot=vector(field(v,"pivot"));t.muzzle=vector(field(v,"muzzle"));t.operatorOffset=vector(field(v,"operatorOffset"));t.yawMin=number(field(v,"yawMin"));t.yawMax=number(field(v,"yawMax"));t.pitchMin=number(field(v,"pitchMin"));t.pitchMax=number(field(v,"pitchMax"));t.useRadius=number(field(v,"useRadius"));const auto& infinity=field(v,"infiniteAmmo");require(infinity.type==J::boolean,"Mounted infiniteAmmo boolean required");t.infiniteAmmo=infinity.b;for(auto pair:{std::pair{"projectileModel",&t.projectileModel},std::pair{"rig",&t.rig},std::pair{"maleMotion",&t.maleMotion},std::pair{"femaleMotion",&t.femaleMotion}})if(auto it=v.o.find(pair.first);it!=v.o.end())*pair.second=text(it->second);if(auto it=v.o.find("operatorOrbit");it!=v.o.end()){require(it->second.type==J::boolean,"Mounted operatorOrbit boolean required");t.operatorOrbit=it->second.b;}if(auto it=v.o.find("ejection");it!=v.o.end())t.ejection=vector(it->second);if(auto it=v.o.find("ejectionDirection");it!=v.o.end())t.ejectionDirection=vector(it->second);if(auto it=v.o.find("eye");it!=v.o.end())t.eye=vector(it->second);if(auto it=v.o.find("collisionCenter");it!=v.o.end())t.collisionCenter=vector(it->second);if(auto it=v.o.find("collisionHalfExtents");it!=v.o.end())t.collisionHalfExtents=vector(it->second);require(valid(t)&&names.insert(t.id).second,"Invalid or duplicate mounted type");next.types.push_back(std::move(t));}
 const auto& ps=field(root,"placements");require(ps.type==J::array&&ps.a.size()<=1024,"Mounted placement capacity");std::set<std::pair<uint8_t,uint16_t>> ids;std::array<unsigned,256> counts{};
 for(const auto&v:ps.a){keys(v,{"map","id","type","origin","yaw"});Instance i;i.map=uint8_t(integer(field(v,"map"),255));i.id=uint16_t(integer(field(v,"id"),32767));i.type=text(field(v,"type"));i.origin=vector(field(v,"origin"));i.yaw=number(field(v,"yaw"));require(valid(i)&&next.find(i.type)&&ids.emplace(i.map,i.id).second&&++counts[i.map]<=128,"Invalid or duplicate mounted placement");next.placements.push_back(std::move(i));}
 *this=std::move(next);error.clear();return true;
 }catch(const std::exception&e){error=e.what();return false;}}
Vec3 rotate(Vec3 v,float yaw,float pitch){const float cp=std::cos(pitch),sp=std::sin(pitch),cy=std::cos(yaw),sy=std::sin(yaw);const float y=v[1]*cp+v[2]*sp,z=v[2]*cp-v[1]*sp;return {v[0]*cy+z*sy,y,z*cy-v[0]*sy};}
Vec3 operator_position(const Instance&i,const Type&t){return operator_position(i,t,i.yaw);}
Vec3 operator_position(const Instance&i,const Type&t,float yaw){
 if(!t.operatorOrbit)return add(i.origin,rotate(t.operatorOffset,i.yaw));
 const Vec3 pivot{t.pivot[0],0,t.pivot[2]},delta{t.operatorOffset[0]-pivot[0],t.operatorOffset[1],t.operatorOffset[2]-pivot[2]};
 return add(add(i.origin,rotate(pivot,i.yaw)),rotate(delta,yaw));
}
bool within_use_range(const Instance&i,const Type&t,Vec3 feet){
 if(!finite(feet)||!finite(i.origin)||!finite(t.operatorOffset,10000)||
    (t.operatorOrbit&&!finite(t.pivot,10000))||!std::isfinite(i.yaw)||std::abs(i.yaw)>pi||
    !std::isfinite(t.useRadius)||t.useRadius<100||t.useRadius>5000)return false;
 const auto position=operator_position(i,t);if(!finite(position))return false;
 double squared=0;for(unsigned axis=0;axis<3;++axis){const double delta=double(position[axis])-feet[axis];squared+=delta*delta;}
 return squared<=double(t.useRadius)*t.useRadius;
}
std::vector<Vec3> operator_path(const Instance&i,const Type&t,float fromYaw,float toYaw){
 if(!t.operatorOrbit)return {operator_position(i,t)};
 const float from=std::remainder(fromYaw-i.yaw,2*pi),to=std::remainder(toYaw-i.yaw,2*pi),turn=to-from;
 const float radius=std::hypot(t.operatorOffset[0]-t.pivot[0],t.operatorOffset[2]-t.pivot[2]);
 // Keep the chord-to-arc deviation below 0.25 mm, smaller than capsule skin.
 const float step=std::min(.1f,std::sqrt(2.f/std::max(radius,1.f)));
 const unsigned count=std::clamp(unsigned(std::ceil(std::abs(turn)/step)),1u,1024u);
 std::vector<Vec3> path;path.reserve(count);for(unsigned n=1;n<=count;++n)path.push_back(operator_position(i,t,i.yaw+from+turn*float(n)/float(count)));return path;
}
Vec3 pivot_position(const Instance&i,const Type&t){return add(i.origin,rotate(t.pivot,i.yaw));}
Vec3 muzzle_position(const Instance&i,const Type&t,float yaw,float pitch){auto delta=t.muzzle;for(int n=0;n<3;++n)delta[n]-=t.pivot[n];return add(pivot_position(i,t),rotate(delta,yaw,pitch-t.bindPitch));}
Vec3 launch_direction(const Type&t,float yaw,float pitch){return rotate(t.launchDirection,yaw,pitch-t.bindPitch);}
stage::Collision with_collision(const stage::Collision& base,const Registry&r,uint8_t map){
 // Reserved native object IDs make repeated assembly idempotent and allow
 // object-world replacements to retain bases without duplicating triangles.
 auto vertices=base.vertices;auto triangles=base.triangles;const auto oldCount=triangles.size();std::erase_if(triangles,[](const auto&t){return (t.object&0xffff0000u)==0xf0320000u;});
 if(triangles.size()!=oldCount){std::vector<unsigned> remap(vertices.size(),~0u);std::vector<Vec3> compact;compact.reserve(vertices.size());for(auto&t:triangles)for(auto&v:t.vertices){if(remap[v]==~0u){remap[v]=unsigned(compact.size());compact.push_back(vertices[v]);}v=remap[v];}vertices=std::move(compact);}
 constexpr unsigned faces[12][3]={{0,2,1},{1,2,3},{4,5,6},{5,7,6},{0,1,4},{1,5,4},{2,6,3},{3,6,7},{0,4,2},{2,4,6},{1,3,5},{3,7,5}};
 for(const auto&i:r.scene(map)){const auto*t=r.find(i.type);if(!t||t->collisionHalfExtents==Vec3{})continue;if(!valid(i)||!valid(*t))throw std::invalid_argument("Mounted base collision");const auto first=unsigned(vertices.size());
  for(unsigned corner=0;corner<8;++corner){Vec3 p=t->collisionCenter;for(unsigned n=0;n<3;++n)p[n]+=t->collisionHalfExtents[n]*((corner&(1u<<n))?1.f:-1.f);vertices.push_back(add(i.origin,rotate(p,i.yaw)));}
  for(const auto&face:faces)triangles.push_back({{first+face[0],first+face[1],first+face[2]},stage::attribute::native_solid,0,~0u,0xf0320000u|i.id});
 }
 return stage::Collision::make(std::move(vertices),std::move(triangles),base.materials);
}
void clamp_aim(const Instance&i,const Type&t,float&yaw,float&pitch){yaw=std::remainder(i.yaw+std::clamp(std::remainder(yaw-i.yaw,2*pi),t.yawMin,t.yawMax),2*pi);pitch=std::clamp(pitch,t.pitchMin,t.pitchMax);}
}
