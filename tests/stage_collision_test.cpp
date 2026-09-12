#include "stage_collision.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2win::stage;
static void check(bool v){if(!v)throw std::runtime_error("Collision query regression");}
int main(int argc,char**argv){try{
 std::istringstream in("MGO2WIN.STAGE_COLLISION 1 3 1\n-2 0 -2\n2 0 -2\n0 0 2\n0 1 2 8192 7\n");auto c=Collision::read(in);
 auto h=c.ray({0,10,0},{0,-2,0},20);check(h&&h->distance==10&&h->position[1]==0);check(!c.ray({4,10,0},{0,-1,0},20));check(!c.ray({0,10,0},{0,-1,0},9));check(!c.ray({0,10,0},{0,0,0},20));check(c.triangles[0].attribute==8192);
 for(auto s:{"MGO2WIN.STAGE_COLLISION 1 500001 0","MGO2WIN.STAGE_COLLISION 1 0 1 0 0 0 0 0","MGO2WIN.STAGE_COLLISION 1 0 0 trailing"}){bool rejected=false;try{std::istringstream bad(s);Collision::read(bad);}catch(...){rejected=true;}check(rejected);}
 if(argc>1){std::ifstream f(argv[1]);auto real=Collision::read(f);check(real.triangles.size()>100000);auto floor=real.ray({-42878.8359f,3000,29781.7969f},{0,-1,0},10000);check(bool(floor));std::cout<<"world triangles="<<real.triangles.size()<<" base-home ground y="<<floor->position[1]<<'\n';}
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
