#include "special_action_motion.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using namespace mgo2mt;
static void require(bool ok,const char*what){if(!ok)throw std::runtime_error(what);}
template<class F>static void rejects(F fn){try{fn();}catch(const std::exception&){return;}throw std::runtime_error("invalid motion accepted");}
int main(int argc,char**argv){try{
 require(argc==2,"provide real male bank");std::ifstream file(argv[1],std::ios::binary);
 std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});require(!bytes.empty(),"read bank");
 player::SpecialMotionBank bank(bytes);PlayerMotionBank raw(bytes);
 auto start=bank.sample(player::SpecialPhase::start,0);auto hold=bank.sample(player::SpecialPhase::hold,0);auto end=bank.sample(player::SpecialPhase::end,0);
 require(start&&hold&&end&&!start->rotations.empty(),"all phases decoded");
 require(start->root[1]>900&&hold->root[1]>900&&end->root[1]>900,"original standing posture");
 require(start->rotations!=hold->rotations,"start and held poses distinct");
 auto terminal=bank.sample(player::SpecialPhase::start,player::special_start_ms/1000.0);
 auto expected=raw.sample(PlayerMotion::Idle,39.0/60.0);
 require(terminal->rotations==expected->rotations&&terminal->root==expected->root,"CPU cycle minus base start end");
 terminal=bank.sample(player::SpecialPhase::end,player::special_end_ms/1000.0);expected=raw.sample(PlayerMotion::Run,44.0/60.0);
 require(terminal->rotations==expected->rotations&&terminal->root==expected->root,"CPU cycle minus base end boundary");
 require(bank.sample(player::SpecialPhase::start,1e200)->rotations==bank.sample(player::SpecialPhase::start,10)->rotations,"large time clamps");
 auto loop=bank.sample(player::SpecialPhase::hold,120.0/original::nominal_motion_fps);
 require(loop->rotations==hold->rotations,"hold loops on original cycle");
 require(!bank.sample(player::SpecialPhase(255),0)&&!bank.sample(player::SpecialPhase::start,-1)&&!bank.sample(player::SpecialPhase::start,std::numeric_limits<double>::quiet_NaN()),"invalid phase clock rejects");
 auto invalid=bytes;invalid[16]^=1;rejects([&]{player::SpecialMotionBank bad(invalid);});
 invalid=bytes;invalid[12]=char(PlayerMotion::SelectionSalute);rejects([&]{player::SpecialMotionBank bad(invalid);});
 invalid=bytes;invalid.pop_back();rejects([&]{player::SpecialMotionBank bad(invalid);});
 invalid=bytes;invalid.push_back(0);rejects([&]{player::SpecialMotionBank bad(invalid);});
 std::cout<<"special motion: original three clips, private labels, hold loop, 651/735 timing and malformed rejection passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
