#include "combat_cycle.h"
#include "dedicated_peer.h"
#include "host_session.h"
#include "host_briefing.h"
#include "host_room.h"
#include "character_client.h"
#include "stage_music.h"
#include <algorithm>
#include <iostream>
#include <set>
using namespace mgo2mt;
namespace cw=combat::wire;
namespace {
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
std::vector<uint8_t> profile(){std::vector<uint8_t>b(45);b[0]=2;b.insert(b.end(),{'T','e','s','t',0});return b;}
LobbyPacket reply(uint16_t request,uint32_t code=0,uint32_t id=0){LobbyPacket p;p.command=request+1;p.payload={uint8_t(code>>24),uint8_t(code>>16),uint8_t(code>>8),uint8_t(code)};if(id)p.payload.insert(p.payload.end(),{uint8_t(id>>24),uint8_t(id>>16),uint8_t(id>>8),uint8_t(id)});return p;}
struct Network {
 combat::Cycle&cycle;host::Lifecycle room;std::vector<uint16_t> serverCalls;
 host::Hello owner{100,123,2,1,{{{192,0,2,1},5732}}};
 std::array<host::Hello,2> hello{{{200,456,2,2,{{{192,0,2,2},5730}}},{300,789,2,2,{{{192,0,2,3},5730}}}}};
 std::array<combat::Identity,2> ids{{{1,257,200},{2,258,300}}};
 std::array<host::Player,2> roster{{{1,257,200,"A",{}},{2,258,300,"B",{}}}};
 std::array<std::optional<host::Machine>,2> client;std::array<std::optional<host::DedicatedPeer>,2> server;
 std::array<bool,2> admitted{},synced{};std::array<uint64_t,2> clientEpoch{},firedEpoch{};std::array<uint32_t,2> sequence{};
 std::array<std::optional<uint64_t>,2> snapshotAt;
 std::array<std::set<uint64_t>,2> freshGrants,endedEpochs;std::set<uint64_t> starts;
 uint64_t now=0;unsigned sentClient=0,sentServer=0,losses=0,restarts=0,oldRejects=0;bool joinedLate=false,withheldNewScene=false;
 stage::MusicPlayback music;stage::Track track{"fixture:one",L"同じBGM",{},false};unsigned musicStarts=0;
 host::RoomExchange exchange(){return [&](uint16_t op,std::span<const uint8_t>b){serverCalls.push_back(op);if(op==0x4316)return reply(op,0,123);if(op==0x43ca){check(b.size()==1&&b[0]==0,"native marker remains zero, no original meaning invented");return reply(op);}if(op==0x4392)throw std::runtime_error("single rotation must not issue setGame DB write");return reply(op);};}
 explicit Network(combat::Cycle&c):cycle(c){host::Settings settings;settings.capacity=3;std::atomic_bool stop=false;check(room.create(settings,exchange(),stop).status==host::RoomControlStatus::success,"offline mock creates room lifecycle");connect(0);}
 void connect(unsigned i){check(cycle.objects(),"original registry available");client[i].emplace(hello[i],owner.character,profile(),now,cycle.objects()->registry());server[i].emplace(owner,hello[i],now);}
 void queue(unsigned i,std::vector<uint8_t>b){check(server[i]->queue(std::move(b),now)!=0,"reliable queue bounded");}
 void metadata(unsigned i,bool next){snapshotAt[i].reset();auto&o=*cycle.objects();check(o.request().has_value(),"scene request available");server[i]->objects(o.registry(),*o.request(),ids[i].slot);queue(i,next?host::next_round_snapshot(cycle.request().generation,cycle.request().round):host::room_snapshot(std::array<host::Rotation,1>{{cycle.request().rotation}},cycle.request().generation,0,cycle.request().round));queue(i,host::phase_update(0));}
 void auto_client(unsigned i){auto r=client[i]->result();if(r.stage!=host::Stage::joined||!r.preparation)return;auto p=*r.preparation;auto self=*p.players[ids[i].slot];
  check(r.match.request.has_value(),"joined stage lifetime stays present through ended/new epoch");
  if(clientEpoch[i]!=p.epoch){clientEpoch[i]=p.epoch;sequence[i]=0;check(!self.loaded&&!self.ready&&!self.deployed,"new offer requires loaded/READY/deploy again");}
  if(p.phase==cw::RoundPhase::ended){endedEpochs[i].insert(p.epoch);cw::Command c;c.epoch=p.epoch;c.sequence=sequence[i]+1;c.kind=cw::CommandKind::ready;c.enabled=true;check(!client[i]->combat_command(c,now),"ended UI transport rejects READY");if(musicStarts)check(!music.select(&track),"same BGM survives ended wait");return;}
  if(p.lastCommand!=sequence[i])return;
  auto send=[&](cw::Command c){c.epoch=p.epoch;c.life=self.life;c.sequence=sequence[i]+1;if(client[i]->combat_command(c,now))++sequence[i];};
  if(!self.loaded){cw::Command loaded;loaded.kind=cw::CommandKind::loaded;loaded.enabled=true;loaded.generation=p.generation;loaded.sceneRevision=r.scene?r.scene->revision:999;
   if(r.scene_status!=stage::SceneSyncStatus::ready){check(!client[i]->combat_command(cw::Command{p.epoch,sequence[i]+1,cw::CommandKind::loaded,true,0,p.generation,999,{},self.life},now),"new scene must arrive before loaded is sent");withheldNewScene=true;return;}send(loaded);return;}
  if(p.phase==cw::RoundPhase::waiting&&!self.ready){cw::Command ready;ready.kind=cw::CommandKind::ready;ready.enabled=true;send(ready);return;}
  if(p.phase!=cw::RoundPhase::waiting&&!self.deployed){cw::Command grant;grant.kind=cw::CommandKind::loadout;grant.weapons={25,0,0};send(grant);return;}
  if(!self.deployed||!r.combat_state||!r.combat_state->players[ids[i].slot])return;
  const auto&body=*r.combat_state->players[ids[i].slot];if(freshGrants[i].insert(p.epoch).second){check(body.life==1&&body.ammo==30&&body.reserve==90,"fresh round grants initial AK inventory once");if(music.select(&track))++musicStarts;}
  if(firedEpoch[i]!=p.epoch){cw::Input input;input.epoch=p.epoch;input.sequence=1;input.life=body.life;input.pose=body.pose;input.weapon=25;input.firePressed=true;check(client[i]->combat_input(input,now),"single shot queued");firedEpoch[i]=p.epoch;}
 }
 void step(){
  for(unsigned i=0;i<2;++i)if(client[i]){
   auto packets=client[i]->poll(now);for(auto p=packets.rbegin();p!=packets.rend();++p){if(++sentClient%17==0){++losses;continue;}server[i]->receive(*p,now);server[i]->receive(*p,now);}
   for(auto&e:server[i]->events()){
    if(e[0]==2&&!admitted[i]){admitted[i]=true;queue(i,host::roster_record({0,256,100,"HOST",{}},owner));for(unsigned j=0;j<2;++j)if(admitted[j])queue(i,host::roster_record(roster[j],hello[j]));queue(i,{7,0,0,0,0,0,3});for(unsigned j=0;j<2;++j)if(j!=i&&admitted[j])queue(j,host::roster_record(roster[i],hello[i]));}
    else if(e[0]==10&&!synced[i]){synced[i]=true;metadata(i,false);check(cycle.admit(ids[i],now),"admit after original room sync");}
    else if(cw::recognized(e))cycle.service().receive(ids[i],e,now);
   }
   for(auto slot:server[i]->snapshot_requests())if(slot==ids[i].slot&&!snapshotAt[i])snapshotAt[i]=now+500;
   if(snapshotAt[i]&&now>=*snapshotAt[i]){auto image=cycle.objects()->snapshot(ids[i].slot);check(bool(image),"host snapshot");server[i]->queue_object(std::move(*image),now);snapshotAt[i].reset();}
  }
  cycle.poll(now);
  if(cycle.service().take_round_start()){check(starts.insert(cycle.epoch()).second,"one start per epoch");check(room.round_started(0,exchange()).status==host::RoomControlStatus::success,"mock round marker acknowledged");}
  for(auto&d:cycle.service().deliveries()){auto it=std::find(ids.begin(),ids.end(),d.recipient);check(it!=ids.end(),"known recipient");queue(unsigned(it-ids.begin()),std::move(d.payload));}
  if(cycle.advance(now)){++restarts;for(unsigned i=0;i<2;++i)if(synced[i])metadata(i,true);
   cw::Input stale;stale.epoch=cycle.epoch()-1;stale.sequence=999;stale.weapon=25;stale.firePressed=true;auto c=cw::Command{stale.epoch,999,cw::CommandKind::loadout};c.weapons={25,0,0};check(!cycle.service().receive(ids[0],cw::encode(stale),now)&&!cycle.service().receive(ids[0],cw::encode(c),now),"stale prior epoch input and loadout rejected");++oldRejects;
   check(cycle.service().authority().snapshot().players==combat::Snapshot{}.players,"new epoch clears all combat bodies");check(cycle.selector()->counters()==std::array<uint64_t,3>{0,0,0},"new round uses fresh initial spawn group counters");
  }
  for(unsigned i=0;i<2;++i)if(client[i]){auto packets=server[i]->poll(now);for(auto p=packets.rbegin();p!=packets.rend();++p){if(++sentServer%19==0){++losses;continue;}client[i]->receive(*p,now);client[i]->receive(*p,now);}client[i]->combat_events();check(host::active(client[i]->result().stage)&&!server[i]->closed(),"live peer identities retained through cycles and loss");auto_client(i);}
  if(!client[1]&&!freshGrants[0].empty()&&now>=1000){connect(1);joinedLate=true;}
  now+=10;
 }
};
}
int main(int argc,char**argv){try{
 check(argc>=3&&argc<=5,"catalog and original stage root required, optional rule/map");auto catalog=std::make_shared<weapons::Catalog>();std::string error;check(catalog->load(argv[1],error),"reviewed catalog");
 combat::Cycle::Options options;options.stageRoot=argv[2];check(options.itemFacts.load(std::filesystem::path(argv[1]).parent_path()/"item_drop_policy.json",error),"real default pickup catalog");options.rotations={{uint8_t(argc>4?std::stoi(argv[4]):20),uint8_t(argc>3?std::stoi(argv[3]):1),0}};options.capacity=3;options.round.countdownMs=60000;options.round.roundDurationMs=8000;options.endedDisplayMs=1200;
 auto random=[](){return combat::spawn::Random{{1,2,3,4}};};combat::Cycle cycle(options,catalog,random);check(cycle.repeat_enabled()&&cycle.world()&&cycle.selector(),"actual native runtime content ready");Network net(cycle);
 while(net.now<45000&&(net.endedEpochs[0].size()<3||net.endedEpochs[1].size()<3))net.step();
 check(net.starts.size()>=3&&net.restarts>=2&&net.endedEpochs[0].size()>=3&&net.endedEpochs[1].size()>=3,"two encrypted memory PCs complete three native rounds");check(net.freshGrants[0].size()>=3&&net.freshGrants[1].size()>=3&&net.oldRejects>=2,"fresh inventory and stale rejection each epoch");check(net.joinedLate&&net.withheldNewScene&&net.losses&&net.musicStarts==1,"late join, applied-scene gate, loss and continuous same-track BGM");check(std::count(net.serverCalls.begin(),net.serverCalls.end(),0x43ca)==net.starts.size()&&std::count(net.serverCalls.begin(),net.serverCalls.end(),0x4392)==0,"one mock start marker per round; no rotation DB control");
 cycle.remove(net.ids[1]);check(!cycle.service().authority().snapshot().players[2],"departure clears current-round body");
 options.capacity=1;combat::Cycle empty(options,catalog,random);check(!empty.selector(),"dedicated-only capacity remains valid without player spawn");options.capacity=3;
 options.rotations.push_back({20,1,0});combat::Cycle multiple(options,catalog,random);check(!multiple.repeat_enabled(),"multiple rotation is not silently repeated/skipped");options.rotations={{20,1,2}};options.round.dpEnabled=true;combat::Cycle dp(options,catalog,random);check(!dp.repeat_enabled(),"DP cycle extension remains disabled");
 std::cout<<"GWCB v7 actual native Cycle / reviewed native scene contract / two encrypted memory PCs / rounds="<<net.starts.size()<<" restarts="<<net.restarts<<" loss="<<net.losses<<" BGM starts="<<net.musicStarts<<"; no real sockets, account or DB used\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
