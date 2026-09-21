#include "product_identity.h"
#include "combat_spawn.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace mgo2mt::combat::spawn {
namespace {
size_t offset(Variant v,Kind k,uint8_t team,uint8_t index){
 if(unsigned(v)>1||unsigned(k)>1||team>1||index>15)throw std::out_of_range("TDM spawn index");
 return (((size_t(v)*2+size_t(k))*2+team)*16+index);
}
bool finite(Vec3 p){return std::all_of(p.begin(),p.end(),[](float v){return std::isfinite(v)&&std::abs(v)<1000000;});}
void require(bool value,const char*message){if(!value)throw std::runtime_error(message);}
bool body(const Pose&p){return finite(p.feet)&&std::isfinite(p.yaw)&&std::abs(p.yaw)<=3.14159274f&&std::isfinite(p.pitch)&&std::abs(p.pitch)<=1.4f&&(p.capsule.radius==260||p.capsule.radius==350)&&p.capsule.skin==2&&p.capsule.height>=2*p.capsule.radius&&(p.capsule.height==1700||p.capsule.height==1100||p.capsule.height==560);}
bool overlaps(const Pose&a,const Pose&b){
 float alo=a.feet[1]+a.capsule.radius,ahi=a.feet[1]+a.capsule.height-a.capsule.radius;
 float blo=b.feet[1]+b.capsule.radius,bhi=b.feet[1]+b.capsule.height-b.capsule.radius;
 float gap=std::max({0.f,alo-bhi,blo-ahi});float x=a.feet[0]-b.feet[0],z=a.feet[2]-b.feet[2];
 float radius=a.capsule.radius+b.capsule.radius;
 return x*x+z*z+gap*gap<=radius*radius;
}
}
Profile Profile::read(std::istream&in){
 Profile p;std::array<bool,128> seen{};unsigned rows=0;bool magic=false,stage=false,map=false,rule=false;
 std::string line;size_t extent=0;
 while(std::getline(in,line)){
  extent+=line.size()+1;require(line.size()<=1024&&extent<=65536,"TDM spawn extent");
  if(auto at=line.find('#');at!=std::string::npos)line.resize(at);
  std::istringstream row(line);std::string key;if(!(row>>key))continue;
  if(!magic){unsigned version;require(key==mgo2mt::brand::Format{"MGO2MT_TDM_SPAWNS"}&&bool(row>>version)&&version==1,"TDM spawn header");magic=true;}
  else if(key=="stage"){std::string name;require(!stage&&bool(row>>name)&&name=="n022a","TDM spawn stage");stage=true;}
  else if(key=="map"){int value;require(!map&&bool(row>>value)&&value==20,"TDM spawn map");map=true;}
  else if(key=="rule"){int value;require(!rule&&bool(row>>value)&&value==1,"TDM spawn rule");rule=true;}
  else if(key=="spawn"){
   Entry e;std::string variant,kind,hash;int team,index,yaw;
   require(bool(row>>variant>>kind>>team>>index>>hash>>e.position[0]>>e.position[1]>>e.position[2]>>yaw),"TDM spawn row");
   require((variant=="normal"||variant=="mini")&&(kind=="initial"||kind=="respawn")&&team>=0&&team<2&&index>=0&&index<16&&yaw>=-32768&&yaw<=32767&&finite(e.position),"TDM spawn row value");
   require(hash.size()==8&&hash.starts_with("0x")&&std::all_of(hash.begin()+2,hash.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');}),"TDM spawn hash");
   e.hash=uint32_t(std::stoul(hash,nullptr,16));require(e.hash!=0,"TDM spawn zero hash");
   e.variant=variant=="normal"?Variant::normal:Variant::mini;e.kind=kind=="initial"?Kind::initial:Kind::respawn;
   e.team=uint8_t(team);e.index=uint8_t(index);e.yaw=int16_t(yaw);
   auto at=offset(e.variant,e.kind,e.team,e.index);require(!seen[at],"TDM spawn duplicate index");seen[at]=true;p.entries_[at]=e;++rows;
  }else throw std::runtime_error("TDM spawn unknown field");
  std::string tail;require(!(row>>tail),"TDM spawn trailing field");
 }
 require(!in.bad()&&magic&&stage&&map&&rule&&rows==128,"TDM spawn incomplete profile");
 // Exact mini arrays repeat eight source hashes/coordinates; a partial table
 // must not silently become an eight-entry RNG distribution.
 for(auto k:{Kind::initial,Kind::respawn})for(uint8_t t=0;t<2;++t)for(uint8_t i=0;i<8;++i){
  auto a=p.at(Variant::mini,k,t,i),b=p.at(Variant::mini,k,t,i+8);b.index=a.index;
  require(a==b,"TDM spawn mini repetition");
 }
 return p;
}
const Entry&Profile::at(Variant v,Kind k,uint8_t t,uint8_t i)const{return entries_[offset(v,k,t,i)];}
bool Random::valid()const{return std::any_of(words.begin(),words.end(),[](uint64_t v){return v!=0;});}
uint64_t Random::next(){auto temp=words[0]^(words[0]<<11);auto last=words[3];words[0]=words[1];words[1]=words[2];words[2]=last;return words[3]=(temp>>8)^temp^last^(last>>19);}
Random Random::cold_start(uint8_t localMemberId,uint8_t cacheByte161,std::array<uint8_t,3> counts){Random r{{0,0x159a55e5,0x1f123bb5,0x05491333}};r.stage_setup(localMemberId,cacheByte161,counts);return r;}
void Random::stage_setup(uint8_t localMemberId,uint8_t cacheByte161,std::array<uint8_t,3> counts){
 if(std::any_of(counts.begin(),counts.end(),[](auto c){return c>48;}))throw std::invalid_argument("Spawn count");
 if(!valid())throw std::invalid_argument("TDM spawn RNG setup without prior state");
 words[0]=10351ull*localMemberId+93467ull*cacheByte161+123456789ull;
 // 759558 iterates groups 0, 1 AND 2 (cmpwi r27,2 at 759A7C).
 // n022a TDM includes 16+16+32 entries; collision probing draws no extra RNG.
 for(auto count:counts)for(unsigned i=0;i<count;++i)next();
}
Selector::Selector(Profile profile,Context context,Random random):profile_(std::move(profile)),context_(context),random_(random){
 if(!context.epoch||context.map!=20||context.rule!=1||!context.participantCapacity||context.participantCapacity>16||!random.valid())throw std::invalid_argument("TDM spawn context/RNG");
 // A default/unloaded Profile cannot pass as reviewed data.
 for(auto&e:profile_.entries())if(!e.hash)throw std::invalid_argument("TDM spawn profile absent");
}
std::optional<Proposal> Selector::propose(Kind kind,uint8_t rawTeam)const{
 if(unsigned(kind)>1||rawTeam>1||revision_==std::numeric_limits<uint64_t>::max())return {};
 Proposal p;p.epoch=context_.epoch;p.revision=revision_;p.kind=kind;p.rawTeam=rawTeam;p.normalizedTeam=rawTeam^uint8_t(context_.teamSwap);
 p.variant=context_.participantCapacity<=8?Variant::mini:Variant::normal;p.randomAfter=random_;
 p.arrayIndex=kind==Kind::initial?uint8_t(initial_[p.normalizedTeam]%16):uint8_t(uint32_t(p.randomAfter.next())%16);
 const auto&e=profile_.at(p.variant,p.kind,p.normalizedTeam,p.arrayIndex);p.hash=e.hash;p.sourcePosition=e.position;p.creationPose.feet=e.position;
 if(kind==Kind::initial)p.creationPose.feet[1]+=800.f;
 p.creationPose.yaw=float(e.yaw)*(6.2831853071795864769f/65536.f);
 return p;
}
bool Selector::commit(const Proposal&p){
 auto next=propose(p.kind,p.rawTeam);if(!next||*next!=p)return false;
 if(p.kind==Kind::initial)++initial_[p.normalizedTeam];else random_=p.randomAfter;
 ++revision_;return true;
}
std::optional<uint8_t> raw_team(uint8_t nativeTeam){if(nativeTeam==1||nativeTeam==2)return uint8_t(nativeTeam-1);return {};}
Placement place(const Proposal&p,const stage::Collision&world,std::span<const Pose> occupied,float maximumDrop){
 Placement r;r.pose=p.creationPose;
 if(!p.epoch||!p.revision||!p.hash||!body(r.pose)||!std::isfinite(maximumDrop)||maximumDrop<=0||maximumDrop>30000||occupied.size()>24||std::any_of(occupied.begin(),occupied.end(),[](const auto&q){return !body(q);}))return r;
 if(!world.clear(r.pose.feet,r.pose.capsule)){
  // Native standing capsule is taller than the original initial control.
  // Resolve an observed ceiling only by vertical descent within the original
  // 800-unit initial lift. Every contact must face down; walls stay blocking.
  auto a=r.pose.feet,b=a;a[1]+=r.pose.capsule.radius;b[1]+=r.pose.capsule.height-r.pose.capsule.radius;
  auto contacts=world.contacts(a,b,r.pose.capsule.radius,r.pose.capsule.skin,64);float correction=0;
  if(p.kind!=Kind::initial||contacts.empty()||contacts.size()>=64){r.reject=PlacementReject::obstructed;return r;}
  for(const auto&c:contacts){
   if(c.normal[1]>-.70710678f||c.point[1]<a[1]||!std::isfinite(c.penetration)||c.penetration<0){r.reject=PlacementReject::obstructed;return r;}
   float separation=0;for(unsigned axis=0;axis<3;++axis)separation+=(b[axis]-c.point[axis])*c.normal[axis];
   correction=std::max(correction,(r.pose.capsule.radius+2*r.pose.capsule.skin-separation)/-c.normal[1]);
  }
  if(correction>800||correction>=maximumDrop){r.reject=PlacementReject::obstructed;return r;}
  r.pose.feet[1]-=correction;
  if(!world.clear(r.pose.feet,r.pose.capsule)){r.reject=PlacementReject::obstructed;return r;}
  r.ceilingCorrection=correction;
 }
 auto hit=world.sweep(r.pose.feet,{0,-(maximumDrop-r.ceilingCorrection),0},r.pose.capsule);
 if(!hit){r.reject=PlacementReject::no_floor;return r;}
 if(hit->normal[1]<.70710678f){r.reject=PlacementReject::steep;return r;}
 auto fall=hit->fraction*(maximumDrop-r.ceilingCorrection);r.drop=fall+r.ceilingCorrection;r.pose.feet[1]-=fall;
 if(!world.clear(r.pose.feet,r.pose.capsule)){r.reject=PlacementReject::obstructed;return r;}
 if(std::any_of(occupied.begin(),occupied.end(),[&](const auto&q){return overlaps(r.pose,q);})){r.reject=PlacementReject::occupied;return r;}
 r.reject=PlacementReject::none;return r;
}
}
