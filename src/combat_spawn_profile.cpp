#include "product_identity.h"
#include "combat_spawn_profile.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
namespace mgo2mt::combat::spawn {
namespace {
size_t group_index(Variant v,Kind k,uint8_t t){if(unsigned(v)>1||unsigned(k)>1||t>2)throw std::invalid_argument("Spawn group");return (unsigned(v)*2+unsigned(k))*3+t;}
void require(bool b){if(!b)throw std::runtime_error("Invalid stage spawn profile");}
}
const std::vector<Entry>& StageProfile::group(Variant v,Kind k,uint8_t t)const{return groups_[group_index(v,k,t)];}
StageProfile StageProfile::read(std::istream& in){
 StageProfile p;std::string magic;unsigned version,map,rule,count;
 require(bool(in>>magic>>version>>p.stage_>>map>>rule>>count)&&magic==mgo2mt::brand::Format{"MGO2MT_COMBAT_SPAWNS"}&&version==1&&map>0&&map<256&&rule<=1&&count>=8&&count<=576);
 require(p.stage_.size()==5&&p.stage_[0]=='n'&&p.stage_[4]=='a'&&std::all_of(p.stage_.begin()+1,p.stage_.begin()+4,[](char c){return c>='0'&&c<='9';}));
 p.map_=uint8_t(map);p.rule_=uint8_t(rule);
 for(unsigned row=0;row<count;++row){Entry e;unsigned variant,kind,team,index;int yaw;std::string key,hash;
  require(bool(in>>key>>variant>>kind>>team>>index>>hash>>e.position[0]>>e.position[1]>>e.position[2]>>yaw)&&key=="spawn"&&variant<2&&kind<2&&team<3&&index<48&&yaw>=-32768&&yaw<=32767);
  require(hash.size()==8&&hash.starts_with("0x")&&std::all_of(hash.begin()+2,hash.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');}));
  e.hash=uint32_t(std::stoul(hash,nullptr,16));require(e.hash&&std::all_of(e.position.begin(),e.position.end(),[](float x){return std::isfinite(x)&&std::abs(x)<1000000;}));
  e.variant=Variant(variant);e.kind=Kind(kind);e.team=uint8_t(team);e.index=uint8_t(index);e.yaw=int16_t(yaw);
  auto& group=p.groups_[group_index(e.variant,e.kind,e.team)];require(index==group.size());group.push_back(e);
 }
 std::string tail;require(!(in>>tail)&&!in.bad());
 for(auto v:{Variant::normal,Variant::mini})for(auto k:{Kind::initial,Kind::respawn})for(uint8_t t=0;t<3;++t)require(!p.group(v,k,t).empty());
 return p;
}
StageSelector::StageSelector(StageProfile profile,Context context,Random random):profile_(std::move(profile)),context_(context),random_(random){
 if(!context.epoch||context.map!=profile_.map()||context.rule!=profile_.rule()||context.rule>1||!context.participantCapacity||context.participantCapacity>16||!random.valid())throw std::invalid_argument("Stage spawn context");
 const auto variant=context.participantCapacity<=8?Variant::mini:Variant::normal;
 for(uint8_t team=0;team<3;++team){const auto count=profile_.group(variant,Kind::initial,team).size();auto& p=permutations_[team];p.assign(count,255);
  for(uint8_t i=0;i<count;++i){auto at=uint32_t(random_.next())%count;while(p[at]!=255)at=(at+1)%count;p[at]=i;}
 }
}
std::optional<Proposal> StageSelector::propose(Kind kind,uint8_t rawTeam)const{
 if(unsigned(kind)>1||rawTeam>1||revision_==std::numeric_limits<uint64_t>::max()||(kind==Kind::initial&&std::any_of(counters_.begin(),counters_.end(),[](auto n){return n==std::numeric_limits<uint64_t>::max();})))return {};
 Proposal p;p.epoch=context_.epoch;p.revision=revision_;p.kind=kind;p.rawTeam=rawTeam;p.normalizedTeam=context_.rule==0?0:rawTeam^uint8_t(context_.teamSwap);
 p.variant=context_.participantCapacity<=8?Variant::mini:Variant::normal;p.randomAfter=random_;
 const auto& group=profile_.group(p.variant,kind,p.normalizedTeam);if(group.empty())return {};
 const auto at=kind==Kind::initial?counters_[p.normalizedTeam]%group.size():uint32_t(p.randomAfter.next())%group.size();
 p.arrayIndex=kind==Kind::initial&&context_.rule==0?permutations_[p.normalizedTeam][at]:uint8_t(at);
 const auto&e=group[p.arrayIndex];p.hash=e.hash;p.sourcePosition=e.position;p.creationPose.feet=e.position;if(kind==Kind::initial)p.creationPose.feet[1]+=800;
 p.creationPose.yaw=float(e.yaw)*(6.2831853071795864769f/65536.f);return p;
}
bool StageSelector::commit(const Proposal&p){auto expected=propose(p.kind,p.rawTeam);if(!expected||*expected!=p)return false;if(p.kind==Kind::initial)++counters_[p.normalizedTeam];else random_=p.randomAfter;++revision_;return true;}
}
