#pragma once
#include "combat_authority.h"
#include <optional>
namespace mgo2mt::mounted {
// Presentation clock only: flight position and lifetime belong to the HOST.
struct FlightFrame {uint16_t instance=0;double seconds=0;bool landing=false,blast=false;};
class FlightPresentation {
 combat::Identity identity_{};uint64_t epoch_=0,observed_=0,landed_=0,lastUpdate_=0;uint32_t life_=0;
 uint16_t flight_=0,last_=0,elapsed_=0;stage::Vec3 landingFeet_{};bool blast_=false;
public:
 void clear(){*this={};}
 std::optional<FlightFrame> update(uint64_t epoch,const combat::Player* p,uint64_t now){
  if(!p||!epoch||!p->alive||(p->stunned&&!p->blastFlight)){clear();return {};}
  if(epoch_!=epoch||identity_!=p->identity||life_!=p->life||now<lastUpdate_){clear();epoch_=epoch;identity_=p->identity;life_=p->life;}
  lastUpdate_=now;
  if(p->flightId){
   if(flight_!=p->flightId||blast_!=p->blastFlight||elapsed_!=p->flightElapsedMs){observed_=now;elapsed_=p->flightElapsedMs;}
   blast_=p->blastFlight;
   flight_=last_=p->flightId;landed_=0;
   return FlightFrame{flight_,(double(elapsed_)+double(std::min<uint64_t>(now-observed_,250)))/1000.,false,blast_};
  }
  if(flight_){landed_=now;landingFeet_=p->pose.feet;flight_=0;}
  if(landed_&&now-landed_<650&&!p->mountedId&&!p->reloadUntil&&!p->aiming&&p->evadeKind==combat::EvadeKind::none&&p->specialPhase==combat::SpecialPhase::none){
   float moved=0;for(unsigned i=0;i<3;++i)moved+=std::abs(p->pose.feet[i]-landingFeet_[i]);
   if(moved<100)return FlightFrame{last_,double(now-landed_)/1000.,true,blast_};
  }
  landed_=0;return {};
 }
};
class FlightPosition {
 combat::Identity identity_{};uint64_t epoch_=0,at_=0,last_=0;uint32_t life_=0;uint16_t flight_=0;bool blast_=false;
 stage::Vec3 from_{},target_{};
 stage::Vec3 sample(uint64_t now)const{auto p=from_;const float t=std::clamp(float(now-at_)/50.f,0.f,1.f);for(unsigned i=0;i<3;++i)p[i]+=(target_[i]-from_[i])*t;return p;}
public:
 stage::Vec3 update(uint64_t epoch,const combat::Player&p,uint64_t now){
  if(epoch_!=epoch||identity_!=p.identity||life_!=p.life||flight_!=p.flightId||blast_!=p.blastFlight||!p.flightId||now<last_){epoch_=epoch;identity_=p.identity;life_=p.life;flight_=p.flightId;blast_=p.blastFlight;from_=target_=p.pose.feet;at_=now;}
  auto visible=sample(now);last_=now;
  if(target_!=p.pose.feet){from_=visible;target_=p.pose.feet;at_=now;float jump=0;for(unsigned i=0;i<3;++i)jump+=std::abs(target_[i]-from_[i]);if(jump>5000)from_=target_;}
  return sample(now);
 }
};
}
