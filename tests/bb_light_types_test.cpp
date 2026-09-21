#include "stage_lighting.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace mgo2mt::stage;
static void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
static std::string config(const std::string&record){return "MGO2MT.STAGE_LIGHTS 4 0\n0 -1 0 0 0 0 0 0 0 0 0 0 0 1 0 1 1 1 -100 -100 -100 100 100 100\nPOINTS 0\nAUTHORED 1\n"+record;}
static std::string row="2 -10 -10 -10 10 10 10 0 2 0 0 0 -1 0 0 1 .5 0 .8 .2 4 512 258 1 2 -10 -10 -10 10 10 10\n";
int main(int argc,char**argv){try{
 std::istringstream input(config(row));auto l=Lighting::read(input);check(l.authored.size()==1&&l.points.empty(),"typed record preserved");auto&a=l.authored.front();
 check(a.flags==512&&a.identity.groupFlags==258,"raw static and group flags preserved");
 check(std::abs(l.sample({0,0,0},{0,1,0}).color[0]-.5f)<1e-6f,"native spot axis attenuation");
 check(l.sample({0,3,0},{0,-1,0}).color[0]==0,"spot rejects rear");
 check(l.sample({0,0,0},{0,-1,0}).color[0]==0,"back facing normal dark");
 check(l.sample({0,-2,0},{0,1,0}).color[0]==0,"strict radius boundary");
 auto point=a;point.kind=4;point.identity.groupFlags=260;l.authored.push_back(point);
 check(Lighting::authored_sample(point,{0,3,0},{0,-1,0})[0]>.7f,"type4 native point retains type");
 check(l.enable(3,1,false)==0&&l.enable(2,2,false)==0,"key and id both checked");
 check(l.enable_sphere({0,0,0},2,false)==0,"strict original reference sphere boundary");
 check(l.enable_sphere({0,0,0},2.01f,false)==2,"original sphere includes both types");
 check(l.sample({0,0,0},{0,1,0}).color[0]==0,"disabled samples dark");
 check(l.enable(2,1,true)==2&&l.enable(2,1,true)==0,"idempotent enable and restore");
 l.authored[0].identity.groupFlags=2;check(l.enable(2,1,false)==1,"inactive authored group not mutated");
 l.authored[0].identity.groupFlags=258;l.authored[0].identity.groupMinimum={50,50,50};l.authored[0].identity.groupMaximum={60,60,60};
 check(l.enable_sphere({0,2,0},1,false)==0,"authored group AABB gates toggle");
 for(const auto&bad:{std::string("4"),std::string("2 -10 -10 -10 10 10 10 0 2 0 0 0 -1 0 0 1 .5 0 .2 .8 4 512 258 1 2 -10 -10 -10 10 10 10"),std::string("8 ")+row.substr(2),row+"extra"}){
  bool threw=false;try{std::istringstream in(config(bad));Lighting::read(in);}catch(...){threw=true;}check(threw,"malformed typed config rejected");
 }
 // No v4 tag is required by old version 3 readers; prior fields retain values.
 std::string old=config("");old.replace(old.find("4 0"),3,"3 0");old.erase(old.find("AUTHORED"));std::istringstream prior(old);check(Lighting::read(prior).authored.empty(),"v3 remains compatible");
 if(argc>1){std::ifstream in(argv[1]);auto actual=Lighting::read(in);unsigned two=0,four=0,active=0;for(const auto&x:actual.authored){two+=x.kind==2;four+=x.kind==4;active+=(x.identity.groupFlags&256)!=0;}
  check(two==4&&four==11&&active==12,"actual BB 4 type2 and 11 type4 records with original eligibility");
  bool lit=false;for(const auto&x:actual.authored){Vec3 p{},n{};for(int j=0;j<3;++j)p[j]=(x.minimum[j]+x.maximum[j])*.5f;for(int j=0;j<3;++j)n[j]=x.position[j]-p[j];auto c=Lighting::authored_sample(x,p,n);for(float v:c){check(std::isfinite(v)&&v>=0,"actual native lighting finite");lit|=v>0;}}
  check(lit,"actual BB typed contribution exists");
 }
 std::cout<<"BB type2/type4 raw fields, native sampling, original toggle predicates and v3 compatibility PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
