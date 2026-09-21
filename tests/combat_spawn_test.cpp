#include "combat_spawn.h"
#include "combat_world.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
using namespace mgo2mt;
using namespace mgo2mt::combat::spawn;
void check(bool ok,const char*s){if(!ok)throw std::runtime_error(s);}
bool near(float a,float b,float tolerance=.02f){return std::abs(a-b)<tolerance;}
std::string fixture(){
 std::ostringstream out;out<<"MGO2MT_TDM_SPAWNS 1\nstage n022a\nmap 20\nrule 1\n";
 for(unsigned v=0;v<2;++v)for(unsigned k=0;k<2;++k)for(unsigned t=0;t<2;++t)for(unsigned i=0;i<16;++i){
  auto index=v?i%8:i;out<<"spawn "<<(v?"mini":"normal")<<' '<<(k?"respawn":"initial")<<' '<<t<<' '<<i<<" 0x"<<std::hex<<std::setw(6)<<std::setfill('0')<<(0x100000+v*0x1000+k*0x100+t*0x20+index)<<std::dec<<' '<<int(index*1000+t*20000)<<" 100 0 "<<(t?-32768:16384)<<'\n';
 }return out.str();
}
Profile load(const std::string&s){std::istringstream in(s);return Profile::read(in);}
bool refuses(const std::string&s){try{load(s);return false;}catch(const std::exception&){return true;}}
stage::Collision floor(){return stage::Collision::make({{-200000,0,-200000},{-200000,0,200000},{200000,0,200000},{200000,0,-200000}},{{{0,1,2}},{{0,2,3}}});}
int main(int argc,char**argv){try{
 auto source=fixture();auto profile=load(source);check(profile.entries().size()==128,"complete 128 source entries");
 Random r{{1,2,3,4}};for(uint64_t expected:{2061ull,6175ull,4ull,8224ull,4194381ull,8396986ull,8388750ull,25174430ull})check(r.next()==expected,"original xorshift64 four-word golden");
 Random high{{0xfedcba9876543210ull,0x123456789abcdef0ull,0xff00ff00ff00ff00ull,0xaaaaaaaa55555555ull}};check(high.next()==0xb1b9ceacfd13695dull,"64-bit shift/truncation golden differs from xorshift32");
 auto seeded=Random::cold_start(3,7);check(seeded==Random{{0x3593c98ec07a7992ull,0x779d548e319093fbull,0x7b308bc20aac8423ull,0x73e928f4c648af5cull}},"ELF cold tail plus setup seed and 64 permutation draws");
 seeded.stage_setup(3,8);check(seeded==Random{{0xe2bf28ff65b968aull,0x98346b5af534666eull,0x14c4247c7c5d19f7ull,0x124a76c6b026b12dull}},"next setup preserves three words rather than resetting RNG each round");
 check(raw_team(1)==0&&raw_team(2)==1&&!raw_team(0)&&!raw_team(3),"native vs NT team domains");
 Selector normal(profile,{7,9,false},{{1,2,3,4}});auto a=normal.propose(Kind::initial,0);check(a&&a->arrayIndex==0&&a->variant==Variant::normal&&near(a->creationPose.feet[1],900)&&near(a->creationPose.yaw,1.5707963f),"initial source position plus exactly Y800");
 check(normal.propose(Kind::initial,0)==a&&normal.counters()[0]==0,"propose/failed admission consumes nothing");auto bad=*a;bad.creationPose.feet[0]+=1;check(!normal.commit(bad)&&normal.counters()[0]==0,"modified proposal cannot commit");check(normal.commit(*a)&&!normal.commit(*a),"commit once, stale proposal rejected");
 auto b=normal.propose(Kind::initial,1);check(b&&b->arrayIndex==0&&near(b->creationPose.yaw,-3.14159265f),"team counters independent and signed yaw");check(normal.commit(*b),"other team commit");
 for(int i=1;i<16;++i){auto n=normal.propose(Kind::initial,0);check(n&&n->arrayIndex==i&&normal.commit(*n),"initial increment in host call order");}check(normal.propose(Kind::initial,0)->arrayIndex==0,"initial table wraps at 16, not slot index");
 auto respawn=normal.propose(Kind::respawn,0);check(respawn&&respawn->arrayIndex==13&&near(respawn->creationPose.feet[1],100),"respawn low32 RNG modulo16 with no Y lift");check(normal.random()==Random{{1,2,3,4}},"proposed RNG retained until commit");check(normal.commit(*respawn)&&normal.propose(Kind::respawn,0)->arrayIndex==15,"RNG committed exactly once");
 Selector swapped(profile,{8,8,true},{{1,2,3,4}});auto m=swapped.propose(Kind::initial,0);check(m&&m->variant==Variant::mini&&m->normalizedTeam==1&&m->rawTeam==0,"mini capacity boundary and round team swap");check(!swapped.commit(*a)&&!swapped.propose(Kind::initial,2)&&!swapped.propose(Kind(9),0),"other epoch/raw team/kind rejected");
 for(auto context:{Context{0,8,false},Context{1,0,false},Context{1,17,false},Context{1,8,false,15,1},Context{1,8,false,20,2}}){bool rejected=false;try{Selector q(profile,context,{{1,2,3,4}});}catch(...){rejected=true;}check(rejected,"unsupported selection context rejected");}
 bool badSeed=false;try{Selector q(profile,{1,8,false},{});}catch(...){badSeed=true;}check(badSeed,"zero RNG state rejected");
 auto missing=source.substr(0,source.rfind("spawn "));check(refuses(missing),"partial profile rejected");check(refuses(source+"spawn normal initial 0 0 0x100000 0 100 0 16384\n"),"duplicate entry rejected");check(refuses(source+"unknown 1\n"),"unknown field rejected");check(refuses(source+"rule 1\n"),"duplicate metadata rejected");
 for(auto value:{"nan","inf","1000001"}){auto wrong=source;auto pos=wrong.find(" 100 0 ");wrong.replace(pos+1,3,value);check(refuses(wrong),"nonfinite/out-of-range source position rejected");}
 auto corrupt=source;auto pos=corrupt.find("spawn mini initial 0 8 ");pos=corrupt.find("0x",pos);corrupt.replace(pos,8,"0xffffff");check(refuses(corrupt),"mini duplicate hash sequence preserved");
 auto world=floor();Selector placementSource(profile,{10,9,false},{{1,2,3,4}});auto proposed=*placementSource.propose(Kind::initial,0);auto placed=place(proposed,world,{});check(bool(placed)&&near(placed.pose.feet[1],2)&&near(placed.drop,898),"native vertical landing keeps exact XZ and skin");
 std::array< combat::Pose,1> occupied{placed.pose};auto blocked=place(proposed,world,occupied);check(blocked.reject==PlacementReject::occupied&&placementSource.counters()[0]==0,"occupied location waits without consuming order or lateral scatter");
 auto empty=stage::Collision::make({},{});check(place(proposed,empty,{}).reject==PlacementReject::no_floor,"no floor rejected");check(place(proposed,world,{},std::numeric_limits<float>::quiet_NaN()).reject==PlacementReject::invalid,"invalid landing extent rejected");
 auto penetrates=proposed;penetrates.creationPose.feet[1]=-500;check(place(penetrates,world,{}).reject==PlacementReject::obstructed,"initial ground penetration rejected");
 auto roofWorld=stage::Collision::make({{-200000,0,-200000},{-200000,0,200000},{200000,0,200000},{200000,0,-200000},{-200000,2500,-200000},{-200000,2500,200000},{200000,2500,200000},{200000,2500,-200000}},{{{0,1,2}},{{0,2,3}},{{4,5,6}},{{4,6,7}}});
 auto ceiling=place(proposed,roofWorld,{});check(bool(ceiling)&&near(ceiling.ceilingCorrection,104)&&near(ceiling.pose.feet[1],2)&&ceiling.pose.feet[0]==proposed.creationPose.feet[0],"upper-cap-only initial ceiling resolves down without lateral shift");
 auto asRespawn=proposed;asRespawn.kind=Kind::respawn;check(place(asRespawn,roofWorld,{}).reject==PlacementReject::obstructed,"respawn cannot borrow initial ceiling correction");
 auto lowerCeiling=proposed;lowerCeiling.creationPose.feet[1]=1550;auto lowered=place(lowerCeiling,roofWorld,{});check(bool(lowered)&&near(lowered.ceilingCorrection,754)&&near(lowered.pose.feet[1],2),"ceiling through capsule segment resolves within original800lift without shiftingXZ");
 auto excessive=proposed;excessive.creationPose.feet[1]=1600;check(place(excessive,roofWorld,{}).reject==PlacementReject::obstructed,"ceiling correction beyond original800lift remains blocked");
 if(argc>=3){
  std::ifstream in(argv[1]);auto real=Profile::read(in);auto root=std::filesystem::path(argv[2]);auto hostWorld=combat::World::load(root);auto registry=stage::load_object_registry(root/"n022a.objects.cfg");
  stage::SceneSnapshot snapshot{{1,7,0,0,{20,1,0},host::MatchTransition::initial},1,{}};for(auto&e:registry.entries)snapshot.objects.push_back({e.bindingId,0,0});check(hostWorld.apply(snapshot),"host intact object world is complete");
  std::ofstream report;if(argc>=4){report.open(argv[3]);check(bool(report),"spawn audit report writable");report<<"variant,kind,team,index,hash,x,raw_y,z,creation_y,creation_clear,landing_status,landed_y,drop,ceiling_correction\n";report<<std::setprecision(10);}
  unsigned originalClear=0,landed=0,blockedCount=0;std::array<unsigned,8> counts{};
  for(auto&e:real.entries()){
   Proposal p;p.epoch=7;p.revision=1;p.kind=e.kind;p.variant=e.variant;p.normalizedTeam=e.team;p.arrayIndex=e.index;p.hash=e.hash;p.sourcePosition=e.position;p.creationPose.feet=e.position;p.creationPose.yaw=float(e.yaw)*6.2831853071795864769f/65536.f;if(e.kind==Kind::initial)p.creationPose.feet[1]+=800;
   bool clear=hostWorld.collision()->clear(p.creationPose.feet,p.creationPose.capsule);auto placement=place(p,*hostWorld.collision(),{});originalClear+=clear;landed+=bool(placement);blockedCount+=!placement;if(placement)++counts[(unsigned(e.variant)*2+unsigned(e.kind))*2+e.team];
   if(report)report<<unsigned(e.variant)<<','<<unsigned(e.kind)<<','<<unsigned(e.team)<<','<<unsigned(e.index)<<','<<e.hash<<','<<e.position[0]<<','<<e.position[1]<<','<<e.position[2]<<','<<p.creationPose.feet[1]<<','<<clear<<','<<unsigned(placement.reject)<<','<<placement.pose.feet[1]<<','<<placement.drop<<','<<placement.ceilingCorrection<<'\n';
  }
  // Expose real-world failures as evidence, never replace them with successful
  // synthetic coordinates. Per-source golden findings are asserted after audit.
  check(originalClear==96&&landed==128&&blockedCount==0,"reviewed intact n022a creation96/128, native landing128/128");
  if(report){report.flush();check(bool(report),"spawn audit written completely");}
  std::cout<<"real creation_clear="<<originalClear<<"/128 landed="<<landed<<" blocked="<<blockedCount<<" group counts=";for(auto n:counts)std::cout<<n<<',';std::cout<<'\n';
 }
 std::cout<<"TDM selection, 64-bit RNG, rollback/epoch, malformed profile and conservative placement passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
