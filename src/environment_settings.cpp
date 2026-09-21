#include "product_identity.h"
#include "environment_settings.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif
namespace mgo2mt::environment {
Config read(std::istream&in){
 std::string magic;unsigned version;Config c;unsigned flags,time,sound,upper[3],lower[3],gain;
 if(!(in>>magic>>version>>flags>>time>>sound>>upper[0]>>upper[1]>>upper[2]>>lower[0]>>lower[1]>>lower[2]>>gain)||magic!=mgo2mt::brand::Format{"MGO2MT_ENVIRONMENT"}||version!=1||flags>63||time>5||sound>5||gain>4000)throw std::runtime_error("Invalid HOST environment settings");
 for(unsigned i=0;i<3;++i){if(upper[i]>255||lower[i]>255)throw std::runtime_error("Invalid hemisphere color");c.upper[i]=uint8_t(upper[i]);c.lower[i]=uint8_t(lower[i]);}
 c.weatherOverride=flags&1;c.fog=flags&2;c.rain=flags&4;c.snow=flags&8;c.sandstorm=flags&16;c.manualHemisphere=flags&32;c.time=Time(time);c.sound=Sound(sound);c.gainMilli=uint16_t(gain);std::string tail;if(in>>tail)throw std::runtime_error("Trailing HOST environment setting");return c;
}
Config load(const std::filesystem::path&p){if(!std::filesystem::exists(p))return {};if(std::filesystem::file_size(p)>4096)throw std::runtime_error("HOST environment settings extent");std::ifstream in(p);if(!in)throw std::runtime_error("HOST environment settings read");return read(in);}
void save(const std::filesystem::path&p,const Config&c){
 if(!valid(c))throw std::invalid_argument("HOST environment settings range");if(!p.parent_path().empty())std::filesystem::create_directories(p.parent_path());auto tmp=p;tmp+=L".tmp";
 {std::ofstream out(tmp,std::ios::trunc);const unsigned flags=(c.weatherOverride?1:0)|(c.fog?2:0)|(c.rain?4:0)|(c.snow?8:0)|(c.sandstorm?16:0)|(c.manualHemisphere?32:0);
  out<<"MGO2MT_ENVIRONMENT 1\n"<<flags<<' '<<unsigned(c.time)<<' '<<unsigned(c.sound);for(auto v:c.upper)out<<' '<<unsigned(v);for(auto v:c.lower)out<<' '<<unsigned(v);out<<' '<<c.gainMilli<<'\n';out.flush();if(!out)throw std::runtime_error("HOST environment settings write");}
 if(load(tmp)!=c)throw std::runtime_error("HOST environment settings roundtrip");
#ifdef _WIN32
 if(!MoveFileExW(tmp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("HOST environment settings replace");
#else
 std::filesystem::rename(tmp,p);
#endif
}
}
