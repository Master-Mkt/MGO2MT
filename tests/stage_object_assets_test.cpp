#include "stage_assets.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

using namespace mgo2win;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
stage::Result loaded(stage::Assets& assets){
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
 auto result=assets.result();
 while(result.status==stage::Status::loading&&std::chrono::steady_clock::now()<deadline){
  std::this_thread::sleep_for(std::chrono::milliseconds(2));result=assets.result();
 }
 check(result.status==stage::Status::preview_ready,"actual n022a assets must finish loading");return result;
}
size_t triangles(const std::shared_ptr<const stage::Collision>& collision){return collision?collision->triangles.size():0;}
using Counts=std::map<uint32_t,size_t>;
Counts counts(const std::shared_ptr<const stage::Collision>& collision,size_t authored=0){
 Counts result;if(!collision)return result;check(authored<=collision->triangles.size(),"authored collision prefix remains present");
 for(size_t i=authored;i<collision->triangles.size();++i)++result[collision->triangles[i].object];return result;
}
size_t count(const Counts& counts,uint32_t component){auto found=counts.find(component);return found==counts.end()?0:found->second;}
size_t count(const Counts& counts,const stage::ObjectBinding& binding,bool hitOnly){
 size_t result=0;for(const auto& part:binding.parts)if(part.collision&&part.hitOnly==hitOnly)result+=count(counts,part.componentId);return result;
}
size_t indices(const std::shared_ptr<const CharacterModel>& model){size_t result=0;if(model)for(const auto& part:model->parts)result+=part.count;return result;}
size_t visible_indices(const stage::ObjectBinding& binding,uint8_t state){size_t result=0;for(const auto& part:binding.parts)if((state&part.mask)==part.value)result+=indices(part.model);return result;}

// Only files created by this fixture are removed. Never modify source assets.
struct TemporaryBindings {
 std::filesystem::path root=std::filesystem::temp_directory_path()/("mgo2win-object-assets-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 TemporaryBindings(){check(std::filesystem::create_directory(root),"temporary directory created");std::filesystem::create_directory(root/"objects");}
 ~TemporaryBindings(){std::error_code error;for(const auto& p:{root/"objects/model.gwm",root/"escape.gwm",root/"n022a.objects.cfg",root/"n022a.bindings.cfg",root/"objects",root})std::filesystem::remove(p,error);}
 void write(const std::string& value){std::ofstream out(root/"n022a.bindings.cfg");out<<value;check(bool(out),"temporary binding written");}
};
void invalid_bindings(const std::filesystem::path& source,const stage::ObjectRegistry& registry){
 TemporaryBindings temp;std::filesystem::copy_file(source/"n022a.objects.cfg",temp.root/"n022a.objects.cfg");
 const auto model=source/"objects/s01a_car_a0_sk.gwm";
 std::filesystem::copy_file(model,temp.root/"objects/model.gwm");std::filesystem::copy_file(model,temp.root/"escape.gwm");
 auto fixture=[&](const char* path,uint64_t partMask){
  std::ostringstream text;text<<"MGO2WIN.STAGE_BINDINGS 1 n022a_success32 32\n";
  for(size_t i=0;i<registry.entries.size();++i){const auto& entry=registry.entries[i];
   text<<entry.bindingId<<' '<<unsigned(entry.width)<<' '<<(i>=17?int(i-17):-1)<<" 0 0 0 0 0 0 "<<(i==0?1:0)<<" 0\n";
   if(i==0)text<<"1 0 0 "<<path<<' '<<partMask<<" - 0 0 0 0 0 0 0 0\n";
  }return text.str();
 };
 auto valid=fixture("objects/model.gwm",0);temp.write(valid);
 auto accepted=stage::read_object_bindings(temp.root);check(accepted.size()==32&&accepted[0].parts.size()==1,"malformed-file fixture has a valid complete control");
 check(accepted[0].parts[0].model->parts.size()<64,"test uses a real model with bounded original parts");
 auto rejects=[&](const std::string& text,const char* why){temp.write(text);bool rejected=false;try{stage::read_object_bindings(temp.root);}catch(const std::exception&){rejected=true;}check(rejected,why);};
 rejects(fixture("objects/model.gwm",uint64_t(1)<<63),"part mask outside real model parts rejected");
 // escape.gwm exists: failure must not rely on a missing traversal target.
 rejects(fixture("objects/../escape.gwm",0),"path traversal to an existing file rejected");
 rejects(valid.substr(0,valid.size()/2),"truncated 32-binding registry rejected");
 rejects(fixture("objects/missing.gwm",0),"missing component asset rejected");
 temp.write(valid);check(stage::read_object_bindings(temp.root).size()==32,"invalid reads cannot poison the next valid load");
}
}

