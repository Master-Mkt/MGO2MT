#include "gameplay_config.h"
#include "combat_initial_profile.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
namespace {
void check(bool v,const std::string&m){if(!v)throw std::runtime_error(m);}
std::string read(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);std::string s{std::istreambuf_iterator<char>(f),{}};std::erase(s,'\r');return s;}
std::string replace(std::string s,const std::string&a,const std::string&b){auto i=s.find(a);check(i!=s.npos,"mutation source missing: "+a);s.replace(i,a.size(),b);return s;}
void equivalent(const combat::Weapon&a,const combat::Weapon&b){
#define SAME(x) check(a.x==b.x,"migrated field differs: "+std::to_string(a.id)+"." #x)
 SAME(id);SAME(damage);SAME(staminaDamage);SAME(intervalMs);SAME(reloadMs);SAME(magazine);SAME(reserve);SAME(range);SAME(shotCue);SAME(impactCue);SAME(automatic);SAME(bodyCue);SAME(reloadRefillMs);SAME(fireIntervalTicks);SAME(nativePrimaryMastery);SAME(materialMap);SAME(nativeAkPenetration);SAME(nativeAkHitRegions);SAME(heldOnly);SAME(nativeAkAccuracy);SAME(nativeProjectile);SAME(originalFirearm);SAME(meleeAttack);SAME(nativePlaced);SAME(behaviorSourceId);SAME(mountedOnly);
 check(a.reloadMotion.has_value()==b.reloadMotion.has_value(),"reload motion presence");if(a.reloadMotion){check(a.reloadMotion->baseTick==b.reloadMotion->baseTick&&a.reloadMotion->intervals==b.reloadMotion->intervals&&a.reloadMotion->refillTick==b.reloadMotion->refillTick&&a.reloadMotion->rate==b.reloadMotion->rate,"exact reload motion");}
#undef SAME
}
}
int main(int argc,char**argv){try{
 check(argc==3,"usage: gameplay_config_test gameplay.json scratchDirectory");const auto path=std::filesystem::path(argv[1]);const auto text=read(path);gameplay::Config config;std::string error;check(config.load(path,error),error);
 auto old=combat::initial_profiles(20,1,0),next=config.profiles(20);check(next.size()>=old.size(),"all legacy definitions retained");
 for(size_t i=0;i<old.size();++i){equivalent(old[i],next[i]);auto d=config.find(old[i].id);check(d&&d->visual.id==old[i].id,"visual identity");if(const auto*p=original_weapon::find(old[i].id);p&&p->bullet.range){const auto&b=d->weapon.ballistics;check(bool(b),"explicit original ballistics");check(b->range==p->bullet.range&&b->speed==p->bullet.speed&&b->decayStart==p->bullet.decayStart&&b->decayEnd==p->bullet.decayEnd&&b->minimumForce==p->bullet.minimumForce&&b->penetration==p->bullet.penetration,"exact original ballistics");}if(old[i].id<128)check(!d->visual.modelPath.empty()&&d->visual.motionId==old[i].id,"ordinary original model/motion");}
 check(config.profiles(7)[0].materialMap==7&&config.profiles(20)[0].materialMap==20,"host-selected map overlays config");
 auto rejection=[&](const std::string&s,const char*why){const auto before=config.profiles(20);check(!config.load_text(s,error)&&!error.empty(),why);check(config.profiles(20).size()==before.size()&&config.find(25)->weapon.damage==275&&config.visual(25)->modelPath=="weapons/id_025.gwm","failed reload atomic");};
 rejection(replace(text,"\"version\": 1","\"version\": 2"),"unknown version");
 rejection(replace(text,"\"version\": 1","\"version\": 1, \"version\": 1"),"duplicate key");
 rejection(replace(text,"\"id\": 3,\n      \"name\"","\"id\": 25,\n      \"name\""),"duplicate weapon ID");
 rejection(replace(text,"\"damage\": 275","\"damag\": 275"),"unknown/misspelled key");
 rejection(replace(text,"\"damage\": 275","\"damage\": -1"),"negative damage");
 rejection(replace(text,"\"damage\": 275","\"damage\": 1.25"),"fractional integer");
 rejection(replace(text,"\"damage\": 275","\"damage\": 1e999"),"nonfinite number");
 rejection(replace(text,"\"damage\": 275","\"damage\": true"),"boolean not integer");
 rejection(replace(text,"\"reloadRefillMs\": 0","\"reloadRefillMs\": 60000"),"conflicting reload timing");
 rejection(replace(text,"\"fireIntervalTicks\": 30","\"fireIntervalTicks\": 0"),"invalid fire ticks");
 rejection(replace(text,"\"speed\": 473750.0","\"speed\": 0"),"invalid ballistic speed");
 rejection(replace(text,"\"handsPath\": \"weapons/hands.gwh\"","\"handsPath\": \"../hands.gwh\""),"resource traversal");
 rejection(replace(text,"\"category\": \"PRIMARY\"","\"category\": \"RIFLE\""),"unknown category");
 rejection(replace(text,"\"id\": 23","\"id\": 499"),"catalog missing profile");
 rejection(replace(text,"\"motionId\": 25","\"motionId\": 0"),"missing explicit motion source");
 rejection(replace(text,"\"mountedOnly\": false","\"mountedOnly\": true"),"mounted-only weapon excluded from carry catalog");
 rejection(replace(text,"\"behaviorSourceId\": 0","\"behaviorSourceId\": 500"),"unknown action source alias");
 rejection(text+"{}","trailing JSON");rejection(std::string(1024*1024+1,' '),"size bound");rejection(std::string("{\"x\":\"")+char(-1)+"\"}","invalid UTF8");
 for(const auto*bad:{"../x","a/../x","/x","C:/x","a\\x","a//x","a/./x","a/","nul.txt","CON .gwm","dir/COM1.gwm","dir/LPT9.foo","dir/COM\xc2\xb9.gwm","a:gwm","a. ","a/file.","a/?"})check(!gameplay::relative_resource_path(bad),"unsafe resource accepted");
 check(gameplay::relative_resource_path("weapons/custom/m2.gwm"),"nested safe resource");
 gameplay::Config edited;auto modified=replace(text,"\"damage\": 275","\"damage\": 550");check(edited.load_text(modified,error),error);check(edited.find(25)->weapon.damage==550&&config.find(25)->weapon.damage==275,"JSON damage is independently configurable");
 modified=replace(text,"\"modelPath\": \"weapons/id_025.gwm\"","\"modelPath\": \"weapons/custom/AK.gwm\"");check(edited.load_text(modified,error)&&edited.visual(25)->modelPath=="weapons/custom/AK.gwm","resource path configurable");
 modified=replace(text,"\"modelPath\": \"weapons/id_025.gwm\"","\"modelPath\": \"weapons/id_025.gwm\", \"iconPath\": \"weapon_editor_assets/icons/custom.PNG\"");
 check(edited.load_text(modified,error)&&edited.visual(25)->iconPath=="weapon_editor_assets/icons/custom.PNG","optional JSON PNG icon path");
 rejection(replace(modified,"weapon_editor_assets/icons/custom.PNG","../outside.png"),"icon path cannot escape data");
 rejection(replace(modified,"weapon_editor_assets/icons/custom.PNG","icons/unsupported.dds"),"icon path requires PNG");
 auto legacy=text;if(auto where=legacy.find("MGO2MT.Gameplay");where!=legacy.npos)legacy.replace(where,15,"MGO2WIN.Gameplay");
 check(edited.load_text(legacy,error),"legacy product schema remains readable");
 modified=replace(text,"\"effectId\": 25","\"effectId\": 25, \"flashTexture\": 592620, \"smokeTexture\": 13275831");check(edited.load_text(modified,error)&&edited.visual(25)->flashTexture==592620&&edited.visual(25)->smokeTexture==13275831,"explicit original effect texture keys");
 // Optional body-image path and per-explosive native blast policy are strict,
 // atomic and compatible with existing files that omit these fields.
 const std::string blast=R"({"enabled":true,"horizontalSpeed":3200,"upwardSpeed":2400,"minimumScale":0.4,"gravity":9800,"maxFlightMs":4000})";
 auto withBlast=[&](uint16_t id,const std::string& value){auto result=text;auto row=result.find("\"id\": "+std::to_string(id),result.find("\"weapons\": ["));check(row!=result.npos,"blast weapon exists");auto at=result.find("\"parameters\": {",row)+15;result.insert(at,"\"blastMotion\":"+value+",");return result;};
 check(edited.load_text(withBlast(52,blast),error),error);check(edited.find(52)->weapon.blastMotion&&edited.find(52)->weapon.blastMotion->horizontalSpeed==3200,"configured blast reaches profile");
 check(edited.load_text(withBlast(52,replace(blast,"true","false")),error)&&!edited.find(52)->weapon.blastMotion->enabled,"explicit blast disable");
 check(edited.load_text(withBlast(52,"null"),error)&&!edited.find(52)->weapon.blastMotion,"null retains native default");
 rejection(withBlast(25,blast),"rifle cannot claim explosive motion");
 rejection(withBlast(52,replace(blast,"4000","30001")),"blast time bound");
 rejection(withBlast(52,replace(blast,"0.4","1.1")),"blast minimum scale bound");
 rejection(withBlast(52,replace(blast,"3200","-1")),"negative blast speed");
 rejection(withBlast(52,replace(blast,"enabled","enabeld")),"misspelled blast field");
 const auto imagePath=replace(text,"\"effectsManifest\": \"fx/original.gwfx\"","\"effectsManifest\": \"fx/original.gwfx\", \"damageEffectsManifest\": \"fx/damage.gwfx\"");
 check(edited.load_text(imagePath,error)&&edited.resources().damageEffectsManifest=="fx/damage.gwfx","damage images configurable");
 rejection(replace(imagePath,"fx/damage.gwfx","../damage.gwfx"),"body image path cannot escape data");
 // Catalog consumers already use Catalog::load(TSV). A sibling JSON takes
 // priority, and invalid present JSON cannot silently change the loadout.
 const auto scratch=std::filesystem::path(argv[2]);std::filesystem::create_directories(scratch);const auto json=scratch/"gameplay.json";
 {std::ofstream f(json,std::ios::binary);f<<replace(text,"\"displayName\": \"AK102\"","\"displayName\": \"JSON AK\"");}
 weapons::Catalog catalog;check(catalog.load(scratch/"weapon_catalog.tsv",error),error);check(catalog.find(weapons::Category::primary,25)->display_name=="JSON AK","JSON catalog priority without TSV");
 check(catalog.name(128)=="GEKKO VULCAN"&&catalog.name(25)=="AK102"&&catalog.name(499).empty(),"nonselectable definition names and unknown identity");
 {std::ofstream f(json,std::ios::binary);f<<"{}";}
 check(!catalog.load(scratch/"weapon_catalog.tsv",error)&&catalog.find(weapons::Category::primary,25)->display_name=="JSON AK","invalid JSON preserves previous catalog");
 std::filesystem::remove(json);check(!config.load(scratch/"missing.json",error)&&config.find(25)->weapon.damage==275,"missing file preserves configuration");
 std::cout<<"gameplay JSON: "<<old.size()<<" exact legacy profiles, strict refusal, atomic reload, resource and catalog override PASS\n";
 return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
