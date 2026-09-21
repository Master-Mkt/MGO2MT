#include "original_hit_regions.h"
#include "original_bullet_penetration.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
using namespace original_hit_regions;
void check(bool value,const char* label){if(!value)throw std::runtime_error(label);}
int main(){try{
 check(boxes.size()==12&&boxes[0].bone==4&&boxes[1].bone==3&&boxes[0].halfExtent==Vec3{95,110,98}&&boxes[1].offset==Vec3{0,60,-20},"current 13-entry constructor, not old 12-entry table");
 for(uint8_t bone=0;bone<21;++bone){
  auto plain=ak102_region_damage(275,1000,bone,1000,false,true);
  check(plain&&plain->damage==(bone<=4?275:165)&&!plain->headshot,"base normalized by bone, not user 100/80/48 percent");
 }
 for(auto f:{0u,1u,2u,3u,0xffffffffu})for(auto cls:{0u,4u,7u})for(auto rnd:{-1,0,1})
  check(headshot_allowed(f,cls,rnd)==(bool(f&2)&&(cls!=4||rnd<0)),"original shared flag and class4 RNG exception");
 for(uint8_t bone:std::array<uint8_t,2>{3,4}){
  auto full=ak102_region_damage(275,1000,bone,1000,true,true);check(full&&full->damage==1000&&full->headshot,"unattenuated enabled HS maxHP floor");
  auto high=ak102_region_damage(1200,1000,bone,1000,true,true);check(high&&high->damage==1200,"maxHP floor does not reduce base");
  auto zero=ak102_region_damage(275,1000,bone,0,true,true);check(zero&&zero->damage==1100,"raw force zero is distinct from raw1000 at regional stage");
  auto blocked=ak102_region_damage(275,1000,bone,1000,true,false);check(blocked&&blocked->damage==275&&!blocked->headshot,"unknown or disallowed HS does not become automatic");
 }
 auto force=original_bullet_penetration::target_force(1000,0,5000,100);
 check(force&&*force==900,"one passed surface gives force900");
 auto base=original_bullet_penetration::ak102_base_hp(*force);check(base&&*base==247,"force first signed division truncates");
 auto limb=ak102_region_damage(*base,1000,14,uint16_t(*force),false,false);check(limb&&limb->damage==148,"247 times float .6 truncates148, not275*.6*.9 rounded");
 auto head=ak102_region_damage(*base,1000,4,uint16_t(*force),true,true);check(head&&head->damage==988,"attenuated HS scales already-truncated247 by4");
 check(ak102_region_damage(1,1000,6,1000,false,false)->damage==0,"float regional truncation");
 check(!ak102_region_damage(-1,1000,0,1000,false,false)&&!ak102_region_damage(275,0,4,1000,true,true),"native invalid negative/maxHP rejected");
 check(!ak102_region_damage(0x7fffffff,1000,4,900,true,true),"conversion overflow safely rejects");
 std::cout<<"Original current BOX, joint ratios, conditional HS and penetration order PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
