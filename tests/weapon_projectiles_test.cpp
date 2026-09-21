#include "weapon_projectiles.h"
#include "weapon_extension_policy.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt::projectile;
static void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);}
static const Scope scope{7,12};static const Owner owner{1,2,99,1};
static Shot shot(uint64_t id=1,uint16_t weapon=50){return {scope,owner,id,weapon,{0,100,0},{0,0,1}};}
static Trace clear=[](Vec3,Vec3,float,Owner)->std::optional<Contact>{return {};};
static void membership(){Pool p(1);p.reset(scope);auto s=shot();
 check(p.spawn(s,native_rpg7,0)==Submit::accepted,"first shot");
 check(p.spawn(s,native_rpg7,0)==Submit::sequence,"no duplicate");
 check(p.spawn(shot(2),native_rpg7,0)==Submit::capacity,"capacity before sequence consumption");
 p.remove({owner.slot,owner.instance,owner.character,2});check(p.size()==1,"other life cannot remove");
 p.remove(owner);check(!p.size(),"remove exact");check(p.spawn(shot(2),native_rpg7,0)==Submit::accepted,"capacity transaction preserved");
 p.reset({8,12});check(p.spawn(s,native_rpg7,0)==Submit::scope,"old epoch");p.reset(scope);
 s.owner.life=0;check(p.spawn(s,native_rpg7,0)==Submit::identity,"life zero");s=shot();s.direction[0]=1;
 check(p.spawn(s,native_rpg7,0)==Submit::invalid,"unit direction");s=shot();s.origin[0]=NAN;
 check(p.spawn(s,native_rpg7,0)==Submit::invalid,"finite origin");s=shot();check(p.spawn(s,native_mk2,0)==Submit::profile,"profile identity");
 check(p.spawn(s,native_rpg7,UINT64_MAX)==Submit::clock,"deadline overflow");
}
static void contact_and_trail(){Pool p;p.reset(scope);check(p.spawn(shot(),native_rpg7,100)==Submit::accepted,"spawn rocket");
 Trace wall=[](Vec3 from,Vec3 direction,float maximum,Owner source)->std::optional<Contact>{
  check(source==owner,"source identity passed");if(direction[2]<=0)return {};const float d=(1500-from[2])/direction[2];
  if(d>=0&&d<=maximum)return Contact{d,{0,0,-1},{},45};return {};};
 auto a=p.advance(140,wall);check(a.impacts.empty()&&a.trails.size()==2&&p.size()==1,"rocket travels, no instant explosion");
 check(std::abs(a.trails[0].position[2]-600)<.01,"smoke distance");
 auto b=p.advance(200,wall);check(b.impacts.size()==1&&p.size()==0,"first wall consumes missile");
 check(b.impacts[0].owner==owner&&b.impacts[0].scope==scope&&b.impacts[0].acceptedShotId==1&&b.impacts[0].object==45,"impact provenance");
 check(std::abs(b.impacts[0].position[2]-1500)<.01&&!b.impacts[0].fuse,"wall impact position");
 check(p.advance(250,wall).impacts.empty(),"impact exactly once");check(p.spawn(shot(),native_rpg7,250)==Submit::sequence,"finished shot replay blocked");
 p.reset(scope);check(p.spawn(shot(1,2),native_mk2,0)==Submit::accepted,"MK2 flight");
 Owner target{2,3,100,7};Trace body=[target](Vec3,Vec3,float maximum,Owner)->std::optional<Contact>{return Contact{maximum*.5f,{0,0,-1},target,0};};
 auto dart=p.advance(20,body);check(dart.impacts.size()==1&&dart.impacts[0].weapon==2&&dart.impacts[0].target==target,"dart target full identity");check(dart.trails.empty(),"dart no rocket smoke");
}
static void fuse_and_failure(){Pool p;p.reset(scope);check(p.spawn(shot(1,53),native_white_phosphorus,0)==Submit::accepted,"WP spawn");
 unsigned contacts=0;Trace floor=[&](Vec3 from,Vec3 d,float max,Owner)->std::optional<Contact>{++contacts;
  if(d[1]>=0)return {};float distance=-from[1]/d[1];if(distance>=0&&distance<=max)return Contact{distance,{0,1,0},{},1};return {};};
 for(uint64_t t=100;t<3000;t+=100){auto r=p.advance(t,floor);check(r.impacts.empty(),"WP bounce does not prematurely explode");}
 auto r=p.advance(3000,floor);check(r.impacts.size()==1&&r.impacts[0].fuse&&r.impacts[0].weapon==53,"fuse exactly due");check(contacts>10&&!p.size(),"bounded collision steps");
 p.reset(scope);p.spawn(shot(),native_rpg7,100);check(p.advance(99,clear).discarded==1&&!p.size(),"rollback cancels without impact");
 p.reset(scope);p.spawn(shot(),native_rpg7,0);check(p.advance(1001,clear).discarded==1,"huge gap cannot skip wall");
 p.reset(scope);p.spawn(shot(),native_rpg7,0);Trace invalid=[](Vec3,Vec3,float max,Owner)->std::optional<Contact>{return Contact{max+1,{0,1,0},{},0};};
 check(p.advance(20,invalid).discarded==1,"invalid trace result cancels");
 p.reset(scope);p.spawn(shot(),native_rpg7,0);Trace throws=[](Vec3,Vec3,float,Owner)->std::optional<Contact>{throw std::runtime_error("scene unavailable");};
 check(p.advance(20,throws).discarded==1,"trace exception isolated");
 p.reset(scope);auto shortFlight=native_rpg7;shortFlight.range=100;check(p.spawn(shot(),shortFlight,0)==Submit::accepted,"short flight");
 r=p.advance(20,clear);check(r.impacts.empty()&&r.discarded==1,"range expiry no fake impact");
}
static void ammo(){Ammo empty{10,0,30};auto a=refill(empty);check(a&&*a==Ammo{10,10,20},"empty magazine plus reserve");
 check(empty==Ammo{10,0,30},"transaction does not change input");a=refill({10,4,3});check(a&&*a==Ammo{10,7,0},"finite partial refill");
 check(!refill({10,10,30})&&!refill({10,0,0})&&!refill({10,11,2}),"full dry invalid refuse");
 a=consume({1,1,3});check(a&&*a==Ammo{1,0,3},"rocket one round");check(!consume(*a),"no empty shot");
 a=refill(*a);check(a&&*a==Ammo{1,1,2},"rocket reload conservation");
 using namespace mgo2mt::weapon_extensions;
 check(original_base_damage(2,1000)==Damage{0,245},"original MK2 stamina only");
 check(original_base_damage(50,1000)==Damage{1125,0},"original RPG HP base");
 check(original_base_damage(2,500)==Damage{0,122}&&original_base_damage(50,0)==Damage{1125,0},"force integer/default");
 check(!original_base_damage(0x01000002,1000)&&!original_base_damage(3,1000)&&!original_base_damage(2,-1),"unknown variant/force refused");
}
int main(){try{membership();contact_and_trail();fuse_and_failure();ammo();std::cout<<"PASS scoped projectiles, finite flight, smoke, WP fuse/bounce, rejection and ammo conservation\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
