#include "combat_round.h"
#include "combat_service.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
namespace {
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
std::shared_ptr<const stage::Collision> floor(){return std::make_shared<const stage::Collision>(stage::Collision::make({{-20000,0,-20000},{20000,0,-20000},{20000,0,20000},{-20000,0,20000}},{{{0,1,2}},{{0,2,3}}}));}
}
int main(int argc,char**argv){try{
 check(argc==2,"catalog argument");auto catalog=std::make_shared<weapons::Catalog>();std::string error;check(catalog->load(argv[1],error),"reviewed price catalog loads");
 // Synthetic timing/damage solely for lifecycle tests; never shipped as an original profile.
 combat::Weapon ak{25,35,0,100,300,30,90,10000,9001,9002,true},m4=ak;m4.id=24;
 std::array profiles{ak,m4};combat::Authority authority;authority.begin(9,floor(),profiles);
 bool blockSpawn=true;unsigned grants=0;
 auto spawn=[&](combat::Authority&a,combat::Identity id,uint8_t team,std::span<const uint16_t> inventory,uint64_t now){if(blockSpawn)return false;combat::Pose pose;pose.feet={float(id.slot*2000),2,0};if(!a.join(id,team,pose,1000,1000,inventory,now))return false;++grants;return true;};
 combat::RoundCoordinator::Policy policy{1000,1,true,true};policy.roundDurationMs=300000;combat::RoundCoordinator round(9,policy,catalog,profiles,spawn);
 combat::Identity first{1,0x101,200},second{2,0x102,300};check(round.join(first,100)&&round.join(second,200),"two players admitted");
 check(!round.state(first,200).roundClock&&!round.state(first,200).roundRemainingMs,"briefing never reuses the game clock");
 check(round.state(first,200).players[1]->team==1&&round.state(first,200).players[2]->team==2,"native balanced assignment");
 combat::wire::Command command;command.epoch=9;command.sequence=1;command.kind=combat::wire::CommandKind::ready;command.enabled=true;
 check(round.command(first,command,authority,200)&&round.state(first,200).error==combat::wire::CommandError::not_loaded,"START cannot invent a loaded scene");
 auto loaded=[&](combat::Identity id,uint32_t sequence,bool value,uint64_t now){combat::wire::Command c;c.epoch=9;c.sequence=sequence;c.kind=combat::wire::CommandKind::loaded;c.enabled=value;c.generation=1;c.sceneRevision=1;check(round.command(id,c,authority,now),"loaded command");};
 loaded(first,2,true,250);command.sequence=3;check(round.command(first,command,authority,300),"ready first");
 command.sequence=4;command.enabled=false;check(round.command(first,command,authority,350)&&!round.state(first,350).players[1]->ready,"START can be cancelled");
 round.poll(authority,1100);check(round.state(first,1100).phase==combat::wire::RoundPhase::waiting,"timer waits for second client's actual scene completion");
 loaded(second,1,true,1150);round.poll(authority,1150);check(round.take_start()&&!round.take_start()&&round.state(first,1150).phase==combat::wire::RoundPhase::selecting&&!authority.active(),"countdown opens weapon selection once without manufacturing a spawn");
 check(round.state(first,1150).roundClock&&round.state(first,1150).roundRemainingMs==300000,"native game clock starts at weapon selection, before any deployment");
 combat::wire::Command choose;choose.epoch=9;choose.sequence=5;choose.kind=combat::wire::CommandKind::loadout;choose.weapons={24,0,0};
 check(round.command(first,choose,authority,1200)&&round.state(first,1200).error==combat::wire::CommandError::insufficient_dp,"2000 DP weapon rejected against host 1000 balance");
 choose.sequence=6;choose.weapons={25,0,0};check(round.command(first,choose,authority,1250)&&round.state(first,1250).error==combat::wire::CommandError::spawn&&round.state(first,1250).dpBalance==1000&&grants==0,"blocked spawn does not charge DP or grant ammo");
 blockSpawn=false;choose.sequence=7;check(round.command(first,choose,authority,1300)&&round.state(first,1300).dpBalance==0&&grants==1&&authority.active(),"host grant charges once and activates accepted player");
 check(!round.command(first,choose,authority,1400)&&grants==1,"replayed command cannot duplicate grant or charge");
 choose.sequence=8;check(round.command(first,choose,authority,1450)&&round.state(first,1450).error==combat::wire::CommandError::already_deployed&&grants==1,"new request while deployed cannot refill ammunition");
 check(!round.command({1,0x999,200},choose,authority,1500),"forged slot incarnation rejected");
 auto old=choose;old.epoch=8;check(!round.command(first,old,authority,1500),"old round command rejected");
 // A late participant gets current state; it must independently load/select.
 combat::Identity late{3,0x103,400};check(round.join(late,1600),"join an active round");check(!round.state(late,1600).players[3]->loaded&&round.state(late,1600).dpBalance==1000,"late join does not inherit another wallet or preparation");
 // Native game time is tied to one round, not the first player's spawn.
 auto clockRevision=round.state(first,1600).revision;
 for(uint64_t t=1600;t<2150;++t)round.poll(authority,t);
 check(round.state(first,2149).revision==clockRevision&&round.state(first,2149).roundRemainingMs==300000,"subsecond polls cannot flood preparation revisions");
 round.poll(authority,2150);check(round.state(first,2150).roundRemainingMs==299000&&round.state(first,2150).revision==clockRevision+1,"one second tick publishes once");
 round.poll(authority,61150);check(round.state(first,61150).roundRemainingMs==240000&&round.state(late,61150).roundRemainingMs==240000,"one minute elapsed and late join shares original round clock");
 round.poll(authority,2000);check(round.state(first,2000).roundRemainingMs==240000,"clock regression cannot add game time");
 round.poll(authority,301150);check(round.state(first,301150).roundClock&&!round.state(first,301150).roundRemainingMs&&round.state(first,301150).phase==combat::wire::RoundPhase::active,"zero clock does not invent round-end or rotation behavior");
 clockRevision=round.state(first,301150).revision;round.poll(authority,900000);check(round.state(first,900000).revision==clockRevision&&!round.state(first,900000).roundRemainingMs,"expired clock remains zero without periodic empty updates");
 auto state=round.state(first,1600);auto stateBytes=combat::wire::encode(state);check(std::get<combat::wire::Preparation>(combat::wire::decode(stateBytes))==state,"host preparation snapshot round-trip");
 check(std::get<combat::wire::Command>(combat::wire::decode(combat::wire::encode(choose)))==choose,"selection command round-trip");
 for(size_t n=0;n<stateBytes.size();++n){bool rejected=false;try{combat::wire::decode(std::span(stateBytes).first(n));}catch(const combat::wire::Invalid&){rejected=true;}check(rejected,"all truncated preparation records rejected");}
 auto contaminated=choose;contaminated.team=2;bool rejected=false;try{combat::wire::encode(contaminated);}catch(const combat::wire::Invalid&){rejected=true;}check(rejected,"unused command fields cannot smuggle team changes");
 auto invalidClock=state;invalidClock.roundClock=false;invalidClock.roundRemainingMs=1000;rejected=false;try{combat::wire::encode(invalidClock);}catch(const combat::wire::Invalid&){rejected=true;}check(rejected,"unknown clock cannot carry fabricated remaining time");
 invalidClock=state;invalidClock.phase=combat::wire::RoundPhase::waiting;rejected=false;try{combat::wire::encode(invalidClock);}catch(const combat::wire::Invalid&){rejected=true;}check(rejected,"briefing cannot claim active round clock");
 invalidClock=state;invalidClock.roundRemainingMs=1001;rejected=false;try{combat::wire::encode(invalidClock);}catch(const combat::wire::Invalid&){rejected=true;}check(rejected,"subsecond timer payload rejected");
 invalidClock=state;invalidClock.roundRemainingMs=combat::wire::maximumRoundDurationMs+1000;rejected=false;try{combat::wire::encode(invalidClock);}catch(const combat::wire::Invalid&){rejected=true;}check(rejected,"timer payload has a bounded maximum");
 // New round-native contract is negotiated independently from prior v1.
 stateBytes[5]=1;check(!combat::wire::recognized(stateBytes),"previous native version is not interpreted as the new command contract");
 combat::RoundCoordinator missing(10,policy,catalog,{},{});missing.join(first,0);missing.poll(authority,90000);check(!missing.state(first,90000).runtimeReady&&missing.state(first,90000).phase==combat::wire::RoundPhase::waiting,"missing original profile never starts gameplay");
 combat::RoundCoordinator manual(11,{1000,1,false,false},catalog,profiles,spawn);manual.join(first,0);check(!manual.state(first,0).roundClock,"default clock policy remains unknown");check(manual.state(first,0).players[1]->team==0,"manual room does not auto-assign");
 combat::wire::Command team;team.epoch=11;team.sequence=1;team.kind=combat::wire::CommandKind::team;team.team=2;check(manual.command(first,team,authority,10)&&manual.state(first,10).players[1]->team==2,"explicit team command");
 // All-ready path before deadline and room restrictions are checked separately.
 auto restrictPolicy=policy;restrictPolicy.restrictions[0]=1;restrictPolicy.restrictions[25/8]|=1u<<(25%8);
 combat::Authority restrictedAuthority;restrictedAuthority.begin(12,floor(),profiles);combat::RoundCoordinator restricted(12,restrictPolicy,catalog,profiles,spawn);restricted.join(first,0);
 combat::wire::Command c;c.epoch=12;c.sequence=1;c.kind=combat::wire::CommandKind::loaded;c.enabled=true;c.generation=1;c.sceneRevision=1;restricted.command(first,c,restrictedAuthority,0);
 c={};c.epoch=12;c.sequence=2;c.kind=combat::wire::CommandKind::ready;c.enabled=true;restricted.command(first,c,restrictedAuthority,1);restricted.poll(restrictedAuthority,1);check(restricted.take_start(),"all-ready commits before countdown");
 c={};c.epoch=12;c.sequence=3;c.kind=combat::wire::CommandKind::loadout;c.weapons={25,0,0};restricted.command(first,c,restrictedAuthority,2);check(restricted.state(first,2).error==combat::wire::CommandError::restricted,"restricted AK cannot be granted");
 combat::RoundCoordinator freshClock(13,policy,catalog,profiles,spawn);freshClock.join(first,0);check(!freshClock.state(first,0).roundClock,"new epoch does not inherit prior expired clock");
 c={};c.epoch=13;c.sequence=1;c.kind=combat::wire::CommandKind::loaded;c.enabled=true;c.generation=1;c.sceneRevision=1;freshClock.command(first,c,authority,0);c={};c.epoch=13;c.sequence=2;c.kind=combat::wire::CommandKind::ready;c.enabled=true;freshClock.command(first,c,authority,1);freshClock.poll(authority,1);check(freshClock.state(first,1).roundRemainingMs==300000,"new epoch starts its own full clock");
 auto excessive=policy;excessive.roundDurationMs=combat::wire::maximumRoundDurationMs+1;rejected=false;try{combat::RoundCoordinator badClock(14,excessive,catalog,profiles,spawn);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"native clock policy duration bounded");
 std::cout<<"round: scene readiness, reversible START, countdown/all-ready, host DP/restrictions, transactional spawn, duplicate/replay rejection and late join passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
