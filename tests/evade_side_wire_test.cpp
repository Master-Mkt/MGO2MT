#include "combat_wire.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2mt::combat;
namespace {void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
template<class F>bool rejects(F f){try{f();}catch(const wire::Invalid&){return true;}return false;}}
int main(){try{
 wire::Frame frame;frame.snapshot.epoch=1;frame.snapshot.revision=1;frame.status=wire::Status::active;
 for(unsigned i=0;i<24;++i){Player p;p.identity={uint8_t(i),uint16_t(i+1),100+i};p.life=1;p.hp=p.maxHp=p.stamina=p.maxStamina=1000;p.alive=true;p.team=uint8_t(i%3);p.pose.feet={float(i*2000),4,0};p.weapon=25;p.ammo=30;p.reserve=90;p.evadeKind=EvadeKind::backstep;p.evadeSerial=7;p.evadeElapsedMs=123;p.burning=(i%2)!=0;frame.snapshot.players[i]=p;}
 const auto before=wire::encode(frame);check(before.size()<=2000,"24-player base bound");
 for(unsigned i=0;i<24;++i)frame.snapshot.players[i]->evadeKind=i%2?EvadeKind::rollLeft:EvadeKind::rollRight;
 const auto bytes=wire::encode(frame);check(bytes.size()==before.size(),"side kind uses spare bit without expanding packets");
 const auto decoded=std::get<wire::Frame>(wire::decode(bytes));check(decoded==frame,"24 alternating side rolls preserve all fields");Replica replica;check(replica.snapshot(decoded.snapshot),"replica accepts both side actions");
 for(unsigned kind=0;kind<8;++kind){wire::Input input;input.epoch=1;input.weapon=25;input.evadeKind=EvadeKind(kind);input.evadeRequest=kind?1:0;
  if(kind<=4)check(std::get<wire::Input>(wire::decode(wire::encode(input)))==input,"input side request roundtrip");else check(rejects([&]{wire::encode(input);}),"unknown input action rejected");
  auto invalid=frame;invalid.snapshot.players[0]->evadeKind=EvadeKind(kind);if(kind==0){invalid.snapshot.players[0]->evadeSerial=0;invalid.snapshot.players[0]->evadeElapsedMs=0;}
  if(kind<=4)check(std::get<wire::Frame>(wire::decode(wire::encode(invalid)))==invalid,"snapshot kind roundtrip");else check(rejects([&]{wire::encode(invalid);}),"unknown snapshot action rejected");
 }
 auto invalid=frame;invalid.snapshot.players[0]->cover.attached=true;check(rejects([&]{wire::encode(invalid);}),"side roll cannot overlap wall cover");
 invalid=frame;invalid.snapshot.players[0]->reloadUntil=99;check(rejects([&]{wire::encode(invalid);}),"side roll cannot overlap reload");
 std::cout<<"GWCB18 left/right input and 24-player compact frame PASS bytes="<<bytes.size()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
