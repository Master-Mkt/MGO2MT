#include "combat_wire.h"
#include "combat_sop_view.h"
#include <bit>
#include <cmath>
#include <algorithm>
#include <set>
namespace mgo2mt::combat::wire {
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
 void pose(Pose&p){vec(p.feet);f(p.yaw);f(p.pitch);auto radius=uint16_t(p.capsule.radius);value(radius,2);require(radius==260||radius==350||radius==800);auto height=uint16_t(p.capsule.height);value(height,2);require(height>=radius*2&&(height==1700||height==1100||height==560||height==4200)&&std::abs(p.yaw)<=3.14159274f&&std::abs(p.pitch)<=1.4f);p.capsule={float(radius),float(height),2};}
 void player(Player&p){if(writing)require(valid_special(p)&&valid_evade(p)&&valid_cover(p)&&valid_special_pc(p)&&valid_mounted(p)&&valid_flight(p));identity(p.identity);value(p.team,1);pose(p.pose);value(p.hp,4);value(p.maxHp,4);value(p.stamina,4);value(p.maxStamina,4);uint8_t extension=(p.burning?1:0)|((p.ladderAnchor||p.mountedId||p.flightId)?2:0)|(p.weapon?4:0)|(p.reloadUntil?8:0)|(uint8_t(p.evadeKind)&4?16:0)|((uint8_t(p.specialPc.action)&4)?32:0)|(p.aiming?64:0)|((p.hawkeyeLevel&2)?128:0);value(extension,1);p.aiming=(extension&64)!=0;p.burning=(extension&1)!=0;if(extension&2){uint16_t anchor=(p.mountedId||p.flightId)?0:p.ladderAnchor;value(anchor,2);if(anchor){p.ladderAnchor=anchor;p.mountedId=p.flightId=p.flightElapsedMs=0;p.blastFlight=false;}else{p.ladderAnchor=0;uint16_t action=p.flightId?uint16_t(0x8000|p.flightId):p.mountedId;value(action,2);require(action&0x7fff);if(action&0x8000){p.mountedId=0;p.flightId=action&0x7fff;uint8_t elapsed=uint8_t(p.flightElapsedMs/250)|(p.blastFlight?128:0);value(elapsed,1);p.blastFlight=(elapsed&128)!=0;p.flightElapsedMs=uint16_t(elapsed&127)*250;}else{p.mountedId=action;p.flightId=p.flightElapsedMs=0;p.blastFlight=false;}}}else {p.ladderAnchor=p.mountedId=p.flightId=p.flightElapsedMs=0;p.blastFlight=false;}if(extension&4){value(p.weapon,2);value(p.ammo,2);value(p.reserve,2);require(p.weapon);}else{require(!writing||(!p.weapon&&!p.ammo&&!p.reserve));p.weapon=p.ammo=p.reserve=0;}if(extension&8){value(p.reloadUntil,8);require(p.reloadUntil);value(p.reloadElapsedMs,2);require(p.reloadElapsedMs<=60000);}else {p.reloadUntil=0;p.reloadElapsedMs=0;}uint8_t flags=(p.alive?1:0)|(p.stunned?2:0)|(uint8_t(p.specialPhase)<<2)|((uint8_t(p.evadeKind)&3)<<4)|(p.cover.attached?64:0)|(p.cover.lean?128:0);value(flags,1);require(p.team<=2&&(p.weapon||(!p.ammo&&!p.reserve&&!p.reloadUntil))&&p.ammo<=1000&&p.reserve<=10000&&p.maxHp&&p.maxHp<=1000000&&p.maxStamina&&p.maxStamina<=1000000&&p.hp<=p.maxHp&&p.stamina<=p.maxStamina);p.alive=flags&1;p.stunned=flags&2;p.specialPhase=SpecialPhase((flags>>2)&3);p.evadeKind=EvadeKind(((flags>>4)&3)|((extension&16)?4:0));p.cover.attached=(flags&64)!=0;if(p.cover.attached)f(p.cover.normalYaw);else p.cover.normalYaw=0;if(flags&128){uint8_t side=p.cover.lean<0?1:2;value(side,1);require(side==1||side==2);p.cover.lean=side==1?-1:1;}else p.cover.lean=0;if(p.evadeKind!=EvadeKind::none){value(p.evadeSerial,4);value(p.evadeElapsedMs,2);}else{p.evadeSerial=0;p.evadeElapsedMs=0;}require(valid_special(p)&&valid_evade(p)&&valid_cover(p));value(p.life,4);require(p.life);require(!writing||valid_skills(p));uint8_t skill=(p.reloadLevel)|(p.masteryLevel<<2)|(p.surveyorLevel<<4)|(p.verifiedSkills?64:0)|((p.hawkeyeLevel&1)?128:0);value(skill,1);p.reloadLevel=skill&3;p.masteryLevel=(skill>>2)&3;p.surveyorLevel=(skill>>4)&3;p.verifiedSkills=skill&64;p.hawkeyeLevel=uint8_t((skill>>7)|((extension>>6)&2));require(valid_skills(p));require(p.alive==(p.hp!=0)&&p.stunned==(p.alive&&p.stamina==0));require(!writing||p.oxygen<=water_gameplay::Oxygen::full);uint16_t oxygen=uint16_t(p.oxygen|(p.faceSubmerged?0x8000:0)|(p.specialPc.kind==special_pc::Kind::gekko?0x4000:0));value(oxygen,2);require((oxygen&0x3fff)<=water_gameplay::Oxygen::full);p.oxygen=oxygen&0x3fff;p.faceSubmerged=(oxygen&0x8000)!=0;p.specialPc.kind=(oxygen&0x4000)?special_pc::Kind::gekko:special_pc::Kind::human;if(p.specialPc.kind==special_pc::Kind::gekko){uint16_t action=uint16_t(p.specialPc.elapsedMs|((uint16_t(p.specialPc.action)&3)<<13)|(p.specialPc.nameVisible?0:0x8000));value(action,2);p.specialPc.nameVisible=!(action&0x8000);p.specialPc.action=special_pc::Action(((action>>13)&3)|((extension&32)?4:0));p.specialPc.elapsedMs=action&0x1fff;if(p.specialPc.action!=special_pc::Action::none)value(p.specialPc.serial,4);else p.specialPc.serial=0;}else{require(!(extension&32));p.specialPc={};}require(valid_cover(p)&&valid_special_pc(p)&&valid_mounted(p)&&valid_flight(p));require(!p.burning||p.alive);require(!p.ladderAnchor||(p.alive&&!p.stunned&&p.specialPc.kind==special_pc::Kind::human&&p.pose.capsule.height==1700&&!p.reloadUntil&&!p.cover.attached&&!p.cover.lean&&p.evadeKind==EvadeKind::none&&p.specialPhase==SpecialPhase::none));}
 void snapshot(Snapshot&s){value(s.epoch,8);value(s.revision,8);value(s.eventWatermark,8);require(s.epoch&&s.revision);uint8_t count=uint8_t(std::count_if(s.players.begin(),s.players.end(),[](auto&p){return bool(p);}));value(count,1);require(count<=24);if(writing){for(auto&p:s.players)if(p)player(*p);}else{for(unsigned i=0;i<count;++i){Player p;player(p);require(!s.players[p.identity.slot]);s.players[p.identity.slot]=p;}}}
 void event(Event&e,uint64_t epoch){if(writing)require(e.epoch==epoch);e.epoch=epoch;value(e.id,8);auto kind=uint8_t(e.kind);value(kind,1);require(e.id&&kind<=uint8_t(EventKind::knockback));e.kind=EventKind(kind);identity(e.source);identity(e.target,true);value(e.weapon,2);value(e.cue,4);value(e.hp,4);value(e.stamina,4);value(e.hpDamage,4);value(e.staminaDamage,4);value(e.object,4);vec(e.position);vec(e.normal);value(e.sourceLife,4);value(e.targetLife,4);if(e.kind==EventKind::shot)e.shotDistance=std::bit_cast<float>(uint32_t(u(std::bit_cast<uint32_t>(e.shotDistance),4)));require(valid_shot_distance(e));require(e.sourceLife&&(e.target.slot<24?e.targetLife!=0:e.targetLife==0));require((e.weapon||e.kind==EventKind::catapultLaunch)&&valid_catapult_event(e)&&valid_knockback_event(e)&&e.hp<=1000000&&e.stamina<=1000000&&e.hpDamage<=1000000&&e.staminaDamage<=1000000);if(e.kind==EventKind::damage||e.kind==EventKind::death)require(e.target.slot<24);if(e.kind==EventKind::itemPickup)require(e.source==e.target&&e.sourceLife==e.targetLife&&(e.object==1||e.object==2)&&!e.cue&&!e.hp&&!e.stamina&&!e.hpDamage&&!e.staminaDamage);}
 void boolean(bool&b){uint8_t v=b?1:0;value(v,1);require(v<=1);b=v!=0;}
 void environment(Environment&e){value(e.epoch,8);value(e.revision,8);require(e.epoch&&e.revision);auto&c=e.config;require(!writing||mgo2mt::environment::valid(c));
  uint8_t flags=(c.weatherOverride?1:0)|(c.fog?2:0)|(c.rain?4:0)|(c.snow?8:0)|(c.sandstorm?16:0)|(c.manualHemisphere?32:0);value(flags,1);require(flags<=63);
  c.weatherOverride=flags&1;c.fog=flags&2;c.rain=flags&4;c.snow=flags&8;c.sandstorm=flags&16;c.manualHemisphere=flags&32;
  value(c.time,1);value(c.sound,1);for(auto&v:c.upper)value(v,1);for(auto&v:c.lower)value(v,1);value(c.gainMilli,2);require(mgo2mt::environment::valid(c));
 }
 void debug(DebugFlights&d){value(d.epoch,8);value(d.scene,8);identity(d.recipient);value(d.life,4);value(d.sequence,4);value(d.inputSequence,4);value(d.at,8);boolean(d.truncated);require(d.epoch&&d.scene&&d.life&&d.sequence);require(d.flights.size()<=16);uint8_t count=uint8_t(d.flights.size());value(count,1);require(count<=16);d.flights.resize(count);std::set<std::pair<uint32_t,uint64_t>> unique;
  for(auto&f:d.flights){Identity owner{f.owner.slot,f.owner.instance,f.owner.character};identity(owner);f.owner.slot=owner.slot;f.owner.instance=owner.instance;f.owner.character=owner.character;value(f.owner.life,4);value(f.shot,8);value(f.weapon,2);vec(f.position);vec(f.traceFrom);vec(f.traceTo);value(f.simulatedAt,8);require(f.owner.life&&f.shot&&(f.weapon==2||f.weapon==50||projectile::throwable(f.weapon)||f.weapon==129)&&f.simulatedAt<=d.at&&unique.emplace(f.owner.character,f.shot).second);}
 }
 void sop(SopView& v,const Snapshot& s){
  if(writing)require(valid_sop_view(v,s));
  bool present=v.recipient!=Identity{};boolean(present);
  if(present){identity(v.recipient);value(v.life,4);value(v.visibleMask,3);value(v.activation,4);vec(v.origin);boolean(v.jammed);value(v.inputSequence,4);boolean(v.inputSequenced);value(v.evadeRequest,4);value(v.spreadMilliRadians,2);value(v.coverRequest,4);value(v.specialPcRequest,4);require(v.inputSequenced||!v.inputSequence);}
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
  boolean(p.respawnWaiting);value(p.respawnRemainingMs,4);require(p.respawnRemainingMs<=60000&&(p.respawnWaiting||!p.respawnRemainingMs));
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
 else if constexpr(std::is_same_v<T,DebugFlights>)io.debug(v);
 else if constexpr(std::is_same_v<T,Environment>)io.environment(v);
 else{io.value(v.epoch,8);require(v.epoch);if constexpr(std::is_same_v<T,Offer>){io.identity(v.self);io.value(v.configuration,8);}else if constexpr(std::is_same_v<T,Accept>)io.value(v.configuration,8);else if constexpr(std::is_same_v<T,Input>){io.value(v.sequence,4);io.pose(v.pose);io.value(v.weapon,2);uint8_t evade=uint8_t(v.evadeKind);io.value(evade,1);v.evadeKind=EvadeKind(evade);io.value(v.evadeRequest,4);uint8_t flags=(v.fire?1:0)|(v.reload?2:0)|(v.firePressed?4:0)|(v.suspended?8:0)|(v.specialPressed?16:0)|(v.specialHeld?32:0)|(v.aiming?64:0)|(v.meleePressed?128:0);io.value(flags,1);require((!(flags&128)||flags==128)&&(!(flags&8)||flags==8)&&(!(flags&2)||!(flags&5))&&(!(flags&48)||!(flags&7))&&(v.weapon||!(flags&2)));v.fire=flags&1;v.reload=flags&2;v.firePressed=flags&4;v.suspended=flags&8;v.specialPressed=flags&16;v.specialHeld=flags&32;v.aiming=flags&64;v.meleePressed=flags&128;require(!v.aiming||(v.weapon&&!v.reload&&!v.specialPressed&&!v.specialHeld));io.value(v.life,4);require(v.life);require(valid_evade_kind(v.evadeKind)&&((v.evadeKind==EvadeKind::none)==(!v.evadeRequest))&&(!v.evadeRequest||flags==0));uint8_t action=uint8_t(v.cover.action);io.value(action,1);v.cover.action=cover::Action(action);io.value(v.cover.request,4);uint8_t lean=uint8_t(v.cover.lean+1);io.value(lean,1);require(lean<=2);v.cover.lean=int8_t(int(lean)-1);io.boolean(v.cover.firstPerson);require(cover::valid(v.cover)&&(!(flags&8)||v.cover==cover::Intent{})&&(!v.cover.request||(!(flags&55)&&!v.evadeRequest)));uint8_t special=uint8_t(v.specialPc.action);io.value(special,1);v.specialPc.action=special_pc::Action(special);io.value(v.specialPc.request,4);require(special_pc::valid(v.specialPc)&&(!v.specialPc.request||(flags==0&&!v.evadeRequest&&v.cover==cover::Intent{})));uint8_t ladderAction=uint8_t(v.ladder.action);io.value(ladderAction,1);v.ladder.action=ladder::Action(ladderAction);io.value(v.ladder.anchorId,2);io.f(v.ladder.axis);require(ladder::valid(v.ladder));require(!(flags&8)||v.ladder==ladder::Intent{});require(v.ladder==ladder::Intent{}||(flags==0&&!v.evadeRequest&&!v.specialPc.request&&v.cover==cover::Intent{}));require(!v.meleePressed||(!v.evadeRequest&&!v.specialPc.request&&v.cover==cover::Intent{}&&v.ladder==ladder::Intent{}));io.boolean(v.debugPhysics);io.value(v.mounted.action,1);io.value(v.mounted.instance,2);io.value(v.mounted.request,4);require(mounted::valid(v.mounted));require(v.mounted==mounted::Intent{}||(flags==0&&!v.evadeRequest&&v.cover==cover::Intent{}&&v.specialPc==special_pc::Intent{}&&v.ladder==ladder::Intent{}));}}
 },r);
}
}
bool recognized(std::span<const uint8_t>b){return b.size()>=magic.size()&&std::equal(magic.begin(),magic.end(),b.begin());}
Input coalesce_input(const Input&older,const Input&newer){auto out=newer;if(!older.suspended&&!newer.suspended&&older.epoch==newer.epoch&&older.life==newer.life&&older.weapon==newer.weapon){
 if(out.mounted.action==mounted::Action::none&&older.mounted.action!=mounted::Action::none)out.mounted=older.mounted;
 out.meleePressed|=older.meleePressed;out.reload|=older.reload;out.firePressed|=older.firePressed;out.specialPressed|=older.specialPressed;
 // Retain one unconsumed edge while later poses replace its input sequence.
 // Competing intents select the latest evade request; no held auto-repeat.
 if(!out.evadeRequest&&older.evadeRequest){out.evadeKind=older.evadeKind;out.evadeRequest=older.evadeRequest;}
 if(!out.cover.request&&older.cover.request){out.cover.request=older.cover.request;out.cover.action=older.cover.action;}
 if(!out.specialPc.request&&older.specialPc.request)out.specialPc=older.specialPc;
 if(out.ladder.action==ladder::Action::none&&older.ladder.action!=ladder::Action::none)out.ladder=older.ladder;
 if(out.mounted!=mounted::Intent{}){out.fire=out.firePressed=out.reload=out.specialPressed=out.specialHeld=out.aiming=out.meleePressed=false;out.evadeKind=EvadeKind::none;out.evadeRequest=0;out.cover={};out.specialPc={};out.ladder={};}
 if(out.ladder!=ladder::Intent{}){out.fire=out.firePressed=out.reload=out.specialPressed=out.specialHeld=false;out.evadeKind=EvadeKind::none;out.evadeRequest=0;out.cover={};out.specialPc={};}
 if(out.specialPc.request){out.fire=out.firePressed=out.reload=out.specialPressed=out.specialHeld=false;out.evadeKind=EvadeKind::none;out.evadeRequest=0;out.cover={};}
 else if(out.cover.request){out.fire=out.firePressed=out.reload=out.specialPressed=out.specialHeld=false;out.evadeKind=EvadeKind::none;out.evadeRequest=0;}
 else if(out.evadeRequest)out.fire=out.firePressed=out.reload=out.specialPressed=out.specialHeld=false;
 else if(out.specialPressed||out.specialHeld)out.fire=out.firePressed=out.reload=false;else if(out.reload)out.fire=out.firePressed=false;
 if(out.reload||out.specialPressed||out.specialHeld||out.evadeRequest||out.cover.request||out.specialPc.request||out.ladder!=ladder::Intent{})out.aiming=false;
 if(out.reload||out.specialPressed||out.specialHeld||out.evadeRequest||out.cover.request||out.specialPc.request||out.ladder!=ladder::Intent{})out.meleePressed=false;
 else if(out.meleePressed){out.fire=out.firePressed=out.aiming=false;}
 }return out;}
std::vector<uint8_t> encode(const Record&record){IO io;io.writing=true;io.out.assign(magic.begin(),magic.end());io.out.push_back(uint8_t(record.index()));auto copy=record;fields(io,copy);require(io.out.size()<=(std::holds_alternative<DebugFlights>(record)?1152u:2000u));return io.out;}
Record decode(std::span<const uint8_t>b){require(recognized(b)&&b.size()>=7&&b.size()<=2000);IO io;io.in=b;io.at=7;Record r;switch(b[6]){case 0:r=Offer{};break;case 1:r=Accept{};break;case 2:r=Input{};break;case 3:r=Frame{};break;case 4:r=Command{};break;case 5:r=Preparation{};break;case 6:require(b.size()<=1152);r=DebugFlights{};break;case 7:require(b.size()==34);r=Environment{};break;default:throw Invalid();}fields(io,r);require(io.at==b.size());return r;}
}



