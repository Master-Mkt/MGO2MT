#include "character_catalog.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
std::vector<char> read(const char* path){std::ifstream in(path,std::ios::binary);check(bool(in),"Missing selection fixture");return {(std::istreambuf_iterator<char>(in)),{}};}
}
int main(int argc,char**argv){try{
 check(argc==5,"Usage: pc_selection_asset_test appearance.gwc selection0.gwmot selection1.gwmot cbox.gwm");
 CharacterCatalog catalog(read(argv[1]));CharacterModel box(read(argv[4]));check(!box.vertices.empty()&&!box.textures.empty(),"Original box must have geometry and texture");
 for(unsigned gender=0;gender<2;++gender){PlayerMotionBank bank(read(argv[2+gender]));check(bank.size()==3&&!bank.has(PlayerMotion::SelectionMagazine)&&!bank.has(PlayerMotion::Run),"Only recovered menu clips are supplied");
  auto salute=bank.find(PlayerMotion::SelectionSalute),enter=bank.find(PlayerMotion::SelectionBoxEnter),loop=bank.find(PlayerMotion::SelectionBox);
  check(salute&&salute->sourceIndex==33&&salute->sourceKey==(gender?0x5f93c5u:0xa03c44u)&&salute->frames==400&&!salute->loop,"Patch salute identity");
  check(enter&&enter->sourceIndex==3&&enter->sourceKey==(gender?0x8d32c1u:0xcd3fc6u)&&enter->frames==100&&!enter->loop,"Original box entry queue");
  check(loop&&loop->sourceIndex==24&&loop->sourceKey==(gender?0xf48bd8u:0xc4b5f8u)&&loop->frames==100&&loop->loop,"Original box idle queue");
  std::array<uint8_t,28> appearance{};appearance[0]=uint8_t(gender);appearance[2]=11;appearance[3]=22;appearance[15]=46;appearance[17]=57;auto body=catalog.assemble(appearance);check(body.ready(),"Selection character assembly");auto indices=body.model.indices;auto textureCount=body.model.textures.size();
  for(auto action:{PlayerMotion::SelectionSalute,PlayerMotion::SelectionBoxEnter,PlayerMotion::SelectionBox})for(double t:{0.,.4,1.5,3.,6.65,60.}){
   auto pose=bank.sample(action,t);check(pose&&pose->root[0]==0&&pose->root[2]==0,"Selection stays at actor origin");catalog.pose(body,*pose);
   check(body.model.indices==indices&&body.model.textures.size()==textureCount,"Posing preserves original appearance");
   for(const auto&v:body.model.vertices)check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::abs(v.x)<4000&&std::abs(v.y)<4000&&std::abs(v.z)<4000,"Bounded skinned selection pose");
  }
  check(bank.sample(PlayerMotion::SelectionSalute,100)->rotations==bank.sample(PlayerMotion::SelectionSalute,400./60)->rotations,"Salute endpoint holds");
  auto start=bank.sample(PlayerMotion::SelectionBox,0),wrapped=bank.sample(PlayerMotion::SelectionBox,100./60);check(start->rotations==wrapped->rotations&&start->root==wrapped->root,"Box loop wraps exactly");
 }
 std::cout<<"Original male/female salute, box entry/loop, textured prop and full-body posing passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
