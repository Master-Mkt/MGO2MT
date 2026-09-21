#include "weapon_hand_renderer.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
static std::vector<char> read(const std::filesystem::path&p){std::ifstream in(p,std::ios::binary);if(!in)throw std::runtime_error("Missing fixture");return {std::istreambuf_iterator<char>(in),{}};}
static void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(int argc,char**argv){try{
 check(argc==3,"Expected weapon data directory and character fixture data directory");std::filesystem::path data=argv[1],character=argv[2];auto bytes=read(data/"weapons/hands.gwh");weapon_hand::Bank bank(bytes);weapon_hand::Models models(data/"weapons");
 constexpr std::array<unsigned,39> ids{1,2,3,4,7,8,15,18,20,23,24,25,26,30,31,35,37,38,39,41,42,43,44,50,52,53,54,55,56,57,58,59,63,64,65,66,67,69,73};
 CharacterCatalog catalog(read(character/"character/appearance.gwc"));PlayerMotionBank motion(read(character/"character/player.gwmot"));
 for(unsigned gender=0;gender<2;++gender){std::array<uint8_t,28> appearance{};appearance[0]=uint8_t(gender);appearance[2]=11;appearance[3]=22;appearance[15]=46;appearance[17]=57;auto body=catalog.assemble(appearance);check(body.ready(),"Ready original body");
  auto arms=weapon_hand::first_person_arms(body,catalog.skeleton(gender));check(!arms.indices.empty()&&arms.indices.size()<body.model.indices.size(),"First person original arms subset");
  auto remainder=weapon_hand::first_person_body(body,catalog.skeleton(gender));check(remainder.indices.size()+arms.indices.size()==body.model.indices.size(),"Arms and fading body partition original triangles");
  auto triangles=[](const std::vector<uint32_t>& indices){std::vector<std::array<uint32_t,3>> result;for(size_t i=0;i<indices.size();i+=3)result.push_back({indices[i],indices[i+1],indices[i+2]});std::sort(result.begin(),result.end());return result;};auto joined=arms.indices;joined.insert(joined.end(),remainder.indices.begin(),remainder.indices.end());check(triangles(joined)==triangles(body.model.indices),"No original triangle duplicated or omitted during fade");
  for(auto id:ids){check(models.find(id)&&models.binding(id),"All catalog models bound");for(int posture=0;posture<3;++posture){for(auto action:{PlayerMotion::Idle,PlayerMotion::Aim,PlayerMotion::Reload}){auto selected=bank.select(id,action,.3,true,-1,-1,posture);check(bool(selected),"All catalog actions sample");auto pose=*motion.sample(PlayerMotion::Idle,0);weapon_hand::upper_body(pose,*selected,catalog.skeleton(gender));catalog.pose(body,pose);check(bool(weapon_hand::frame(body,selected->point)),"Original MTP connects to posed hand");for(const auto&[key,q]:selected->pose.rotations){double length=0;for(auto v:q){check(std::isfinite(v),"Finite pose");length+=v*v;}check(std::abs(length-1)<.01,"Unit rotation");}}
   auto cqc=bank.select(id,PlayerMotion::Idle,0,false,-1,.15,posture);check(cqc&&cqc->cqc,"Every weapon supports shared CQC");auto firing=bank.select(id,PlayerMotion::Aim,0,true,.06,-1,posture);check(firing&&firing->sighting,"Aimed firing keeps the same model-space hold adapter");}
  }
  auto shot=*bank.select(24,PlayerMotion::Aim,0,true);auto pose=*motion.sample(PlayerMotion::Idle,0);weapon_hand::upper_body(pose,shot,catalog.skeleton(gender));catalog.pose(body,pose);auto head=body.bonePositions.at(0xf5d387);auto before=*weapon_hand::frame(body,shot.point);check(weapon_hand::aim_pitch(body,catalog.skeleton(gender),.35f),"Pitch adapter succeeds");auto after=*weapon_hand::frame(body,shot.point);double moved=0;for(int j=12;j<15;++j)moved+=std::abs(after[j]-before[j]);check(moved>1,"Pitch updates attachment frame");for(auto&v:body.model.vertices)check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z),"Finite pitched vertices");
 }
 for(auto bad:{size_t(4),bytes.size()-1}){bool rejected=false;try{weapon_hand::Bank invalid(std::span<const char>(bytes.data(),bad));}catch(...){rejected=true;}check(rejected,"Truncated bank rejected");}
 std::cout<<"PASS: 39 weapons, both rigs, all stances, attachments, original arms, pitch, CQC and firing presentation\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
