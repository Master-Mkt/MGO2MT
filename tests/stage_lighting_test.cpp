#include "stage_lighting.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <iostream>
#include <limits>
using namespace mgo2mt::stage;
void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
int main(int argc,char**argv){try{
 Hemisphere h;h.minimum={-4,-4,-4};h.maximum={4,4,4};h.extent={1,1,1};h.positive={.5f,.5f,.5f};h.negative={1,1,1};h.flags=0x100;h.direction={0,-1,0};h.front={1,0,0};h.back={0,0,1};
 check(Lighting::weight(h,{0,0,0})==1,"inner volume");check(Lighting::weight(h,{2,0,0})==.5f,"positive feather");check(Lighting::weight(h,{-2,0,0})==0,"negative feather");check(Lighting::weight(h,{4,0,0})==0,"strict outer bounds");
 h.flags|=0x8000;check(Lighting::weight(h,{0,0,0})==0,"disabled volume");h.flags=0x100;
 h.positive[0]=std::numeric_limits<float>::infinity();check(Lighting::weight(h,{1,0,0})==1&&Lighting::weight(h,{1.001f,0,0})==0,"zero-width feather boundary without NaN");h.positive[0]=.5f;
 h.quaternion={0,0,std::sqrt(.5f),std::sqrt(.5f)};check(std::abs(Lighting::weight(h,{0,2,0})-.5f)<.00001f,"rotated positive feather");h.quaternion={0,0,0,1};
 Lighting l;l.hemispheres.push_back(h);auto up=l.sample({0,0,0},{0,1,0}),down=l.sample({0,0,0},{0,-1,0});check(up.color[0]==1&&up.color[2]==0&&down.color[2]==1,"directional front/back colors");
 PointLight point;point.position={0,2,0};point.color={1,.5f,0};point.range=4;point.extendedRange=4;point.flags=0x100;
 check(Lighting::point_sample(point,{0,0,0},{0,1,0})[0]==.5f,"point radius and linear attenuation");check(Lighting::point_sample(point,{0,0,0},{0,-1,0})[0]==0,"back-facing point diffuse");check(Lighting::point_sample(point,{0,-2,0},{0,1,0})[0]==0,"strict point sphere edge");point.flags=0x200;check(Lighting::point_sample(point,{0,0,0},{0,1,0})[0]==0,"static-only authored flag is not dynamic activation");point.flags=0x8100;check(Lighting::point_sample(point,{0,0,0},{0,1,0})[0]==0,"disabled point");
 TransientLights transient;check(transient.add(point,10,1),"temporary light insert");check(transient.sample({0,0,0},{0,1,0},10)[0]==.5f&&transient.sample({0,0,0},{0,1,0},11)[0]==0,"temporary light deadline");transient.expire(11);check(transient.entries.empty(),"expired storage removed");check(!transient.add(point,10,0),"invalid lifetime rejected");for(int i=0;i<128;++i)check(transient.add(point,10,1),"bounded temporary light capacity");check(!transient.add(point,10,1),"temporary light overflow rejected");transient.clear();check(transient.entries.empty(),"stage reset removes temporary lights");
 auto other=h;other.front={0,1,0};l.hemispheres.push_back(other);auto overlap=l.sample({0,0,0},{0,1,0});check(overlap.volumes==2&&overlap.color[0]==.5f&&overlap.color[1]==.5f,"overlap normalizes weights");
 for(auto text:{"", "MGO2MT.STAGE_LIGHTS 1 4097", "MGO2MT.STAGE_LIGHTS 2 0"}){std::istringstream in(text);bool rejected=false;try{Lighting::read(in);}catch(...){rejected=true;}check(rejected,"bad native lighting rejected");}
 if(argc>1){std::ifstream in(argv[1]);auto actual=Lighting::read(in);check(actual.hemispheres.size()==104,"n022a hemisphere count");auto&v=actual.hemispheres[0];auto s=actual.sample(v.center,{0,1,0});check(s.volumes>0&&std::isfinite(s.color[0]),"actual stage evaluates");}
 std::cout<<"hemisphere activation, rotated feathering, asymmetric colors, overlap and parser passed\n";
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
