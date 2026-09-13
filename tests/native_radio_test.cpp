#include "native_radio.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2win::radio;
namespace {
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
const Identity a{0,11,101},b{1,12,102},enemy{2,13,103},old{3,14,104};
const Filter team=[](const Member& s,const Member& r,uint8_t){return s.team==r.team&&s.team>=1&&s.team<=2;};
Context context(){return {77,{{a,1,1,true},{b,1,1,true},{enemy,1,2,true},{old,1,1,true}}};}
void handshake(Service& host,Client& client,Identity id,uint64_t token,const Context& c,uint64_t now=100){
 client.bind(c.epoch,id);auto probe=client.probe(token,now);check(probe.has_value(),"probe");
 auto offer=host.receive(id,*probe,c,now,team);check(offer.size()==1&&offer[0].recipient==id,"offer");
 check(client.receive(offer[0].body,c,now,team)&&client.status()==Status::ready,"ready");
}
}
int main(){try{
 auto c=context();Service host({1000});host.reset(c.epoch);Client ca,cb,ce;
 handshake(host,ca,a,111,c);handshake(host,cb,b,222,c);handshake(host,ce,enemy,333,c);
 auto request=ca.submit(0,c,100);check(request.result==SubmitResult::submitted&&request.body&&request.sequence==1,"submit");
 auto record=decode(*request.body);check(record&&record->identity==a&&record->sequence==1,"codec");
 const Body golden{0xed,'G','W','R','A',1,3,0, 0,0,0,0,0,0,0,77, 0,0,0,0,0,0,0,111, 0,0,11,0,0,0,101,0, 0,0,0,1, 0,0,0,1,0,2,0,0};
 check(*request.body==golden,"golden BE44");
 for(size_t offset:{size_t(0),size_t(1),size_t(2),size_t(3),size_t(4),size_t(5),size_t(7),size_t(31),size_t(42),size_t(43)}) {auto bad=golden;bad[offset]^=0x80;check(!decode(bad),"header/reserved");}
 for(size_t n=0;n<44;++n)check(!decode(std::span(golden).first(n)),"truncated");
 auto extra=std::vector<uint8_t>(golden.begin(),golden.end());extra.push_back(0);check(!decode(extra),"trailing");
 for(auto offset:{24,25,26,27,28,29,30,32,36,40,41}){auto modified=golden;modified[static_cast<size_t>(offset)]^=0xff;auto parsed=decode(modified);if(parsed)check(*encode(*parsed)==modified,"roundtrip modified");}
 auto bad=*record;bad.preset=8;check(!encode(bad),"unreviewed preset");bad=*record;bad.third=1;check(!encode(bad),"third");bad=*record;bad.life=0;check(!encode(bad),"life");bad=*record;bad.sequence=0;check(!encode(bad),"sequence");
 auto deliveries=host.receive(a,*request.body,c,100,team);check(deliveries.size()==2,"same team only and no unsolicited oldpeer");
 for(auto& d:deliveries){auto n=decode(d.body);check(n&&n->sequence==request.sequence&&n->identity==a,"sender request correlation");if(d.recipient==a)check(ca.receive(d.body,c,100,team),"selfecho");else{check(!ca.receive(d.body,c,100,team),"recipient token");check(cb.receive(d.body,c,100,team),"remote");check(!cb.receive(d.body,c,100,team),"duplicate");}}
 check(ca.drain().size()==1&&cb.drain().size()==1&&cb.drain().empty(),"once drain");
 check(host.receive(a,*request.body,c,2000,team).empty(),"request replay");
 auto fast=ca.submit(1,c,101);check(host.receive(a,*fast.body,c,101,team).empty(),"host rate");check(host.receive(a,*fast.body,c,2100,team).empty(),"rate reject cannot replay");
 auto forged=*record;forged.sequence=3;check(host.receive(b,*encode(forged),c,2100,team).empty(),"admitted identity mismatch");forged.token++;check(host.receive(a,*encode(forged),c,2100,team).empty(),"token mismatch");
 auto next=ca.submit(2,c,1100);check(host.receive(a,*next.body,c,1100,team).size()==2,"interval boundary");
 auto probe=encode({Kind::probe,c.epoch,111,a});check(host.receive(a,*probe,c,1100,team).size()==1,"idempotent probe");check(host.receive(a,*next.body,c,2200,team).empty(),"probe preserves replay");probe=encode({Kind::probe,c.epoch,999,a});check(host.receive(a,*probe,c,2200,team).empty(),"probe token replacement rejected");
 auto wrong=c;wrong.epoch++;check(host.receive(a,*next.body,wrong,3000,team).empty(),"epoch context");wrong=c;wrong.members.push_back(wrong.members[0]);check(host.receive(a,*next.body,wrong,3000,team).empty(),"duplicate slot");
 wrong=c;wrong.members[0].identity.instance++;check(host.receive(a,*next.body,wrong,3000,team).empty(),"slot reuse");
 wrong=c;wrong.members[0].eligible=false;check(ca.submit(1,wrong,3000).result==SubmitResult::ineligible,"self ineligible");auto stale=ca.submit(3,c,3000);check(host.receive(a,*stale.body,wrong,3000,team).empty(),"host ineligible");
 c.members[0].life=2;check(host.receive(a,*stale.body,c,3000,team).empty(),"old life");auto respawn=ca.submit(3,c,3100);auto respawnEvents=host.receive(a,*respawn.body,c,3100,team);check(respawnEvents.size()==2&&respawn.sequence>stale.sequence,"respawn sequence retained");
 auto forgedNotice=*record;forgedNotice.kind=Kind::notification;forgedNotice.token=222;forgedNotice.sequence=50;check(!cb.receive(*encode(forgedNotice),c,3100,team),"old source life");forgedNotice.identity=enemy;forgedNotice.life=1;check(!cb.receive(*encode(forgedNotice),c,3100,team),"client team recheck");
 forgedNotice.identity=a;forgedNotice.life=2;check(!cb.receive(*encode(forgedNotice),c,3100,{}),"missing filter");const Filter throws=[](const Member&,const Member&,uint8_t)->bool{throw std::runtime_error("policy");};check(!cb.receive(*encode(forgedNotice),c,3100,throws),"filter exceptions contained");
 for(uint32_t seq=100;seq<165;++seq){forgedNotice.sequence=seq;check(cb.receive(*encode(forgedNotice),c,3200,team)==(seq<164),"bounded queue");}check(cb.drain().size()==64,"bounded drain");check(!cb.receive(*encode(forgedNotice),c,3300,team),"overflow replay rejected");
 Client timeout;timeout.bind(c.epoch,old);auto pending=timeout.probe(444,0);auto late=host.receive(old,*pending,c,0,team);check(late.size()==1,"old peer probe");timeout.tick(3000);check(timeout.status()==Status::unavailable&&!timeout.receive(late[0].body,c,3000,team)&&!timeout.probe(445,4000),"timeout no retry or late offer");
 Client early;early.bind(c.epoch,old);auto q=early.probe(444,0);auto r=host.receive(old,*q,c,0,team);check(early.receive(r[0].body,c,2999,team),"offer before timeout");
 host.remove(b);auto later=ca.submit(4,c,5000);auto remaining=host.receive(a,*later.body,c,5000,team);check(remaining.size()==2,"remove stops recipient (old now probed)");for(auto& d:remaining)check(d.recipient!=b,"removed b");
 Client lobby;auto lobbyContext=c;lobbyContext.members[1].life=0;lobbyContext.members[1].eligible=false;handshake(host,lobby,b,555,lobbyContext);check(lobby.submit(0,lobbyContext,100).result==SubmitResult::ineligible,"life0 handshake only");
 host.reset(78);check(host.receive(a,*later.body,c,6000,team).empty(),"epoch reset");ca.bind(78,a);check(ca.status()==Status::unavailable&&ca.drain().empty(),"client bind reset");
 std::cout<<"native_radio_test PASS: checked codec, negotiation, identity/epoch/life/token, team policy, replay/rate, bounded delivery\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
