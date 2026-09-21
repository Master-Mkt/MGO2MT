#include "material_audio.h"
#include <cstdlib>
#include <iostream>
using namespace mgo2mt::combat::material_audio;
static void require(bool value){if(!value){std::cerr<<"material audio contract failed\n";std::exit(1);}}
int main(){
 // Golden examples from the original four GCX procedures, not material indexes.
 for(auto stage:{Stage::n022a,Stage::n001a,Stage::n004a,Stage::n023a}){
  require(bullet_cue(stage,0x15BCCC)==8000u);
  require(bullet_cue(stage,0x1818AC)==8001u);
  require(bullet_cue(stage,0xE1D8B3)==847u);
  require(bullet_cue(stage,0x48C4B8)==8002u);
  require(bullet_cue(stage,0x189CD4)==8004u);
  require(bullet_cue(stage,0x3B20D3)==8325u);
  require(bullet_cue(stage,0x1818C2).has_value());
  require(*bullet_cue(stage,0x1818C2)==0u); // explicit silence, not unknown
  require(bullet_cue(stage,0xE3A4C9)==1734u); // waveform may be unavailable separately
  require(!bullet_cue(stage,0x1AD7D2)); // group label is not material hash
  require(!bullet_cue(stage,0));
  require(!resolve(stage,0,0x1818AC));
  require(!resolve(stage,0xFFFFFFFFu,0x1818AC));
 }
 require(bullet_cue(Stage::n022a,0xE1D8C5)==8004u);
 require(bullet_cue(Stage::n004a,0xE1D8C5)==8001u); // authored stage-specific group
 require(bullet_cue(Stage::n022a,0x8E8CC)==8000u);
 require(!bullet_cue(Stage::n004a,0x8E8CC)); // absent mapping must not inherit
 require(resolve(Stage::n022a,8007,0x15BCCC)==8005u);
 require(!resolve(Stage::n004a,8007,0x15BCCC));
 require(!bullet_cue(Stage::unknown,0x1818AC));
 require(!bullet_cue(static_cast<Stage>(255),0x1818AC));
 require(stage_for_map(20)==Stage::n022a&&stage_for_map(1)==Stage::n001a);
 require(stage_for_map(4)==Stage::n004a&&stage_for_map(21)==Stage::n023a);
 require(stage_for_map(0)==Stage::unknown&&stage_for_map(255)==Stage::unknown);
 std::cout<<"material audio original conversion, silence, stage boundary and unknown rejection PASS\n";
}

