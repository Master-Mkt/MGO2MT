#include "remote_avatar.h"
#include "host_appearance.h"
#include "host_session.h"
#include "dedicated_peer.h"
#include <algorithm>
#include <iostream>
#include <limits>
using namespace mgo2mt;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
combat::Player body(unsigned slot){combat::Player p;p.identity={uint8_t(slot),uint16_t(256+slot),100+slot};p.hp=p.maxHp=p.stamina=p.maxStamina=1000;p.weapon=25;p.alive=true;return p;}
host::Player appearance(unsigned slot,unsigned gender=0){host::Player p{uint8_t(slot),uint16_t(256+slot),100+slot,"Fixture",""};p.appearance=std::array<uint8_t,28>{};(*p.appearance)[0]=uint8_t(gender);(*p.appearance)[2]=11;(*p.appearance)[3]=22;(*p.appearance)[15]=46;(*p.appearance)[17]=57;return p;}
void profile_and_wire(){
 std::vector<uint8_t>info(579),personal(245),skills(4);info[3]=100;info[4]='P';auto a=*appearance(1,1).appearance;
 for(unsigned i=0;i<28;++i)if(i<9||(i>=13&&i<27))a[i]=uint8_t(i+1);a[0]=1;
 personal[239]=22;std::copy_n(a.begin(),27,personal.begin()+49);auto profile=host::profile_payload(100,info,personal,skills);auto decoded=host::profile_names(profile);check(decoded.level==22&&decoded.appearance==a,"authenticated personal -> profile reverse mapping, all appearance bytes");
 host::Roster r;r.complete=true;auto p=appearance(1),q=appearance(2,1);p.level=22;q.level=0;r.slots[1]=p;r.slots[2]=q;r.slots[1]->appearance.reset();r.slots[2]->appearance.reset();
 auto b=host::appearance_record(std::array{p,q});host::update_appearance(r,b);check(r.slots[1]->appearance==p.appearance&&r.slots[2]->appearance==q.appearance&&r.slots[1]->level==22&&r.slots[2]->level==0,"two distinct appearances");auto revision=r.revision;host::update_appearance(r,b);check(r.revision==revision,"duplicate is idempotent");
 auto rejected=[&](auto bytes){auto previous=r;bool caught=false;try{host::update_appearance(r,bytes);}catch(const host::Invalid&){caught=true;}check(caught&&r==previous,"invalid appearance is atomic");};
 for(size_t size=0;size<b.size();++size){auto cut=b;cut.resize(size);rejected(cut);}auto bad=b;bad.push_back(0);rejected(bad);bad=b;bad[5]=3;rejected(bad);bad=b;bad[7+37]=1;rejected(bad);bad=b;bad[7+37+1]++;rejected(bad);bad=b;bad[7+7]=2;rejected(bad);bad=b;bad[7+7+9]=1;rejected(bad);
 auto legacy=b;legacy[5]=1;legacy.resize(7);for(size_t at=7;at<b.size();at+=37)legacy.insert(legacy.end(),b.begin()+at,b.begin()+at+35);
 host::update_appearance(r,legacy);check(!r.slots[1]->level&&!r.slots[2]->level,"GWAV1 gives unknown level without inventing one");host::update_appearance(r,b);
 auto invalidLevel=b;invalidLevel[42]=1;invalidLevel[43]=1;rejected(invalidLevel);
 host::Hello h{p.character,123,2,2,{}};host::update_roster(r,host::roster_record(p,h));check(r.slots[1]->level==22&&r.slots[1]->appearance==p.appearance,"same identity roster refresh keeps appearance");host::update_roster(r,host::roster_remove(p.instance));auto replacement=p;replacement.instance+=1000;replacement.character+=1000;host::update_roster(r,host::roster_record(replacement,{replacement.character,321,2,2,{}}));check(!r.slots[1]->level&&!r.slots[1]->appearance,"slot reuse cannot inherit appearance");rejected(b);
 std::vector<host::Player> all;for(unsigned i=0;i<24;++i)all.push_back(appearance(i,i%2));check(host::appearance_record(all).size()==895,"24 entries bounded under datagram limit");
}
void scene(){
 remote::Scene scene;combat::Snapshot s{1,1};host::Roster r;r.complete=true;
 for(unsigned i=0;i<24;++i){s.players[i]=body(i);r.slots[i]=appearance(i,i%2);}auto self=s.players[1]->identity;
 check(scene.update(s,r,self,0)&&scene.sample(0).size()==23,"24 slots with self excluded");auto first=scene.sample(0);check(first[1].appearance[0]==0,"remote uses own gender");
 s.players[2]->pose.feet={400,10,0};++s.revision;check(scene.update(s,r,self,200),"new pose");auto sample=scene.sample(250);auto find=[&](auto list,unsigned slot){auto it=std::find_if(list.begin(),list.end(),[&](auto&a){return a.identity.slot==slot;});check(it!=list.end(),"sample exists");return *it;};
 auto moved=find(sample,2);check(moved.origin==combat::Vec3{200,5,0}&&moved.motion==PlayerMotion::Walk,"100ms position interpolation and walk");check(find(scene.sample(700),2).motion==PlayerMotion::Idle,"missing motion updates never extrapolate walking");
 s.players[2]->pose.capsule={260,1100,2};++s.revision;scene.update(s,r,self,800);check(find(scene.sample(900),2).motion==PlayerMotion::CrouchIdle,"crouch capsule");
 s.players[2]->pose.capsule={260,560,2};++s.revision;scene.update(s,r,self,1000);check(find(scene.sample(1100),2).motion==PlayerMotion::ProneIdle,"prone capsule");
 s.players[2]->pose.capsule={260,1700,2};s.players[2]->reloadUntil=9999;s.players[2]->verifiedSkills=true;s.players[2]->masteryLevel=3;s.players[2]->reloadLevel=3;++s.revision;scene.update(s,r,self,1200);check(find(scene.sample(1300),2).motion==PlayerMotion::Reload,"host reload");check(std::abs(find(scene.sample(1300),2).seconds-.15)<.00001,"remote animation uses host latched mastery level3 rate");
 s.players[2]->hp=0;s.players[2]->alive=false;++s.revision;scene.update(s,r,self,1400);auto dead=find(scene.sample(1400),2);check(!dead.alive&&dead.motion==PlayerMotion::PlayDeadProne&&dead.seconds==1000,"dead snapshot maps to explicit static downed pose");auto old=s;
 s.players[2]=body(2);s.players[2]->life=2;s.players[2]->pose.feet={9000,0,0};++s.revision;scene.update(s,r,self,1600);auto revived=find(scene.sample(1600),2);check(revived.alive&&revived.life==2&&revived.origin[0]==9000&&revived.seconds==0,"life resets pose and animation, no old-death blend");old.revision=s.revision+1;check(!scene.update(old,r,self,1601)&&find(scene.sample(1601),2).life==2,"old-life replay rejected");
 s.players[2]->pose.yaw=3.13f;++s.revision;scene.update(s,r,self,1800);s.players[2]->pose.yaw=-3.13f;++s.revision;scene.update(s,r,self,2000);check(std::abs(find(scene.sample(2050),2).yaw)>3,"yaw crosses pi by shortest arc");
 r.slots[2].reset();scene.update(s,r,self,2100);check(scene.sample(2100).size()==22,"roster removal hides without waiting for combat revision");
 r.slots[2]=appearance(2);r.slots[2]->instance++;scene.update(s,r,self,2200);check(scene.sample(2200).size()==22,"slot occupant identity mismatch hidden");
 r.slots[2]=appearance(2,1);scene.update(s,r,self,2300);check(scene.sample(2300).size()==23&&find(scene.sample(2300),2).appearance[0]==1,"late appearance attaches at authoritative current position");
 s.epoch=2;s.revision=1;s.players[2]=body(2);scene.update(s,r,self,2400);check(find(scene.sample(2400),2).origin==combat::Vec3{}&&find(scene.sample(2400),2).life==1,"new epoch discards all old tracks");
 check(!scene.update(s,r,self,2399),"backward local clock rejected");
 s.players[2]->specialPhase=combat::SpecialPhase::start;++s.revision;check(scene.update(s,r,self,2420),"special start snapshot");check(find(scene.sample(2470),2).specialSeconds==.05,"remote special phase clock");
 s.players[2]->specialPhase=combat::SpecialPhase::hold;++s.revision;check(scene.update(s,r,self,2480),"special hold snapshot");check(find(scene.sample(2480),2).specialSeconds==0&&find(scene.sample(2480),2).specialPhase==combat::SpecialPhase::hold,"phase transition resets dedicated clock");
 r.complete=false;scene.update(s,r,self,2500);check(scene.sample(2500).empty(),"incomplete roster hidden");scene.clear();check(scene.sample(9000).empty(),"disconnect clears scene");
}
void reliable_transport(){
 host::Hello h{100,123,2,1,{{{192,0,2,1},5732}}},c{101,456,2,2,{{{192,0,2,2},5730}}};std::vector<uint8_t>profile(45);profile[0]=2;profile.insert(profile.end(),{'P',0});
 host::Machine client(c,h.character,profile,0);host::DedicatedPeer server(h,c,0);auto p=appearance(1,1);unsigned dropped=0;
 for(uint64_t now=0;now<3000;now+=10){for(auto&b:client.poll(now))server.receive(b,now);for(auto&e:server.events()){
   if(e[0]==2){server.queue(host::roster_record(appearance(0),h),now);server.queue(host::roster_record(p,c),now);server.queue({7,0,0,0,0,0,3},now);server.queue(host::appearance_record(std::span(&p,1)),now);}
   if(e[0]==10)server.queue(host::room_snapshot(std::array<host::Rotation,1>{{{20,1,0}}},1),now);}
  auto sends=server.poll(now);for(auto it=sends.rbegin();it!=sends.rend();++it){if(dropped++<2)continue;client.receive(*it,now);client.receive(*it,now);}}
 auto result=client.result();check(result.stage==host::Stage::joined&&!server.closed()&&result.roster.slots[1]->appearance==p.appearance,"encrypted reliable appearance survives loss/reorder/duplicate");
 server.queue(host::roster_remove(p.instance),3000);for(auto&b:server.poll(3000))client.receive(b,3000);check(!client.result().roster.slots[1],"local removal disconnect clears appearance");
}
void sideways_evasion(){
 remote::Scene scene;combat::Snapshot s{7,1};host::Roster r;r.complete=true;s.players[0]=body(0);s.players[1]=body(1);r.slots[0]=appearance(0);r.slots[1]=appearance(1);auto self=s.players[0]->identity;auto&p=*s.players[1];p.pose.yaw=.4f;
 check(scene.update(s,r,self,0),"Side roll fixture initial snapshot");
 auto sample=[&](uint64_t now){auto a=scene.sample(now);check(a.size()==1,"Exactly one scoped remote");return a.front();};
 for(auto kind:{combat::EvadeKind::rollLeft,combat::EvadeKind::rollRight}){p.pose.yaw=.4f+(kind==combat::EvadeKind::rollLeft?-1.57079633f:1.57079633f);p.evadeKind=kind;p.evadeSerial=unsigned(kind)-2;p.evadeElapsedMs=0;++s.revision;uint64_t now=1000*p.evadeSerial;check(scene.update(s,r,self,now),"Left/right kind is accepted through Replica");const float expected=.4f+(kind==combat::EvadeKind::rollLeft?-1.57079633f:1.57079633f);check(std::abs(sample(now).yaw-expected)<1e-5,"Remote body faces approved left/right direction at action start");
  auto before=sample(now+710);check(before.evadeSeconds>.7,"Remote action advances into recovery");p.evadeElapsedMs=660;++s.revision;check(scene.update(s,r,self,now+720),"Delayed side snapshot accepted");auto after=sample(now+720);check(after.evadeSeconds>=before.evadeSeconds&&std::abs(after.yaw-expected)<1e-5,"Delayed snapshot cannot rewind side recovery or heading");
  p.pose.yaw=.4f;p.evadeKind=combat::EvadeKind::none;p.evadeSerial=0;p.evadeElapsedMs=0;++s.revision;check(scene.update(s,r,self,now+800),"HOST ends side action");check(std::abs(sample(now+900).yaw-.4f)<1e-5,"HOST ended action returns ordinary heading");
 }
 p=body(1);p.life=2;++s.revision;check(scene.update(s,r,self,4000)&&sample(4000).evadeKind==combat::EvadeKind::none&&sample(4000).yaw==0,"New life clears side presentation");
}
int main(){try{profile_and_wire();scene();reliable_transport();sideways_evasion();std::cout<<"remote avatar profile/GWAV/identity/life/pose and encrypted reliable loss-reorder-duplicate passed\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
