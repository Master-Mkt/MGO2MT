#include "combat_particle_effects.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace mgo2mt::combat::particles {
namespace {
bool finite(Vec3 p){for(float v:p)if(!std::isfinite(v)||std::abs(v)>=1000000)return false;return true;}
Vec3 plus(Vec3 a,Vec3 b){for(int i=0;i<3;++i)a[i]+=b[i];return a;}
Vec3 scaled(Vec3 a,float n){for(auto&v:a)v*=n;return a;}
std::optional<Vec3> unit(Vec3 p){if(!finite(p))return {};float d=std::hypot(p[0],p[1],p[2]);if(d<1e-6f)return {};return scaled(p,1/d);}
bool owner(const Snapshot&s,Identity id,uint32_t life){if(id.slot>=s.players.size()||!life)return false;const auto&p=s.players[id.slot];return p&&p->identity==id&&p->life==life&&p->alive;}
bool blood(Kind kind){return kind==Kind::humanBlood||kind==Kind::gekkoBlood;}
bool target(const Snapshot&s,Identity id,uint32_t life){if(id.slot>=s.players.size()||!id.instance||!id.character||!life)return false;const auto&p=s.players[id.slot];return p&&p->identity==id&&p->life==life;}
uint64_t duration(Kind kind,const Policy&p){switch(kind){case Kind::humanBlood:return p.bloodMs;case Kind::gekkoBlood:return p.gekkoBloodMs;case Kind::smokeCloud:return 12000;case Kind::explosion:return 2400;case Kind::flash:return 80;case Kind::smoke:return p.smokeMs;default:return p.casingMs;}}
}
bool has_muzzle(uint16_t w) noexcept{switch(w){case 2:case 3:case 4:case 7:case 8:case 9:case 10:case 12:case 15:case 16:case 18:case 20:case 22:case 23:case 24:case 25:case 26:case 30:case 31:case 35:case 37:case 38:case 39:case 41:case 42:case 43:case 44:case 50:case 103:case 104:case 128:case 129:return true;default:return false;}}
bool has_casing(uint16_t w) noexcept{return has_muzzle(w)&&w!=50&&w!=103&&w!=128&&w!=129;}
Pool::Pool(Policy p):policy_(p){if(!p.capacity||p.capacity>4096||!p.smokeMs||p.smokeMs>10000||!p.casingMs||p.casingMs>10000||!p.bloodMs||p.bloodMs>10000||!p.gekkoBloodMs||p.gekkoBloodMs>10000)throw std::invalid_argument("Native particle policy");}
const std::vector<weapon_effect::Emitter>* Pool::configured(const Entry&e)const{if(!config_)return nullptr;const char* channel=e.kind==Kind::flash?"muzzle":e.kind==Kind::humanBlood?"humanBlood":e.kind==Kind::gekkoBlood?"gekkoBlood":e.kind==Kind::smokeCloud?"smokeCloud":e.kind==Kind::explosion?"explosion":e.kind==Kind::casing?"casing":"smoke";return config_->particles(e.weapon,channel);}
uint64_t Pool::lifetime(const Entry&e)const{auto custom=configured(e);if(!custom)return duration(e.kind,policy_);auto n=weapon_effect::duration(*custom);return e.kind==Kind::casing?(std::max)(n,policy_.casingMs):n;}
void Pool::clear(){entries_.clear();seen_.clear();epoch_=scene_=floor_=now_=0;}
void Pool::synchronize(uint64_t epoch,uint64_t scene,uint64_t watermark,uint64_t now){if(!epoch||!scene){clear();return;}if(epoch_!=epoch||scene_!=scene){clear();epoch_=epoch;scene_=scene;floor_=watermark;}if(now<now_)entries_.clear();now_=now;}
void Pool::expire(const Snapshot&s,uint64_t now){if(s.epoch!=epoch_){entries_.clear();return;}if(now<now_)entries_.clear();now_=now;std::erase_if(entries_,[&](const Entry&e){return (blood(e.kind)?!target(s,e.owner,e.life):(e.kind!=Kind::explosion&&e.kind!=Kind::smokeCloud&&!owner(s,e.owner,e.life)))||now-e.born>=lifetime(e);});}
void Pool::dispatch(std::span<const Event> events,const Snapshot&s,uint64_t now,const CasingResolver& casing){expire(s,now);if(!epoch_||s.epoch!=epoch_)return;for(const auto&e:events){
 if(e.epoch!=epoch_||!e.id||e.id<=floor_||e.id>s.eventWatermark||!seen_.insert(e.id).second)continue;
 if(seen_.size()>4096){floor_=*seen_.begin();seen_.erase(seen_.begin());}
 if(!finite(e.position))continue;const auto legacy=source(e.weapon);
 if(e.kind==EventKind::damage){
  if(!e.hpDamage||!target(s,e.target,e.targetLife)||!finite(e.normal))continue;
  const float length=std::hypot(e.normal[0],e.normal[1],e.normal[2]);if(std::abs(length-1.f)>.001f)continue;
  const bool gekko=s.players[e.target.slot]->specialPc.kind==special_pc::Kind::gekko;
  if(entries_.size()==policy_.capacity)entries_.erase(entries_.begin());
  entries_.push_back({gekko?Kind::gekkoBlood:Kind::humanBlood,e.target,e.targetLife,e.position,scaled(e.normal,(gekko?1100.f:700.f)/length),now,e.id,e.weapon,e.normal});continue;
 }
 if(e.kind==EventKind::smoke){if(e.source.slot>=24||!e.source.instance||!e.source.character||!e.sourceLife||legacy<56||legacy>59)continue;if(entries_.size()==policy_.capacity)entries_.erase(entries_.begin());entries_.push_back({Kind::smokeCloud,e.source,e.sourceLife,e.position,{},now,e.id,e.weapon,e.normal});continue;}
 if(e.kind==EventKind::explosion){if(e.source.slot>=24||!e.source.instance||!e.source.character||!e.sourceLife)continue;if(entries_.size()==policy_.capacity)entries_.erase(entries_.begin());entries_.push_back({Kind::explosion,e.source,e.sourceLife,e.position,{},now,e.id,e.weapon,e.normal});continue;}
 if(!owner(s,e.source,e.sourceLife))continue;
 Entry p{Kind::smoke,e.source,e.sourceLife,e.position,{},now,e.id,e.weapon,e.normal};
 if(e.kind==EventKind::projectileTrail&&(legacy==50||legacy==103||legacy==129)){p.velocity={float(int(e.id%11)-5)*12,110,0};}
 else if(e.kind==EventKind::shot&&(has_muzzle(legacy)||(config_&&config_->particles(e.weapon,"muzzle")))){
  auto forward=unit(e.normal);if(!forward)continue;auto right=unit(Vec3{(*forward)[2],0,-(*forward)[0]});if(!right)right=Vec3{1,0,0};
  {
   // Caller resolves original CNP_mzf_def after hand skinning. Native flash
   // geometry and a short smoke puff; accepted events only, never trigger input.
   auto emit=[&](Entry q){if(entries_.size()==policy_.capacity)entries_.erase(entries_.begin());entries_.push_back(q);};
   emit({Kind::flash,e.source,e.sourceLife,e.position,scaled(*forward,1),now,e.id,e.weapon,e.normal});
   emit({Kind::smoke,e.source,e.sourceLife,e.position,{0,100,0},now,e.id,e.weapon,e.normal});
  }
  if(!has_casing(legacy))continue;
  p.kind=Kind::casing;p.direction=*right;p.origin=plus(e.position,plus(scaled(*right,100),Vec3{0,-100,0}));p.velocity=plus(scaled(*right,1000),Vec3{0,650,0});
  // D3D538 / DF67E0 use original slot3 (0x443037) position and +Z.
  // Native flight speed/gravity remain presentation policy, not recovered
  // original case-body physics. Missing/unverified models retain fallback.
  if(casing)if(auto emission=casing(e);emission&&finite(emission->origin))if(auto direction=unit(emission->direction)){p.origin=emission->origin;p.velocity=scaled(*direction,std::hypot(1000.f,650.f));p.direction=*direction;}
 }else continue;
 if(entries_.size()==policy_.capacity)entries_.erase(entries_.begin());entries_.push_back(p);
}}
std::vector<Segment> Pool::sample(const Snapshot&s,uint64_t now){expire(s,now);std::vector<Segment> out;out.reserve(entries_.size());for(const auto&e:entries_){if(configured(e)||e.kind==Kind::explosion||e.kind==Kind::smokeCloud)continue;float t=float(now-e.born)/1000.f;float alpha=1-float(now-e.born)/float(e.kind==Kind::flash?80:e.kind==Kind::smoke?policy_.smokeMs:policy_.casingMs);auto p=plus(e.origin,scaled(e.velocity,t));Segment line;line.kind=e.kind;
 if(blood(e.kind)){const bool gekko=e.kind==Kind::gekkoBlood;line.from=p;line.to=plus(p,scaled(e.velocity,.025f));line.rgba={.65f,.025f,.02f,1-float(now-e.born)/float(duration(e.kind,policy_))};line.widthPixels=gekko?4.f:2.f;out.push_back(line);continue;}
 if(e.kind==Kind::flash){float radius=std::hypot(e.velocity[0],e.velocity[1],e.velocity[2])>0?65.f:230.f;for(auto axis:std::array<Vec3,3>{{{radius,0,0},{0,radius,0},{0,0,radius*1.4f}}}){line.from=plus(p,scaled(axis,-1));line.to=plus(p,axis);line.rgba={1.f,.82f,.3f,alpha};line.widthPixels=5;out.push_back(line);}continue;}
 if(e.kind==Kind::casing){p[1]-=4900*t*t;if(e.landed)p=e.landedPosition;float angle=t*24+float(e.id%17);Vec3 axis{std::cos(angle)*18,std::sin(angle)*18,8};line.from=plus(p,scaled(axis,-1));line.to=plus(p,axis);line.rgba={.92f,.70f,.24f,alpha};line.widthPixels=2;}
 else{float width=40+100*t;line.from=plus(p,Vec3{-width,0,0});line.to=plus(p,Vec3{width,40+60*t,0});line.rgba={.65f,.65f,.65f,alpha*.65f};line.widthPixels=3+4*t;}
 if(finite(line.from)&&finite(line.to))out.push_back(line);
 }return out;}
