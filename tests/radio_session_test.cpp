#include "radio_session.h"
#include "chat_session.h"
#include "preset_radio.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2win::radio;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
const Identity self{0,11,101},peer{1,12,102};
const Filter team=[](const Member& a,const Member& b,uint8_t){return a.team==b.team;};
Context context(){return {71,{{self,1,1,true},{peer,1,1,true}}};}
struct Rig {
 Context c=context();Service host{{1000}};Session s;Client remote;
 Rig(){host.reset(c.epoch);handshake(100);remote.bind(c.epoch,peer);auto p=remote.probe(222,100);auto offers=host.receive(peer,*p,c,100,team);check(offers.size()==1&&remote.receive(offers[0].body,c,100,team),"remote offer");}
 void handshake(uint64_t now){auto out=s.pump(c,self,now,111,true,{},team);check(out.size()==1,"session probe");auto offer=host.receive(self,out[0],c,now,team);check(offer.size()==1,"session offer");s.pump(c,self,now,111,true,{offer[0].body},team);check(s.state().status==Status::ready,"session ready");}
 Body send(uint8_t id,uint64_t now){check(s.submit(s.state().generation,id,now),"queue submission");auto out=s.pump(c,self,now,111,true,{},team);check(out.size()==1&&decode(out[0])->kind==Kind::request,"request once");return out[0];}
 std::vector<Body> echo(const Body& body,uint64_t now){std::vector<Body> in;for(const auto& d:host.receive(self,body,c,now,team))if(d.recipient==self)in.push_back(d.body);return in;}
};
}
int main(){try{
 Rig r;auto g=r.s.state().generation;
 check(!r.s.submit(g-1,0,100)&&!r.s.submit(g,8,100),"stale UI and unreviewed ID rejected");
 check(r.s.submit(g,0,100)&&!r.s.submit(g,1,100),"single queued selection");
 check(r.s.pump(r.c,self,100,111,false,{},team).empty()&&r.s.state().delivery==DeliveryState::queued,"unwritable worker retains one queue");
 auto out=r.s.pump(r.c,self,100,111,true,{},team);check(out.size()==1,"queued sent once");
 auto request=out[0];check(r.s.state().delivery==DeliveryState::awaiting_echo&&!r.s.submit(g,1,100),"one flight");
 auto incoming=r.echo(request,100);check(incoming.size()==1,"host selfecho exists");
 // A different sender's identical sequence is a real event but never an ACK.
 auto remote=r.remote.submit(1,r.c,100);std::vector<Body> peerInput;
 for(const auto& d:r.host.receive(peer,*remote.body,r.c,100,team))if(d.recipient==self)peerInput.push_back(d.body);
 r.s.pump(r.c,self,100,111,true,peerInput,team);
 check(r.s.state().delivery==DeliveryState::awaiting_echo&&r.s.drain().size()==1,"other PC sequence cannot acknowledge self");
 r.s.pump(r.c,self,100,111,true,incoming,team);
 check(r.s.state().delivery==DeliveryState::confirmed,"exact self sequence confirms");
 auto events=r.s.drain();check(events.size()==1&&events[0].sender==self&&events[0].sequence==decode(request)->sequence,"confirmed original event");
 r.s.pump(r.c,self,101,111,true,incoming,team);check(r.s.drain().empty(),"duplicate echo not appended twice");
 check(!r.s.submit(g,1,1099)&&r.s.submit(g,1,1100),"one-second UI rate exact boundary");
 out=r.s.pump(r.c,self,1100,111,true,{},team);check(out.size()==1,"second flight");
 r.s.pump(r.c,self,1101,111,true,incoming,team);check(r.s.state().delivery==DeliveryState::awaiting_echo,"previous self sequence not new ACK");
 r.s.pump(r.c,self,6099,111,true,{},team);check(r.s.state().delivery==DeliveryState::awaiting_echo,"before 5 seconds");
 check(r.s.pump(r.c,self,6100,111,true,{},team).empty()&&r.s.state().delivery==DeliveryState::unconfirmed,"5 seconds is uncertain and never retry");
 check(r.s.pump(r.c,self,7100,111,true,{},team).empty(),"no automatic retry later");
 // A queued old-life menu choice cannot cross respawn.
 check(r.s.submit(g,2,7100),"queue before respawn");r.c.members[0].life=2;
 check(r.s.pump(r.c,self,7101,111,true,incoming,team).empty(),"old life queue and echo discarded");
 check(r.s.state().generation!=g&&!r.s.submit(g,2,7101)&&r.s.drain().empty(),"life invalidates UI generation");
 auto current=r.s.state().generation;check(r.s.submit(current,2,7101),"new life can select");
 r.c.members[0].eligible=false;r.s.pump(r.c,self,7102,111,true,{},team);
 check(r.s.state().delivery==DeliveryState::none&&!r.s.submit(current,2,7102),"ineligible cancels queued selection");
 r.c.members[0].eligible=true;r.c.epoch++;r.host.reset(r.c.epoch);
 out=r.s.pump(r.c,self,7200,333,true,incoming,team);check(out.size()==1&&decode(out[0])->kind==Kind::probe,"new epoch probes and discards stale echo");
 check(r.s.state().generation!=current&&r.s.drain().empty(),"epoch clears history and scope");
 current=r.s.state().generation;r.s.disconnect();check(r.s.state().generation!=current&&!r.s.state().epoch&&!r.s.submit(current,0,7201),"disconnect rejects old UI");
 Rig replaced;auto before=replaced.s.state().generation;auto newIdentity=self;++newIdentity.instance;replaced.c.members[0].identity=newIdentity;
 out=replaced.s.pump(replaced.c,newIdentity,200,444,true,{},team);
 check(out.size()==1&&replaced.s.state().generation!=before&&!replaced.s.submit(before,0,200),"same PC changed slot instance resets scope");
 Rig reversed;reversed.s.pump(reversed.c,self,99,111,true,{},team);check(!reversed.s.state().epoch,"clock regression fails closed");
 Rig queued;check(queued.s.submit(queued.s.state().generation,0,100),"queue timeout setup");
 queued.s.pump(queued.c,self,5099,111,false,{},team);check(queued.s.state().delivery==DeliveryState::queued,"queued before five seconds");
 check(queued.s.pump(queued.c,self,5100,111,true,{},team).empty()&&queued.s.state().delivery==DeliveryState::unconfirmed,"expired queued selection never sends when worker becomes writable");
 Rig zeroLife;zeroLife.c.members[0].life=0;
 zeroLife.s.pump(zeroLife.c,self,101,111,true,{},team);
 check(!zeroLife.s.state().eligible&&!zeroLife.s.submit(zeroLife.s.state().generation,0,101),"raw eligible with life zero remains unavailable");
 Rig batched;auto beforeRespawn=batched.remote.submit(0,batched.c,100);std::vector<Body> oldRemote;
 for(const auto& d:batched.host.receive(peer,*beforeRespawn.body,batched.c,100,team))if(d.recipient==self)oldRemote.push_back(d.body);
 check(oldRemote.size()==1,"remote batch before respawn");batched.c.members[0].life++;
 batched.s.pump(batched.c,self,101,111,true,oldRemote,team);
 check(batched.s.drain().empty(),"old remote notification in receiver respawn tick dropped");
 batched.s.pump(batched.c,self,201,111,true,oldRemote,team);
 check(batched.s.drain().empty(),"discarded respawn notification consumes replay watermark");
 auto afterRespawn=batched.remote.submit(1,batched.c,1100);std::vector<Body> newRemote;
 for(const auto& d:batched.host.receive(peer,*afterRespawn.body,batched.c,1100,team))if(d.recipient==self)newRemote.push_back(d.body);
 batched.s.pump(batched.c,self,1100,111,true,newRemote,team);check(batched.s.drain().size()==1,"fresh subsequent remote event works");
 Session offering;auto offerContext=context();Service offerHost({1000});offerHost.reset(offerContext.epoch);
 auto probe=offering.pump(offerContext,self,100,555,true,{},team);auto offers=offerHost.receive(self,probe[0],offerContext,100,team);
 offerContext.members[0].life=2;offering.pump(offerContext,self,101,555,true,{offers[0].body},team);
 check(offering.state().status==Status::ready&&offering.state().life==2,"offer survives receiver life change");
 // The exact default catalogue text reaches chat history, without acknowledging
 // an ordinary lobby chat flight containing the same text from the same PC.
 mgo2win::chat::Session chat;chat.connect(self.character,mgo2win::chat::EndpointProfile::nomad_jp);chat.enter(20);
 chat.roster({{self.character,"日本語の自分",1},{peer.character,"日本語の相手",1}},true);
 const std::string text(mgo2::radio::defaultCategories()[0].messages[0].text);
 check(text=="ゴーゴーゴー！","original Japanese default text");
 check(chat.submit(chat.state().generation,text,false,100)==mgo2win::chat::Submit::accepted&&chat.take(100).has_value(),"ordinary chat flight");
 check(chat.receive_radio(self.character,text,101),"radio appends joined ID");
 auto cs=chat.state();check(cs.delivery==mgo2win::chat::Delivery::awaiting_echo&&cs.lines.size()==1&&cs.lines[0].radio&&cs.lines[0].text==text&&cs.lines[0].name=="日本語の自分","radio does not ACK ordinary chat and keeps Japanese attribution");
 check(!chat.receive_radio(999,text,102),"unknown PC cannot insert radio history");
 check(chat.receive({self.character,0,text},103)&&chat.state().delivery==mgo2win::chat::Delivery::echo_received,"ordinary matching echo still works");
 for(unsigned i=0;i<70;++i)check(chat.receive_radio(peer.character,text,104+i),"known radio history");
 check(chat.state().lines.size()==64,"radio shares bounded 64-row history");
 chat.leave();check(!chat.receive_radio(self.character,text,200)&&chat.state().lines.empty(),"leave rejects stale radio history");
 std::cout<<"radio_session_test PASS: deterministic HOST, stale UI/life/epoch/identity, queue/rate/echo/deadline, Japanese chat isolation\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
