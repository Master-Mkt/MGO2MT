#include "combat_wire_budget.h"
#include "combat_service.h"
#include "dedicated_peer.h"
#include "host_session.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace mgo2win;
namespace {
namespace cw=combat::wire;
void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);}
std::vector<uint8_t> player_profile(){std::vector<uint8_t> b(45);b[0]=2;b.insert(b.end(),{'T','e','s','t',0});return b;}
std::shared_ptr<const stage::Collision> floor(){return std::make_shared<const stage::Collision>(stage::Collision::make({{-20000,0,-20000},{20000,0,-20000},{20000,0,20000},{-20000,0,20000}},{{{0,1,2}},{{0,2,3}}}));}
// Synthetic fixture values only. These are not recovered game weapon timings.
std::array<combat::Weapon,2> profiles(){combat::Weapon ak{25,1000,0,100,900,7,11,10000,1,2,true},m4=ak;m4.id=24;m4.magazine=5;m4.reserve=8;return {ak,m4};}
cw::Command command(uint64_t epoch,uint32_t sequence,cw::CommandKind kind,uint32_t life=1){cw::Command c;c.epoch=epoch;c.sequence=sequence;c.kind=kind;c.life=life;return c;}
combat::Pose spawn_pose(combat::Identity id){return {{0,2,id.slot==1?0.0f:2000.0f}};}
struct Fixture {
 combat::Service host{19};std::array<combat::Identity,2> ids{{{1,0x101,200},{2,0x102,300}}};bool blocked=false;unsigned grants=0;std::vector<uint32_t> grantLives;
 explicit Fixture(std::shared_ptr<const weapons::Catalog> catalog,bool dp=false){host.configure(floor(),profiles());
  combat::RoundCoordinator::Policy roundPolicy{60000,7,dp,true};roundPolicy.roundDurationMs=300000;
  host.configure_round(roundPolicy,std::move(catalog),[this](combat::Authority&a,combat::Identity id,uint8_t team,std::span<const uint16_t> inventory,uint64_t now){grantLives.push_back(a.spawn_life(id));if(blocked)return false;if(!a.join(id,team,spawn_pose(id),1000,1000,inventory,now))return false;++grants;return true;});}
 bool send(unsigned i,cw::Command c,uint64_t now){return host.receive(ids[i],cw::encode(c),now);}
 void tick(uint64_t now){host.poll(now);host.deliveries();}
 cw::Preparation prep(unsigned i,uint64_t now){return *host.preparation(ids[i],now);}
 void initial(){for(unsigned i=0;i<2;++i){check(host.admit(ids[i],0),"admit fixture");check(host.receive(ids[i],cw::encode(cw::Accept{19}),0),"accept fixture");auto c=command(19,1,cw::CommandKind::loaded);c.enabled=true;c.generation=7;c.sceneRevision=1;check(send(i,c,0),"loaded fixture");c=command(19,2,cw::CommandKind::ready);c.enabled=true;check(send(i,c,1),"ready fixture");}tick(1);for(unsigned i=0;i<2;++i){auto c=command(19,3,cw::CommandKind::loadout);c.weapons={25,0,0};check(send(i,c,2),"initial selection");}tick(2);check(grants==2,"initial one grant each");}
 bool input(unsigned i,uint32_t sequence,uint32_t life,uint64_t now,bool shot=false,bool reload=false){cw::Input in{19,sequence,spawn_pose(ids[i]),25,false,reload,shot,false,life};return host.receive(ids[i],cw::encode(in),now);}
};
struct Network {
 combat::Service&service;
 host::Hello owner{100,0x12345678,2,1,{{{192,0,2,1},5732}}};
 std::array<host::Hello,2> hello{{{200,0xabcdef01,2,2,{{{192,0,2,2},5730}}},{300,0x11223344,2,2,{{{192,0,2,3},5730}}}}};
 std::array<host::Machine,2> client;
 std::array<host::DedicatedPeer,2> server;
 std::array<host::Player,2> roster{{{1,0x101,200,"TestA",{}},{2,0x102,300,"TestB",{}}}};
 std::array<combat::Identity,2> ids{{{1,0x101,200},{2,0x102,300}}};
 std::array<bool,2> admitted{},synced{};
 std::array<unsigned,2> acceptedCommands{},rejectedCommands{},shotEvents{};
 std::array<std::set<uint64_t>,2> eventIds;
 uint64_t now=0;unsigned sentClient=0,sentServer=0,dropped=0,starts=0,offers=0,accepts=0;
 explicit Network(combat::Service&s):service(s),client{{{hello[0],100,player_profile(),0},{hello[1],100,player_profile(),0}}},server{{{owner,hello[0],0},{owner,hello[1],0}}}{}
 void queue(unsigned i,std::vector<uint8_t> payload){check(server[i].queue(std::move(payload),now)!=0,"bounded dedicated reliable queue accepts fixture message");}
 void step(){
  for(unsigned i=0;i<2;++i){
   auto packets=client[i].poll(now);
   for(auto p=packets.rbegin();p!=packets.rend();++p){if(++sentClient%13==0){++dropped;continue;}server[i].receive(*p,now);server[i].receive(*p,now);}
   for(auto&e:server[i].events()){
    check(!e.empty(),"nonempty session event");
    if(e[0]==2&&!admitted[i]){
     admitted[i]=true;queue(i,host::roster_record({0,0x100,100,"Host",{}},owner));
     for(unsigned j=0;j<2;++j)if(admitted[j])queue(i,host::roster_record(roster[j],hello[j]));
     queue(i,{7,0,0,0,0,0,3});
     for(unsigned j=0;j<2;++j)if(i!=j&&admitted[j])queue(j,host::roster_record(roster[i],hello[i]));
    }else if(e[0]==10&&!synced[i]){
     synced[i]=true;queue(i,host::room_snapshot(std::array<host::Rotation,1>{{{20,1,0}}},7));check(service.admit(ids[i],now),"session admission precedes native offer");
    }else if(cw::recognized(e)){
     auto record=cw::decode(e);bool accepted=service.receive(ids[i],e,now);
     if(std::holds_alternative<cw::Accept>(record)){check(accepted,"current native offer is explicitly accepted");++accepts;}
     if(std::holds_alternative<cw::Command>(record))(accepted?acceptedCommands[i]:rejectedCommands[i])++;
    }
   }
  }
  service.poll(now);if(service.take_round_start())++starts;
  for(auto&d:service.deliveries()){
   auto it=std::find(ids.begin(),ids.end(),d.recipient);check(it!=ids.end(),"delivery tied to admitted full identity");
   if(std::holds_alternative<cw::Offer>(cw::decode(d.payload))){check(d.payload[5]==cw::version,"wire offer uses the current GWCB version");++offers;}
   queue(unsigned(it-ids.begin()),std::move(d.payload));
  }
  for(unsigned i=0;i<2;++i){
   auto packets=server[i].poll(now);
   for(auto p=packets.rbegin();p!=packets.rend();++p){if(++sentServer%11==0){++dropped;continue;}client[i].receive(*p,now);client[i].receive(*p,now);}
   for(auto&e:client[i].combat_events()){check(eventIds[i].insert(e.id).second,"duplicate datagrams never replay combat effects");if(e.kind==combat::EventKind::shot)++shotEvents[i];}
   check(!server[i].closed(),"fixture session remains open under bounded loss");
  }
  now+=10;
 }
 void until(const std::function<bool()>&predicate,const char*why,uint64_t timeout=5000){auto end=now+timeout;while(!predicate()&&now<end)step();check(predicate(),why);}
 void drain(uint64_t duration){auto end=now+duration;while(now<end)step();}
 cw::Preparation preparation(unsigned i)const{auto p=client[i].result().preparation;check(bool(p),"preparation received");return *p;}
 void send(unsigned i,const cw::Command&command){check(client[i].combat_command(command,now),"participant queues a native preparation command");}
 void ack(unsigned i,uint32_t sequence){until([&]{auto p=client[i].result().preparation;return p&&p->lastCommand==sequence;},"participant receives host command result");}
};

