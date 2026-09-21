#include "combat_authority.h"
#include "gameplay_config.h"
#include "stage_navigation.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <iomanip>
using namespace mgo2mt;
int main(int argc,char**argv){try{
 if(argc!=2)return 2;std::cout<<std::setprecision(10);std::filesystem::path data=argv[1];std::string error;gameplay::Config config;mounted::Registry registry;if(!config.load(data/"gameplay.json",error)||!registry.load(data/"mounted_weapons.json",error))throw std::runtime_error(error);
 std::ifstream cf(data/"stage/n022a.collision.cfg");auto world=std::make_shared<const stage::Collision>(stage::Collision::read(cf));std::vector<stage::Vec3> hints;std::ifstream sf(data/"stage/n022a.dm-spawns.cfg");std::string line;std::getline(sf,line);while(std::getline(sf,line)){std::istringstream in(line);std::string word;for(int n=0;n<6;++n)in>>word;stage::Vec3 p;if(in>>p[0]>>p[1]>>p[2])hints.push_back(p);}
 for(auto& instance:registry.placements)if(instance.id>1&&instance.map==20){const auto&type=*registry.find(instance.type);bool found=false;
  for(unsigned index: {20u,21u,19u,8u,18u,6u,3u,16u,17u,0u,9u,10u,14u,15u}){if(index>=hints.size())continue;stage::Navigation nav;if(!nav.place(*world,hints[index],30000))continue;
   bool nearby=false;for(const auto& other:registry.placements)if(other.map==20&&other.id<instance.id){const auto at=mounted::operator_position(other,*registry.find(other.type));if(std::hypot(at[0]-nav.feet()[0],at[2]-nav.feet()[2])<10000)nearby=true;}if(nearby)continue;
   for(float yaw:{2.2f,0.f,1.5707963f,-1.5707963f,3.1415926f}){auto offset=mounted::rotate(type.operatorOffset,yaw);for(unsigned n=0;n<3;++n)instance.origin[n]=nav.feet()[n]-offset[n];instance.yaw=yaw;
    combat::Authority authority;authority.begin(12,world,config.profiles(20));if(!authority.configure_mounted(registry,20))continue;combat::Identity self{0,1,101};combat::Pose pose;pose.feet=nav.feet();pose.yaw=yaw;uint16_t inventory[]{25,3,52};if(!authority.join(self,1,pose,1000,1000,inventory,0))continue;authority.active(true);if(authority.pose(self,12,1,pose,100)!=combat::Reject::none)continue;if(authority.mount(self,12,1,{mounted::Action::mount,instance.id,1},100)!=combat::Reject::none)continue;pose=authority.snapshot().players[0]->pose;stage::Vec3 direction{std::sin(pose.yaw)*std::cos(pose.pitch),std::sin(pose.pitch),std::cos(pose.yaw)*std::cos(pose.pitch)};if(!authority.fire(self,{12,1,authority.snapshot().players[0]->weapon,direction},200))continue;
    bool airborne=false,landed=false,blast=false;uint64_t airtime=0,blastAt=0;for(uint64_t now=220;now<=12200;now+=20){authority.advance(now);for(const auto&e:authority.advance_projectiles(now).events)if(e.kind==combat::EventKind::explosion){blast=true;blastAt=now;}const auto state=authority.snapshot();const auto&p=*state.players[0];if(p.flightId){airborne=true;airtime=now-200;}else if(airborne){landed=p.alive;break;}if(type.kind==mounted::Kind::mortar&&blast)break;}
    if(type.kind==mounted::Kind::catapult&&(!landed||airtime<600))continue;if(type.kind==mounted::Kind::mortar&&(!blast||blastAt<1500))continue;
    std::cout<<"{\"id\":"<<instance.id<<",\"spawn\":"<<index<<",\"origin\":["<<instance.origin[0]<<','<<instance.origin[1]<<','<<instance.origin[2]<<"],\"yaw\":"<<yaw<<",\"airMs\":"<<airtime<<"}\n";found=true;break;
   }if(found)break;
  }if(!found)throw std::runtime_error("No admitted placement for "+type.id);
 }
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
