#include "stage_collision.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <sstream>
using namespace mgo2win::stage;
static void check(bool ok){if(!ok)throw std::runtime_error("Dynamic collision regression");}
int main(){try{
 std::string encoded="MGO2WIN.STAGE_COLLISION 2 3 1 1\n4768953 .5 .5 1\n0 0 0\n100 0 0\n0 0 100\n0 1 2 0 0 0\n";
 std::istringstream input(encoded);auto restoredMaterial=Collision::read(input);check(restoredMaterial.material(0).verified&&restoredMaterial.material(0).id==4768953);
 for(auto bad:{std::string("MGO2WIN.STAGE_COLLISION 2 0 0 1\n7 -1 .5 1"),std::string("MGO2WIN.STAGE_COLLISION 2 0 0 1\n7 .5 .5 2"),std::string("MGO2WIN.STAGE_COLLISION 2 0 0 65537"),encoded+"extra"}){bool failed=false;try{std::istringstream stream(bad);Collision::read(stream);}catch(...){failed=true;}check(failed);}
 auto floor=Collision::make({{-2000,0,-2000},{2000,0,-2000},{2000,0,2000},{-2000,0,2000}},{{{0,1,2},0,0,0},{{0,2,3},0,0,0}},{{77,.4f,.25f,true}});
 auto h=floor.sweep_segment({-100,400,0},{100,400,0},{0,-1000,0},50,1);check(h&&std::abs(h->fraction-.349f)<.001f&&h->normal[1]>.99f);
 auto contacts=floor.contacts({-100,40,0},{100,40,0},50);check(!contacts.empty()&&std::abs(contacts[0].penetration-10)<.01f);check(floor.material(contacts[0].triangle).id==77);
 check(floor.contacts({0,400,0},{100,400,0},50).empty());check(floor.contacts({0,0,0},{0,0,0},-1).empty());
 auto local=std::make_shared<Collision>(Collision::make({{-100,0,0},{100,0,0},{100,300,0},{-100,300,0}},{{{0,1,2}},{{0,2,3}}}));
 CollisionInstance instance{91,local,{500,0,0},{0,90,0}};auto world=Collision::combine(floor,{&instance,1});
 auto wall=world.ray({0,150,0},{1,0,0},1000);check(wall&&std::abs(wall->distance-500)<.01f&&world.triangles[wall->triangle].object==91);
 auto restored=Collision::combine(floor,{});check(!restored.ray({0,150,0},{1,0,0},1000));check(floor.triangles.size()==2&&world.triangles.size()==4);
 check(!world.material(wall->triangle).verified);bool rejected=false;try{CollisionInstance duplicate[]={instance,instance};Collision::combine(floor,duplicate);}catch(...){rejected=true;}check(rejected);
 rejected=false;try{Collision::make({{0,0,0}},{{{0,1,2}}});}catch(...){rejected=true;}check(rejected);
 std::cout<<"oriented capsule contacts/sweep, material identity, transformed object removal, immutable base: pass\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