std::vector<Event> Pool::casing_contacts(const Snapshot&s,uint64_t now,const stage::Collision&world){
 expire(s,now);std::vector<Event>out;for(auto&e:entries_){if(e.kind!=Kind::casing||e.landed)continue;
  // Sound is independent of visibility/alpha. An empty visual channel retains
  // the original physical ejection trajectory; the sound setting can mute it.
  std::vector<weapon_effect::Emitter>trajectory;if(auto custom=configured(e)){trajectory=*custom;bool selected=false;for(auto&layer:trajectory){if(!layer.enabled)continue;if(selected){layer.enabled=false;continue;}selected=true;layer.count=1;layer.emissionMs=0;layer.color[3]=1;layer.alphaRandom=0;layer.alphaCurve={{0,1},{1,1}};layer.radius=1;layer.sizeRandom=0;layer.sizeCurve={{0,1},{1,1}};}if(!selected)trajectory.clear();}
  const uint64_t age=now-e.born;auto point=[&](uint64_t ms)->std::optional<Vec3>{if(!trajectory.empty()){auto points=weapon_effect::sample(trajectory,e.origin,e.direction,e.id^(epoch_*0x9e3779b97f4a7c15ull),ms);return points.empty()?std::nullopt:std::optional<Vec3>(points.front().position);}float t=float(ms)*.001f;auto p=plus(e.origin,scaled(e.velocity,t));p[1]-=4900*t*t;return p;};
  uint64_t a=e.contactAge;for(unsigned steps=0;a<age&&steps<4096;++steps){auto b=(std::min)(a+20,age);auto from=point(a),to=point(b);if(from&&to){auto delta=plus(*to,scaled(*from,-1));float distance=std::hypot(delta[0],delta[1],delta[2]);if(distance>1e-6f)if(auto hit=world.ray(*from,scaled(delta,1/distance),distance,{0,stage::attribute::floor|stage::attribute::sound,stage::attribute::through|stage::attribute::recoil})){e.landed=true;e.landedPosition=hit->position;Event contact;contact.epoch=epoch_;contact.id=e.id;contact.kind=EventKind::shot;contact.source=e.owner;contact.sourceLife=e.life;contact.weapon=e.weapon;contact.position=hit->position;contact.normal=hit->normal;out.push_back(contact);break;}}a=b;}e.contactAge=age;
 }return out;
}
std::vector<Sprite> Pool::sprites(const Snapshot&s,uint64_t now){expire(s,now);std::vector<Sprite> out;if(!epoch_||s.epoch!=epoch_)return out;out.reserve(entries_.size()*2);
 for(const auto&e:entries_){if(out.size()>=8192)break;if(auto custom=configured(e)){auto sampled=weapon_effect::sample(*custom,e.origin,e.direction,e.id^(epoch_*0x9e3779b97f4a7c15ull),now-e.born);for(const auto&q:sampled){if(out.size()>=8192)break;Sprite p;p.position=e.landed?e.landedPosition:q.position;p.radius=q.radius;p.rotation=q.rotation;p.rgba=q.rgba;p.texture=q.texture;p.uv=q.uv;p.additive=q.additive;p.stretch=q.stretch;out.push_back(p);}continue;}if(e.kind==Kind::casing)continue;const auto legacy=source(e.weapon);const float t=float(now-e.born)*.001f;Sprite p;p.position=plus(e.origin,scaled(e.velocity,t));p.rotation=float(e.id%31)*.20268f;
  auto atlas=[](unsigned frame,unsigned columns,unsigned rows){float x=float(frame%columns)/columns,y=float(frame/columns)/rows;return std::array<float,4>{x,y,x+1.f/columns,y+1.f/rows};};
  if(blood(e.kind)){
   // Exact original images; animation speed, tint and ballistic spread below
   // are native presentation. Incoming normal is HOST's reverse shot ray,
   // not a claim that the original CPEF emitter/curves have been recovered.
   const bool gekko=e.kind==Kind::gekkoBlood;const float age=float(now-e.born)/float(duration(e.kind,policy_));
   p.position[1]-=900*t*t;p.texture=gekko?0x056a8a:0x668997;p.radius=(gekko?180.f:65.f)+(gekko?400.f:160.f)*age;
   p.rgba={.65f,.025f,.02f,(gekko?.8f:.9f)*(1-age)};
   if(!gekko)p.uv=atlas((std::min)(15u,unsigned(t*60)),4,4);out.push_back(p);
   for(unsigned n=0;n<2;++n){auto q=p;const float side=n?1.f:-1.f;q.position=plus(e.origin,scaled(e.velocity,t*(n?1.35f:.7f)));q.position[0]+=side*t*(gekko?260.f:130.f);q.position[1]+=(n?140.f:260.f)*t-1400*t*t;q.position[2]-=side*t*90;
    q.texture=gekko?(n?0x527461:0x05728a):(n?0xfa8c28:0x527461);q.radius=!gekko&&n?8.f+12.f*age:p.radius*(n?.65f:.45f);q.rotation+=side*.7f;
    q.uv=gekko?(n?atlas(unsigned(e.id%4),2,2):std::array<float,4>{0,0,1,1}):(n?std::array<float,4>{0,0,1,1}:atlas(unsigned(e.id%4),2,2));out.push_back(q);
   }continue;
  }
  if(e.kind==Kind::smokeCloud){p.texture=0x8695e3;p.radius=400+(std::min)(1800.f,t*500);const float alpha=(std::min)(1.f,t*2)*(std::min)(1.f,(12-t)/2)*.65f;p.rgba={1,1,1,alpha};if(legacy==57)p.rgba={1,.15f,.12f,alpha};if(legacy==58)p.rgba={.15f,1,.2f,alpha};if(legacy==59)p.rgba={1,.9f,.15f,alpha};for(unsigned n=0;n<4;++n){auto q=p;float a=float(n)*1.57079633f;q.position[0]+=std::cos(a)*p.radius*.45f;q.position[2]+=std::sin(a)*p.radius*.45f;q.position[1]+=p.radius*.7f+float(n%2)*150;q.uv=atlas(n,2,2);out.push_back(q);}continue;}
  if(e.kind==Kind::flash){p.texture=0x090aec;p.radius=100;p.rgba={1,.78f,.35f,1-float(now-e.born)/80.f};p.uv=atlas(unsigned(e.id%4),2,2);p.additive=true;}
  else if(e.kind==Kind::smoke){p.texture=0xca92b7;p.radius=50+180*t;p.rgba={1,1,1,.55f*(1-float(now-e.born)/float(policy_.smokeMs))};p.uv=atlas((std::min)(31u,unsigned(t*20)),8,4);}
  else{if(legacy==54){if(t>=.35f)continue;p.texture=0x0ee318;p.radius=1000;p.rgba={1,1,1,1-t/.35f};p.additive=true;out.push_back(p);continue;}p.position[1]+=t*350;p.texture=0xcaa2b5;p.radius=700+700*t;p.rgba={1,1,1,.8f*(1-t/2.4f)};p.uv=atlas(unsigned(e.id%4),2,2);out.push_back(p);if(t>=.6f||legacy==55||legacy==63||legacy==65||legacy==67)continue;p.texture=legacy==103?0xd1a31e:0x510d60;p.radius=(legacy==103?800:400)+1600*t;p.rgba={1,1,1,1-t/.6f};p.uv=atlas((std::min)(3u,unsigned(t*6.66f)),2,2);p.additive=true;}
  if(legacy==103){if(e.kind==Kind::flash)p.radius=350;else if(e.kind==Kind::smoke){p.radius=150+350*t;p.uv=atlas(unsigned(e.id%4),2,2);}}
  if(auto found=textures_.find(e.weapon);found!=textures_.end()){if(e.kind==Kind::flash)p.texture=found->second.first;else if(e.kind==Kind::smoke)p.texture=found->second.second;}
  out.push_back(p);
 }
 // Original flame atlas; native placement/phase follows authoritative burning.
 for(const auto&player:s.players)if(player&&player->alive&&player->burning){for(unsigned n=0;n<4;++n){const unsigned frame=unsigned((now/60+n*4)%16);const float phase=float((now+n*173)%900)/900.f;Sprite p;p.position=player->pose.feet;p.position[0]+=std::sin(float(n)*1.5708f)*player->pose.capsule.radius*.5f;p.position[2]+=std::cos(float(n)*1.5708f)*player->pose.capsule.radius*.5f;p.position[1]+=200+phase*player->pose.capsule.height*.65f;p.radius=250;p.texture=0x15c903;p.uv={float(frame%4)*.25f,float(frame/4)*.25f,float(frame%4+1)*.25f,float(frame/4+1)*.25f};p.rgba={1,1,1,.85f};p.additive=true;out.push_back(p);}}
 if(out.size()>8192)out.resize(8192);return out;
}
}
