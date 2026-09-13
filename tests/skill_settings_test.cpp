#include "skill_settings.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2win::skills;
static void check(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
int main(){
 auto root=std::filesystem::temp_directory_path()/("mgo2win-skill-settings-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}}cleanup{root};
 try{
  std::filesystem::create_directories(root);auto file=root/"catalog.tsv";
  {std::ofstream f(file);f<<"MGO2WIN_SKILLS\t1\nSKILL\t0\t1\t1\tSynthetic A\t確認A\nSKILL\t0\t2\t2\tSynthetic A\t確認A\nSKILL\t0\t3\t3\tSynthetic A\t確認A\nSKILL\t1\t1\t4\tSynthetic B\t確認B\n";for(unsigned i=2;i<=10;++i)f<<"SKILL\t"<<i<<"\t1\t1\tSynthetic "<<i<<"\t確認"<<i<<'\n';}
  auto catalog=std::make_shared<Catalog>();std::string error;check(catalog->load(file,error),"bounded synthetic catalog loads");
  check(catalog->find(0,1)&&catalog->ids().size()==11&&catalog->levels(0).size()==3,"skill zero identity and level variants retained");
  check(bool(validate(*catalog,{})),"empty skill selection is permitted");
  Editor edit(catalog,{});check(bool(edit.set(0,3))&&bool(edit.set(2,1)),"three-cost skill and one-cost skill fit four");
  check(edit.status().used==4&&edit.draft().entries.size()==2,"capacity counts cost, not number of entries");auto previous=edit.draft();
  check(edit.set(3,1).result==Validation::over_budget&&edit.draft()==previous,"over-budget addition rolls back atomically");
  check(edit.set(0,99).result==Validation::unknown_level&&edit.draft()==previous,"unknown level cannot grant skills");
  check(edit.set(999,1).result==Validation::unknown_skill&&edit.draft()==previous,"unknown skill cannot grant skills");
  check(bool(edit.set(0,2))&&edit.status().used==3&&edit.draft().entries.size()==2,"changing level replaces its cost");
  check(bool(edit.set(3,1))&&edit.status().used==4,"freed capacity can be assigned");
  check(bool(edit.set(0,0))&&edit.status().used==2,"removing a skill frees its full cost");edit.reset();check(edit.draft().entries.empty()&&!edit.changed(),"cancel restores opening selection");
  check(validate(*catalog,{{{0,1},{0,2}}}).result==Validation::duplicate,"same skill at two levels is rejected");
  for(unsigned cap:{0u,3u,9u,255u})check(validate(*catalog,{},cap).result==Validation::invalid_capacity,"capacity only accepts reviewed 4 to 8 boundary");
  Loadout eight;for(unsigned i=2;i<10;++i)eight.entries.push_back({uint16_t(i),1});
  check(validate(*catalog,eight).result==Validation::over_budget&&bool(validate(*catalog,eight,8)),"future eight-cost entitlement is explicit, default remains four");
  auto nine=eight;nine.entries.push_back({10,1});check(validate(*catalog,nine,8).result==Validation::too_many,"more than eight entries rejected before accumulation");
  auto records=root/"local";check(save(records,77,*catalog,eight,8,error),"eight-capacity preference saves with authorized caller");
  auto loaded=load(records,77,*catalog,8,error);check(loaded&&*loaded==eight&&error.empty(),"saved eight-capacity choice roundtrips");
  check(!load(records,77,*catalog,4,error)&&!error.empty(),"stored choice cannot grant additional capacity on next run");
  std::ifstream f(records/"77.gsk",std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(f)),{});f.close();
  check(bytes.find("CAPACITY")==bytes.npos&&bytes.find("ENTITLEMENT")==bytes.npos&&bytes.find("77")!=bytes.npos,"preference persists no purchase or capacity grant");
  check(!save(records,77,*catalog,eight,4,error)&&load(records,77,*catalog,8,error)==loaded,"rejected save preserves existing record");
  std::filesystem::copy_file(records/"77.gsk",records/"78.gsk");check(!load(records,78,*catalog,8,error)&&!error.empty(),"other character record cannot be imported by filename");
  {std::ofstream out(records/"77.gsk",std::ios::binary);auto corrupted=bytes;corrupted[corrupted.find("SKILL\t2")+6]='3';out<<corrupted;}
  check(!load(records,77,*catalog,8,error)&&!error.empty(),"accidental content corruption rejected");
  check(!load(records,99,*catalog,4,error)&&error.empty(),"missing record has distinct empty status");
  check(!save(records,0,*catalog,{},4,error),"zero or absent character identity cannot persist state");
  {std::ofstream out(file);out<<"MGO2WIN_SKILLS\t1\nSKILL\t1\t1\t1\tA\tA\nSKILL\t1\t1\t1\tA\tA\n";}
  check(!catalog->load(file,error)&&catalog->entries().empty(),"duplicate catalog fails closed without stale rows");
  std::cout<<"Skill cost budget, replacement rollback, future entitlement and bounded per-character persistence passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