void lifecycle(std::shared_ptr<const weapons::Catalog> catalog){
 Fixture f(catalog);f.initial();check(f.prep(0,2).roundClock&&f.prep(0,2).roundRemainingMs==300000,"round clock begins before initial grants");
 check(f.input(1,100,1,100,true),"victim shoots away before death");f.tick(100);
 check(f.input(1,101,1,110,false,true),"victim starts reload");f.tick(110);
 check(f.host.authority().snapshot().players[2]->ammo==6&&f.host.authority().snapshot().players[2]->reloadUntil==1010,"partially used inventory and pending reload");
 check(f.input(0,100,1,120,true),"host receives lethal shot");f.tick(120);f.tick(121);
 auto wait=f.prep(1,121);check(wait.respawnWaiting&&wait.respawnRemainingMs==3000,"self countdown begins on host observed death");check(std::get<cw::Preparation>(cw::decode(cw::encode(wait)))==wait,"death countdown wire roundtrip");
 auto dead=f.host.authority().snapshot();check(!dead.players[2]->alive&&dead.players[2]->hp==0&&dead.players[2]->life==1,"host alone observes death");
 auto premature=command(19,4,cw::CommandKind::loadout);premature.weapons={25,0,0};check(f.send(1,premature,130)&&f.prep(1,130).error==cw::CommandError::already_deployed,"no grant during native wait");
 f.tick(100);check(f.prep(1,100).players[2]->life==1,"clock regression never expires wait");
 f.tick(3120);check(f.prep(1,3120).players[2]->deployed&&f.prep(1,3120).players[2]->life==1,"3000ms not reached one millisecond early");
 f.tick(3121);auto eligible=f.prep(1,3121);check(!eligible.players[2]->deployed&&eligible.players[2]->loaded&&eligible.players[2]->life==2,"host grants next-life selection eligibility at boundary");
 check(!f.host.authority().snapshot().players[2]->alive&&f.host.authority().snapshot().players[2]->life==1&&f.host.authority().snapshot().players[2]->ammo==6,"eligibility cannot heal or refill dead body");
 f.tick(5000);check(f.prep(1,5000).players[2]->life==2,"repeated dead snapshots mint no extra entitlement");
 auto choose=command(19,5,cw::CommandKind::loadout,2);choose.weapons={24,0,0};auto old=choose;old.life=1;old.sequence=100000;check(!f.send(1,old,5001)&&f.prep(1,5001).lastCommand==4,"oldlife high command sequence cannot poison next-life approval");old.life=3;check(!f.send(1,old,5001),"future life cannot be requested");old=choose;old.epoch=18;check(!f.send(1,old,5001),"previous epoch command rejected");
 auto bad=choose;bad.weapons={99,0,0};check(f.send(1,bad,5001)&&f.prep(1,5001).error==cw::CommandError::weapon,"unknown next-life weapon rejected");
 f.blocked=true;choose.sequence=6;auto exactDead=f.host.authority().snapshot();check(f.send(1,choose,5002)&&f.prep(1,5002).error==cw::CommandError::spawn&&f.host.authority().snapshot()==exactDead&&f.grants==2,"failed respawn transaction restores exact dead inventory and revision");check(f.grantLives.back()==2&&f.host.authority().spawn_life(f.ids[1])==1,"next life exposed only inside callback");
 check(!f.host.authority().respawn(f.ids[1],2,[&]{check(f.host.authority().join(f.ids[1],2,spawn_pose(f.ids[1]),1000,1000,std::array<uint16_t,1>{24},5002),"transient replacement joins inside transaction");return false;})&&f.host.authority().snapshot()==exactDead,"post-join callback failure restores exact previous body and inventory");
 bool threw=false;try{f.host.authority().respawn(f.ids[1],2,[&]()->bool{check(f.host.authority().spawn_life(f.ids[1])==2,"throwing callback sees next life");throw std::runtime_error("intentional fixture failure");});}catch(const std::runtime_error&){threw=true;}check(threw&&f.host.authority().snapshot()==exactDead&&f.host.authority().spawn_life(f.ids[1])==1,"exception rollback clears transaction context");
 f.blocked=false;choose.sequence=7;check(f.send(1,choose,5003),"approved next-life loadout");auto fresh=f.host.authority().snapshot();check(f.grants==3&&fresh.players[2]->alive&&fresh.players[2]->life==2&&fresh.players[2]->weapon==24&&fresh.players[2]->ammo==5&&fresh.players[2]->reserve==8&&!fresh.players[2]->reloadUntil,"one fresh inventory from reselected weapon and cleared dead reload");
 f.tick(5003);check(f.prep(1,5003).roundRemainingMs==295000,"next-life deployment cannot restart round clock");
 check(f.prep(1,5003).players[2]->deployed&&f.prep(1,5003).players[2]->life==2,"preparation and body life agree only after successful grant");
 check(!f.send(1,choose,5004),"same command replay rejected");choose.sequence=8;check(f.send(1,choose,5004)&&f.prep(1,5004).error==cw::CommandError::already_deployed&&f.host.authority().snapshot()==fresh,"new sequence cannot refill same life");
 check(!f.input(1,100000,1,5005,true),"old life input rejected before sequence acceptance");
 auto&a=f.host.authority();auto victim=f.ids[1];check(a.pose(victim,19,100000,spawn_pose(victim),5005,1)==combat::Reject::generation&&a.equip(victim,19,24,5005,1)==combat::Reject::generation&&a.reload(victim,19,5005,1).reject==combat::Reject::generation&&a.fire(victim,{19,100000,24,{0,0,1},1},5005).reject==combat::Reject::generation&&a.snapshot()==fresh,"direct authority refuses all stale-life mutations");
 cw::Input newInput{19,1,spawn_pose(victim),24,false,false,true,false,2};check(f.host.receive(victim,cw::encode(newInput),5006),"new life input sequence1 accepted");f.tick(5006);check(a.snapshot().players[2]->ammo==4,"new life host-owned shot sequence reset and fresh weapon fires once");f.tick(5007);check(a.snapshot().players[2]->ammo==4,"old life pending reload/held state not carried across grant");
 check(!a.respawn(victim,3,[]{return true;}),"alive player cannot trigger host respawn API");
 check(f.input(0,101,1,5100,true),"second lethal shot");f.tick(5100);f.tick(5101);f.tick(8101);check(f.prep(1,8101).players[2]->life==3&&!f.prep(1,8101).players[2]->deployed,"second host-observed death mints exactly life3");
 auto replacement=victim;replacement.instance++;f.host.remove(victim);check(f.host.admit(replacement,8200)&&f.host.receive(replacement,cw::encode(cw::Accept{19}),8200),"reconnect admitted with distinct slot incarnation");auto loaded=command(19,1,cw::CommandKind::loaded);loaded.enabled=true;loaded.generation=7;loaded.sceneRevision=1;check(f.host.receive(replacement,cw::encode(loaded),8200),"reconnect loads scene");choose=command(19,2,cw::CommandKind::loadout);choose.weapons={25,0,0};check(f.host.receive(replacement,cw::encode(choose),8201)&&f.host.preparation(replacement,8201)->error==cw::CommandError::already_deployed&&!a.snapshot().players[2]&&f.grants==3,"disconnect cannot turn dead-life entitlement into fresh reconnect inventory");
 // DP and stamina-only behavior remain closed until their original allowance/
 // recovery paths are reviewed; native health death is the only trigger.
 Fixture dp(catalog,true);dp.initial();check(dp.input(0,1,1,100,true),"DP fixture lethal input");dp.tick(100);dp.tick(101);const auto wallet=dp.prep(1,101).dpBalance;check(dp.prep(1,101).respawnWaiting&&dp.prep(1,101).respawnRemainingMs==3000,"HOST starts recipient death countdown");dp.tick(10000);check(!dp.prep(1,10000).players[2]->deployed&&dp.prep(1,10000).players[2]->life==2&&dp.prep(1,10000).dpBalance==wallet&&!dp.prep(1,10000).respawnWaiting,"DP respawn opens selection and preserves wallet without allowance");
 Fixture alive(catalog);alive.initial();alive.tick(100000);check(alive.prep(1,100000).players[2]->life==1,"elapsed room time never grants alive player a new life");
 auto stunProfiles=profiles();stunProfiles[0].damage=0;stunProfiles[0].staminaDamage=1000;combat::Authority stun;stun.begin(1,floor(),stunProfiles);for(unsigned i=0;i<2;++i)check(stun.join(f.ids[i],uint8_t(i+1),spawn_pose(f.ids[i]),1000,1000,std::array<uint16_t,1>{25},0),"stun fixture");stun.active(true);check(bool(stun.fire(f.ids[0],{1,1,25,{0,0,1}},1)),"stun attack");check(stun.snapshot().players[2]->alive&&stun.snapshot().players[2]->stunned&&!stun.respawn(f.ids[1],2,[]{return true;}),"stamina collapse is not health death");
}
void wire_and_replica(){
 auto p=combat::Player{};p.identity={1,1,10};p.team=1;p.pose={{0,2,0}};p.hp=p.maxHp=p.stamina=p.maxStamina=1000;p.weapon=25;p.ammo=7;p.reserve=11;p.alive=true;
 combat::Snapshot initial{1,1,0,{}};initial.players[1]=p;auto q=p;q.identity={2,2,20};q.team=2;q.pose.feet[2]=2000;initial.players[2]=q;
 std::array<combat::Replica,2> replicas;for(auto&r:replicas)check(r.snapshot(initial),"replica initial watermark");
 auto e=combat::Event{};e.epoch=1;e.id=1;e.kind=combat::EventKind::damage;e.source=p.identity;e.target=q.identity;e.weapon=25;e.hpDamage=1000;e.sourceLife=e.targetLife=1;
 auto fresh=initial;fresh.revision=3;fresh.eventWatermark=1;fresh.players[2]->life=2;for(auto&r:replicas)check(r.snapshot(fresh),"replica accepts next life");
 auto next=e;next.id=2;next.kind=combat::EventKind::shot;next.source=q.identity;next.sourceLife=2;next.target={};next.targetLife=0;
 check(replicas[0].events(std::array{next}).empty(),"reordered event is held until missing predecessor");
 check(replicas[0].events(std::array{e}).empty()&&replicas[1].events(std::array{e}).empty(),"delayed prior-life death effects suppressed but event watermark advances");
 for(auto&r:replicas){auto accepted=r.events(std::array{e,next});check(accepted==std::vector<combat::Event>{next},"lost then retried new-life event applied once");check(r.events(std::array{next}).empty(),"duplicate new-life event suppressed");}
 auto rollback=fresh;rollback.revision++;rollback.players[2]->life=1;check(!replicas[0].snapshot(rollback),"higher snapshot revision cannot roll back a life");
 cw::Input input{1,99,p.pose,25,true,false,true,false,2};auto bytes=cw::encode(input);check(std::get<cw::Input>(cw::decode(bytes))==input,"input life roundtrip");for(size_t i=0;i<bytes.size();++i){bool bad=false;try{cw::decode(std::span(bytes).first(i));}catch(const cw::Invalid&){bad=true;}check(bad,"truncated life-stamped input rejected");}auto v3=bytes;v3[5]=3;check(!cw::recognized(v3),"v3 deliberately not guessed as v4");input.life=0;bool rejected=false;try{cw::encode(input);}catch(const cw::Invalid&){rejected=true;}check(rejected,"zero life rejected");
 auto older=cw::Input{1,100,p.pose,25,false,true,false,false,1};auto newer=cw::Input{1,1,p.pose,25,false,false,false,false,2};check(cw::coalesce_input(older,newer)==newer,"old reload not coalesced into new life sequence1");older.reload=false;older.firePressed=true;check(cw::coalesce_input(older,newer)==newer,"old pulse not coalesced into new life");
 auto full=initial;full.eventWatermark=4;
 for(unsigned i=0;i<24;++i){p.identity={uint8_t(i),uint16_t(i+1),100+i};p.oxygen=uint16_t(i*400);p.faceSubmerged=(i%2)!=0;full.players[i]=p;}
 std::vector<combat::Event> events;
 for(unsigned i=1;i<=4;++i){e.id=i;e.source=full.players[0]->identity;e.target=full.players[1]->identity;events.push_back(e);}
 const auto budget=combat_test::budget(cw::Frame{full,cw::Status::active,events});
 std::vector<uint8_t> wire;
 for(size_t count=0;count<=budget.capacity;++count){
  const std::vector<combat::Event> prefix(events.begin(),events.begin()+count);
  wire=cw::encode(cw::Frame{full,cw::Status::active,prefix});
  auto decoded=std::get<cw::Frame>(cw::decode(wire));
  check(wire.size()==budget.baseBytes+budget.eventBytes*count&&wire.size()<=2000,"every exact full-roster prefix fits calculated record bound");
  check(decoded.snapshot==full&&decoded.events==prefix,"full roster preserves life, oxygen, submersion and all included events");
 }
 auto smaller=full;smaller.players[23].reset();const auto smallerBudget=combat_test::budget(cw::Frame{smaller,cw::Status::active,events});
 const std::vector<combat::Event> subset(events.begin(),events.begin()+smallerBudget.capacity);
 auto four=cw::encode(cw::Frame{smaller,cw::Status::active,subset});auto fourDecoded=std::get<cw::Frame>(cw::decode(four));
 check(smallerBudget.capacity>=budget.capacity&&four.size()==smallerBudget.fullBytes&&fourDecoded.snapshot==smaller&&fourDecoded.events==subset,"smaller full roster preserves maximal complete event chunk");
 host::Message message;message.channel=1;message.serial=1;message.payload=wire;
 auto packet=host::encode({1,{message}},host::Keys{1,2});
 check(packet.size()<=host::max_datagram,"largest fitting full-roster fixture fits encrypted host datagram");
 auto decrypted=host::decode(packet,host::Keys{1,2},1);
 check(decrypted.messages.size()==1&&decrypted.messages[0].channel==1&&decrypted.messages[0].serial==1&&decrypted.messages[0].payload==wire,"encrypted full-roster frame roundtrips without payload loss");
 std::cout<<"full_roster_events=3 record_bytes="<<wire.size()<<" datagram_bytes="<<packet.size()<<" four_event_players=23 record_bytes="<<four.size()<<'\n';
}
void encrypted_network(std::shared_ptr<const weapons::Catalog> catalog){
 Fixture f(catalog);Network net(f.host);net.until([&]{return net.client[0].result().preparation&&net.client[1].result().preparation;},"two encrypted sessions receive initial preparation");
 for(unsigned i=0;i<2;++i){auto c=command(19,1,cw::CommandKind::loaded);c.enabled=true;c.generation=7;c.sceneRevision=1;net.send(i,c);}net.ack(0,1);net.ack(1,1);
 for(unsigned i=0;i<2;++i){auto c=command(19,2,cw::CommandKind::ready);c.enabled=true;net.send(i,c);}net.ack(0,2);net.ack(1,2);net.until([&]{return net.preparation(0).phase==cw::RoundPhase::selecting;},"all-ready opens selection");
 auto c=command(19,3,cw::CommandKind::loadout);c.weapons={25,0,0};net.send(0,c);net.send(1,c);net.ack(0,3);net.ack(1,3);net.until([&]{return net.client[0].result().combat_state==f.host.authority().snapshot()&&net.client[1].result().combat_state==f.host.authority().snapshot();},"initial bodies converge");
 cw::Input shoot{19,100,spawn_pose(net.ids[0]),25,false,false,true};check(net.client[0].combat_input(shoot,net.now),"network lethal input");net.until([&]{auto s=f.host.authority().snapshot().players[2];return s&&!s->alive;},"host receives and judges death");
 net.until([&]{return !net.preparation(1).players[2]->deployed&&net.preparation(1).players[2]->life==2;},"host publishes next-life eligibility after native wait",6000);
 auto before=f.host.authority().snapshot();auto prior=c;prior.sequence=9999;check(!net.client[1].combat_command(prior,net.now),"client queue drops prior-life loadout before transport");
 auto fresh=command(19,4,cw::CommandKind::loadout,2);fresh.weapons={24,0,0};net.send(1,fresh);net.ack(1,4);net.until([&]{return net.client[0].result().combat_state==f.host.authority().snapshot()&&net.client[1].result().combat_state==f.host.authority().snapshot();},"both peers receive same new-life inventory");
 check(f.grants==3&&f.host.authority().snapshot().players[2]->life==2&&f.host.authority().snapshot().players[2]->ammo==5,"network grants one fresh reselected inventory");
 cw::Input old{19,1000,spawn_pose(net.ids[1]),24,false,false,true,false,1};check(!net.client[1].combat_input(old,net.now),"client refuses old life input after newlife snapshot");auto input=old;input.life=2;input.sequence=1;check(net.client[1].combat_input(input,net.now),"new-life sequence1 crosses encrypted transport");net.until([&]{return f.host.authority().snapshot().players[2]->ammo==4;},"host accepts sequence1 without cached reload or hold");
 fresh.sequence=5;net.send(1,fresh);net.ack(1,5);check(net.preparation(1).error==cw::CommandError::already_deployed,"network duplicate selection cannot refill");net.drain(600);check(f.host.authority().snapshot().players[2]->ammo==4&&f.grants==3&&net.dropped>0,"loss/reorder/duplicate has no duplicate inventory or fire");

 // Keep all eight reliable slots plus a coalesced reload unsent while host
 // death/eligibility/grant snapshots arrive in the opposite direction. This
 // exercises the client pending-input branch before it gets a poll to clear it.
 for(unsigned i=0;i<10;++i){cw::Input pending{19,1000+i,spawn_pose(net.ids[1]),24,false,i==8,false,false,2};check(net.client[1].combat_input(pending,net.now),"congested old life retains one pending reload");}
 auto deliverHostOnly=[&](uint64_t t){net.now=t;f.host.poll(t);for(auto&d:f.host.deliveries()){auto it=std::find(net.ids.begin(),net.ids.end(),d.recipient);check(it!=net.ids.end(),"host-only fixture delivery");net.queue(unsigned(it-net.ids.begin()),std::move(d.payload));}for(unsigned i=0;i<2;++i){for(auto&b:net.server[i].poll(t))net.client[i].receive(b,t);for(auto&e:net.client[i].combat_events()){check(net.eventIds[i].insert(e.id).second,"host-only delivery effect once");if(e.kind==combat::EventKind::shot)++net.shotEvents[i];}}};
 const auto deathAt=net.now+10;check(f.input(0,101,1,deathAt,true),"host receives second death while victim send queue is full");deliverHostOnly(deathAt);deliverHostOnly(deathAt+1);deliverHostOnly(deathAt+3001);
 auto third=command(19,6,cw::CommandKind::loadout,3);third.weapons={24,0,0};check(f.send(1,third,deathAt+3002),"fixture host receives genuine next-life command independently of held transport");deliverHostOnly(deathAt+3300);
 auto received=net.client[1].result().combat_state;check(received&&received->players[2]->life==3&&received->players[2]->alive,"new life snapshot arrives before client resumes polling");
 input.life=3;input.sequence=1;check(net.client[1].combat_input(input,net.now),"new life sequence1 replaces unsent old high-sequence reload");net.until([&]{return f.host.authority().snapshot().players[2]->ammo==4;},"sequence1 eventually passes ACK-congested transport",5000);net.drain(1100);check(f.grants==4&&f.host.authority().snapshot().players[2]->ammo==4&&!f.host.authority().snapshot().players[2]->reloadUntil,"late old life queued packets cannot reload/refill third life");
 std::cout<<"encrypted_respawn_dropped="<<net.dropped<<" grants="<<f.grants<<'\n';
}
}
int main(int argc,char**argv){try{check(argc==2,"weapon catalog argument");auto catalog=std::make_shared<weapons::Catalog>();std::string error;check(catalog->load(argv[1],error),"reviewed catalog loads");lifecycle(catalog);wire_and_replica();encrypted_network(catalog);std::cout<<"host respawn: native delay, exact rollback, one grant/life, stale input/command/event guards, reconnect anti-refill, DP/stun boundaries and 2 encrypted peers passed\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
