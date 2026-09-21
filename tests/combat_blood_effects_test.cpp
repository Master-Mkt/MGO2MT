#include "combat_particle_effects.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt::combat;
namespace {
unsigned checks=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
struct Fixture {
 Snapshot snapshot;particles::Pool pool;Event event;
 Fixture(){snapshot.epoch=7;snapshot.eventWatermark=10;Player victim;victim.identity={1,12,123};victim.life=2;victim.alive=true;snapshot.players[1]=victim;pool.synchronize(7,1,10,1000);
  event.epoch=7;event.id=11;event.kind=EventKind::damage;event.target=victim.identity;event.targetLife=2;event.source={0,14,321};event.sourceLife=1;event.weapon=25;event.hpDamage=100;event.position={345,1500,-678};event.normal={0,0,-1};}
 void emit(uint64_t now=1000){snapshot.eventWatermark=(std::max)(snapshot.eventWatermark,event.id);pool.dispatch({&event,1},snapshot,now);}
};
void hit_point_and_role(){
 for(bool gekko:{false,true}){Fixture f;if(gekko)f.snapshot.players[1]->specialPc.kind=mgo2mt::special_pc::Kind::gekko;
  f.emit();auto initial=f.pool.sprites(f.snapshot,1000);check(f.pool.size()==1&&initial.size()==3,"one accepted HP hit emits bounded three original sprites");
  for(const auto&s:initial){check(s.position==f.event.position,"blood begins at supplied body surface contact, never feet or muzzle");check(!s.additive,"blood uses alpha blend, not emissive flash");}
  check(initial[0].texture==(gekko?0x056a8a:0x668997),"current target kind selects original human or Gekko image");
  check(initial[1].texture==(gekko?0x05728a:0x527461)&&initial[2].texture==(gekko?0x527461:0xfa8c28),"role's additional original references selected");
  auto moving=f.pool.sprites(f.snapshot,1100);check(moving[0].position[2]<f.event.position[2]&&moving[0].position[1]<f.event.position[1],"reverse shot direction and native gravity drive spray");
  check(f.event.position==Vec3{345,1500,-678},"display never mutates authoritative event");
  for(uint64_t time=1100;time<1000+(gekko?900:650);time+=47)for(const auto&s:f.pool.sprites(f.snapshot,time)){
   check(s.radius>0&&s.rgba[3]>=0&&s.rgba[3]<=1,"bounded radius and fading alpha");
   check(s.uv[0]>=0&&s.uv[1]>=0&&s.uv[2]<=1&&s.uv[3]<=1&&s.uv[0]<s.uv[2]&&s.uv[1]<s.uv[3],"all native atlas frames stay in original image");}
  check(f.pool.sprites(f.snapshot,1000+(gekko?900:650)).empty(),"exact role lifetime expires all blood");
 }
}
void lifecycle(){
 Fixture f;f.snapshot.players[1]->alive=false;f.emit();check(f.pool.size()==1,"lethal damage survives target death in same life");
 f.emit();check(f.pool.size()==1,"accepted event replay does not duplicate");
 f.event.id=13;f.emit();f.event.id=12;f.emit();check(f.pool.size()==3,"out of order current-scope damage accepted once");
 f.snapshot.players[1]->life=3;check(f.pool.sprites(f.snapshot,1001).empty(),"respawn removes previous-life blood");f.event.id=14;f.emit(1001);check(!f.pool.size(),"late previous-life damage rejected");
 f.event.targetLife=3;f.event.id=15;f.emit(1002);check(f.pool.size()==1,"new life can emit new accepted damage");
 f.snapshot.players[1]->identity.instance++;check(f.pool.sprites(f.snapshot,1003).empty(),"slot reuse removes old identity");
 Fixture leave;leave.emit();leave.snapshot.players[1].reset();check(leave.pool.sprites(leave.snapshot,1001).empty(),"departed victim removes blood");
 Fixture scene;scene.emit();scene.pool.synchronize(7,2,scene.snapshot.eventWatermark,1001);scene.emit(1001);check(!scene.pool.size(),"new scene watermark prevents old damage replay");
 Fixture rollback;rollback.emit();check(rollback.pool.sprites(rollback.snapshot,999).empty(),"clock rollback clears blood");rollback.emit(1000);check(!rollback.pool.size(),"clock rollback cannot replay prior event");
 Fixture epoch;epoch.emit();epoch.snapshot.epoch=8;check(epoch.pool.sprites(epoch.snapshot,1001).empty(),"epoch mismatch clears presentation");
}
void rejected_events(){
 for(unsigned which=0;which<12;++which){Fixture f;switch(which){
  case 0:f.event.hpDamage=0;f.event.staminaDamage=100;break;
  case 1:f.event.normal={};break;
  case 2:f.event.normal={0,0,-2};break;
  case 3:f.event.normal[0]=std::numeric_limits<float>::quiet_NaN();break;
  case 4:f.event.position[1]=std::numeric_limits<float>::infinity();break;
  case 5:f.event.targetLife++;break;
  case 6:f.event.target.character++;break;
  case 7:f.event.target.slot=255;break;
  case 8:f.event.epoch++;break;
  case 9:f.event.id=10;break;
  case 10:f.event.kind=EventKind::impact;break;
  case 11:f.event.kind=EventKind::death;break;
 }f.emit();check(f.pool.sprites(f.snapshot,1000).empty(),"invalid, historical, non-HP and non-damage events cannot emit blood");}
 Fixture watermark;watermark.pool.dispatch({&watermark.event,1},watermark.snapshot,1000);check(!watermark.pool.size(),"future event beyond snapshot watermark rejected");
 Fixture knife;knife.event.weapon=1;knife.emit();check(knife.pool.size()==1,"accepted damaging knife body contact uses same blood contract");
}
void bounded(){
 Fixture f;particles::Pool pool({4,1600,900,650,900});pool.synchronize(7,1,10,1000);
 for(unsigned i=0;i<100;++i){f.event.id=11+i;f.snapshot.eventWatermark=f.event.id;pool.dispatch({&f.event,1},f.snapshot,1000);check(pool.size()<=4,"mixed pool capacity bounds blood bursts");}
 check(pool.sprites(f.snapshot,1000).size()==12,"at most three sprites per retained burst");
 for(bool gekko:{false,true}){particles::Policy policy;if(gekko)policy.gekkoBloodMs=10001;else policy.bloodMs=0;bool rejected=false;try{particles::Pool invalid(policy);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid blood lifetime is rejected");}
}
}
int main(){try{hit_point_and_role();lifecycle();rejected_events();bounded();std::cout<<checks<<" blood hit point, original image roles, lethal/respawn lifecycle, replay and bounds PASS\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
