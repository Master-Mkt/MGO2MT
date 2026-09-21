#include "breakable_light_settings.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace mgo2mt::combat;
int main(){try{
 auto check=[](bool v){if(!v)throw std::runtime_error("Breakable light settings boundary");};
 for(unsigned n:{1u,3u,10000u}){std::istringstream in("# native setting\nMGO2MT.BREAKABLE_LIGHTS 1\nlight_hits "+std::to_string(n)+" # impacts\n");check(read_breakable_lights(in).lightHits==n);}
 for(auto value:{"0","10001","65537","-1","nan","1.5","1 trailing"}){std::istringstream in(std::string("MGO2MT.BREAKABLE_LIGHTS 1\nlight_hits ")+value);bool rejected=false;try{read_breakable_lights(in);}catch(...){rejected=true;}check(rejected);}
 for(auto content:{"","MGO2MT.BREAKABLE_LIGHTS 1","MGO2MT.BREAKABLE_LIGHTS 2\nlight_hits 1","MGO2MT.BREAKABLE_LIGHTS 1 extra\nlight_hits 1","MGO2MT.BREAKABLE_LIGHTS 1\nlight_hits 1\nlight_hits 2","MGO2MT.BREAKABLE_LIGHTS 1\nunknown 1"}){std::istringstream in(content);bool rejected=false;try{read_breakable_lights(in);}catch(...){rejected=true;}check(rejected);}
 std::istringstream huge("MGO2MT.BREAKABLE_LIGHTS 1\nlight_hits 1\n#"+std::string(4096,'x'));bool rejected=false;try{read_breakable_lights(huge);}catch(...){rejected=true;}check(rejected);
 std::cout<<"BB native HOST durability strict configuration PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
