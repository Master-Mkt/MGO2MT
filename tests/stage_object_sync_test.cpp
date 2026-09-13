#include "stage_object_sync.h"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <chrono>
using namespace mgo2win;
static void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);}
template<class F>static bool invalid(F fn){try{fn();return false;}catch(const host::Invalid&){return true;}}
int main(){try{
 using Update=host::ObjectStates::Update;
 struct Temp {std::filesystem::path path=std::filesystem::temp_directory_path()/("mgo2win-object-registry-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".cfg");~Temp(){std::error_code error;std::filesystem::remove(path,error);}} temp;
 auto write=[&](const std::string&s){std::ofstream f(temp.path);f<<s;check(bool(f),"temporary registry written");};
 std::ostringstream source;source<<"MGO2WIN.STAGE_OBJECTS 1 n022a_success32 20 1 32\n";
 constexpr uint32_t ids[]={5565312,5565360,5565408,5565472,5565536,5565584,5565648,5565696,5565744,5565776,5565120,5565152,5566224,5566528,5566880,5567184,5567568};
 for(unsigned i=0;i<32;++i)source<<i<<' '<<(i<17?ids[i]:0xc0000000u+i-17)<<' '<<(i<12?1:i<17?8:2)<<' '<<(i<17?"bits":"maximum")<<'\n';
 const auto valid=source.str();write(valid);auto reviewed=stage::load_object_registry(temp.path);
 check(reviewed.complete&&reviewed.entries.size()==32&&reviewed.rule==1&&reviewed.map==20,"exact reviewed successful profile accepted");
 for(const auto&[oldText,newText]:{std::pair{"20 1 32","20 2 32"},std::pair{"n022a_success32","guessed_profile"},std::pair{"0 5565312 1 bits","0 5565313 1 bits"},std::pair{"0 5565312 1 bits","0 5565312 2 bits"},std::pair{"0 5565312 1 bits","0 5565312 1 maximum"}}){auto modified=valid;modified.replace(modified.find(oldText),std::string(oldText).size(),newText);write(modified);check(invalid([&]{stage::load_object_registry(temp.path);}),"changed scope/order/identity/width/update policy cannot authorize registry");}
 write(valid+"unexpected\n");check(invalid([&]{stage::load_object_registry(temp.path);}),"trailing schema records rejected");
 write(valid.substr(0,valid.rfind('\n',valid.size()-2)));check(invalid([&]{stage::load_object_registry(temp.path);}),"truncated schema rejected");
 stage::SceneAuthority scoped(reviewed);host::LoadRequest scope;scope.sequence=1;scope.rotation={20,1,0};scoped.begin(scope);
 check(scoped.snapshot(3)&&scoped.snapshot(3)->size()==13,"full known 82-bit registry uses 13-byte E0");
 scope.sequence=2;scope.rotation.rule=2;scoped.begin(scope);check(!scoped.snapshot(3),"unreviewed rule cannot publish n022a state");
 // Offline reviewed-schema fixture; not a claim about live n022a indexes.
 stage::ObjectRegistry registry{20,{{91,1,Update::bits},{92,8,Update::bits},{93,2,Update::maximum}},true};
 host::LoadRequest request;request.sequence=1;request.generation=7;request.rotation.map=20;
 auto partial=registry;partial.complete=false;stage::SceneReceiver incomplete(partial,3);incomplete.begin(request);
 check(incomplete.status()==stage::SceneSyncStatus::unverified_registry&&!incomplete.snapshot_request()&&!incomplete.snapshot(),"candidate registry cannot request or publish live state");
 check(!incomplete.receive(request,std::array<uint8_t,4>{0xe0,3,0,0}),"candidate cannot decode guessed snapshot indexes");
 stage::SceneReceiver first(registry,3),late(registry,3);first.begin(request);late.begin(request);
 check(first.status()==stage::SceneSyncStatus::waiting_snapshot&&first.snapshot_request()==std::optional{std::array<uint8_t,2>{0xe1,3}},"reviewed registry awaits addressed full snapshot");
 check(!first.receive(request,std::array<uint8_t,1>{0})&&!first.snapshot(),"pre-snapshot delta remains unpublished");
 first.receive(request,std::array<uint8_t,2>{1,128});first.receive(request,std::array<uint8_t,2>{2,2});
 check(!first.receive(request,std::array<uint8_t,4>{0xe0,4,0,0})&&!first.snapshot(),"another slot cannot establish readiness");
 check(invalid([&]{first.receive(request,std::array<uint8_t,3>{0xe0,3,0});})&&!first.snapshot(),"truncated snapshot cannot publish partial scene");
 // E0 contains 0/1/1. Earlier-arriving updates combine into 1/129/2.
 check(first.receive(request,std::array<uint8_t,4>{0xe0,3,2,2}),"whole snapshot published");
 late.receive(request,std::array<uint8_t,4>{0xe0,3,3,5});
 check(first.snapshot()->objects==late.snapshot()->objects,"late join and reordered snapshot converge quietly");
 check(first.snapshot()->objects==std::vector<stage::SceneObjectState>({{91,1,1},{92,129,129},{93,2,2}}),"full registry bindings and quiet baseline");
 check(first.status()==stage::SceneSyncStatus::ready&&!first.snapshot_request(),"object completion stops E1 requests only after complete scene");
 const auto revision=first.snapshot()->revision;
 check(!first.receive(request,std::array<uint8_t,2>{2,1})&&first.snapshot()->revision==revision,"stale max update and duplicate do not republish");
 first.receive(request,std::array<uint8_t,2>{1,4});
 check(first.snapshot()->objects[1]==stage::SceneObjectState{92,133,129}&&first.snapshot()->revision>revision,"post-initial change retains restoration baseline");
 check(!first.receive(request,std::array<uint8_t,4>{0xe0,3,0,0})&&first.snapshot()->objects[1].current==133,"duplicate snapshot cannot resurrect components");
 const auto current=*first.snapshot();
 check(invalid([&]{first.receive(request,std::array<uint8_t,1>{1});})&&*first.snapshot()==current,"malformed multibit delta preserves whole published scene");
 auto next=request;next.sequence=2;next.generation=8;first.begin(next);
 check(!first.snapshot()&&first.status()==stage::SceneSyncStatus::waiting_snapshot,"new generation discards prior scene while missing snapshot");
 check(!first.receive(request,std::array<uint8_t,4>{0xe0,3,3,5})&&!first.snapshot(),"old request cannot complete new generation");
 first.receive(next,std::array<uint8_t,4>{0xe0,3,0,0});check(first.snapshot()->objects[0].current==0,"new generation starts from new host baseline");
 auto wrongMap=next;wrongMap.sequence=3;wrongMap.rotation.map=12;first.begin(wrongMap);
 check(!first.snapshot()&&!first.snapshot_request()&&first.status()==stage::SceneSyncStatus::unverified_registry,"map mismatch discards prior registry use");
 first.begin(std::nullopt);check(first.status()==stage::SceneSyncStatus::idle&&!first.snapshot(),"exit clears all replicated scene state");
 auto bad=registry;bad.entries[1].bindingId=91;check(invalid([&]{stage::SceneReceiver receiver(bad,3);}),"duplicate scene binding rejected");
 bad=registry;bad.entries[0].width=0;check(invalid([&]{stage::SceneReceiver receiver(bad,3);}),"unknown bit width rejected");
 std::cout<<"scene snapshot completeness, pending deltas, generation and atomic publication passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
