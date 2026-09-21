#include "stage_environment.h"
namespace mgo2mt::stage {
namespace {
environment::Config light_key(const environment::Config&c){environment::Config k;k.time=c.time;k.manualHemisphere=c.manualHemisphere;if(c.manualHemisphere){k.time=environment::Time::original;k.upper=c.upper;k.lower=c.lower;k.gainMilli=c.gainMilli;}return k;}
}
bool environment_override(const environment::Config&c){return c.manualHemisphere||c.time!=environment::Time::original;}
Lighting environment_lighting(const Lighting&original,const environment::Config&c){
 if(!environment::valid(c))throw std::invalid_argument("Stage environment settings");auto light=original;if(!environment_override(c))return light;
 // A global manual hemisphere replaces local LT3 hemisphere colors, while
 // authored point/spot lights and their destruction state remain unchanged.
 light.hemispheres.clear();light.axis={0,1,0};light.ambientScale={1,1,1};
 if(c.manualHemisphere){for(unsigned i=0;i<3;++i){light.back[i]=float(c.upper[i])/255.f*c.gainMilli/1000.f;light.front[i]=float(c.lower[i])/255.f*c.gainMilli/1000.f;}return light;}
 struct Native {Vec3 upper,lower,sun,direction;};
 constexpr Native colors[]={
  {{},{},{},{0,-1,0}},
  {{.48f,.36f,.40f},{.15f,.12f,.16f},{.44f,.25f,.16f},{.9f,-.25f,.35f}},
  {{.60f,.70f,.82f},{.24f,.25f,.27f},{.65f,.57f,.45f},{.6f,-.65f,.3f}},
  {{.72f,.80f,.90f},{.30f,.29f,.27f},{.78f,.76f,.68f},{.1f,-1.f,.2f}},
  {{.52f,.35f,.30f},{.19f,.13f,.16f},{.64f,.31f,.15f},{-.85f,-.25f,.3f}},
  {{.13f,.17f,.26f},{.035f,.045f,.075f},{.045f,.06f,.10f},{-.3f,-.8f,.2f}}};
 const auto&p=colors[unsigned(c.time)];light.back=p.upper;light.front=p.lower;light.direct=p.sun;light.direction=p.direction;return light;
}
weather::Settings environment_weather(std::string_view stage,const environment::Config&c){if(!environment::valid(c))throw std::invalid_argument("Stage weather settings");auto out=weather::Settings::defaults(stage);if(c.weatherOverride){out.preset=weather::Preset::clear;out.rainEnabled=c.rain;out.snowEnabled=c.snow;}return out;}
void EnvironmentCache::update(std::shared_ptr<const CharacterModel>model,std::shared_ptr<const Lighting>light,const environment::Config&c){
 if(!environment::valid(c))throw std::invalid_argument("Stage environment settings");const auto key=light_key(c);if(source_==model&&original_==light&&config_==key)return;source_=std::move(model);original_=std::move(light);config_=key;model_=source_;light_=original_;
 if(!source_||!environment_override(c))return;auto changed=std::make_shared<Lighting>(environment_lighting(original_?*original_:Lighting{},c));auto copy=std::make_shared<CharacterModel>(*source_);changed->sample_vertices(copy->vertices);model_=std::move(copy);light_=std::move(changed);
}
}
