#ifdef NDEBUG
#undef NDEBUG
#endif
#include "stage_water.h"
#include "stage_collision.h"
#include <cassert>
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
using namespace mgo2win::stage;
static void add(std::string& b,unsigned x){for(unsigned s=0;s<32;s+=8)b.push_back(char(x>>s));}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}assert(bad);}
int main(int argc,char** argv){
 const WaterTriangle horizontal{{Vec3{0,0,0},Vec3{0,0,10},Vec3{10,0,0}}};
 auto surface=WaterSurface::make({horizontal});
 for(unsigned kind=0;kind<8;++kind)assert(original_water_level_kind(kind)==(kind==3||kind==4));
 assert(!original_water_level_kind(2)); // Original level/foot consumers reject named polygons.
 auto hit=surface.crossing({2,5,2},{2,-5,2});assert(hit&&hit->fraction==.5f&&hit->position==Vec3({2,0,2})&&hit->normal[1]==1);
 assert(surface.crossing({2,-5,2},{2,5,2})); // Both sides; no invented air/water side.
 assert(surface.crossing({2,5,2},{2,0,2})->fraction==1);
 assert(!surface.crossing({2,0,2},{2,5,2})); // (from,to] prevents repeated start contact.
 assert(!surface.crossing({2,0,2},{3,0,2})&&!surface.crossing({2,0,2},{2,0,2}));
 assert(!surface.crossing({8,5,8},{8,-5,8})); // Outside finite triangle, same plane.
 assert(surface.crossing({5,5,5},{5,-5,5})); // Shared edge is included.
 auto upper=horizontal;for(auto& p:upper.vertices)p[1]=2;
 assert(WaterSurface::make({horizontal,upper}).crossing({2,5,2},{2,-5,2})->triangle==1);
 assert(WaterSurface::make({horizontal,horizontal}).crossing({2,5,2},{2,-5,2})->triangle==0);
 auto vertical=horizontal;for(auto& p:vertical.vertices)std::swap(p[0],p[1]);
 assert(WaterSurface::make({vertical}).crossing({5,2,2},{-5,2,2}));
 const float nan=std::numeric_limits<float>::quiet_NaN();assert(!surface.crossing({nan,0,0},{0,0,0}));
 auto invalid=horizontal;invalid.vertices[0][0]=nan;rejects([&]{WaterSurface::make({invalid});});
 rejects([]{WaterSurface::make({WaterTriangle{}});});
 rejects([&]{WaterSurface::make(std::vector<WaterTriangle>(4097,horizontal));});
 std::string bytes="GWS1";add(bytes,1);add(bytes,1);for(auto p:horizontal.vertices)for(float x:p)add(bytes,std::bit_cast<unsigned>(x));
 std::istringstream valid(bytes);assert(WaterSurface::read(valid).triangles().size()==1);
 for(size_t n=0;n<bytes.size();++n)rejects([&]{std::istringstream in(bytes.substr(0,n));WaterSurface::read(in);});
 rejects([&]{std::istringstream in(bytes+"x");WaterSurface::read(in);});
 auto badVersion=bytes;badVersion[4]=2;rejects([&]{std::istringstream in(badVersion);WaterSurface::read(in);});
 rejects([&]{std::istringstream in(bytes);Water::read(in);}); // Formats cannot be confused.
 if(argc==4){
  std::ifstream si(argv[1],std::ios::binary),wi(argv[2],std::ios::binary),ci(argv[3]);
  auto actual=WaterSurface::read(si);auto volume=Water::read(wi);
  auto full=std::make_shared<Collision>(Collision::read(ci));auto movement=movement_collision(full);
  assert(actual.triangles().size()==77&&volume.fields().empty());
  unsigned samples=0;
  for(const auto& t:actual.triangles()){
   Vec3 center{},a{},b{},normal{};
   for(unsigned k=0;k<3;++k){center[k]=(t.vertices[0][k]+t.vertices[1][k]+t.vertices[2][k])/3;a[k]=t.vertices[1][k]-t.vertices[0][k];b[k]=t.vertices[2][k]-t.vertices[0][k];}
   normal={a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
   const float length=std::sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
   Vec3 from=center,to=center;
   for(unsigned k=0;k<3;++k){normal[k]/=length;from[k]+=normal[k]*10;to[k]-=normal[k]*10;}
   auto crossing=actual.crossing(from,to);assert(crossing);
   assert(!volume.level(center)&&!volume.control_level(center,center[1]-100)&&!volume.on_foot(center,center[1]-100));
   if(samples++==0){
    Vec3 direction{-normal[0],-normal[1],-normal[2]};auto raw=full->ray(from,direction,20);
    assert(raw&&full->triangles[raw->triangle].attribute==0x40048000ULL);
    std::cout<<"AA sample from="<<from[0]<<','<<from[1]<<','<<from[2]<<" to="<<to[0]<<','<<to[1]<<','<<to[2]<<" hit="<<crossing->position[0]<<','<<crossing->position[1]<<','<<crossing->position[2]<<" fraction="<<crossing->fraction<<'\n';
   }
  }
  assert(samples==77);
  for(const auto& t:movement->triangles)assert(t.attribute!=0x40048000ULL);
  std::cout<<"AA 77 actual surface crossings; empty volume stays dry; movement excludes water while raw collision retains it PASS\n";
 }else assert(argc==1);
 std::cout<<"stage_water_surface: finite/two-sided/nearest/edges/start exclusion, invalid bytes, volume isolation PASS\n";
}
