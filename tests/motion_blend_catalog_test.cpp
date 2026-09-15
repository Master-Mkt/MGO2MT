#include "character_catalog.h"
#include "motion_blend.h"
#include "selection_model.h"
#include "special_action_motion.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2win;
namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
std::vector<char> read(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);check(bool(in),"motion asset missing");return {(std::istreambuf_iterator<char>(in)),{}};}
template<class F>void rejects(F action){bool caught=false;try{action();}catch(const std::runtime_error&){caught=true;}check(caught,"invalid complete pose accepted");}
float distance(std::array<float,3>a,std::array<float,3>b){float sum=0;for(unsigned i=0;i<3;++i)sum+=(a[i]-b[i])*(a[i]-b[i]);return std::sqrt(sum);}
void same_skin(const PreparedCharacter&a,const PreparedCharacter&b){
 check(a.bonePositions==b.bonePositions&&a.model.indices==b.model.indices&&a.model.vertices.size()==b.model.vertices.size(),"partial/complete bone or topology mismatch");
 for(size_t i=0;i<a.model.vertices.size();++i){const auto&x=a.model.vertices[i];const auto&y=b.model.vertices[i];check(x.x==y.x&&x.y==y.y&&x.z==y.z&&x.nx==y.nx&&x.ny==y.ny&&x.nz==y.nz,"partial/complete skin mismatch");}
}
void healthy(const CharacterCatalog&catalog,const PreparedCharacter&body){
 auto bones=catalog.skeleton(body.gender);check(body.bonePositions.size()==bones.size(),"every catalog bone retained");
 for(const auto& bone:bones){auto point=body.bone_position(bone.key);check(bool(point),"bone missing");for(float v:*point)check(std::isfinite(v)&&std::abs(v)<10000,"bone finite and bounded");
  if(bone.parent>=0){float original=distance(bone.position,bones[bone.parent].position);float posed=distance(*point,*body.bone_position(bones[bone.parent].key));check(std::abs(original-posed)<.15f,"pose must preserve skeletal segment length");}}
 for(const auto&v:body.model.vertices){check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::abs(v.x)<10000&&std::abs(v.y)<10000&&std::abs(v.z)<10000,"skinned vertex finite and bounded");float n=v.nx*v.nx+v.ny*v.ny+v.nz*v.nz;check(std::isfinite(n)&&std::abs(n-1)<.001f,"skinned normal unit length");}
}
float vertex_delta(const PreparedCharacter&a,const PreparedCharacter&b){float result=0;check(a.model.vertices.size()==b.model.vertices.size(),"blend topology stable");for(size_t i=0;i<a.model.vertices.size();++i){const auto&x=a.model.vertices[i];const auto&y=b.model.vertices[i];result=std::max(result,distance({x.x,x.y,x.z},{y.x,y.y,y.z}));}return result;}
void transition(const CharacterCatalog&catalog,PreparedCharacter body,const MotionPose&initial,const MotionPose&target){
 MotionBlend engine;auto from=catalog.complete_pose(body.gender,initial),to=catalog.complete_pose(body.gender,target);
 catalog.pose(body,engine.update(1,from,0));auto before=body;
 catalog.pose(body,engine.update(2,to,10));check(engine.active()&&engine.progress()==0&&engine.last_update_accepted(),"transition starts at frozen display without consuming dt");same_skin(body,before);
 catalog.pose(body,engine.update(2,to,.000001));healthy(catalog,body);check(vertex_delta(body,before)<.15f,"arbitrarily short transition step must not jump vertices");
 float travel=0;
 for(unsigned step=0;step<24;++step){auto previous=body;catalog.pose(body,engine.update(2,to,1./120));healthy(catalog,body);travel+=vertex_delta(body,previous);}
 check(!engine.active()&&engine.progress()==1,"original pose transition reaches endpoint");
 MotionBlend endpoint;auto expected=body;catalog.pose(expected,endpoint.update(1,to,0));same_skin(body,expected);
 // A new source while blending must freeze the actual displayed mesh, not the
 // preceding clip's endpoint. Check again with real fully skinned geometry.
 engine.update(3,from,0);catalog.pose(body,engine.update(3,from,.07));before=body;
 catalog.pose(body,engine.update(4,to,.1));same_skin(body,before);check(engine.progress()==0,"interrupted blend starts at display");
 catalog.pose(body,engine.update(4,to,0));same_skin(body,before);
 check(std::isfinite(travel),"transition travel finite");
}
}
int main(int argc,char**argv){try{
 if(argc!=6){std::cerr<<"usage: motion_blend_catalog_test appearance.gwc player.gwmot special_male.gwmot selection0.gwmot selection1.gwmot\n";return 2;}
 CharacterCatalog catalog(read(argv[1]));PlayerMotionBank player(read(argv[2]));
 player::SpecialMotionBank special(read(argv[3]));size_t poses=0,transitions=0;
 for(unsigned gender=0;gender<2;++gender){
  std::array<uint8_t,28> appearance{};appearance[0]=uint8_t(gender);appearance[2]=11;appearance[3]=22;appearance[15]=46;appearance[17]=57;
  auto body=catalog.assemble(appearance),other=body;check(body.ready(),"both genders assemble");auto bones=catalog.skeleton(gender);
  MotionPose sparse;sparse.rootBone=bones.front().key;sparse.root={17,-29,43};
  auto complete=catalog.complete_pose(gender,sparse);check(complete.rotations.size()==bones.size()&&complete.root==sparse.root&&complete.rootBone==sparse.rootBone,"complete pose structure");
  for(const auto&[key,q]:complete.rotations){(void)key;check(q==std::array<float,4>{0,0,0,1},"missing rotations use identity, not catalog frame zero");}
  catalog.pose(body,sparse);catalog.pose(other,complete);same_skin(body,other);
  for(const auto&bone:bones){auto xyz=*body.bone_position(bone.key);for(unsigned axis=0;axis<3;++axis)check(std::abs(xyz[axis]-bone.position[axis]-sparse.root[axis])<.01f,"identity completion preserves bind coordinate and root");}
  sparse.rotations[bones.front().key]={0,std::sqrt(.5f),0,std::sqrt(.5f)};
  sparse.rotations[0]={std::numeric_limits<float>::quiet_NaN(),0,0,0};
  complete=catalog.complete_pose(gender,sparse);check(!complete.rotations.contains(0),"unknown hash discarded, consistent with renderer");
  catalog.pose(body,sparse);catalog.pose(other,complete);same_skin(body,other);healthy(catalog,body);
  check(catalog.complete_pose(gender,complete).rotations==complete.rotations,"completion idempotent");
  for(double t:{0.,.137,.73,double(catalog.frames())/60.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::max()}){
   auto sampled=catalog.sample_pose(gender,t);check(sampled.rotations.size()==bones.size(),"catalog sample is complete");catalog.pose(body,t);catalog.pose(other,sampled);same_skin(body,other);healthy(catalog,body);++poses;
  }
  for(unsigned action=0;action<15;++action)for(double t:{0.,.137,.62,8.}){
   auto sampled=player.sample(PlayerMotion(action),t);check(bool(sampled),"original gameplay clip present");catalog.pose(body,*sampled);catalog.pose(other,catalog.complete_pose(gender,*sampled));same_skin(body,other);healthy(catalog,body);++poses;
  }
  PlayerMotionBank selection(read(argv[4+gender]));
  for(auto action:{PlayerMotion::SelectionSalute,PlayerMotion::SelectionMagazine,PlayerMotion::SelectionBoxEnter,PlayerMotion::SelectionBox})for(double t:{0.,.025,.137,8.}){
   auto sampled=selection_pose(selection,action,t);if(!sampled)continue;catalog.pose(body,*sampled);catalog.pose(other,catalog.complete_pose(gender,*sampled));same_skin(body,other);healthy(catalog,body);++poses;
  }
  const auto idle=*player.sample(PlayerMotion::Idle,.1);
  for(auto action:{PlayerMotion::Run,PlayerMotion::CrouchIdle,PlayerMotion::ProneIdle,PlayerMotion::SupineIdle,PlayerMotion::Reload}){auto to=*player.sample(action,.6);transition(catalog,body,idle,to);transition(catalog,body,to,idle);transitions+=2;}
  for(auto action:{PlayerMotion::SelectionSalute,PlayerMotion::SelectionMagazine,PlayerMotion::SelectionBoxEnter,PlayerMotion::SelectionBox})if(auto to=selection_pose(selection,action,.13)){auto fallback=catalog.sample_pose(gender,.5);transition(catalog,body,fallback,*to);transition(catalog,body,*to,fallback);transitions+=2;}
  auto enter=selection_pose(selection,PlayerMotion::SelectionBoxEnter,100),box=selection_pose(selection,PlayerMotion::SelectionBox,0);
  check(enter&&box,"both genders have original box enter and loop");transition(catalog,body,*enter,*box);++transitions;
  if(gender==0)for(auto phase:{player::SpecialPhase::start,player::SpecialPhase::hold,player::SpecialPhase::end})for(double t:{0.,.13,.64,2.}){
   auto sampled=special.sample(phase,t);check(bool(sampled),"original special phase");catalog.pose(body,*sampled);catalog.pose(other,catalog.complete_pose(gender,*sampled));same_skin(body,other);healthy(catalog,body);++poses;
  }
  if(gender==0){auto start=*special.sample(player::SpecialPhase::start,.64),hold=*special.sample(player::SpecialPhase::hold,.2),end=*special.sample(player::SpecialPhase::end,0);transition(catalog,body,idle,start);transition(catalog,body,start,hold);transition(catalog,body,hold,end);transition(catalog,body,end,idle);transitions+=4;}
  rejects([&]{auto bad=sparse;bad.rootBone=0;catalog.complete_pose(gender,bad);});
  rejects([&]{auto bad=sparse;bad.root[0]=1000001;catalog.complete_pose(gender,bad);});
  rejects([&]{auto bad=sparse;bad.root[1]=std::numeric_limits<float>::quiet_NaN();catalog.complete_pose(gender,bad);});
  for(auto q:{std::array<float,4>{0,0,0,0},std::array<float,4>{0,0,0,2},std::array<float,4>{0,0,std::numeric_limits<float>::infinity(),1}})rejects([&]{auto bad=sparse;bad.rotations[bad.rootBone]=q;catalog.complete_pose(gender,bad);});
 }
 rejects([&]{catalog.sample_pose(2,0);});rejects([&]{catalog.complete_pose(2,{});});
 std::cout<<"Catalog complete-pose equivalence, original clips, both rigs, bone lengths and normalized skin PASS; poses="<<poses<<" transitions="<<transitions<<" male_bones="<<catalog.skeleton(0).size()<<" female_bones="<<catalog.skeleton(1).size()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
