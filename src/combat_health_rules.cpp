#include "combat_health_rules.h"
#include <fstream>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
namespace mgo2win::combat {
HealthRules HealthRules::read(std::istream& source){
 HealthRules result;std::string line;std::set<std::string> seen;size_t bytes=0;bool header=false;
 auto fail=[](){throw std::runtime_error("Invalid combat_health.cfg");};
 while(std::getline(source,line)){
  bytes+=line.size()+1;if(bytes>4096)fail();line.resize(line.find('#')==std::string::npos?line.size():line.find('#'));
  std::istringstream row(line);row.imbue(std::locale::classic());std::string key,extra;if(!(row>>key))continue;
  if(!header){unsigned version=0;if(key!="MGO2WIN_HEALTH"||!(row>>version)||version!=1||(row>>extra))fail();header=true;continue;}
  if(!seen.insert(key).second)fail();
  if(key=="fall_safe_height"){if(!(row>>result.falling.safeHeight))fail();}
  else if(key=="fall_severe_height"){if(!(row>>result.falling.severeHeight))fail();}
  else if(key=="fall_fatal_height"){if(!(row>>result.falling.fatalHeight))fail();}
  else if(key=="fall_severe_damage_permille"){if(!(row>>result.falling.severePermille))fail();}
  else if(key=="gekko_full_recovery_ms"){if(!(row>>result.regeneration.fullRecoveryMs))fail();}
  else fail();if(row>>extra)fail();
 }
 if(!source.eof()||!header||seen.size()!=5||!result.valid())fail();return result;
}
HealthRules HealthRules::load(const std::filesystem::path& path){
 if(std::filesystem::file_size(path)>4096)throw std::runtime_error("Combat health settings too large");
 std::ifstream in(path);if(!in)throw std::runtime_error("Combat health settings unavailable");return read(in);
}
}
