#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "evade_input.h"
#include "player_motion_mapping.h"
#include <array>
#include <iostream>
#include <stdexcept>

using namespace mgo2win;
using Kind=combat::EvadeKind;
using Ack=player::EvadeInput::Ack;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
constexpr uint64_t epoch=0x100000002ull;
constexpr combat::Identity self{3,0x1234,0x81234567};
constexpr uint32_t life=17;
struct Fixture {
 player::EvadeInput input;combat::SopView view;combat::Player state;
 Fixture(){input.scope(epoch,self,life);view.recipient=self;view.life=life;state.identity=self;state.life=life;state.alive=true;}
 void press(Kind kind=Kind::roll,uint64_t now=100){check(input.press(kind,now),"fresh press accepted");view.evadeRequest=input.request();state.evadeSerial=input.request();state.evadeKind=kind;}
 Ack acknowledge(uint64_t now=200){return input.acknowledge(view,state,epoch,now);}
};

int main(){try{
 // Animation completion cannot make a still-pending request share action bits.
 for(auto kind:{Kind::roll,Kind::backstep,Kind::rollLeft,Kind::rollRight})for(unsigned mask=0;mask<32;++mask){
  Fixture waiting;waiting.press(kind);combat::wire::Input packet;packet.epoch=epoch;packet.life=life;packet.sequence=1;packet.weapon=25;
  packet.fire=mask&1;packet.firePressed=mask&2;packet.reload=mask&4;packet.specialPressed=mask&8;packet.specialHeld=mask&16;
  waiting.input.apply(packet);check(packet.evadeRequest&&packet.evadeKind==kind&&!packet.fire&&!packet.firePressed&&!packet.reload&&!packet.specialPressed&&!packet.specialHeld,"pending request overrides every competing action combination, even after local motion ends");
  check(std::get<combat::wire::Input>(combat::wire::decode(combat::wire::encode(packet)))==packet,"actual delayed-ACK packet remains a canonical wire input");
  packet.suspended=true;waiting.input.apply(packet);check(!packet.evadeRequest&&packet.evadeKind==Kind::none,"suspended packet never replays pending evade");
  check(std::get<combat::wire::Input>(combat::wire::decode(combat::wire::encode(packet)))==packet,"suspended pending packet is canonical");
 }
 for(auto side:{Kind::rollLeft,Kind::rollRight}){
  Fixture request;request.press(side);check(request.acknowledge()==Ack::accepted,"exact side kind and serial ACK accepted");check(request.acknowledge()==Ack::none,"side ACK consumed once");
  Fixture mismatch;mismatch.press(side);mismatch.state.evadeKind=side==Kind::rollLeft?Kind::rollRight:Kind::rollLeft;check(mismatch.acknowledge()==Ack::rejected,"opposite-side ACK cannot approve requested travel direction");
 }
 player::EvadeInput empty;check(!empty.pending()&&empty.request()==0&&empty.kind()==Kind::none&&!empty.press(Kind::roll,100),"unscoped input cannot send");
 for(unsigned invalid=0;invalid<5;++invalid){auto id=self;uint64_t ep=epoch;uint32_t generation=life;
  switch(invalid){case 0:ep=0;break;case 1:generation=0;break;case 2:id.slot=24;break;case 3:id.instance=0;break;case 4:id.character=0;break;}
  player::EvadeInput input;input.scope(ep,id,generation);check(!input.press(Kind::roll,0)&&!input.pending(),"invalid epoch/life/full identity refuses press");
 }
 Fixture valid;check(!valid.input.press(Kind::none,100)&&!valid.input.press(static_cast<Kind>(255),100),"none/unknown request kinds rejected");
 valid.press();const auto first=valid.input.request();check(first!=0&&valid.input.pending()&&valid.input.kind()==Kind::roll,"press creates one stable pending request");
 check(!valid.input.press(Kind::backstep,900)&&valid.input.request()==first&&valid.input.kind()==Kind::roll,"new press while pending cannot replace the original request");
 valid.input.scope(epoch,self,life);check(valid.input.pending()&&valid.input.request()==first,"repeated same scope does not clear pending work");
 check(valid.acknowledge()==Ack::accepted&&!valid.input.pending()&&valid.input.kind()==Kind::none&&valid.input.request()==0,"matching ACK accepts and drains exactly once");
 check(valid.acknowledge()==Ack::none,"repeated ACK cannot retrigger an accepted request");
 valid.press(Kind::backstep,300);check(valid.input.request()==first+1,"accepted press does not reuse its serial");
 valid.input.cancel();check(!valid.input.pending()&&valid.input.request()==0&&valid.input.kind()==Kind::none,"cancel clears pending payload");
 valid.input.scope(epoch,self,life);valid.press(Kind::roll,400);check(valid.input.request()==first+2,"cancel and same-scope rebind preserve serial counter");

 // A stale view or a different full recipient/player identity must not settle it.
 for(unsigned mismatch=0;mismatch<10;++mismatch){Fixture f;f.press();const auto request=f.input.request();uint64_t ep=epoch;
  switch(mismatch){case 0:++ep;break;case 1:++f.view.recipient.slot;break;case 2:++f.view.recipient.instance;break;
   case 3:++f.view.recipient.character;break;case 4:++f.view.life;break;case 5:++f.state.identity.slot;break;
   case 6:++f.state.identity.instance;break;case 7:++f.state.identity.character;break;case 8:++f.state.life;break;case 9:f.view.evadeRequest=0;break;}
  check(f.input.acknowledge(f.view,f.state,ep,200)==Ack::none&&f.input.pending()&&f.input.request()==request,"wrong epoch/recipient/player/life/empty ACK leaves the request pending");
 }
 Fixture old;old.press();old.input.cancel();old.press(Kind::roll,300);old.view.evadeRequest=old.input.request()-1;
 check(old.acknowledge(400)==Ack::none&&old.input.pending(),"old ACK serial ignored even when player snapshot matches the new request");
 old.view.evadeRequest=old.input.request();check(old.acknowledge(401)==Ack::accepted,"later exact ACK accepts the original pending request");
 Fixture rejected;rejected.press();rejected.state.evadeSerial=0;rejected.state.evadeKind=Kind::none;
 check(rejected.acknowledge()==Ack::rejected&&!rejected.input.pending(),"processed request without matching admitted serial is rejected");
 Fixture wrongKind;wrongKind.press();wrongKind.state.evadeKind=Kind::backstep;check(wrongKind.acknowledge()==Ack::rejected,"different admitted kind is not the requested action");
 Fixture superseded;superseded.press();++superseded.view.evadeRequest;++superseded.state.evadeSerial;
 check(superseded.acknowledge()==Ack::rejected,"a later processed request does not masquerade as acceptance of this serial");
 Fixture completed;completed.press();completed.state.evadeKind=Kind::none;completed.state.evadeSerial=0;completed.state.evadeElapsedMs=0;
 check(completed.acknowledge()==Ack::rejected&&!completed.input.pending(),"footer-only ACK with no active action stops prediction; completed/rejected requests are never replayed");

 Fixture beforeTimeout;beforeTimeout.press();check(beforeTimeout.acknowledge(1599)==Ack::accepted,"ACK just before 1500ms timeout accepted");
 Fixture timedOut;timedOut.press();check(!timedOut.input.press(Kind::roll,1400),"held/repeated pending press cannot extend timeout");
 check(timedOut.acknowledge(1600)==Ack::expired&&!timedOut.input.pending(),"1500ms boundary expires pending request");
 timedOut.press(Kind::roll,1700);check(timedOut.input.request()==2,"timeout does not reuse an old serial");
 Fixture rollback;rollback.press();check(rollback.acknowledge(99)==Ack::expired&&!rollback.input.pending(),"clock rollback expires pending work");
 Fixture clockZero;clockZero.press(Kind::roll,0);check(clockZero.acknowledge(1)==Ack::accepted,"zero clock value is valid when request is pending");

 for(unsigned change=0;change<5;++change){Fixture f;f.press();f.input.cancel();f.press(Kind::backstep,300);auto id=self;auto ep=epoch;auto generation=life;
  switch(change){case 0:++ep;break;case 1:++id.slot;break;case 2:++id.instance;break;case 3:++id.character;break;case 4:++generation;break;}
  f.input.scope(ep,id,generation);check(!f.input.pending()&&f.input.kind()==Kind::none,"new epoch/full identity/life drops old pending request");
  check(f.input.press(Kind::roll,400)&&f.input.request()==1,"a genuinely different scope starts its own serial space");
  check(f.input.acknowledge(f.view,f.state,epoch,500)==Ack::none&&f.input.pending(),"old-scope ACK cannot acknowledge new-scope serial1");
 }

 // Explicit mapping protects gameplay from the selection-only IDs 15..18.
 using M=player::Motion;using P=PlayerMotion;
 const std::array<std::pair<M,P>,17> mappings{{{M::idle,P::Idle},{M::walk,P::Walk},{M::run,P::Run},
  {M::crouch_idle,P::CrouchIdle},{M::crouch_walk,P::CrouchWalk},{M::prone_idle,P::ProneIdle},
  {M::prone_forward,P::ProneForward},{M::prone_backward,P::ProneBackward},{M::supine_idle,P::SupineIdle},
  {M::supine_forward,P::SupineForward},{M::supine_backward,P::SupineBackward},
  {M::dead_prone,P::PlayDeadProne},{M::dead_supine,P::PlayDeadSupine},{M::aim,P::Aim},{M::reload,P::Reload},
  {M::roll,P::Roll},{M::backstep,P::Backstep}}};
 for(const auto& [input,expected]:mappings){const auto result=render_motion(input);check(result==expected,"explicit gameplay-to-asset motion mapping");check(unsigned(result)<15||unsigned(result)>=19,"gameplay mapping never selects PC-selection-only clips");}
 check(unsigned(P::Roll)==19&&unsigned(P::Backstep)==20,"new clips use reserved asset IDs19/20");
 check(unsigned(M::roll)==15&&render_motion(M::roll)!=P::SelectionSalute,"raw-cast collision with selection salute is avoided");
 check(render_motion(static_cast<M>(999))==P::Idle,"unknown gameplay motion fails closed to idle");
 std::cout<<"evade input scope/full identity/serial/ACK/timeout and explicit 17-motion mapping passed\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
