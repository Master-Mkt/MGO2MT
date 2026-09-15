#include "stage_assets.h"
#include "stage_profiles.h"
#include <fstream>
#include <stdexcept>
#include <cmath>
#include <set>
namespace mgo2win::stage {
std::vector<ObjectBinding> read_object_bindings(const std::filesystem::path& root,bool loadModels,uint8_t map){
 if(!runtime_stage_supported(map))throw std::invalid_argument("Unknown stage bindings route");
 auto registry=load_object_registry(asset_path(root,map,".objects.cfg"));auto path=asset_path(root,map,".bindings.cfg");
 auto require=[](bool value){if(!value)throw std::runtime_error("Invalid stage object bindings");};
 require(std::filesystem::file_size(path)<=1024*1024);std::ifstream in(path);std::string magic,profile;unsigned version,count;
 require(bool(in>>magic>>version>>profile>>count)&&magic=="MGO2WIN.STAGE_BINDINGS"&&version==1&&registry.map==map&&((map==20&&profile=="n022a_success32")||(registry.nativeLights&&map==7&&profile=="n007a_native_lights_v1")||(registry.nativeStatic&&profile==std::string(runtime_profile(map)->stage)+"_native_static_v1"))&&count==registry.entries.size());
 auto asset=[&](const std::string& name){require(name.starts_with("objects/")&&name.size()<128&&name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_./-")==std::string::npos&&name.find("..") == std::string::npos);auto result=root/name;require(std::filesystem::is_regular_file(result)&&std::filesystem::file_size(result)<=8*1024*1024);return result;};
 auto vector=[&](Vec3& v){for(auto&x:v)require(bool(in>>x)&&std::isfinite(x)&&std::abs(x)<1000000);};
 std::map<std::pair<std::string,uint64_t>,std::shared_ptr<const CharacterModel>> models;
 std::map<std::string,std::shared_ptr<const Collision>> collisions;std::set<unsigned> components;
 std::vector<ObjectBinding> result;result.reserve(count);
 for(unsigned i=0;i<count;++i){ObjectBinding b;unsigned width,parts,lights;
  require(bool(in>>b.bindingId>>width>>b.cboxOrdinal)&&b.bindingId==registry.entries[i].bindingId&&width==registry.entries[i].width&&b.cboxOrdinal==(map==20&&i>=17?int(i-17):-1));b.width=uint8_t(width);vector(b.position);vector(b.degrees);
  require(bool(in>>parts>>lights)&&parts<=16&&lights<=16);
  for(unsigned j=0;j<parts;++j){ObjectPartBinding p;unsigned mask,value,overridePlacement,hitOnly;std::string model,collision;uint64_t partMask;ObjectTransform transform;
   require(bool(in>>p.componentId>>mask>>value>>model>>partMask>>collision>>hitOnly>>overridePlacement)&&p.componentId&&components.insert(p.componentId).second&&mask<(1u<<width)&&(value&~mask)==0&&overridePlacement<=1&&hitOnly<=1);
   p.mask=uint8_t(mask);p.value=uint8_t(value);p.hitOnly=hitOnly!=0;vector(transform.position);vector(transform.degrees);if(overridePlacement)p.placement=transform;
   if(model!="-"&&loadModels){auto key=std::pair{model,partMask};auto found=models.find(key);if(found!=models.end())p.model=found->second;else{
    auto modelPath=asset(model);std::ifstream stream(modelPath,std::ios::binary|std::ios::ate);auto size=stream.tellg();require(size>=48);std::vector<char> bytes(static_cast<size_t>(size));stream.seekg(0);require(bool(stream.read(bytes.data(),size)));
    auto loaded=std::make_shared<CharacterModel>(bytes);if(partMask){require(loaded->parts.size()<=64&&(loaded->parts.size()==64||!(partMask>>loaded->parts.size())));std::vector<ModelPart> selected;for(size_t index=0;index<loaded->parts.size();++index)if(partMask&(uint64_t(1)<<index))selected.push_back(loaded->parts[index]);loaded->parts=std::move(selected);}p.model=loaded;models.emplace(key,std::move(loaded));
   }}else if(model=="-")require(partMask==0);
   if(collision!="-"){auto found=collisions.find(collision);if(found!=collisions.end())p.collision=found->second;else{std::ifstream stream(asset(collision));auto loaded=std::make_shared<const Collision>(Collision::read(stream));p.collision=loaded;collisions.emplace(collision,std::move(loaded));}}
   require(model!="-"||bool(p.collision));b.parts.push_back(std::move(p));
  }
  for(unsigned j=0;j<lights;++j){ObjectLightRule r;unsigned mask,value,enabled;require(bool(in>>mask>>value>>r.light.key>>r.light.id>>enabled)&&mask>0&&mask<(1u<<width)&&(value&~mask)==0&&enabled<=1);r.mask=uint8_t(mask);r.value=uint8_t(value);r.light.enabled=enabled!=0;vector(r.light.center);require(bool(in>>r.light.radius)&&std::isfinite(r.light.radius)&&r.light.radius>=0&&r.light.radius<=1000000);b.lights.push_back(r);}
  result.push_back(std::move(b));
 }
 std::string tail;require(!(in>>tail));return result;
}
}
