#include "host_rules.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2win::host;
void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
int main(){try{
 RoundRules s({1000,1,true});ParticipantToken host{0,1,100},a{1,1,101},b{2,1,102},watch{3,1,103};
 check(s.join(host,ParticipantRole::dedicated_host,0)&&!s.deadline()&&s.player_count()==0,"dedicated host never starts the countdown");
 check(s.advance(5000)==StartReason::none,"empty dedicated room never starts");
 check(s.join(a,ParticipantRole::player,100)&&s.deadline()==1100,"first player starts countdown");
 check(s.join(watch,ParticipantRole::spectator,200)&&s.player_count()==1,"spectator excluded");
 check(!s.set_ready(a,s.generation(),true),"START cannot assert incomplete resources are ready");
 check(s.set_prepared(a,s.generation(),true)&&s.set_ready(a,s.generation(),true),"prepared player can press START");
 check(s.join(a,ParticipantRole::player,900)&&s.participants()[1]->ready&&s.deadline()==1100,"duplicate join preserves ready and deadline");
 check(s.set_ready(a,s.generation(),false)&&s.advance(1099)==StartReason::none,"START can be cancelled before countdown");
 check(s.advance(1100)==StartReason::countdown&&s.preparation_started()&&s.phase()==RoundPhase::preparing,"countdown commits preparation once");
 check(s.advance(9999)==StartReason::none&&!s.set_ready(a,s.generation(),false),"start committed once");
 auto old=s.generation();s.reset(2000);check(s.generation()!=old&&!s.preparation_started()&&!s.participants()[1]->ready&&!s.participants()[1]->prepared,"new round clears readiness");
 check(!s.set_prepared(a,old,true)&&!s.set_ready(a,old,true),"stale round events rejected");
 check(s.join(b,ParticipantRole::player,2100),"second player joins");
 check(s.set_prepared(a,s.generation(),true)&&s.set_ready(a,s.generation(),true),"first player ready");
 check(s.advance(99999)==StartReason::none,"timer cannot bypass another player's unfinished initial state");
 check(s.set_prepared(b,s.generation(),true)&&s.set_ready(b,s.generation(),true),"second player ready");
 check(s.advance(2500)==StartReason::all_ready,"all active players START before countdown; host and spectator excluded");
 s.reset(3000);check(s.set_prepared(a,s.generation(),true)&&s.set_ready(a,s.generation(),true)&&s.set_prepared(a,s.generation(),false)&&!s.participants()[1]->ready,"invalidating resources cancels ready");
 check(!s.leave({1,2,101})&&!s.set_prepared({1,2,101},s.generation(),true),"stale occupant cannot alter live slot");
 check(!s.join({1,2,104},ParticipantRole::player,3000),"occupied slot cannot silently replace player");
 check(!s.join({4,1,101},ParticipantRole::player,3000),"duplicate character cannot count twice");
 check(s.leave(a)&&s.leave(b)&&!s.deadline(),"last gameplay player leaving disarms countdown");
 ParticipantToken newer{1,2,104};check(s.join(newer,ParticipantRole::player,5000)&&s.deadline()==6000&&!s.set_ready(a,s.generation(),true),"reused slot rejects delayed previous occupant");
 check(!s.join({24,1,105},ParticipantRole::player,0)&&!s.join({4,1,0},ParticipantRole::player,0),"invalid identity rejected");
 RoundRules minimum({0,2,false});minimum.join(a,ParticipantRole::player,0);minimum.set_prepared(a,1,true);minimum.set_ready(a,1,true);check(minimum.advance(1)==StartReason::none,"minimum players respected");
 minimum.join(b,ParticipantRole::player,0);minimum.set_prepared(b,1,true);check(minimum.advance(1)==StartReason::countdown,"zero countdown starts with enough prepared players");
 RoundRules bound({1000,1,false});bound.join(a,ParticipantRole::player,std::numeric_limits<uint64_t>::max()-2);check(bound.deadline()==std::numeric_limits<uint64_t>::max(),"deadline addition cannot wrap");
 WeaponOption basic{1,0,true,true},paid{2,500,false,true},restricted{3,0,true,false};
 check(weapon_access(basic,false,0)==WeaponAccess::allowed&&weapon_access(paid,false,99999)==WeaponAccess::dp_disabled,"DP disabled retains catalog's basic weapon range");
 check(weapon_access(paid,true,499)==WeaponAccess::insufficient_dp&&weapon_access(paid,true,500)==WeaponAccess::allowed,"DP exact budget unlocks weapon");
 check(weapon_access(restricted,true,99999)==WeaponAccess::restricted,"DP never overrides weapon restriction");
 auto quote=quote_loadout(std::array{paid,WeaponOption{4,600,false,true}},true,1000);check(quote.access==WeaponAccess::insufficient_dp&&quote.cost==1100,"combined loadout cannot overspend individually affordable choices");
 check(quote_loadout(std::array{basic,paid},true,500).access==WeaponAccess::allowed,"exact combined budget");
 check(quote_loadout(std::array{WeaponOption{5,10,true,true}},false,0).cost==0,"DP disabled never charges");
 std::array<uint16_t,7> ids{10,11,12,13,14,15,10};SpecialPolicy specials(ids);
 check(specials.choices().size()==6&&!specials.allows(999),"catalog defines full supported special range");
 for(unsigned player=0;player<24;++player)for(auto id:ids)check(specials.allows(id),"every player may concurrently select the same or any special");
 std::cout<<"host readiness, countdown, DP budget and unrestricted special policy passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
