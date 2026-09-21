#include "stage_floor_blend.h"
#include "stage_normals.h"
#include "stage_lighting.h"
#include "stage_collision.h"
#include "combat_kill_feed.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <cstring>
#include <random>
using namespace mgo2mt;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
std::vector<char> read(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);check(bool(f),"read fixture");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){try{
 check(argc==3,"base data and blend asset required");auto root=std::filesystem::path(argv[1]);auto b=read(root/"stage/n022a.gwm"),side=read(argv[2]);CharacterModel m(b);auto original=m.vertices;
 check(stage::apply_floor_blend(m,b,side)==82,"82 original parts restored");check(m.parts[403].floorBlend==m.parts[406].floorBlend,"shared original blend layer");
 check(m.vertices[62413].u2!=m.vertices[62413].u||m.vertices[62413].v2!=m.vertices[62413].v,"original third UV retained");
 for(size_t i=0;i<m.vertices.size();++i){auto v=m.vertices[i];v.u1=original[i].u1;v.v1=original[i].v1;v.u2=v.v2=0;check(!std::memcmp(&v,&original[i],sizeof(v)),"geometry/authored color remains unchanged");}
 for(unsigned k=0;k<5;++k){auto bad=side;if(k==0)bad.pop_back();if(k==1)bad[20]^=1;if(k==2)bad[52]=char(255),bad[53]=char(255),bad[54]=char(255),bad[55]=char(127);if(k==3)bad[56]=0,bad[57]=0,bad[58]=char(128),bad[59]=char(127);if(k==4)bad.push_back(0);CharacterModel x(b);bool rejected=false;try{stage::apply_floor_blend(x,b,bad);}catch(...){rejected=true;}check(rejected&&x.textures.size()==m.textures.size()-43,"corrupt blend rejected before mutation");}
 stage::load_original_normals(m,b,root/"stage/n022a.gwn");std::ifstream lf(root/"stage/n022a.lighting.cfg");auto light=stage::Lighting::read(lf);auto expected=m.vertices;
 const auto start=std::chrono::steady_clock::now();for(auto&v:expected){auto c=light.sample({v.x,v.y,v.z},{v.nx,v.ny,v.nz}).color;v.lr=c[0];v.lg=c[1];v.lb=c[2];v.lit=1;}auto middle=std::chrono::steady_clock::now();auto evaluations=light.sample_vertices(m.vertices);auto end=std::chrono::steady_clock::now();
 check(!std::memcmp(expected.data(),m.vertices.data(),expected.size()*sizeof(ModelVertex)),"block lighting bit-for-bit identical");std::cout<<"lighting vertices="<<m.vertices.size()<<" evaluated="<<evaluations<<" reference_ms="<<std::chrono::duration<double,std::milli>(middle-start).count()<<" optimized_ms="<<std::chrono::duration<double,std::milli>(end-middle).count()<<'\n';
 std::vector<stage::Vec3>vertices;std::vector<stage::CollisionTriangle>triangles;std::vector<stage::Collision> individual;
 std::mt19937 random(19371);std::uniform_real_distribution<float>value(-1000,1000);
 for(unsigned i=0;i<300;++i){stage::Vec3 p{value(random),value(random),value(random)},q{p[0]+150,p[1]+20,p[2]-90},r{p[0]-40,p[1]+140,p[2]+110};auto base=unsigned(vertices.size());vertices.insert(vertices.end(),{p,q,r});triangles.push_back({{base,base+1,base+2}});individual.push_back(stage::Collision::make({p,q,r},{{{0,1,2}}}));}
 auto world=stage::Collision::make(vertices,triangles);
 for(unsigned i=0;i<1000;++i){stage::Vec3 o{value(random),value(random),value(random)},d{value(random),value(random),value(random)};auto all=world.ray_all(o,d,4000);std::vector<std::pair<float,size_t>>reference;for(size_t j=0;j<individual.size();++j)if(auto hit=individual[j].ray(o,d,4000))reference.emplace_back(hit->distance,j);std::sort(reference.begin(),reference.end());check(all.size()==reference.size(),"BVH exact ray hit count matches exhaustive triangles");for(size_t j=0;j<all.size();++j)check(all[j].triangle==reference[j].second&&std::abs(all[j].distance-reference[j].first)<.01f,"BVH hit order/distance");auto nearest=world.ray(o,d,4000);check(bool(nearest)==!all.empty(),"nearest/all consistency");if(nearest)check(nearest->triangle==all.front().triangle,"nearest triangle unchanged");}
 combat::KillFeed feed;feed.scope(9);host::Roster roster;roster.slots[1]=host::Player{1,2,10,"attacker"};roster.slots[2]=host::Player{2,3,20,"victim"};feed.roster(roster);combat::Event death;death.epoch=9;death.id=1;death.kind=combat::EventKind::death;death.source={1,2,10};death.target={2,3,20};death.sourceLife=death.targetLife=1;death.weapon=25;
 check(feed.consume(std::span(&death,1),100).size()==1,"kill history once");check(feed.consume(std::span(&death,1),100).empty(),"no replay");death.id=2;check(feed.consume(std::span(&death,1),100).empty(),"no duplicate victim life");death.id=3;death.targetLife=2;check(feed.consume(std::span(&death,1),200).size()==1,"respawn can die again");roster.slots[2]=host::Player{2,4,30,"new occupant"};feed.roster(roster);check(feed.name(death.target)=="victim","slot reuse cannot rename history");feed.expire(8200);check(feed.entries.empty(),"feed expiry");feed.scope(10);check(feed.consume(std::span(&death,1),9000).empty(),"old round rejected");
 combat::Snapshot snapshot;snapshot.epoch=9;snapshot.revision=1;combat::Replica ordinary,history;check(ordinary.snapshot(snapshot)&&history.snapshot(snapshot),"replica init");death.id=1;check(ordinary.events(std::span(&death,1)).empty(),"old life effect filtered");check(history.events(std::span(&death,1),true).size()==1,"historical death retained independently of current actor");check(history.events(std::span(&death,1),true).empty(),"historical death cursor deduplicated");
 std::cout<<"floor blend / lighting / 1000 exhaustive ray comparisons / kill history PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
