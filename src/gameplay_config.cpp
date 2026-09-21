#include "product_identity.h"
#include "gameplay_config.h"
#include "multi_ui_json.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>
namespace mgo2mt::gameplay {
namespace {
using Value=multi_ui::json::Value;
[[noreturn]]void bad(const std::string& at,const char* message){throw std::runtime_error(at+": "+message);}
void object(const Value&v,const std::string&at,std::initializer_list<std::string_view> keys){if(v.type!=Value::object)bad(at,"expected object");for(const auto&[key,value]:v.o)if(std::find(keys.begin(),keys.end(),key)==keys.end())bad(at+"."+key,"unknown field");}
const Value& get(const Value&v,const char*key,const std::string&at){auto p=v.o.find(key);if(p==v.o.end())bad(at+"."+key,"missing field");return p->second;}
uint32_t integer(const Value&v,const std::string&at,uint32_t lo,uint32_t hi){if(v.type!=Value::number||v.n<lo||v.n>hi||std::floor(v.n)!=v.n)bad(at,"integer out of range");return uint32_t(v.n);}
float scalar(const Value&v,const std::string&at,float lo,float hi){if(v.type!=Value::number||!std::isfinite(v.n)||v.n<lo||v.n>hi)bad(at,"number out of range");return float(v.n);}
bool boolean(const Value&v,const std::string&at){if(v.type!=Value::boolean)bad(at,"expected boolean");return v.b;}
std::string text(const Value&v,const std::string&at,size_t maximum=512){if(v.type!=Value::string||v.s.empty()||v.s.size()>maximum||std::any_of(v.s.begin(),v.s.end(),[](unsigned char c){return c<32||c==127;}))bad(at,"invalid text");return v.s;}
std::string path(const Value&v,const std::string&at,bool optional=false){if(optional&&v.type==Value::null)return {};auto s=text(v,at,240);if(!relative_resource_path(s))bad(at,"expected safe data-relative resource path");return s;}
template<size_t N>std::array<float,N> vector(const Value&v,const std::string&at,float bound){if(v.type!=Value::array||v.a.size()!=N)bad(at,"wrong vector size");std::array<float,N> r;for(size_t i=0;i<N;++i)r[i]=scalar(v.a[i],at,-bound,bound);return r;}
void profile_valid(const combat::Weapon&w,const std::string&at){
 if(!combat::valid_weapon_tuning(w))bad(at+".tuning","invalid weapon tuning or incompatible action");
 if(w.heldOnly){if(w.damage||w.staminaDamage||w.intervalMs||w.reloadMs||w.magazine||w.reserve||w.range||w.shotCue||w.impactCue||w.bodyCue||w.automatic||w.reloadRefillMs||w.reloadMotion||w.fireIntervalTicks||w.nativePrimaryMastery||w.nativeAkPenetration||w.nativeAkHitRegions||w.nativeAkAccuracy||w.nativeProjectile||w.originalFirearm||w.meleeAttack||w.nativePlaced||w.mountedOnly||w.ballistics||w.blastMotion)bad(at,"held-only profile has attack fields");return;}
 if(w.blastMotion&&(!combat::blast_motion::valid(*w.blastMotion)||(!w.nativeProjectile&&!w.nativePlaced)||(!w.damage&&!w.staminaDamage)))bad(at,"blast motion requires a damaging explosive profile");
 const auto source=w.behaviorSourceId?w.behaviorSourceId:w.id;
 if(source!=w.id&&!original_weapon::find(source))bad(at,"only an explicit recovered firearm can be an action source alias");
 if(source!=w.id&&(w.nativePlaced||w.nativeProjectile||w.meleeAttack||special_pc::weapon(w.id)||special_pc::weapon(source)))bad(at,"this action does not support a new weapon ID alias");
 if(w.nativePlaced&&(source!=64&&source!=65&&source!=66&&source!=67&&source!=69))bad(at,"unsupported placed action source");
 if(w.nativeProjectile&&(source!=50&&!projectile::throwable(source)&&source!=129&&source!=103))bad(at,"unsupported projectile action source");
 if(w.id==103&&(!w.mountedOnly||!w.nativeProjectile))bad(at,"mortar projectile requires a mounted-only profile");
 if(w.meleeAttack&&source!=1&&source!=73)bad(at,"unsupported melee action source");
 if((unsigned(w.nativePlaced)+unsigned(w.nativeProjectile)+unsigned(w.meleeAttack))>1)bad(at,"conflicting attack modes");
 if(!w.damage&&!w.staminaDamage&&!(w.nativeProjectile&&projectile::throwable(source))&&!w.nativePlaced)bad(at,"attack has no damage");
 if(!w.magazine||!w.range)bad(at,"attack needs magazine and range");
 if(w.fireIntervalTicks){if(w.intervalMs||!original::fire_interval_ns(*w.fireIntervalTicks))bad(at,"conflicting or invalid fire timing");}else if(!w.intervalMs)bad(at,"missing fire timing");
 if(w.reloadMotion){if(w.reloadMs||w.reloadRefillMs||!original::reload_timing(*w.reloadMotion))bad(at,"conflicting or invalid reload timing");}else if(!w.reloadMs||w.reloadRefillMs>w.reloadMs)bad(at,"invalid reload deadlines");
 const uint32_t duration=w.reloadMotion?original::reload_timing(*w.reloadMotion)->endMs:w.reloadMs;if(std::ceil(double(duration)/combat::weapon_reload_scale(w))>60000)bad(at+".tuning.reloadSpeedScale","effective reload duration exceeds 60000 ms");
 if(w.originalFirearm&&!w.ballistics&&!original_weapon::find(source))bad(at,"original firearm requires explicit ballistics");
 if(w.nativeAkHitRegions&&!w.nativeAkPenetration)bad(at,"AK hit regions require AK penetration");
 if(w.mountedOnly&&(w.nativePlaced||(w.nativeProjectile&&w.id!=103)||w.meleeAttack))bad(at,"unsupported mounted attack");
 if((w.id==55||(w.id>=56&&w.id<=59)||w.id==63||w.id==69)&&(w.damage||w.staminaDamage))bad(at,"this effect-only weapon has no configurable direct damage");
 if((w.nativeAkAccuracy||w.nativePrimaryMastery||w.nativeAkPenetration)&&source!=25)bad(at,"AK adapter requires explicit AK action source");
 if(w.nativePrimaryMastery&&(w.id!=25||!w.reloadMotion||w.reloadMotion->baseTick!=5||w.reloadMotion->intervals!=210||w.reloadMotion->refillTick!=650||w.reloadMotion->rate!=1))bad(at,"AK mastery requires ID 25 and reviewed reload motion");
}
}
bool relative_resource_path(std::string_view s){
 if(s.empty()||s.size()>240||s.front()=='/'||s.back()=='/'||s.find_first_of("\\:<>\"|?*")!=s.npos)return false;
 for(unsigned char c:s)if(c<32||c==127)return false;
 size_t start=0;for(;;){auto end=s.find('/',start);auto part=s.substr(start,end==s.npos?s.size()-start:end-start);if(part.empty()||part=="."||part==".."||part.back()=='.'||part.back()==' ')return false;
  auto stem=part.substr(0,part.find('.'));while(!stem.empty()&&stem.back()==' ')stem.remove_suffix(1);std::string upper(stem);for(auto&c:upper)c=char(std::toupper(static_cast<unsigned char>(c)));
  if(upper=="CON"||upper=="PRN"||upper=="AUX"||upper=="NUL"||upper=="CONIN$"||upper=="CONOUT$"||(upper.size()==4&&(upper.starts_with("COM")||upper.starts_with("LPT"))&&upper[3]>='1'&&upper[3]<='9'))return false;
  if(upper.size()==5&&(upper.starts_with("COM")||upper.starts_with("LPT"))&&(upper.substr(3)=="\xc2\xb9"||upper.substr(3)=="\xc2\xb2"||upper.substr(3)=="\xc2\xb3"))return false;
  if(end==s.npos)break;start=end+1;
 }return true;
}
bool Config::load(const std::filesystem::path& p,std::string&error){try{std::ifstream in(p,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("cannot open "+p.string());auto size=in.tellg();if(size<=0||size>1024*1024)throw std::runtime_error("gameplay JSON size limit");std::string s(size_t(size),'\0');in.seekg(0);if(!in.read(s.data(),size))throw std::runtime_error("cannot read gameplay JSON");return load_text(s,error);}catch(const std::exception&e){error=e.what();return false;}}
bool Config::load_text(std::string_view source,std::string&error){
 try{Config next;auto root=multi_ui::json::parse(source);object(root,"$",{"format","version","provenance","resources","initialDp","catalog","weapons"});
  if(text(get(root,"format","$"),"$.format")!=mgo2mt::brand::Format{"MGO2MT.Gameplay"})bad("$.format","unsupported format");integer(get(root,"version","$"),"$.version",1,1);text(get(root,"provenance","$"),"$.provenance",2048);
  const auto&resourceObject=get(root,"resources","$");object(resourceObject,"$.resources",{"handsPath","modelsIndexPath","modelRoot","audioManifest","effectsManifest","damageEffectsManifest","weaponEffectsManifest"});
  for(auto [key,out]:{std::pair{"handsPath",&next.resources_.handsPath},{"modelsIndexPath",&next.resources_.modelsIndexPath},{"modelRoot",&next.resources_.modelRoot},{"audioManifest",&next.resources_.audioManifest},{"effectsManifest",&next.resources_.effectsManifest}})*out=path(get(resourceObject,key,"$.resources"),std::string("$.resources.")+key);
  if(auto field=resourceObject.o.find("damageEffectsManifest");field!=resourceObject.o.end())next.resources_.damageEffectsManifest=path(field->second,"$.resources.damageEffectsManifest",true);
  if(auto field=resourceObject.o.find("weaponEffectsManifest");field!=resourceObject.o.end())next.resources_.weaponEffectsManifest=path(field->second,"$.resources.weaponEffectsManifest",true);
  const auto&dp=get(root,"initialDp","$");if(dp.type!=Value::null)next.initialDp_=integer(dp,"$.initialDp",0,UINT32_MAX);
  const auto&weapons=get(root,"weapons","$");if(weapons.type!=Value::array||weapons.a.empty()||weapons.a.size()>128)bad("$.weapons","expected 1..128 definitions");std::set<uint16_t> ids;
  for(size_t i=0;i<weapons.a.size();++i){const auto&row=weapons.a[i];const auto at="$.weapons["+std::to_string(i)+"]";object(row,at,{"id","name","provenance","massCandidate","parameters","visual"});Definition d;auto&w=d.weapon;w.id=uint16_t(integer(get(row,"id",at),at+".id",1,511));if(!ids.insert(w.id).second)bad(at+".id","duplicate weapon ID");d.name=text(get(row,"name",at),at+".name",128);d.provenance=text(get(row,"provenance",at),at+".provenance",2048);if(auto m=row.o.find("massCandidate");m!=row.o.end()&&m->second.type!=Value::null)d.massCandidate=int(integer(m->second,at+".massCandidate",0,1000000));
   const auto&p=get(row,"parameters",at);const auto pa=at+".parameters";object(p,pa,{"damage","staminaDamage","intervalMs","reloadMs","magazine","reserve","range","shotCue","impactCue","bodyCue","reloadRefillMs","reloadMotion","fireIntervalTicks","automatic","nativePrimaryMastery","nativeAkPenetration","nativeAkHitRegions","heldOnly","nativeAkAccuracy","nativeProjectile","originalFirearm","meleeAttack","nativePlaced","mountedOnly","stageMaterialAudio","behaviorSourceId","ballistics","blastMotion","tuning"});
   for(auto[key,out]:{std::pair{"damage",&w.damage},{"staminaDamage",&w.staminaDamage},{"shotCue",&w.shotCue},{"impactCue",&w.impactCue},{"bodyCue",&w.bodyCue}})*out=integer(get(p,key,pa),pa+"."+key,0,1000000);
   for(auto[key,out]:{std::pair{"intervalMs",&w.intervalMs},{"reloadMs",&w.reloadMs},{"reloadRefillMs",&w.reloadRefillMs}})*out=integer(get(p,key,pa),pa+"."+key,0,60000);
   w.magazine=uint16_t(integer(get(p,"magazine",pa),pa+".magazine",0,1000));w.reserve=uint16_t(integer(get(p,"reserve",pa),pa+".reserve",0,10000));w.range=scalar(get(p,"range",pa),pa+".range",0,1000000);w.behaviorSourceId=uint16_t(integer(get(p,"behaviorSourceId",pa),pa+".behaviorSourceId",0,511));
   for(auto[key,out]:{std::pair{"automatic",&w.automatic},{"nativePrimaryMastery",&w.nativePrimaryMastery},{"nativeAkPenetration",&w.nativeAkPenetration},{"nativeAkHitRegions",&w.nativeAkHitRegions},{"heldOnly",&w.heldOnly},{"nativeAkAccuracy",&w.nativeAkAccuracy},{"nativeProjectile",&w.nativeProjectile},{"originalFirearm",&w.originalFirearm},{"meleeAttack",&w.meleeAttack},{"nativePlaced",&w.nativePlaced},{"mountedOnly",&w.mountedOnly}})*out=boolean(get(p,key,pa),pa+"."+key);
   const auto&fire=get(p,"fireIntervalTicks",pa);if(fire.type!=Value::null)w.fireIntervalTicks=integer(fire,pa+".fireIntervalTicks",1,17982);
   const auto&reload=get(p,"reloadMotion",pa);if(reload.type!=Value::null){const auto ra=pa+".reloadMotion";object(reload,ra,{"baseTick","intervals","refillTick","rate"});original::ReloadMotion m;m.baseTick=integer(get(reload,"baseTick",ra),ra+".baseTick",1,1000);m.intervals=integer(get(reload,"intervals",ra),ra+".intervals",2,3600);m.refillTick=integer(get(reload,"refillTick",ra),ra+".refillTick",1,3600000);m.rate=scalar(get(reload,"rate",ra),ra+".rate",.001f,16);w.reloadMotion=m;}
   const auto&ball=get(p,"ballistics",pa);if(ball.type!=Value::null){const auto ba=pa+".ballistics";object(ball,ba,{"range","speed","decayStart","decayEnd","minimumForce","penetration"});original_weapon::Ballistic b;b.range=scalar(get(ball,"range",ba),ba+".range",1,1000000);b.speed=scalar(get(ball,"speed",ba),ba+".speed",1,10000000);b.decayStart=scalar(get(ball,"decayStart",ba),ba+".decayStart",0,1000000);b.decayEnd=scalar(get(ball,"decayEnd",ba),ba+".decayEnd",b.decayStart,1000000);b.minimumForce=int(integer(get(ball,"minimumForce",ba),ba+".minimumForce",0,1000));b.penetration=int(integer(get(ball,"penetration",ba),ba+".penetration",0,1000000));if(b.decayEnd>b.range||b.range!=w.range)bad(ba,"ballistic range must match weapon range and contain decay");w.ballistics=b;}
   if(auto field=p.o.find("blastMotion");field!=p.o.end()&&field->second.type!=Value::null){const auto&b=field->second;const auto ba=pa+".blastMotion";object(b,ba,{"enabled","horizontalSpeed","upwardSpeed","minimumScale","gravity","maxFlightMs"});combat::blast_motion::Policy policy;policy.enabled=boolean(get(b,"enabled",ba),ba+".enabled");policy.horizontalSpeed=scalar(get(b,"horizontalSpeed",ba),ba+".horizontalSpeed",0,20000);policy.upwardSpeed=scalar(get(b,"upwardSpeed",ba),ba+".upwardSpeed",0,15000);policy.minimumScale=scalar(get(b,"minimumScale",ba),ba+".minimumScale",0,1);policy.gravity=scalar(get(b,"gravity",ba),ba+".gravity",100,50000);policy.maxFlightMs=integer(get(b,"maxFlightMs",ba),ba+".maxFlightMs",100,30000);w.blastMotion=policy;}
   if(auto field=p.o.find("tuning");field!=p.o.end()&&field->second.type!=Value::null){const auto&t=field->second;const auto ta=pa+".tuning";object(t,ta,{"weightKg","moveSpeedScale","reloadSpeedScale","chamberCapacity","accuracy"});auto&out=w.tuning;
    if(auto f=t.o.find("weightKg");f!=t.o.end())out.weightKg=scalar(f->second,ta+".weightKg",0,100);
    if(auto f=t.o.find("moveSpeedScale");f!=t.o.end())out.moveSpeedScale=scalar(f->second,ta+".moveSpeedScale",.1f,2);
    if(auto f=t.o.find("reloadSpeedScale");f!=t.o.end())out.reloadSpeedScale=scalar(f->second,ta+".reloadSpeedScale",.1f,5);
    if(auto f=t.o.find("chamberCapacity");f!=t.o.end())out.chamberCapacity=uint8_t(integer(f->second,ta+".chamberCapacity",0,8));
    if(auto f=t.o.find("accuracy");f!=t.o.end()&&f->second.type!=Value::null){const auto&a=f->second;const auto aa=ta+".accuracy";object(a,aa,{"base","maximum","perShot","recovery","movingMultiplier","crouchMultiplier","proneMultiplier","pellets"});weapon_accuracy::Policy policy;
     for(auto[key,dst]:{std::pair{"base",&policy.base},{"maximum",&policy.maximum}})if(auto v=a.o.find(key);v!=a.o.end())*dst=integer(v->second,aa+"."+key,0,100000);
     if(auto v=a.o.find("perShot");v!=a.o.end())policy.perShot=integer(v->second,aa+".perShot",1,100000);
     if(auto v=a.o.find("recovery");v!=a.o.end())policy.recovery=integer(v->second,aa+".recovery",1,1000000);
     if(auto v=a.o.find("movingMultiplier");v!=a.o.end())policy.movingMultiplier=scalar(v->second,aa+".movingMultiplier",1,8);
     if(auto v=a.o.find("crouchMultiplier");v!=a.o.end())policy.crouchMultiplier=scalar(v->second,aa+".crouchMultiplier",.1f,1);
     if(auto v=a.o.find("proneMultiplier");v!=a.o.end())policy.proneMultiplier=scalar(v->second,aa+".proneMultiplier",.1f,1);
     if(auto v=a.o.find("pellets");v!=a.o.end())policy.pellets=uint8_t(integer(v->second,aa+".pellets",1,32));out.accuracy=policy;
    }
   }
   d.stageMaterialAudio=boolean(get(p,"stageMaterialAudio",pa),pa+".stageMaterialAudio");profile_valid(w,pa);
   const auto&v=get(row,"visual",at);auto&vis=d.visual;vis.id=w.id;const auto va=at+".visual";object(v,va,{"motionId","effectId","flashTexture","smokeTexture","modelPath","secondaryModelPath","iconPath","flags","muzzle","magPosition","magRotation","autoAim","tracer","lockRange","lockWidth","lockYaw"});vis.motionId=uint16_t(integer(get(v,"motionId",va),va+".motionId",0,511));vis.effectId=uint16_t(integer(get(v,"effectId",va),va+".effectId",0,511));for(auto[key,out]:{std::pair{"flashTexture",&vis.flashTexture},{"smokeTexture",&vis.smokeTexture}})if(auto field=v.o.find(key);field!=v.o.end())*out=integer(field->second,va+"."+key,0,UINT32_MAX);vis.modelPath=path(get(v,"modelPath",va),va+".modelPath",true);vis.secondaryModelPath=path(get(v,"secondaryModelPath",va),va+".secondaryModelPath",true);if(auto icon=v.o.find("iconPath");icon!=v.o.end()){vis.iconPath=path(icon->second,va+".iconPath",true);if(!vis.iconPath.empty()&&!(vis.iconPath.size()>=4&&vis.iconPath[vis.iconPath.size()-4]=='.'&&std::tolower(static_cast<unsigned char>(vis.iconPath[vis.iconPath.size()-3]))=='p'&&std::tolower(static_cast<unsigned char>(vis.iconPath[vis.iconPath.size()-2]))=='n'&&std::tolower(static_cast<unsigned char>(vis.iconPath.back()))=='g'))bad(va+".iconPath","expected a data-relative .png image");}vis.flags=integer(get(v,"flags",va),va+".flags",0,7);vis.muzzle=vector<3>(get(v,"muzzle",va),va+".muzzle",100000);vis.magPosition=vector<3>(get(v,"magPosition",va),va+".magPosition",100000);vis.magRotation=vector<4>(get(v,"magRotation",va),va+".magRotation",1);float norm=0;for(float q:vis.magRotation)norm+=q*q;if(norm<.99f||norm>1.01f)bad(va+".magRotation","expected unit quaternion");
   vis.autoAim=boolean(get(v,"autoAim",va),va+".autoAim");vis.tracer=boolean(get(v,"tracer",va),va+".tracer");vis.lockRange=scalar(get(v,"lockRange",va),va+".lockRange",0,1000000);vis.lockWidth=scalar(get(v,"lockWidth",va),va+".lockWidth",0,100000);vis.lockYaw=scalar(get(v,"lockYaw",va),va+".lockYaw",0,3.141593f);if((vis.flags&2)!=(!vis.secondaryModelPath.empty()?2u:0u))bad(va,"secondary model and flag disagree");if(!vis.modelPath.empty()&&!vis.motionId)bad(va,"held model requires explicit motion source");next.definitions_.push_back(std::move(d));
  }
  const auto&cat=get(root,"catalog","$");if(cat.type!=Value::array||cat.a.empty()||cat.a.size()>128)bad("$.catalog","expected 1..128 entries");std::set<std::pair<weapons::Category,uint16_t>> entries;
  for(size_t i=0;i<cat.a.size();++i){const auto&r=cat.a[i];const auto at="$.catalog["+std::to_string(i)+"]";object(r,at,{"id","category","displayName","dpCost","availableWithoutDp","restrictionBit"});weapons::Entry e;e.id=uint16_t(integer(get(r,"id",at),at+".id",0,511));if(e.id&&!ids.contains(e.id))bad(at+".id","catalog ID has no weapon definition");if(auto d=next.find(e.id);d&&d->weapon.mountedOnly)bad(at+".id","mounted-only weapon cannot enter handheld catalog");auto c=text(get(r,"category",at),at+".category");if(c=="PRIMARY")e.category=weapons::Category::primary;else if(c=="SECONDARY")e.category=weapons::Category::secondary;else if(c=="SUPPORT")e.category=weapons::Category::support;else bad(at+".category","unknown category");if(!entries.emplace(e.category,e.id).second)bad(at,"duplicate catalog category/ID");e.display_name=text(get(r,"displayName",at),at+".displayName",128);const auto&cost=get(r,"dpCost",at);if(cost.type!=Value::null)e.dp_cost=integer(cost,at+".dpCost",0,UINT32_MAX);const auto&available=get(r,"availableWithoutDp",at);if(available.type!=Value::null)e.available_without_dp=boolean(available,at+".availableWithoutDp");const auto&bit=get(r,"restrictionBit",at);if(bit.type!=Value::null)e.restriction_bit=uint8_t(integer(bit,at+".restrictionBit",1,127));next.entries_.push_back(std::move(e));}
  *this=std::move(next);error.clear();return true;
 }catch(const std::exception&e){error=std::string("Gameplay configuration: ")+e.what();return false;}
}
const Definition*Config::find(uint16_t id)const{for(const auto&d:definitions_)if(d.weapon.id==id)return &d;return nullptr;}
const Visual*Config::visual(uint16_t id)const{auto d=find(id);return d?&d->visual:nullptr;}
std::vector<combat::Weapon> Config::profiles(uint8_t map)const{std::vector<combat::Weapon> r;r.reserve(definitions_.size());for(const auto&d:definitions_){r.push_back(d.weapon);r.back().materialMap=d.stageMaterialAudio?map:0;}return r;}
}
