#include "round_items.h"
#include "stage_profiles.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <set>
#include <tuple>
namespace mgo2win::items {
bool RoundItems::valid()const{
 if(rules.size()>512||replacements.size()>512)return false;std::map<uint8_t,uint32_t> counts;
 for(const auto&r:rules){if(!stage::runtime_stage_supported(r.map)||r.domain>Domain::equipment||!r.item||r.item>65535||!r.count||r.count>4096||(counts[r.map]+=r.count)>4096)return false;
  if(r.position){auto p=*r.position;for(float x:{p.x,p.y,p.z,p.yaw})if(!std::isfinite(x)||std::abs(x)>=1000000)return false;}}
 std::set<std::tuple<uint8_t,GcxSource,uint32_t>> sources;
 for(const auto&r:replacements)if(!stage::runtime_stage_supported(r.map)||r.source>GcxSource::cbox||r.domain>Domain::equipment||!r.item||r.item>65535||!sources.emplace(r.map,r.source,r.sourceOffset).second)return false;
 return true;
}
std::string RoundItems::serialize()const{std::ostringstream out;out<<std::setprecision(9)<<"MGO2WIN_ROUND_ITEMS 2\nenabled "<<enabled<<"\ngcx "<<useGcx<<'\n';for(const auto&r:rules){out<<"item "<<unsigned(r.map)<<' '<<(r.domain==Domain::weapon?"weapon":"equipment")<<' '<<r.item<<' '<<r.count;if(r.position)out<<" at "<<r.position->x<<' '<<r.position->y<<' '<<r.position->z<<' '<<r.position->yaw;else out<<" spawn";out<<'\n';}for(const auto&r:replacements)out<<"replace "<<unsigned(r.map)<<' '<<(r.source==GcxSource::cbox?"cbox":"pickup")<<' '<<r.sourceOffset<<' '<<(r.domain==Domain::weapon?"weapon":"equipment")<<' '<<r.item<<'\n';return out.str();}
bool RoundItems::parse(std::string_view data,std::string& error){try{
 if(data.size()>1048576)throw std::runtime_error("Round item file too large");RoundItems next;std::istringstream in{std::string(data)};std::string line;bool header=false,enabledSeen=false,gcxSeen=false;unsigned version=0;
 while(std::getline(in,line)){if(line.size()>1024)throw std::runtime_error("Round item line too long");std::istringstream row(line);std::string key,extra;if(!(row>>key)||key[0]=='#')continue;
  if(!header){if(key!="MGO2WIN_ROUND_ITEMS"||!(row>>version)||(version!=1&&version!=2)||(row>>extra))throw std::runtime_error("Round item version");next.useGcx=version==2;header=true;continue;}
  if(key=="enabled"){unsigned n=0;if(enabledSeen||!(row>>n)||n>1||(row>>extra))throw std::runtime_error("Round item enabled");next.enabled=n!=0;enabledSeen=true;}
  else if(key=="gcx"){unsigned n=0;if(version!=2||gcxSeen||!(row>>n)||n>1||(row>>extra))throw std::runtime_error("Round item GCX switch");next.useGcx=n!=0;gcxSeen=true;}
  else if(key=="replace"){GcxReplacement r;unsigned map=0;std::string source,domain;if(version!=2||!(row>>map>>source>>r.sourceOffset>>domain>>r.item)||map>255||next.replacements.size()>=512||(row>>extra))throw std::runtime_error("Round item GCX replacement");r.map=uint8_t(map);if(source=="pickup")r.source=GcxSource::pickup;else if(source=="cbox")r.source=GcxSource::cbox;else throw std::runtime_error("Round item GCX source");if(domain=="weapon")r.domain=Domain::weapon;else if(domain=="equipment")r.domain=Domain::equipment;else throw std::runtime_error("Round item replacement domain");next.replacements.push_back(r);}
  else if(key=="item"){SpawnRule r;unsigned map=0;std::string domain,mode;if(!(row>>map>>domain>>r.item>>r.count>>mode)||map>255||next.rules.size()>=512)throw std::runtime_error("Round item row");r.map=uint8_t(map);if(domain=="weapon")r.domain=Domain::weapon;else if(domain=="equipment")r.domain=Domain::equipment;else throw std::runtime_error("Round item domain");if(mode=="at"){Position p;if(!(row>>p.x>>p.y>>p.z>>p.yaw))throw std::runtime_error("Round item coordinates");r.position=p;}else if(mode!="spawn")throw std::runtime_error("Round item location");if(row>>extra)throw std::runtime_error("Round item extra field");next.rules.push_back(r);}
  else throw std::runtime_error("Unknown round item field");
 }
 if(!header||!enabledSeen||(version==2&&!gcxSeen)||!next.valid())throw std::runtime_error("Invalid round item settings");*this=std::move(next);error.clear();return true;
 }catch(const std::exception&e){error=e.what();return false;}}
bool RoundItems::load(const std::filesystem::path&p,std::string&error){try{std::ifstream in(p,std::ios::binary|std::ios::ate);auto n=in.tellg();if(n<0||n>1048576)throw std::runtime_error("Round item file extent");std::string bytes(size_t(n),'\0');in.seekg(0);if(!in.read(bytes.data(),n))throw std::runtime_error("Round item file read");return parse(bytes,error);}catch(const std::exception&e){error=e.what();return false;}}
std::vector<Seed> resolve_round_items(const RoundItems& settings,uint8_t map,const stage::Collision& collision,std::span<const stage::Vec3> anchors,const ItemTemplate& lookup){
 if(!settings.valid()||!lookup)throw std::invalid_argument("Round item resolution");std::vector<Seed> result;if(!settings.enabled)return result;size_t ordinal=0;
 for(const auto&r:settings.rules)if(r.map==map){auto contents=lookup(r.domain,r.item);if(!contents)throw std::runtime_error("Unknown configured item");
  if(contents->domain!=r.domain||contents->item!=r.item||contents->quantity!=1)throw std::runtime_error("Configured item template identity");
  for(uint32_t i=0;i<r.count;++i){std::optional<Position> selected;
   for(unsigned attempt=0;attempt<128;++attempt){stage::Vec3 base{};float yaw=0;
    if(r.position){base={r.position->x,r.position->y,r.position->z};yaw=r.position->yaw;}else{if(anchors.empty())throw std::runtime_error("No original spawn anchors for items");base=anchors[(ordinal+attempt)%anchors.size()];}
    const auto k=r.position?i+attempt:ordinal/anchors.size()+attempt/anchors.size();if(k){const float angle=float(k)*2.39996323f;const float radius=400.f*std::sqrt(float(k));base[0]+=std::sin(angle)*radius;base[2]+=std::cos(angle)*radius;}
    auto from=base;from[1]+=1200;auto hit=collision.ray(from,{0,-1,0},3200);if(!hit||hit->normal[1]<.707f)continue;auto p=hit->position;p[1]+=12;
    if(!collision.clear(p,{25,60,2}))continue;if(std::any_of(result.begin(),result.end(),[&](const auto&s){return std::hypot(s.position.x-p[0],s.position.z-p[2])<150&&std::abs(s.position.y-p[1])<200;}))continue;
    selected=Position{p[0],p[1],p[2],yaw};break;
   }
   if(!selected)throw std::runtime_error("Configured item has no safe floor");result.push_back({*contents,*selected});++ordinal;
  }
 }
 return result;
}
}
