#include "combat_world.h"
#include <iostream>
#include <chrono>
#include <thread>
using namespace mgo2mt;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(int argc,char**argv){try{
 check(argc==2||argc==3,"native stage path required");auto root=std::filesystem::path(argv[1]);auto world=combat::World::load(root);auto registry=stage::load_object_registry(root/"n022a.objects.cfg");
 auto bindings=stage::read_object_bindings(root,false);for(auto&b:bindings)for(auto&p:b.parts)check(!p.model,"host collision never loads an original model or texture");
 stage::Assets client(argc==3?std::filesystem::path(argv[2]):root);host::LoadRequest request{1,7,0,0,{20,1,0},host::MatchTransition::initial};client.select(request);auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
 while(client.result().status==stage::Status::loading&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(5));check(client.result().status==stage::Status::preview_ready,"client scene loaded for independent comparison");
 stage::SceneSnapshot s{request,1,{}};for(auto&e:registry.entries)s.objects.push_back({e.bindingId,0,0});
 auto compare=[&]{check(world.apply(s)&&client.object_states(s),"same complete snapshot accepted by host and participant");auto c=client.result();check(world.collision()->vertices==c.collision->vertices&&world.collision()->triangles.size()==c.collision->triangles.size(),"solid geometry identical");check(world.targets()->vertices==c.objectHitCollision->vertices&&world.targets()->triangles.size()==c.objectHitCollision->triangles.size(),"GM_HIT target geometry identical");};
 compare();auto intact=world.collision()->triangles.size();s.objects[10].current=1;++s.revision;compare();check(world.collision()->triangles.size()+46==intact,"broken drum removes exactly original 46 navigation triangles");
 s.objects[17].current=3;++s.revision;compare();check(world.collision()->triangles.size()+66==intact,"final CBOX state removes its 20 triangles");
 auto previous=world.collision();auto bad=s;bad.objects.pop_back();check(!world.apply(bad)&&world.collision()==previous,"partial scene cannot replace host geometry");bad=s;bad.request.rotation.map=15;check(!world.apply(bad)&&world.collision()==previous,"other stage cannot borrow n022a geometry");
 std::cout<<"host collision-only scene matches client geometry for intact, broken drum and removed box; partial/wrong map rejected\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
