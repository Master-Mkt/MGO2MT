#include "combat_service.h"
#include "host_skill_wire.h"
#include "combat_wire_budget.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
namespace hs=mgo2mt::host_skills;
namespace {
void check(bool v,const char*message){if(!v)throw std::runtime_error(message);}
void put(std::vector<uint8_t>&b,size_t at,uint64_t value,unsigned count){while(count){b[at+count-1]=uint8_t(value);value>>=8;--count;}}
hs::Verified verified(std::shared_ptr<const skills::Catalog> catalog,combat::Identity id,uint64_t epoch,unsigned hawkeye){
 hs::Receiver receiver(catalog,100);receiver.begin(9,99,epoch);check(receiver.want(id.slot,id.instance,id.character),"authenticated participant queued");auto request=receiver.take(1);check(bool(request),"skill request");
 skills::Loadout loadout{{{3,1},{7,2}}};if(hawkeye)loadout.entries.push_back({6,uint8_t(hawkeye)});auto cost=skills::validate(*catalog,loadout,8);check(bool(cost),"fixture fits original skill capacity");
 std::vector<uint8_t>b(72+loadout.entries.size()*4);put(b,0,0x47575348,4);b[4]=1;put(b,8,request->nonce,8);put(b,16,9,4);put(b,20,id.character,4);put(b,24,99,4);put(b,28,1000+id.character,4);put(b,32,1,4);b[36]=8;b[37]=uint8_t(cost.used);b[38]=uint8_t(loadout.entries.size());std::copy(hs::catalog_digest.begin(),hs::catalog_digest.end(),b.begin()+40);
 for(size_t i=0;i<loadout.entries.size();++i){put(b,72+i*4,loadout.entries[i].id,2);b[74+i*4]=loadout.entries[i].level;}
 check(receiver.receive(b,2),"lobby verifies original skill6 selection");auto values=receiver.drain();check(values.size()==1&&values[0].level(6)==hawkeye,"only verified selection supplies HAWKEYE");return values.front();
}
std::shared_ptr<const stage::Collision> floor(){return std::make_shared<const stage::Collision>(stage::Collision::make({{-30000,0,-30000},{30000,0,-30000},{30000,0,30000},{-30000,0,30000}},{{{0,1,2}},{{0,2,3}}}));}
combat::Pose pose(float z){combat::Pose p;p.feet={0,2,z};return p;}
void check_wire(const combat::Snapshot&snapshot){combat::wire::Frame frame{snapshot,combat::wire::Status::active};auto encoded=combat::wire::encode(frame);check(std::get<combat::wire::Frame>(combat::wire::decode(encoded))==frame,"all authoritative fields survive GWCB22");auto old=encoded;old[5]=20;bool rejected=false;try{combat::wire::decode(old);}catch(const combat::wire::Invalid&){rejected=true;}check(rejected,"old protocol cannot misinterpret new skill bits");}
void lifecycle(std::shared_ptr<const skills::Catalog> catalog){
 const combat::Identity a{0,10,101},b{1,11,202};const std::array<uint16_t,1> gear{25};const std::array<combat::Weapon,1> profiles{{{25,1000,0,100,1000,3,12,10000,900,901}}};
 auto opposing=pose(3000);opposing.yaw=3.14159265358979323846f;
 for(unsigned level=0;level<=3;++level){combat::Service service(7);service.configure(floor(),profiles);check(service.admit(a)&&service.admit(b),"service identities admitted");check(service.install_loadout(verified(catalog,a,7,level)),"verified pre-spawn loadout installed");auto&authority=service.authority();check(authority.join(a,1,pose(0),1000,1000,gear,0)&&authority.join(b,2,opposing,1000,1000,gear,0),"first spawn");authority.active(true);
  auto first=authority.snapshot();check(first.players[0]->hawkeyeLevel==level&&first.players[0]->verifiedSkills&&first.players[0]->masteryLevel==1&&first.players[0]->surveyorLevel==2,"HAWKEYE level and independent existing skills preserved");check(!first.players[1]->verifiedSkills&&!first.players[1]->hawkeyeLevel,"unverified other player remains level0");check_wire(first);
  service.deliveries();check(service.receive(a,combat::wire::encode(combat::wire::Accept{7}),0),"versioned skill frame admitted");bool delivered=false;for(const auto&d:service.deliveries())if(d.recipient==a){auto message=combat::wire::decode(d.payload);if(auto f=std::get_if<combat::wire::Frame>(&message)){check(f->snapshot==first,"actual service frame preserves authority HAWKEYE");delivered=true;}}check(delivered,"actual host delivery observed");
  auto late=verified(catalog,a,7,level==3?1:3);check(!service.install_loadout(late)&&authority.snapshot()==first,"mid-life selected draft cannot overwrite frozen host loadout");
  check(bool(authority.fire(b,{7,1,25,{0,0,-1},1},10)),"host attack for death lifecycle");check(!authority.snapshot().players[0]->alive&&authority.snapshot().players[0]->hawkeyeLevel==level,"death preserves selected round skill");
  check(!authority.respawn(a,2,[]{return false;})&&authority.snapshot().players[0]->hawkeyeLevel==level,"failed respawn leaves frozen skill intact");check(authority.respawn(a,2,[&]{return authority.join(a,1,pose(0),1000,1000,gear,20);}),"validated next-life grant");auto respawn=authority.snapshot();check(respawn.players[0]->life==2&&respawn.players[0]->hawkeyeLevel==level&&respawn.players[0]->verifiedSkills,"respawn retains authoritative level");check_wire(respawn);
  service.remove(a);auto replacement=a;++replacement.instance;++replacement.character;check(service.admit(replacement)&&!service.install_loadout(late),"old identity cannot grant skill to reused slot");check(authority.join(replacement,1,pose(0),1000,1000,gear,30),"replacement spawn");check(!authority.snapshot().players[0]->verifiedSkills&&!authority.snapshot().players[0]->hawkeyeLevel,"slot reuse clears HAWKEYE");
  authority.begin(8,floor(),profiles);check(!authority.install_loadout(late)&&authority.join(a,1,pose(0),1000,1000,gear,40),"old epoch loadout cannot cross new round");check(!authority.snapshot().players[0]->hawkeyeLevel&&!authority.snapshot().players[0]->verifiedSkills,"round reset clears HAWKEYE");
 }
}
void packing(){
 combat::wire::Frame frame;frame.snapshot.epoch=7;frame.snapshot.revision=1;combat::Player p;p.identity={0,1,101};p.team=1;p.pose=pose(0);p.hp=p.maxHp=p.stamina=p.maxStamina=1000;p.weapon=25;p.ammo=3;p.reserve=12;p.alive=true;p.verifiedSkills=true;frame.snapshot.players[0]=p;auto baseline=combat::wire::encode(frame);
 for(unsigned level=0;level<=3;++level){frame.snapshot.players[0]->hawkeyeLevel=uint8_t(level);auto bytes=combat::wire::encode(frame);check(bytes.size()==baseline.size(),"HAWKEYE preserves byte budget");unsigned differences=0;for(size_t i=0;i<bytes.size();++i)if(bytes[i]!=baseline[i]){++differences;check((bytes[i]^baseline[i])==128,"only reserved high bits used");}check(differences==unsigned((level&1)!=0)+unsigned((level&2)!=0),"two-bit level packing");check_wire(frame.snapshot);}
 frame.snapshot.players[0]->hawkeyeLevel=4;check(!combat::valid_skills(*frame.snapshot.players[0]),"out of range skill invalid");bool rejected=false;try{combat::wire::encode(frame);}catch(const combat::wire::Invalid&){rejected=true;}check(rejected,"encoder rejects level4");frame.snapshot.players[0]->hawkeyeLevel=3;frame.snapshot.players[0]->verifiedSkills=false;check(!combat::valid_skills(*frame.snapshot.players[0]),"unverified nonzero skill invalid");rejected=false;try{combat::wire::encode(frame);}catch(const combat::wire::Invalid&){rejected=true;}check(rejected,"encoder rejects unverified HAWKEYE");
 frame.snapshot.players[0]=p;p.verifiedSkills=false;auto unverified=frame;unverified.snapshot.players[0]=p;auto unverifiedBytes=combat::wire::encode(unverified);size_t verificationByte=baseline.size();for(size_t i=0;i<baseline.size();++i)if(baseline[i]!=unverifiedBytes[i]){check(verificationByte==baseline.size()&&(baseline[i]^unverifiedBytes[i])==64,"one existing verification bit");verificationByte=i;}frame.snapshot.players[0]->hawkeyeLevel=3;auto forged=combat::wire::encode(frame);check(verificationByte<forged.size(),"verification bit located independently");forged[verificationByte]&=uint8_t(~64);rejected=false;try{combat::wire::decode(forged);}catch(const combat::wire::Invalid&){rejected=true;}check(rejected,"decoder rejects nonzero unverified HAWKEYE");
 for(unsigned i=0;i<24;++i){p.identity={uint8_t(i),uint16_t(i+1),100+i};p.verifiedSkills=true;p.hawkeyeLevel=uint8_t(i%4);frame.snapshot.players[i]=p;}check_wire(frame.snapshot);auto budget=combat_test::budget(frame);check(budget.capacity>0&&budget.fullBytes<=2000,"24-player event frame still fits native budget");
}
}
int main(int argc,char**argv){try{check(argc==2,"skill catalog path required");auto catalog=std::make_shared<skills::Catalog>();std::string error;check(catalog->load(argv[1],error),"original skill catalog");lifecycle(catalog);packing();std::cout<<"PASS HAWKEYE LV0-3 verified host lifecycle, respawn/identity/epoch reset, compact GWCB22 and full-roster budget\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
