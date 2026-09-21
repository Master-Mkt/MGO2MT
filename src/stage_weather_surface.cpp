#include "product_identity.h"
#include "stage_weather_surface.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
namespace mgo2mt::stage::weather {
namespace {
void require(bool b){if(!b)throw std::runtime_error("Invalid weather settings");}
bool valid(Vec3 p){for(float x:p)if(!std::isfinite(x)||std::abs(x)>1000000)return false;return true;}
bool valid(const Settings&s){return s.preset>=Preset::clear&&s.preset<=Preset::fog_sand&&std::isfinite(s.intensity)&&s.intensity>=0&&s.intensity<=1&&std::isfinite(s.wetSeconds)&&s.wetSeconds>=1&&s.wetSeconds<=3600&&std::isfinite(s.snowSeconds)&&s.snowSeconds>=1&&s.snowSeconds<=3600&&std::isfinite(s.drySeconds)&&s.drySeconds>=1&&s.drySeconds<=3600&&std::isfinite(s.meltSeconds)&&s.meltSeconds>=1&&s.meltSeconds<=3600;}
}
Settings Settings::defaults(std::string_view stage){Settings s;if(stage=="n022a")s.preset=Preset::fog_sand;return s;}
Settings Settings::read(std::istream&in){Settings s;std::string key,value;unsigned version=0,enabled=0;
 require(bool(in>>key>>version)&&key==mgo2mt::brand::Format{"MGO2MT.WEATHER"}&&version==1);
 require(bool(in>>key>>enabled)&&key=="ENABLED"&&enabled<=1);s.enabled=enabled;
 require(bool(in>>key>>value)&&key=="PRESET");if(value=="clear")s.preset=Preset::clear;else if(value=="rain")s.preset=Preset::rain;else if(value=="snow")s.preset=Preset::snow;else if(value=="fog_sand")s.preset=Preset::fog_sand;else require(false);
 for(auto [name,p]:{std::pair{"INTENSITY",&s.intensity},{"WET_SECONDS",&s.wetSeconds},{"SNOW_SECONDS",&s.snowSeconds},{"DRY_SECONDS",&s.drySeconds},{"MELT_SECONDS",&s.meltSeconds}})require(bool(in>>key>>*p)&&key==name);
 require(bool(in>>key)&&key=="END");require(!(in>>key));require(valid(s));return s;
}
void SurfaceController::reset(){collision_.reset();grid_={};order_.clear();next_=0;previous_=-1;revision_=0;}
void SurfaceController::reset_grid(Vec3 eye){
 const auto old=std::move(grid_);grid_={};grid_.originX=std::floor(eye[0]/grid_.cellSize)*grid_.cellSize-grid_.width/2*grid_.cellSize;grid_.originZ=std::floor(eye[2]/grid_.cellSize)*grid_.cellSize-grid_.width/2*grid_.cellSize;grid_.collisionRevision=revision_;grid_.cells.resize(size_t(grid_.width)*grid_.width);order_.resize(grid_.cells.size());std::iota(order_.begin(),order_.end(),0);next_=0;
 // Reuse only the same immutable collision revision. Changed roofs must be probed anew.
 if(old.collisionRevision==revision_&&!old.cells.empty())for(unsigned z=0;z<grid_.width;++z)for(unsigned x=0;x<grid_.width;++x){int ox=int(std::lround((grid_.originX+x*grid_.cellSize-old.originX)/old.cellSize)),oz=int(std::lround((grid_.originZ+z*grid_.cellSize-old.originZ)/old.cellSize));if(ox>=0&&oz>=0&&ox<int(old.width)&&oz<int(old.width))grid_.cells[size_t(z)*grid_.width+x]=old.cells[size_t(oz)*old.width+ox];}
 std::stable_sort(order_.begin(),order_.end(),[&](size_t a,size_t b){auto distance=[&](size_t i){int x=int(i%grid_.width)-int(grid_.width/2),z=int(i/grid_.width)-int(grid_.width/2);return x*x+z*z;};return distance(a)<distance(b);});
}
std::shared_ptr<const SurfaceGrid> SurfaceController::sample(const Settings&s,double seconds,Vec3 eye,std::shared_ptr<const Collision>collision,uint64_t revision){
 if(!valid(s)||!valid(eye)||!std::isfinite(seconds)||seconds<0||!s.enabled||!collision||collision->vertices.empty()){reset();return {};}
 if(previous_>=0&&seconds<previous_)reset();
 float dt=previous_<0||seconds<previous_?0:float(std::min(seconds-previous_,.25));previous_=seconds;
 bool changed=collision_!=collision||revision_!=revision;
 if(changed){collision_=std::move(collision);revision_=revision;grid_={};ceiling_=-1000000;floor_=1000000;for(auto p:collision_->vertices){ceiling_=std::max(ceiling_,p[1]);floor_=std::min(floor_,p[1]);}ceiling_+=100;floor_-=100;reset_grid(eye);}
 else if(grid_.cells.empty()||std::abs(eye[0]-(grid_.originX+grid_.width/2*grid_.cellSize))>8000||std::abs(eye[2]-(grid_.originZ+grid_.width/2*grid_.cellSize))>8000)reset_grid(eye);
 const bool rain=s.rainEnabled.value_or(s.preset==Preset::rain),snow=s.snowEnabled.value_or(s.preset==Preset::snow);
 if(rain||snow){unsigned queries=0;while(next_<order_.size()&&queries<64){const auto index=order_[next_++];auto&cell=grid_.cells[index];if(cell.normalY>0)continue;
   const float x=grid_.originX+float(index%grid_.width)*grid_.cellSize,z=grid_.originZ+float(index/grid_.width)*grid_.cellSize;
   auto hit=collision_->ray({x,ceiling_,z},{0,-1,0},ceiling_-floor_);++queries;
   if(hit&&std::abs(hit->normal[1])>=.35f){cell.height=hit->position[1];cell.normalY=std::abs(hit->normal[1]);}
  }}
 bool active=false;for(auto&cell:grid_.cells){if(cell.normalY<=0)continue;cell.wetness=std::clamp(cell.wetness+dt*(rain?s.intensity/s.wetSeconds:-1/s.drySeconds),0.f,1.f);cell.snow=std::clamp(cell.snow+dt*(snow?s.intensity/s.snowSeconds:-1/s.meltSeconds),0.f,1.f);active|=cell.wetness>0||cell.snow>0;}
 return active?std::make_shared<const SurfaceGrid>(grid_):nullptr;
}
float surface_coverage(const SurfaceGrid&g,Vec3 p,float normal){
 if(!valid(p)||!std::isfinite(normal)||normal<=.35f||g.width<2||g.width>64||g.cells.size()!=size_t(g.width)*g.width||!std::isfinite(g.cellSize)||g.cellSize<=0)return 0;
 float fx=(p[0]-g.originX)/g.cellSize,fz=(p[2]-g.originZ)/g.cellSize;int x=int(std::floor(fx)),z=int(std::floor(fz));if(x<0||z<0||x>=int(g.width)-1||z>=int(g.width)-1)return 0;fx-=x;fz-=z;
 float amount=1,low=1000000,high=-1000000,height=0;for(int dz=0;dz<=1;++dz)for(int dx=0;dx<=1;++dx){auto&c=g.cells[size_t(z+dz)*g.width+x+dx];if(c.normalY<=.35f)return 0;low=std::min(low,c.height);high=std::max(high,c.height);height+=c.height*(dx?fx:1-fx)*(dz?fz:1-fz);amount=std::min(amount,std::max(c.wetness,c.snow));}if(high-low>g.cellSize||std::abs(height-p[1])>=100)return 0;return amount*std::clamp((normal-.35f)/.45f,0.f,1.f);
}
}
