#include "weapon_connection_points.h"
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt;
namespace {
void check(bool v,const char*message){if(!v)throw std::runtime_error(message);}
void word(std::vector<char>&b,uint32_t v){for(unsigned i=0;i<4;++i)b.push_back(char(v>>(i*8)));}
void set(std::vector<char>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=char(v>>(i*8));}
std::vector<char> fixture(){std::vector<char>b{'G','C','P','1'};word(b,1);word(b,2);for(uint32_t key:{0u,weapon_hand::connections::ejection}){word(b,25);word(b,key);b.insert(b.end(),32,char(0x11));for(float v:{10.f,20.f,30.f,10.f,20.f,130.f})word(b,std::bit_cast<uint32_t>(v));}return b;}
}
int main(){try{
 weapon_hand::connections::Table points;std::string error;auto bytes=fixture();check(points.decode(bytes,error)&&error.empty()&&points.rows().size()==2,"load valid synthetic rows");check(points.find(25,0)&&points.find(25,weapon_hand::connections::ejection)&&!points.find(26,0),"lookup role and identity");
 CharacterModel model;model.parts.resize(2);for(auto&p:model.parts){p.original.present=true;p.original.mdnSha256=std::string(64,'1');}
 auto line=first_person_sight::local_axis(25,model,points);check(line&&line->rear==std::array<float,3>{10,20,30}&&line->front==std::array<float,3>{10,20,130},"resource drives sight pair");model.parts[1].original.mdnSha256=std::string(64,'2');check(!first_person_sight::local_axis(25,model,points),"one mismatched model part refuses all sight data");model.parts.clear();check(!first_person_sight::local_axis(25,model,points),"empty model does not accept source identity");
 unsigned rejected=0;auto reject=[&](std::vector<char>b){check(points.decode(bytes,error),"reset before rejection");check(!points.decode(b,error)&&!error.empty()&&points.rows().empty()&&!points.find(25,0),"bad resource fails closed without stale points");++rejected;};
 auto bad=bytes;bad[0]='X';reject(bad);bad=bytes;set(bad,4,2);reject(bad);bad=bytes;set(bad,8,4097);reject(bad);bad=bytes;bad.pop_back();reject(bad);bad=bytes;bad.push_back(0);reject(bad);bad=bytes;set(bad,12,0);reject(bad);bad=bytes;set(bad,12,512);reject(bad);bad=bytes;set(bad,16,0x1000000);reject(bad);bad=bytes;set(bad,12+64+4,0);reject(bad);bad=bytes;for(size_t i=20;i<52;++i)bad[i]=0;reject(bad);bad=bytes;bad[20+64]=0x12;reject(bad);bad=bytes;set(bad,52,std::bit_cast<uint32_t>(std::numeric_limits<float>::quiet_NaN()));reject(bad);bad=bytes;set(bad,52,std::bit_cast<uint32_t>(1000001.f));reject(bad);bad=bytes;for(unsigned i=0;i<12;++i)bad[64+i]=bad[52+i];reject(bad);
 check(points.decode(bytes,error),"restore after invalid resource");check(!points.load("missing-connection-points-do-not-create.gwcp",error)&&points.rows().empty(),"missing resource does not preserve old table");
 std::cout<<"PASS synthetic external CNP/sight identity, "<<rejected<<" malformed cases and missing resource fail-closed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