int main(int argc,char** argv){try{
 check(argc==2,"usage: stage_object_assets_test <native-stage-directory>");
 const std::filesystem::path root=argv[1];const auto registry=stage::load_object_registry(root/"n022a.objects.cfg");
 check(registry.complete&&registry.map==20&&registry.rule==1&&registry.entries.size()==32,"actual reviewed n022a/TDM profile");
 invalid_bindings(root,registry);
 stage::Assets assets(root);host::LoadRequest request{1,7,0,0,{20,1,0},host::MatchTransition::initial};assets.select(request);auto base=loaded(assets);
 check(base.objectBindings&&base.objectBindings->size()==32&&base.authoredCollision&&base.cboxes.size()==15,"all actual bindings, authored geometry and selected CBOX layout load together");
 check(!base.objectSnapshot&&!base.objectModel,"asset loading alone does not establish initial object state");
 const auto bindings=base.objectBindings;const size_t authored=base.authoredCollision->triangles.size();
 check(authored==145875,"reviewed original background collider count");
 stage::SceneSnapshot snapshot{request,1,{}};for(const auto& entry:registry.entries)snapshot.objects.push_back({entry.bindingId,0,0});
 auto apply=[&](){check(assets.object_states(snapshot),"complete actual object snapshot applies");return assets.result();};
 auto current=apply();const size_t intactNavigation=triangles(current.collision),intactHits=triangles(current.objectHitCollision);
 check(intactHits==(32+12)*12,"32 bottle boxes plus twelve drum boxes are hit-only");
 auto navigation=counts(current.collision,authored),hits=counts(current.objectHitCollision);
 size_t carTriangles=0;
 for(size_t i=0;i<10;++i){auto n=count(navigation,bindings->at(i),false);check(n>0,"each car keeps a recovered GEOM obstacle");carTriangles+=n;}
 check(intactNavigation==authored+carTriangles+2*46+15*20,"only verified GEOM components join navigation");
 for(size_t i=10;i<12;++i){check(count(navigation,bindings->at(i),false)==46,"each intact drum has 46 navigation triangles");check(count(hits,bindings->at(i),true)==72,"each intact drum has six 12-triangle hit boxes");}
 for(size_t i=12;i<17;++i){check(count(navigation,bindings->at(i),false)==0,"bottle hit boxes are excluded from navigation");check(count(hits,bindings->at(i),true)==(i<15?6u:7u)*12,"each bottle group retains one box per original child");}
 for(size_t i=17;i<32;++i)check(count(navigation,bindings->at(i),false)==20,"each selected intact CBOX has 20 GEOM triangles");

 const auto oldModel=current.objectModel;const auto oldCollision=current.collision,oldHits=current.objectHitCollision;
 check(assets.object_states(snapshot),"same revision is idempotent");current=assets.result();
 check(current.objectModel==oldModel&&current.collision==oldCollision&&current.objectHitCollision==oldHits,"same snapshot performs no scene rebuild");
 auto conflict=snapshot;conflict.objects[0].current=1;
 check(!assets.object_states(conflict)&&assets.result().objectModel==oldModel,"different contents at the same revision rejected");

 const auto initialRenderIndices=indices(current.objectModel);int64_t carIndexDelta=0;
 for(size_t i=0;i<10;++i){snapshot.objects[i].current=1;carIndexDelta+=int64_t(visible_indices(bindings->at(i),1))-int64_t(visible_indices(bindings->at(i),0));}
 ++snapshot.revision;current=apply();navigation=counts(current.collision,authored);
 check(triangles(current.collision)==intactNavigation&&triangles(current.objectHitCollision)==intactHits,"destroyed cars retain stable GEOM and unrelated hit boxes");
 check(int64_t(indices(current.objectModel))==int64_t(initialRenderIndices)+carIndexDelta,"car glass part selection changes without deleting full bodies");
 for(size_t i=0;i<10;++i)check(count(navigation,bindings->at(i),false)>0,"all ten destroyed car colliders remain");

 const auto beforeDrumIndices=indices(current.objectModel);snapshot.objects[10].current=1;++snapshot.revision;current=apply();navigation=counts(current.collision,authored);hits=counts(current.objectHitCollision);
 check(triangles(current.collision)==intactNavigation-46&&count(navigation,bindings->at(10),false)==0&&count(navigation,bindings->at(11),false)==46,"one drum destruction removes exactly its GEOM");
 check(triangles(current.objectHitCollision)==intactHits-72&&count(hits,bindings->at(10),true)==0&&count(hits,bindings->at(11),true)==72,"one drum destruction removes only its six hit boxes");
 check(indices(current.objectModel)==beforeDrumIndices-visible_indices(bindings->at(10),0),"drum model disappears with collision in the same revision");

 const auto beforeBottleNavigation=navigation,beforeBottleHits=hits;const auto beforeBottleIndices=indices(current.objectModel);
 snapshot.objects[12].current=1;++snapshot.revision;current=apply();navigation=counts(current.collision,authored);hits=counts(current.objectHitCollision);
 check(navigation==beforeBottleNavigation&&triangles(current.collision)==intactNavigation-46,"bottle destruction leaves walking colliders unchanged");
 check(triangles(current.objectHitCollision)==intactHits-72-12&&count(hits,bindings->at(12),true)==5*12,"one bottle bit removes exactly one hit box");
 size_t removed=0;for(const auto& part:bindings->at(12).parts)if(part.hitOnly&&part.collision){if((part.mask&1)!=0){check(count(hits,part.componentId)==0,"selected bottle child removed");++removed;}else check(count(hits,part.componentId)==count(beforeBottleHits,part.componentId),"other bottle children remain");}
 check(removed==1,"bottle bit identifies one component");
 check(indices(current.objectModel)==beforeBottleIndices-visible_indices(bindings->at(12),0)+visible_indices(bindings->at(12),1),"bottle visual and hit state change together");

 const size_t beforeCboxNavigation=triangles(current.collision),beforeCboxHits=triangles(current.objectHitCollision);
 for(uint8_t damage:{uint8_t(1),uint8_t(2),uint8_t(3)}){
  const auto previous=snapshot.objects[17].current;const auto previousIndices=indices(current.objectModel);
  snapshot.objects[17].current=damage;++snapshot.revision;current=apply();navigation=counts(current.collision,authored);
  check(count(navigation,bindings->at(17),false)==(damage<3?20u:0u),"CBOX retains collider through stages 1/2 and removes it at 3");
  check(triangles(current.collision)==beforeCboxNavigation-(damage==3?20:0)&&triangles(current.objectHitCollision)==beforeCboxHits,"CBOX state changes no unrelated navigation or hit geometry");
  check(indices(current.objectModel)==previousIndices-visible_indices(bindings->at(17),previous)+visible_indices(bindings->at(17),damage),"CBOX damage model partitions match the same revision");
 }
 for(size_t i=18;i<32;++i)check(count(navigation,bindings->at(i),false)==20,"other fourteen selected boxes remain");

 const auto oldSnapshot=snapshot;request.sequence=2;request.generation=8;request.round=1;request.transition=host::MatchTransition::next_round;assets.select(request);current=loaded(assets);
 check(!current.objectSnapshot&&!current.objectModel&&!current.objectHitCollision&&triangles(current.collision)==authored,"new generation clears all old replicated components while awaiting E0");
 check(!assets.object_states(oldSnapshot)&&!assets.result().objectSnapshot,"old-generation snapshot rejected after stage reload");
 snapshot.request=request;snapshot.revision=oldSnapshot.revision+1;for(auto& state:snapshot.objects)state.current=state.initial=0;current=apply();
 check(triangles(current.collision)==intactNavigation&&triangles(current.objectHitCollision)==intactHits,"new round restores all32 entries using new selected box placements");
 std::cout<<"actual n022a 32-object state/model/GEOM/GM_HIT integration and malformed binding rejection passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
