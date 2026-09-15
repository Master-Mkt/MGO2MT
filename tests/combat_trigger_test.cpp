#include "combat_service.h"
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace mgo2win;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Match {
 // Nine independent matches live for the whole test. Keep their Services
 // on the heap so their Authority state does not exhaust Windows' 1 MiB stack.
 combat::Identity id{1,1,100};std::unique_ptr<combat::Service> hostStorage=std::make_unique<combat::Service>(1);combat::Service& host=*hostStorage;uint32_t sequence=0;
 explicit Match(bool automatic=true){
  auto floor=std::make_shared<const stage::Collision>(stage::Collision::make({{-20000,0,-20000},{20000,0,-20000},{20000,0,20000},{-20000,0,20000}},{{{0,1,2}},{{0,2,3}}}));
  // Synthetic timing and ammunition, deliberately unrelated to real AK102.
  combat::Weapon gun{23,10,0,100,300,20,40,10000,1,0,automatic};gun.reloadRefillMs=200;
  host.configure(floor,std::array{gun});check(host.admit(id),"admit");
  check(host.authority().join(id,1,{{0,2,0}},100,100,std::array<uint16_t,1>{23},0),"grant");
  host.authority().active(true);check(host.receive(id,combat::wire::encode(combat::wire::Accept{1}),0),"accept");host.deliveries();
 }
 void input(uint64_t now,bool held=false,bool pressed=false,bool reload=false,bool suspended=false,combat::Pose pose={{0,2,0}},uint16_t weapon=23){
  check(host.receive(id,combat::wire::encode(combat::wire::Input{1,++sequence,pose,weapon,held,reload,pressed,suspended}),now),"input accepted for authority validation");
 }
 void tick(uint64_t now){host.poll(now);host.deliveries();}
 combat::Player player(){return *host.authority().snapshot().players[id.slot];}
};
}
int main(){try{
 {
  Match aiming;combat::wire::Input in;in.epoch=1;in.sequence=1;in.pose={{0,2,0}};in.weapon=23;in.aiming=true;
  auto bytes=combat::wire::encode(in);check(bytes[5]==19&&std::get<combat::wire::Input>(combat::wire::decode(bytes)).aiming,"Aim state round trip");
  check(aiming.host.receive(aiming.id,bytes,100),"Aim input admitted");aiming.tick(100);check(aiming.player().aiming&&aiming.player().ammo==20,"Aim presentation does not fire");
  combat::wire::Frame frame;frame.status=combat::wire::Status::active;frame.snapshot=aiming.host.authority().snapshot();auto copied=std::get<combat::wire::Frame>(combat::wire::decode(combat::wire::encode(frame)));check(copied.snapshot.players[1]->aiming,"Remote aim state survives snapshot");
  auto old=bytes;old[5]=17;check(!combat::wire::recognized(old),"Older peers rejected explicitly");
  auto up=in;up.sequence=2;up.aiming=false;check(!combat::wire::coalesce_input(in,up).aiming,"Latest aim release wins");
  auto reload=in;reload.sequence=2;reload.aiming=false;reload.reload=true;check(!combat::wire::coalesce_input(in,reload).aiming,"Reload cancels aim");
  check(aiming.host.receive(aiming.id,combat::wire::encode(up),150),"Aim release admitted");aiming.tick(150);check(!aiming.player().aiming,"Aim release applied");
  in.sequence=3;check(aiming.host.receive(aiming.id,combat::wire::encode(in),200),"Second aim admitted");aiming.tick(200);aiming.tick(2000);check(!aiming.player().aiming,"Timed-out aim clears");
 }
 Match automatic;automatic.input(100,true);automatic.tick(100);automatic.tick(199);check(automatic.player().ammo==19,"interval enforced on host ticks");
 automatic.tick(200);automatic.tick(300);check(automatic.player().ammo==17,"held automatic fires without additional packets");
 automatic.input(350);automatic.tick(350);automatic.tick(450);check(automatic.player().ammo==17,"newest release stops automatic fire");
 Match tap;tap.input(100,true,true);tap.input(100);tap.tick(100);tap.tick(200);check(tap.player().ammo==19,"coalesced tap is one shot, not a held trigger");
 Match semi(false);semi.input(100,true,true);semi.tick(100);semi.input(200,true);semi.tick(200);semi.tick(300);check(semi.player().ammo==19,"semiautomatic requires a new edge");
 semi.input(310);semi.tick(310);semi.input(320,true,true);semi.tick(320);check(semi.player().ammo==18,"release then press rearms semi");
 semi.input(330,false,true);semi.tick(330);semi.tick(420);check(semi.player().ammo==17,"single short press survives interval rejection once");semi.tick(520);check(semi.player().ammo==17,"short press is consumed once");
 Match stale;stale.input(100,true);stale.tick(100);stale.tick(590);check(stale.player().ammo==18,"delayed tick never catches up multiple shots");stale.tick(600);stale.tick(1000);check(stale.player().ammo==18,"input age boundary stops held fire");
 Match invalid;invalid.input(100,true);invalid.tick(100);invalid.input(200,true,false,false,false,{{10000,2,0}});invalid.tick(200);invalid.tick(300);check(invalid.player().ammo==19,"rejected pose also cancels previous held fire");
 Match weapon;weapon.input(100,true);weapon.tick(100);weapon.input(200,true,false,false,false,{{0,2,0}},999);weapon.tick(200);weapon.tick(300);check(weapon.player().ammo==19,"unowned weapon cancels held fire");
 Match semiLag(false);semiLag.input(100,true,true);semiLag.tick(100);semiLag.tick(600);semiLag.input(700,true);semiLag.tick(700);check(semiLag.player().ammo==19,"recovered held packet after timeout cannot invent a semiautomatic edge");
 Match pause;pause.input(100,true,true);pause.input(100,false,false,false,true);pause.tick(100);pause.tick(200);check(pause.player().ammo==20,"focus/menu suspend cancels queued short press");
 pause.input(210,true);pause.tick(210);pause.host.authority().active(false);pause.tick(220);pause.host.authority().active(true);pause.tick(320);check(pause.player().ammo==19,"round pause cannot leave an armed trigger");
 Match reload;reload.input(100,true);reload.tick(100);reload.input(120,false,false,true);reload.tick(120);reload.input(150,true);reload.tick(150);reload.tick(320);check(reload.player().ammo==20&&reload.player().reserve==39&&reload.player().reloadUntil==420,"refill precedes motion completion without shooting");reload.tick(419);reload.tick(420);check(reload.player().ammo==19&&!reload.player().reloadUntil,"held automatic resumes at host reload completion");
 // Both sender queues use this wire operation: preserve a pulse and latest
 // position, preserve release, and let suspension supersede every action.
 combat::wire::Input down{1,1,{{0,2,0}},23,true,false,true},up{1,2,{{0,2,20}},23};
 auto merged=combat::wire::coalesce_input(down,up);check(merged.pose.feet[2]==20&&!merged.fire&&merged.firePressed,"sender preserves tap and release separately");
 up.suspended=true;merged=combat::wire::coalesce_input(merged,up);check(merged.suspended&&!merged.firePressed,"sender suspension cancels pending actions");
 auto encoded=combat::wire::encode(down);auto old=encoded;old[5]=2;bool rejected=false;try{combat::wire::decode(old);}catch(const combat::wire::Invalid&){rejected=true;}check(rejected,"v2 cannot be decoded as v14 input");
 // GWCB14 preserves the flags prefix and appends ladder action1/anchor2/axis4.
 constexpr size_t flagsOffset=7+8+4+24+2+1+4;check(encoded.size()==flagsOffset+1+4+7+5+7&&encoded[flagsOffset]==5,"fixture identifies flags before life/cover/special-PC/ladder tails");
 check(std::get<combat::wire::Input>(combat::wire::decode(encoded))==down,"neutral ladder preserves complete trigger input");
 auto missingLadder=encoded;missingLadder.resize(missingLadder.size()-7);rejected=false;try{combat::wire::decode(missingLadder);}catch(const combat::wire::Invalid&){rejected=true;}check(rejected,"v14 rejects old input extent instead of defaulting absent ladder");
 auto ladderWithFire=down;ladderWithFire.ladder={mgo2win::ladder::Action::enter,1,0};rejected=false;try{combat::wire::encode(ladderWithFire);}catch(const combat::wire::Invalid&){rejected=true;}check(rejected,"ladder and trigger cannot share active input");
 for(unsigned flags=0;flags<256;++flags){auto bytes=encoded;bytes[flagsOffset]=uint8_t(flags);bool accepted=true;try{combat::wire::decode(bytes);}catch(const combat::wire::Invalid&){accepted=false;}check(accepted==(flags==0||flags==1||flags==2||flags==4||flags==5||flags==8||flags==16||flags==32||flags==48||flags==64||flags==65||flags==68||flags==69),"only unambiguous input flags are accepted");}
 std::cout<<"host trigger scheduling: automatic, semi, release, coalesced pulse, reload, timeout, rejected pose/weapon, suspend, native v14 passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
