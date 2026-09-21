#include "player_motion.h"
#include "motion_blend.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
using namespace mgo2mt;
static void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}
int main(int argc,char**argv){try{
 require(argc==2,"usage: evade_motion_bank_test evade.gwmot");
 std::ifstream in(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(in)),{});
 PlayerMotionBank bank(bytes);require(bank.size()==3,"three separate phases");
 struct Expected{uint32_t action,key,index,frames;float startY,endY;};
 for(const auto&e:std::vector<Expected>{{19,0x57BB63,56,40,931.5f,236.75f},{21,0x52DE74,57,45,249.75f,978.f},{20,0x55B29B,61,45,1023.5f,1078.f}}){
  const auto action=PlayerMotion(e.action);auto*c=bank.find(action);
  require(c&&c->sourceKey==e.key&&c->sourceIndex==e.index&&c->frames==e.frames&&!c->loop&&c->tracks.size()==53,"exact reviewed original clip");
  auto start=bank.sample(action,0),end=bank.sample(action,double(e.frames)/60),late=bank.sample(action,1000);
  require(start&&end&&late&&start->root[1]==e.startY&&end->root[1]==e.endY,"original vertical pose retained");
  require(start->root[0]==0&&start->root[2]==0&&end->root[0]==0&&end->root[2]==0,"navigation owns root XZ");
  require(end->root==late->root&&end->rotations==late->rotations,"nonloop clamps without restarting");
  for(unsigned f=0;f<=e.frames;++f){auto pose=bank.sample(action,double(f)/60);require(bool(pose),"all original frames readable");for(const auto&[bone,q]:pose->rotations){(void)bone;float n=0;for(float v:q){require(std::isfinite(v),"finite sampled pose");n+=v*v;}require(std::abs(n-1)<1e-5f,"normalized sampled bones");}}
 }
 const auto roll=*bank.sample(PlayerMotion(19),40./60),recover=*bank.sample(PlayerMotion(21),0);
 require(recover.root[1]-roll.root[1]==13,"real phase offset preserved");
 MotionBlend blend;const auto displayed=blend.update(19,roll,0,5);auto first=blend.update(21,recover,.016,5);
 require(first.root==displayed.root&&first.rotations==displayed.rotations&&blend.progress()==0,"phase change starts from displayed roll without pop");
 auto mid=blend.update(21,*bank.sample(PlayerMotion(21),.1),.1,5);
 require(blend.active()&&std::abs(blend.progress()-.5f)<1e-6f&&mid.root[1]>roll.root[1],"recovery advances through existing blend");
 std::cout<<"evade original 3-clip bank / sampled frames / no root travel / phase blend PASS\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
