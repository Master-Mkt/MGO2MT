#include "item_box_presentation.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
items::ClientState state(){items::ClientState s;s.connection=9;s.status=items::ClientStatus::ready;s.context={{1,2},{0,7,100,1},{0,4,0,0},true};s.world=items::SnapshotState{{1,2},1,{1024,1024},{}};items::Entity e;e.key={{1,2},1};e.position={0,4,1000,0};e.contents.item=25;e.contents.quantity=1;s.world->entities.push_back(e);return s;}
int main(int argc,char**argv){try{
 check(argc==3,"Item box test weapon catalog and original items root");weapons::Catalog catalog;std::string error;check(catalog.load(argv[1],error),"catalog");items::Contents item;item.item=25;check(item_box::profile(item,&catalog).size==item_box::Size::primary&&item_box::asset(item,&catalog)==item_box::Asset::large,"AK native primary uses original large model");item.item=3;check(item_box::profile(item,&catalog).size==item_box::Size::secondary&&item_box::asset(item,&catalog)==item_box::Asset::medium,"OPERATOR native secondary uses original medium model");item.item=52;check(item_box::profile(item,&catalog).size==item_box::Size::reserve&&item_box::asset(item,&catalog)==item_box::Asset::smallBox,"grenade native support uses original small model");item.item=9999;check(item_box::profile(item,&catalog).size==item_box::Size::unknown&&item_box::asset(item,&catalog)==item_box::Asset::unavailable,"unknown has no borrowed or fabricated visual");item.domain=items::Domain::equipment;item.item=25;check(item_box::profile(item,&catalog).size==item_box::Size::equipment,"equipment namespace kept separate");item.item=10;check(item_box::asset(item,&catalog)==item_box::Asset::large,"equipment10 joins original world113 large");item.item=22;check(item_box::asset(item,&catalog)==item_box::Asset::smallBox,"equipment22 joins original world140 small, not weapon22");
 auto world=stage::Collision::make({{-100000,0,-100000},{-100000,0,100000},{100000,0,100000},{100000,0,-100000}},{{{0,1,2}},{{0,2,3}}});item_box::Presentation p;check(p.load_catalog(argv[1],error),"presentation catalog");auto s=state();auto boxes=p.update(s,world,3,100);check(boxes.size()==1&&boxes[0].grounded&&boxes[0].position==stage::Vec3{0,4,1000}&&boxes[0].yaw==0,"exact HOST feet position");boxes=p.update(s,world,3,350);check(std::abs(boxes[0].yaw+1.57079633f)<.00001f,"left quarter-turn after quarter second");boxes=p.update(s,world,3,1100);check(boxes[0].yaw==0,"one full turn per second");
 s.world->revision=2;s.world->entities[0].position.y=500;boxes=p.update(s,world,3,1200);check(!boxes[0].grounded&&boxes[0].yaw==0&&boxes[0].position[1]==500,"falling follows HOST without autonomous spin or gravity");s.world->entities[0].position.y=4;s.world->revision=3;boxes=p.update(s,world,3,1300);check(boxes[0].yaw==0&&boxes[0].grounded,"new landing starts angle clock");boxes=p.update(s,world,3,1550);check(boxes[0].yaw< -1.5f,"landed spin");
 s.context.actor.life=2;boxes=p.update(s,world,3,1600);check(boxes[0].yaw==0,"new life clears old presentation time");boxes=p.update(s,world,4,1800);check(boxes[0].yaw==0,"new scene resets");check(p.update(s,world,4,1799).empty(),"clock rollback cannot redraw old phase");s.world->revision=2;check(p.update(s,world,4,1801).empty(),"stale snapshot not presented");s=state();p.clear();
 for(uint64_t i=2;i<=200;++i){auto e=s.world->entities.front();e.key.id=i;e.position.x=float(i*10);s.world->entities.push_back(e);}boxes=p.update(s,world,3,0);check(boxes.size()==128&&boxes.front().key.id==1&&boxes.back().key.id==128,"nearest deterministic bounded draw selection");
 s.world->entities[0].position.x=std::numeric_limits<float>::quiet_NaN();check(p.update(s,world,3,1).empty(),"nonfinite snapshot rejected atomically");s=state();s.context.active=false;check(p.update(s,world,3,2).empty(),"room inactive clears");
 s=state();p.clear();s.world->entities[0].kind=items::PlacementKind::round;
 for(uint64_t i=2;i<=8192;++i){auto e=s.world->entities.front();e.key.id=i;e.kind=items::PlacementKind::installed;s.world->entities.push_back(e);}check(p.update(s,world,3,3).size()==1,"8192 total inventory with installed items still presents round boxes");
 s.world->entities.back().position.x=std::numeric_limits<float>::infinity();check(p.update(s,world,3,4).empty(),"filtered installed item still receives finite validation");s.world->entities.back().position.x=0;auto excess=s.world->entities.back();excess.key.id=8193;s.world->entities.push_back(excess);check(p.update(s,world,3,5).empty(),"8193 entities fail bounded snapshot contract");
 const auto originals=item_box::original_models(argv[2]);
 const std::array<size_t,3> vertexCounts{1139,1651,1561},indexCounts{2562,3522,3546};
 for(unsigned i=0;i<3;++i){
  const auto& original=originals[i];auto m=item_box::model(original,item_box::Size(i));const auto h=item_box::profile(item_box::Size(i)).halfExtent;
  check(original.vertices.size()==vertexCounts[i]&&original.indices.size()==indexCounts[i]&&original.parts.size()==2&&original.textures.size()==1,"three distinct reviewed MDN conversions, no generated cube");
  check(m.vertices.size()==original.vertices.size()&&m.indices==original.indices&&m.parts.size()==original.parts.size(),"original topology retained");
  check(m.bounds[1]==0&&std::abs(m.bounds[0]+m.bounds[3])<.001f&&std::abs(m.bounds[2]+m.bounds[5])<.001f,"original bottom-center normalized to HOST position");
  check(m.bounds[3]<=h[0]+.001f&&m.bounds[4]<=2*h[1]+.001f&&m.bounds[5]<=h[2]+.001f,"visual fits unchanged native physics envelope");
  const float scale=(m.bounds[3]-m.bounds[0])/(original.bounds[3]-original.bounds[0]);
  check(std::abs(m.bounds[4]/(original.bounds[4]-original.bounds[1])-scale)<.00001f&&std::abs((m.bounds[5]-m.bounds[2])/(original.bounds[5]-original.bounds[2])-scale)<.00001f,"uniform scale preserves original mesh proportions");
  for(size_t n=0;n<m.vertices.size();++n){const auto& a=m.vertices[n];const auto& b=original.vertices[n];check(a.y>=-.001f&&std::isfinite(a.x)&&std::isfinite(a.z),"finite original geometry with ground pivot");check(a.nx==b.nx&&a.ny==b.ny&&a.nz==b.nz&&a.u==b.u&&a.v==b.v&&a.u1==b.u1&&a.v1==b.v1&&a.ar==b.ar&&a.ag==b.ag&&a.ab==b.ab&&a.aa==b.aa,"original normals, UV and authored color preserved");}
  for(size_t n=0;n<m.parts.size();++n){const auto&a=m.parts[n];const auto&b=original.parts[n];check(a.first==b.first&&a.count==b.count&&a.texture==b.texture&&a.materialShader==b.materialShader&&a.tint==b.tint,"original material bindings retained");}
  const auto&a=m.textures[0];const auto&b=original.textures[0];check(a.width==b.width&&a.height==b.height&&a.codec==b.codec&&a.pixels==b.pixels&&a.pixels.size()>32760,"actual original BC image retained byte-for-byte");
 }
 check(originals[1].textures[0].pixels!=originals[2].textures[0].pixels,"medium and small use distinct original diffuse content");
 bool rejected=false;try{item_box::original_models(std::filesystem::path(argv[2])/"missing-original-models");}catch(const std::runtime_error&){rejected=true;}check(rejected,"missing originals fail explicitly without synthetic fallback");
 auto invalid=originals[0];invalid.bounds[3]=invalid.bounds[0];rejected=false;try{item_box::model(invalid,item_box::Size::primary);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"degenerate original extent rejected");
 rejected=false;try{item_box::model(originals[0],item_box::Size::unknown);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"unknown category never generates a substitute mesh");
 std::ifstream input(std::filesystem::path(argv[2])/"ibox_item_mid.gwm",std::ios::binary);std::vector<char> bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};check(!bytes.empty(),"original medium byte fixture");
 rejected=false;try{CharacterModel truncated(std::span<const char>(bytes).first(bytes.size()-1));}catch(const std::runtime_error&){rejected=true;}check(rejected,"truncated original texture payload rejected");bytes.push_back(0);
 rejected=false;try{CharacterModel trailing(bytes);}catch(const std::runtime_error&){rejected=true;}check(rejected,"unexpected original model trailing extent rejected");
 std::cout<<"Original MDN item boxes / exact images / bottom-center uniform fit / unchanged HOST profile / grounded left rotation / missing assets PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
