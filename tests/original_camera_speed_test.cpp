#include "original_camera_speed.h"
#include "camera_settings.h"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace mgo2win;
static void require(bool v){if(!v)throw std::runtime_error("camera speed policy check failed");}
int main(){
 using namespace original::camera_speed;
 for(unsigned value=0;value<65536;++value){
  const auto stored=std::uint16_t(value),bounded=std::uint16_t(value<10?value:9);
  require(clamp_stored(stored)==bounded);
  require(field_1cc_factor(stored)==.5f+.125f*bounded);
  require(field_1d0_factor(stored)==bounded+1.f);
  require(initial_field_1ce_factor(stored)==bounded+1.f);
  require(field_1d2_transition_ticks(stored)==10*(9u-bounded));
 }
 require(!valid_display(0)&&valid_display(1)&&valid_display(10)&&!valid_display(11));
 const auto legacy=camera::decode("MGO2WIN.CAMERA 1 0 1 0 1 0 1\n");
 require(legacy.has_value()&&legacy->speed==std::array<unsigned,3>{5,5,5});
 require(legacy->rates(false,false)==std::array<float,2>{2,1.5f});
 for(unsigned mode=0;mode<3;++mode)for(unsigned value=1;value<=10;++value){
  camera::Settings settings;settings.speed[mode]=value;settings.reversed[mode*2]=true;
  require(camera::decode(camera::encode(settings))==settings);
  const auto rates=settings.rates(mode==2,mode!=0);
  require(rates[0]==2.f*(float(value)/5.f)&&rates[1]==1.5f*(float(value)/5.f));
  require(settings.motion(mode==2,mode!=0,.25f,.75f)==std::array<float,2>{.25f,-.75f});
  require(settings.rates((mode+1)%3==2,(mode+1)%3!=0)==std::array<float,2>{2,1.5f});
 }
 for(const auto bad:{"MGO2WIN.CAMERA 2 0 0 0 0 0 0 0 5 5\n","MGO2WIN.CAMERA 2 0 0 0 0 0 0 11 5 5\n",
  "MGO2WIN.CAMERA 2 0 0 0 0 0 0 05 5 5\n","MGO2WIN.CAMERA 2 0 0 0 0 0 0 5 5\n",
  "MGO2WIN.CAMERA 2 0 0 0 0 0 0 5 5 5 1\n","MGO2WIN.CAMERA 2 0 0 0 0 0 0 -1 5 5\n",
  "MGO2WIN.CAMERA 2 0 0 0 0 0 0 4294967296 5 5\n","MGO2WIN.CAMERA 1 0 0 0 0 0 0 5 5 5\n"})require(!camera::decode(bad));
 const auto encoded=camera::encode(*legacy);for(std::size_t n=0;n<encoded.size();++n)require(!camera::decode(std::string_view(encoded).substr(0,n)));
 camera::Settings invalid;invalid.speed[0]=std::numeric_limits<unsigned>::max();
 require(invalid.rates(false,false)==std::array<float,2>{2,1.5f});
 bool rejected=false;try{camera::encode(invalid);}catch(const std::invalid_argument&){rejected=true;}require(rejected);
 std::cout<<"camera field policies, 65536 stored bounds, independent rates and v1/v2 codec passed\n";
}
