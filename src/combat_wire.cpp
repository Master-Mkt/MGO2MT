#include "combat_wire.h"
#include "combat_sop_view.h"
#include <bit>
#include <cmath>
#include <algorithm>
#include <set>
namespace mgo2win::combat::wire {
namespace {
constexpr std::array<uint8_t,6> magic{opcode,'G','W','C','B',version};
void require(bool b){if(!b)throw Invalid();}
struct IO {
 std::vector<uint8_t> out;std::span<const uint8_t> in;size_t at=0;bool writing=false;
 uint64_t u(uint64_t n,unsigned count){if(writing){for(unsigned i=0;i<count;++i)out.push_back(uint8_t(n>>(i*8)));return n;}require(at<=in.size()&&count<=in.size()-at);n=0;for(unsigned i=0;i<count;++i)n|=uint64_t(in[at++])<<(i*8);return n;}
 template<class T> void value(T&v,unsigned n){v=T(u(uint64_t(v),n));}
 void f(float&v){v=std::bit_cast<float>(uint32_t(u(std::bit_cast<uint32_t>(v),4)));require(std::isfinite(v)&&std::abs(v)<1000000);}
 void vec(Vec3&v){for(auto&x:v)f(x);}
 void identity(Identity&id,bool optional=false){value(id.slot,1);value(id.instance,2);value(id.character,4);require((id.slot<24&&id.instance&&id.character)||(optional&&id==Identity{}));}
 void pose(Pose&p){vec(p.feet);f(p.yaw);f(p.pitch);auto radius=uint16_t(p.capsule.radius);value(radius,2);require(radius==260||radius==350);auto height=uint16_t(p.capsule.height);value(height,2);require(height>=radius*2&&(height==1700||height==1100||height==560)&&std::abs(p.yaw)<=3.14159274f&&std::abs(p.pitch)<=1.4f);p.capsule={float(radius),float(height),2};}
 void player(Player&p){if(writing)require(valid_special(p));identity(p.identity);value(p.team,1);pose(p.pose);value(p.hp,4);value(p.maxHp,4);value(p.stamina,4);value(p.maxStamina,4);value(p.weapon,2);value(p.ammo,2);value(p.reserve,2);value(p.reloadUntil,8);uint8_t flags=(p.alive?1:0)|(p.stunned?2:0)|(uint8_t(p.specialPhase)<<2);value(flags,1);require(flags<=15&&p.team<=2&&(p.weapon||(!p.ammo&&!p.reserve&&!p.reloadUntil))&&p.ammo<=1000&&p.reserve<=10000&&p.maxHp&&p.maxHp<=1000000&&p.maxStamina&&p.maxStamina<=1000000&&p.hp<=p.maxHp&&p.stamina<=p.maxStamina);p.alive=flags&1;p.stunned=flags&2;p.specialPhase=SpecialPhase(flags>>2);require(valid_special(p));value(p.life,4);require(p.life);require(!writing||valid_skills(p));uint8_t skill=(p.reloadLevel)|(p.masteryLevel<<2)|(p.surveyorLevel<<4)|(p.verifiedSkills?64:0);value(skill,1);require(!(skill&128));p.reloadLevel=skill&3;p.masteryLevel=(skill>>2)&3;p.surveyorLevel=(skill>>4)&3;p.verifiedSkills=skill&64;require(valid_skills(p));require(p.alive==(p.hp!=0)&&p.stunned==(p.alive&&p.stamina==0));}
 void snapshot(Snapshot&s){value(s.epoch,8);value(s.revision,8);value(s.eventWatermark,8);require(s.epoch&&s.revision);uint8_t count=uint8_t(std::count_if(s.players.begin(),s.players.end(),[](auto&p){return bool(p);}));value(count,1);require(count<=24);if(writing){for(auto&p:s.players)if(p)player(*p);}else{for(unsigned i=0;i<count;++i){Player p;player(p);require(!s.players[p.identity.slot]);s.players[p.identity.slot]=p;}}}
 void event(Event&e,uint64_t epoch){if(writing)require(e.epoch==epoch);e.epoch=epoch;value(e.id,8);auto kind=uint8_t(e.kind);value(kind,1);require(e.id&&kind<=uint8_t(EventKind::reload));e.kind=EventKind(kind);identity(e.source);identity(e.target,true);value(e.weapon,2);value(e.cue,4);value(e.hp,4);value(e.stamina,4);value(e.hpDamage,4);value(e.staminaDamage,4);value(e.object,4);vec(e.position);vec(e.normal);value(e.sourceLife,4);value(e.targetLife,4);require(e.sourceLife&&(e.target.slot<24?e.targetLife!=0:e.targetLife==0));require(e.weapon&&e.hp<=1000000&&e.stamina<=1000000&&e.hpDamage<=1000000&&e.staminaDamage<=1000000);if(e.kind==EventKind::damage||e.kind==EventKind::death)require(e.target.slot<24);}
 void boolean(bool&b){uint8_t v=b?1:0;value(v,1);require(v<=1);b=v!=0;}
 void sop(SopView& v,const Snapshot& s){
  if(writing)require(valid_sop_view(v,s));
  bool present=v.recipient!=Identity{};boolean(present);
  if(present){identity(v.recipient);value(v.life,4);value(v.visibleMask,3);value(v.activation,4);vec(v.origin);boolean(v.jammed);value(v.inputSequence,4);boolean(v.inputSequenced);require(v.inputSequenced||!v.inputSequence);}
  else v={};require(valid_sop_view(v,s));
 }
 void command(Command&c){
  value(c.epoch,8);value(c.sequence,4);auto kind=uint8_t(c.kind);value(kind,1);require(c.epoch&&kind<=uint8_t(CommandKind::loadout));c.kind=CommandKind(kind);
  boolean(c.enabled);value(c.team,1);value(c.generation,1);value(c.sceneRevision,8);for(auto&w:c.weapons)value(w,2);value(c.life,4);require(c.life);
  const bool noWeapons=c.weapons==std::array<uint16_t,3>{};
  switch(c.kind){
   case CommandKind::loaded:require(!c.team&&c.generation&&noWeapons&&(!c.enabled||c.sceneRevision));break;
   case CommandKind::ready:require(!c.team&&!c.generation&&!c.sceneRevision&&noWeapons);break;
   case CommandKind::team:require(!c.enabled&&c.team>=1&&c.team<=2&&!c.generation&&!c.sceneRevision&&noWeapons);break;
   case CommandKind::loadout:require(!c.enabled&&!c.team&&!c.generation&&!c.sceneRevision&&!noWeapons);break;
  }
 }
 void preparation(Preparation&p){
  value(p.epoch,8);value(p.revision,8);identity(p.self);value(p.generation,1);require(p.epoch&&p.revision&&p.generation);
  auto phase=uint8_t(p.phase);value(phase,1);require(phase<=uint8_t(RoundPhase::ended));p.phase=RoundPhase(phase);
  boolean(p.runtimeReady);boolean(p.autoAssign);boolean(p.freeForAll);require(!p.freeForAll||p.autoAssign);boolean(p.countdown);value(p.remainingMs,4);require(p.countdown||!p.remainingMs);
  value(p.lastCommand,4);auto error=uint8_t(p.error);value(error,1);require(error<=uint8_t(CommandError::already_deployed));p.error=CommandError(error);
  boolean(p.dpEnabled);value(p.dpBalance,4);require(p.dpBalance<=1000000000);for(auto&b:p.restrictions)value(b,1);
  require(p.supported.size()<=128);uint8_t count=uint8_t(p.supported.size());value(count,1);require(count<=128);p.supported.resize(count);
  std::set<uint16_t> supported;for(auto&w:p.supported){value(w,2);require(w&&supported.insert(w).second);}
  value(p.requiredCategories,1);require(p.requiredCategories<=7&&(!p.runtimeReady||(!supported.empty()&&p.requiredCategories)));
  for(auto&w:p.selected)value(w,2);
  uint8_t players=uint8_t(std::count_if(p.players.begin(),p.players.end(),[](auto&r){return bool(r);}));value(players,1);require(players<=24);
  auto row=[&](RoundPlayer&r){identity(r.id);value(r.team,1);boolean(r.loaded);boolean(r.ready);boolean(r.deployed);value(r.life,4);require(r.life);require(r.team<=2&&(!p.freeForAll||!r.team)&&(!r.ready||(r.loaded&&(p.freeForAll||r.team))));value(r.kills,4);value(r.deaths,4);require(r.kills<=1000000000&&r.deaths<=1000000000);};
  if(writing){for(auto&r:p.players)if(r)row(*r);}else for(unsigned i=0;i<players;++i){RoundPlayer r;row(r);require(!p.players[r.id.slot]);p.players[r.id.slot]=r;}
  std::set<uint32_t> identities;for(unsigned i=0;i<24;++i)if(auto&r=p.players[i])require(r->id.slot==i&&identities.insert(r->id.character).second);
  require(p.players[p.self.slot]&&p.players[p.self.slot]->id==p.self);
  boolean(p.roundClock);value(p.roundRemainingMs,4);
  require((p.roundClock||!p.roundRemainingMs)&&(!p.roundClock||p.phase!=RoundPhase::waiting)&&p.roundRemainingMs<=maximumRoundDurationMs&&p.roundRemainingMs%1000==0);
  require(p.phase!=RoundPhase::ended||(p.roundClock&&!p.roundRemainingMs&&!p.countdown));
 }
};
void fields(IO&io,Record&r){
 std::visit([&](auto&v){using T=std::decay_t<decltype(v)>;
 if constexpr(std::is_same_v<T,Frame>){io.snapshot(v.snapshot);auto status=uint8_t(v.status);io.value(status,1);require(status<=uint8_t(Status::ended));v.status=Status(status);uint8_t n=uint8_t(v.events.size());require(v.events.size()<=4);io.value(n,1);require(n<=4);v.events.resize(n);for(auto&e:v.events){io.event(e,v.snapshot.epoch);require(e.id<=v.snapshot.eventWatermark);}io.sop(v.sop,v.snapshot);require(v.status==Status::active||!v.sop.visibleMask);}
 else if constexpr(std::is_same_v<T,Command>)io.command(v);
 else if constexpr(std::is_same_v<T,Preparation>)io.preparation(v);
 else{io.value(v.epoch,8);require(v.epoch);if constexpr(std::is_same_v<T,Offer>)io.identity(v.self);else if constexpr(std::is_same_v<T,Input>){io.value(v.sequence,4);io.pose(v.pose);io.value(v.weapon,2);uint8_t flags=(v.fire?1:0)|(v.reload?2:0)|(v.firePressed?4:0)|(v.suspended?8:0)|(v.specialPressed?16:0)|(v.specialHeld?32:0);io.value(flags,1);require(!(flags&0xc0)&&(!(flags&8)||flags==8)&&(!(flags&2)||!(flags&5))&&(!(flags&48)||!(flags&7))&&(v.weapon||!(flags&7)));v.fire=flags&1;v.reload=flags&2;v.firePressed=flags&4;v.suspended=flags&8;v.specialPressed=flags&16;v.specialHeld=flags&32;io.value(v.life,4);require(v.life);}}
 },r);
}
}
bool recognized(std::span<const uint8_t>b){return b.size()>=magic.size()&&std::equal(magic.begin(),magic.end(),b.begin());}
Input coalesce_input(const Input&older,const Input&newer){auto out=newer;if(!older.suspended&&!newer.suspended&&older.epoch==newer.epoch&&older.life==newer.life&&older.weapon==newer.weapon){out.reload|=older.reload;out.firePressed|=older.firePressed;out.specialPressed|=older.specialPressed;if(out.specialPressed||out.specialHeld)out.fire=out.firePressed=out.reload=false;else if(out.reload)out.fire=out.firePressed=false;}return out;}
std::vector<uint8_t> encode(const Record&record){IO io;io.writing=true;io.out.assign(magic.begin(),magic.end());io.out.push_back(uint8_t(record.index()));auto copy=record;fields(io,copy);require(io.out.size()<=2000);return io.out;}
Record decode(std::span<const uint8_t>b){require(recognized(b)&&b.size()>=7&&b.size()<=2000);IO io;io.in=b;io.at=7;Record r;switch(b[6]){case 0:r=Offer{};break;case 1:r=Accept{};break;case 2:r=Input{};break;case 3:r=Frame{};break;case 4:r=Command{};break;case 5:r=Preparation{};break;default:throw Invalid();}fields(io,r);require(io.at==b.size());return r;}
}
