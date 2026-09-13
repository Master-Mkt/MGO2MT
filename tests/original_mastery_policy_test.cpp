#include "original_mastery_policy.h"
#include "original_reload_timing.h"
#include <iostream>
#include <limits>
#include <stdexcept>

static void check(bool value,const char*name){if(!value)throw std::runtime_error(name);}
int main(){try{
 using namespace mgo2win::original_mastery;
 for(unsigned cls=0;cls<=8;++cls){
  check(skill_for_weapon_class(cls)==(cls>=2&&cls<=6?cls-1:0),"current class switch");
  for(unsigned level=0;level<=3;++level){
   check(reload_rate_for_class(cls,level,0)==(cls>=2&&cls<=6?reload_rates[level]:1.f),"class-specific getter");
   check(reload_rate_for_class(cls,level,0x40)==1.f,"original exception bypasses mastery");
  }
 }
 check(skill_for_weapon_class(ak102_weapon_class)==ak102_mastery_skill,"current AK class maps to rifle skill3");
 check(!ak102_reload_rate(24,3,0)&&!ak102_reload_rate(76,3,0)&&!ak102_reload_rate(0,3,0),"no other weapon fallback");
 check(!ak102_reload_rate(25,4,0)&&!ak102_reload_rate(25,std::numeric_limits<unsigned>::max(),0),"invalid level rejected");
 check(!ak102_reload_rate(25,4,0x40),"invalid level remains invalid under exception");
 check(reload_rates==mgo2win::original::rifle_reload_rates,"shared existing timing uses same original float32 rates");
 const std::array<unsigned,4> refill{2169,1902,1669,1452},end{3487,3037,2687,2336};
 for(unsigned level=0;level<=3;++level){
  for(unsigned flags=0;flags<256;++flags){
   auto rate=ak102_reload_rate(25,level,static_cast<uint8_t>(flags));
   check(rate&&*rate==((flags&0x40)?1.f:reload_rates[level]),"only first-byte bit6 changes rate");
  }
  auto motion=mgo2win::original::ak102_reload;motion.rate=*ak102_reload_rate(25,level,0);
  auto timing=mgo2win::original::reload_timing(motion);
  check(timing&&timing->refillMs==refill[level]&&timing->endMs==end[level],"rate reaches nominal refill/end consumer");
  motion.rate=*ak102_reload_rate(25,level,0x40);
  timing=mgo2win::original::reload_timing(motion);
  check(timing&&timing->refillMs==refill[0]&&timing->endMs==end[0],"exception keeps normal timing");
 }
 std::cout<<"PASS current AK class/skill mapping, exact rates, byte flag exception, invalid inputs and reload timing\n";
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
