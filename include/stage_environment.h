#pragma once
#include "environment_settings.h"
#include "stage_lighting.h"
#include "stage_weather_surface.h"
#include <memory>
namespace mgo2mt::stage {
// Native presentation presets, not original GCX time-of-day constants.
bool environment_override(const environment::Config&);
Lighting environment_lighting(const Lighting&,const environment::Config&);
weather::Settings environment_weather(std::string_view,const environment::Config&);
class EnvironmentCache {
 std::shared_ptr<const CharacterModel> source_,model_;
 std::shared_ptr<const Lighting> original_,light_;
 environment::Config config_;
public:
 void clear(){source_.reset();model_.reset();original_.reset();light_.reset();}
 void update(std::shared_ptr<const CharacterModel>,std::shared_ptr<const Lighting>,const environment::Config&);
 const std::shared_ptr<const CharacterModel>& model()const{return model_;}
 const std::shared_ptr<const Lighting>& lighting()const{return light_;}
};
}
