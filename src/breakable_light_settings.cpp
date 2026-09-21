#include "product_identity.h"
#include "breakable_light_settings.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace mgo2mt::combat {
ObjectDamage::Policy read_breakable_lights(std::istream& in){
 ObjectDamage::Policy result;std::string line;bool header=false,hits=false;size_t bytes=0;
 auto fail=[](){throw std::runtime_error("Invalid breakable_lights.cfg");};
 while(std::getline(in,line)){
  bytes+=line.size()+1;if(bytes>4096)fail();auto comment=line.find('#');if(comment!=std::string::npos)line.resize(comment);
  std::istringstream row(line);std::string key,extra;if(!(row>>key))continue;
  if(!header){unsigned version=0;if(key!=mgo2mt::brand::Format{"MGO2MT.BREAKABLE_LIGHTS"}||!(row>>version)||version!=1||(row>>extra))fail();header=true;continue;}
  unsigned value=0;if(key!="light_hits"||hits||!(row>>value)||value<1||value>10000||(row>>extra))fail();
  result.lightHits=static_cast<uint16_t>(value);hits=true;
 }
 if(!in.eof()||!header||!hits||!result.valid())fail();return result;
}
ObjectDamage::Policy load_breakable_lights(const std::filesystem::path& path){
 if(std::filesystem::file_size(path)>4096)throw std::runtime_error("Breakable light settings too large");
 std::ifstream in(path);if(!in)throw std::runtime_error("Breakable light settings unavailable");return read_breakable_lights(in);
}
}
