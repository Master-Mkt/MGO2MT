#include "weapon_hand_renderer.h"
#include "weapon_connection_points.h"
#include <wrl/client.h>
#include <d3d11.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
static void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
static std::vector<char> read(const std::filesystem::path&p){std::ifstream in(p,std::ios::binary);check(bool(in),"source fixture");return {std::istreambuf_iterator<char>(in),{}};}
int main(int argc,char**argv){try{
 check(argc==4,"weapon, character and external connection resources");std::filesystem::path w=argv[1],b=argv[2];weapon_hand::Models models(w/"weapons");std::string connectionError;check(models.load_connections(argv[3],connectionError),connectionError.c_str());weapon_hand::Bank bank(read(w/"weapons/hands.gwh"));CharacterCatalog catalog(read(b/"character/appearance.gwc"));PlayerMotionBank motion(read(b/"character/player.gwmot"));
 Microsoft::WRL::ComPtr<ID3D11Device>d;Microsoft::WRL::ComPtr<ID3D11DeviceContext>c;D3D_FEATURE_LEVEL level;check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c)),"WARP device");
 std::array<uint8_t,28> a{};a[2]=11;a[3]=22;a[15]=46;a[17]=57;auto body=catalog.assemble(a);weapon_hand::Actor actor;unsigned count=0,frames=0;
 for(auto id:bank.weapons()){
  auto hand=bank.select(id,PlayerMotion::Aim,0,true);check(bool(hand),"original hold");auto pose=*motion.sample(PlayerMotion::Idle,0);weapon_hand::upper_body(pose,*hand,catalog.skeleton(0));catalog.pose(body,pose);actor.clear();check(actor.update(d.Get(),c.Get(),models,body,*hand,.1),"posed original weapon");
  auto all=actor.connection_frames(0,{0,0,0});frames+=unsigned(all.size());auto axis=actor.connection(0x443037,0,{0,0,0});std::cout<<"cnp_coverage weapon="<<id<<" points="<<all.size()<<" ejection="<<bool(axis)<<'\n';if(axis){++count;auto moved=actor.connection(0x443037,0,{3000,1200,-4000});for(unsigned i=0;i<3;++i)check(std::abs((moved->front[i]-moved->rear[i])-(axis->front[i]-axis->rear[i]))<.001f,"translation does not rotate emitter");}
  if(id==25){
   check(bool(axis),"AK casing connector");auto matrix=*weapon_hand::frame(body,hand->point);
   const auto* expected=models.connection_points().find(id,weapon_hand::connections::ejection);check(expected!=nullptr,"external ejection source");const auto local=expected->axis.rear;std::array<float,3> direction{};for(unsigned i=0;i<3;++i)direction[i]=(expected->axis.front[i]-local[i])*.01f;
   for(unsigned j=0;j<3;++j){float p=matrix[12+j],v=0;for(unsigned i=0;i<3;++i){p+=local[i]*matrix[i*4+j];v+=direction[i]*matrix[i*4+j];}check(std::abs(p-axis->rear[j])<.03f&&std::abs((axis->front[j]-axis->rear[j])*.01f-v)<.0001f,"AK external CNP position and third basis");}
   auto muzzle=actor.muzzle(0,{0,0,0});float gap=0;for(unsigned i=0;i<3;++i)gap+=std::pow((*muzzle)[i]-axis->rear[i],2);check(gap>10000,"ejection is not a muzzle offset");
  }
  actor.hide();check(!actor.connection(0x443037,0,{0,0,0})&&actor.connection_frames(0,{0,0,0}).empty(),"hidden actor cannot emit stale connection");
 }
 // Seven archived model IDs have no selectable hold clips. The current 39-ID
 // catalog consumes 159 of the 189 audited points, including all 22 gun emitters.
 std::cout<<"cnp_coverage total="<<frames<<" ejection="<<count<<'\n';check(count==22&&frames==159,"current catalog connector coverage");actor.clear();check(actor.weapon_id()==0&&actor.connection_frames(0,{0,0,0}).empty(),"clear resets connector identity");
 std::cout<<"PASS 159 current catalog CNP frames, 22 ejection points, current hand/world transform and visibility\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
