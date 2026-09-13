#include "stage_collision.h"
#include <sstream>
#include <iostream>
#include <limits>
#include <fstream>
using namespace mgo2win::stage;
namespace {void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}bool near(float a,float b){return std::abs(a-b)<.0001f;}}
int main(int argc,char**argv){try{
 // Deliberately unsorted, opposite-facing layers and coincident distinct faces.
 std::vector<Vec3> v{{-2,-2,4},{2,-2,4},{0,2,4},{-2,-2,2},{0,2,2},{2,-2,2}};
 std::vector<CollisionTriangle> t{{{0,1,2},0x1234000000008000ull,128,0,91},{{3,4,5},0x8000000000000000ull,64,1,92},{{3,4,5},7,64,1,93}};
 auto c=Collision::make(v,t,{{10,.5f,.4f,true,250,true},{11,.25f,.3f,true,-5,true}});
 auto h=c.ray_all({0,0,0},{0,0,7},10);check(h.size()==3,"all distinct coincident triangles retained");
 check(h[0].triangle==1&&h[1].triangle==2&&h[2].triangle==0&&near(h[0].distance,2)&&near(h[2].distance,4),"sorted normalized-ray world distances with stable ties");
 check(h[0].frontFace&&!h[2].frontFace&&h[0].normal[2]==-1&&h[2].normal[2]==1,"unflipped face orientation");
 check(h[0].material.resistance==-5&&h[0].material.resistanceVerified&&h[2].material.resistance==250&&h[2].attribute==0x1234000000008000ull&&h[0].object==92,"signed resistance full64 attr object material retained");
 auto nearest=c.ray({0,0,0},{0,0,7},10);check(nearest&&nearest->triangle==h[0].triangle&&nearest->normal[2]==-1,"legacy nearest unchanged");
 auto reverse=c.ray_all({0,0,5},{0,0,-1},3);check(reverse.size()==3&&reverse[0].frontFace&&!reverse[1].frontFace,"reverse front/back and inclusive maximum");
 check(c.ray_all({0,0,0},{0,0,1},1.99f).empty()&&c.ray_all({0,0,0},{1,0,0},10).empty(),"finite segment and parallel exclusion");
 for(float invalid:{0.f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})check(c.ray_all({0,0,0},{0,0,1},invalid).empty(),"invalid extent");
 check(c.ray_all({0,0,0},{0,0,0},10).empty()&&c.ray_all({0,0,0},{0,0,std::numeric_limits<float>::infinity()},10).empty(),"invalid direction");
 // Shared diagonal is exposed for a consumer to merge using its full surface identity.
 auto square=Collision::make({{-1,-1,2},{1,-1,2},{1,1,2},{-1,1,2}},{{{0,1,2}},{{0,2,3}}});
 check(square.ray_all({0,0,0},{0,0,1},2).size()==2,"shared edge not silently discarded");
 auto unknown=square.material(0);check(unknown.resistance==1000&&!unknown.resistanceVerified,"unresolved resistance fallback");
 auto g=std::make_shared<const Collision>(c);auto combined=Collision::combine(Collision{},std::array<CollisionInstance,1>{{{73,g,{10,0,0},{0,180,0}}}});
 auto moved=combined.ray_all({10,0,0},{0,0,-1},10);check(moved.size()==3&&moved[0].object==73&&moved[0].frontFace&&moved[0].material.resistance==-5,"instance transform preserves oriented normals and material");
 for(unsigned version:{1u,2u,3u}){std::ostringstream out;out<<"MGO2WIN.STAGE_COLLISION "<<version<<" 3 1 ";if(version>=2)out<<"1 17 .25 .75 1 ";if(version==3)out<<"-2147483648 1 ";out<<"-1 -1 2 1 -1 2 0 1 2 0 1 2 32768 64 ";if(version>=2)out<<"0";std::istringstream in(out.str());auto loaded=Collision::read(in);auto value=loaded.ray_all({0,0,0},{0,0,1},3);check(value.size()==1,"cfg compatibility");check(value[0].material.resistance==(version==3?std::numeric_limits<int32_t>::min():1000)&&value[0].material.resistanceVerified==(version==3),"v1v2 unknown vs v3 signed resistance");}
 for(auto row:{"100 0","2147483648 1","100 2"}){std::istringstream in(std::string("MGO2WIN.STAGE_COLLISION 3 0 0 1 1 .5 .5 1 ")+row);bool refused=false;try{Collision::read(in);}catch(...){refused=true;}check(refused,"reject malformed resistance record");}
 // BVH order is deliberately unrelated to range; all 200 surfaces survive.
 v.clear();t.clear();for(int i=200;i>0;--i){unsigned b=unsigned(v.size());v.insert(v.end(),{{-1,-1,float(i)},{1,-1,float(i)},{0,1,float(i)}});t.push_back({{b,b+1,b+2}});}auto many=Collision::make(v,t).ray_all({0,0,0},{0,0,1},200);check(many.size()==200&&many.front().distance==1&&many.back().distance==200,"all BVH hits no arbitrary cap");
 if(argc>1){std::ifstream in(argv[1]);auto actual=Collision::read(in);size_t verified=0;for(auto m:actual.materials)verified+=m.resistanceVerified;check(verified>0,"actual material resistance records recovered");std::cout<<"actual materials="<<actual.materials.size()<<" resistanceVerified="<<verified<<'\n';}
 std::cout<<"stage_collision_ray_all_test PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<"stage_collision_ray_all_test FAIL "<<e.what()<<'\n';return 1;}}
