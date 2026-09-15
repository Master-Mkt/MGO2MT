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
 // Actual save converter writes UI-1 into packed byte1/2/3 and expanded
 // +8/+12/+6, respectively. Distinct values catch shoulder/subject swaps.
 const std::array<std::uint8_t,5> packed{0x80,0x1e,0x7d,0x9b,0xa3};
 require(decode_packed_speeds(packed)==ExpandedSpeeds{1,7,9,3});
 for(std::size_t length=0;length<5;++length)
  require(!decode_packed_speeds(std::span<const std::uint8_t>(packed).first(length)));
 for(unsigned normal=0;normal<16;++normal)for(unsigned shoulder=0;shoulder<16;++shoulder)
  for(unsigned subject=0;subject<16;++subject){
   const std::array<std::uint8_t,5> raw{0xff,std::uint8_t(normal*16+15),
    std::uint8_t(shoulder*16+10),std::uint8_t(subject*16+5),0xfc};
   require(decode_packed_speeds(raw)==ExpandedSpeeds{std::uint16_t(normal),std::uint16_t(shoulder),std::uint16_t(subject),12});
  }
 constexpr std::array<float,10> normalCurve{.5f,.625f,.75f,.875f,1.f,1.125f,1.25f,1.375f,1.5f,1.625f};
 for(unsigned display=1;display<=10;++display){
  require(mode_factor(Mode::normal,display)==normalCurve[display-1]);
  require(relative_to_default(Mode::normal,display)==normalCurve[display-1]);
  for(auto mode:{Mode::shoulder,Mode::firstPerson}){
   require(mode_factor(mode,display)==float(display));
   require(relative_to_default(mode,display)==float(display)/5.f);
  }
 }
 for(auto mode:{Mode::normal,Mode::shoulder,Mode::firstPerson}){
  require(relative_to_default(mode,5)==1.f);
  require(!mode_factor(mode,0)&&!mode_factor(mode,11)&&!mode_factor(mode,std::numeric_limits<unsigned>::max()));
  require(!relative_to_default(mode,0)&&!relative_to_default(mode,11));
 }
 require(!mode_factor(Mode(255),5)&&!relative_to_default(Mode(255),5));
 const auto legacy=camera::decode("MGO2WIN.CAMERA 1 0 1 0 1 0 1\n");
 require(legacy.has_value()&&legacy->speed==std::array<unsigned,3>{5,5,5});
 require(legacy->rates(false,false)==std::array<float,2>{2,1.5f});
 for(unsigned mode=0;mode<3;++mode)for(unsigned value=1;value<=10;++value){
  camera::Settings settings;settings.speed[mode]=value;settings.reversed[mode*2]=true;
  require(camera::decode(camera::encode(settings))==settings);
  const auto rates=settings.rates(mode==2,mode!=0);
  const float expectedScale=mode==0?normalCurve[value-1]:float(value)/5.f;
  require(rates[0]==2.f*expectedScale&&rates[1]==1.5f*expectedScale);
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
