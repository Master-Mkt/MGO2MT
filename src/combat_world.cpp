#include "combat_world.h"
#include "stage_profiles.h"
#include <fstream>
namespace mgo2mt::combat {
World World::load(const std::filesystem::path&root,uint8_t map){
 World w;auto file=stage::asset_path(root,map,".collision.cfg");if(!std::filesystem::is_regular_file(file)||std::filesystem::file_size(file)>32*1024*1024)throw std::runtime_error("Host collision extent");std::ifstream in(file);w.base_=std::make_shared<const stage::Collision>(stage::Collision::read(in));
 w.registry_=stage::load_combat_object_registry(stage::asset_path(root,map,".objects.cfg"));w.bindings_=stage::read_object_bindings(root,false,map);
 if(std::filesystem::file_size(stage::asset_path(root,map,".cbox.cfg"))>16384)throw std::runtime_error("Host CBOX extent");std::ifstream boxes(stage::asset_path(root,map,".cbox.cfg"));w.cboxes_=stage::CboxLayout::read(boxes);return w;
}
bool World::apply(const stage::SceneSnapshot&s){
 if(!base_||!s.revision||s.request.rotation.map!=registry_.map||(registry_.rule&&s.request.rotation.rule!=*registry_.rule)||((registry_.nativeStatic||registry_.nativeLights||registry_.combatRulesOnly)&&s.request.rotation.rule>1)||s.objects.size()!=registry_.entries.size())return false;
 if(snapshot_&&s.request==snapshot_->request){if(s.revision<snapshot_->revision)return false;if(s.revision==snapshot_->revision)return s==*snapshot_;}
 try{auto boxes=cboxes_.select(s.request.generation);std::vector<stage::CollisionInstance>solid,hit;
  for(size_t i=0;i<bindings_.size();++i){const auto&b=bindings_[i];const auto&state=s.objects[i];if(state.bindingId!=b.bindingId||state.current>=(1u<<b.width)||state.initial>=(1u<<b.width))return false;auto position=b.position,degrees=b.degrees;
   if(b.cboxOrdinal>=0){if(size_t(b.cboxOrdinal)>=boxes.size())return false;position=boxes[b.cboxOrdinal].anchor.position;degrees={0,boxes[b.cboxOrdinal].rotationRadians*180.f/3.14159265359f,0};}
   for(const auto&p:b.parts)if(p.collision&&(state.current&p.mask)==p.value)(p.hitOnly?hit:solid).push_back({p.componentId,p.collision,p.placement?p.placement->position:position,p.placement?p.placement->degrees:degrees});
  }
  auto world=std::make_shared<const stage::Collision>(stage::Collision::combine(*base_,solid));auto hits=std::make_shared<const stage::Collision>(stage::Collision::combine(stage::Collision::make({},{}),hit));world_=std::move(world);hits_=std::move(hits);snapshot_=s;return true;
 }catch(...){return false;}
}
}
