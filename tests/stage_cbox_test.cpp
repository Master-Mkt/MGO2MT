#include "stage_cbox.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
using namespace mgo2mt::stage;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(int argc,char**argv){try{
 CboxLayout layout;layout.count=15;
 // Identical hashes are real: all 46 n022a CBOX children use 0x15275E.
 for(unsigned i=0;i<46;++i)layout.anchors.push_back({100+32*i,0x15275e,{float(i),0,0}});
 // Captured integer selection results from the original ELF's PPC slices
 // 73DE18..34, 73DE50..60, 73DE80/88/AC/B4, with signed low16 at 73DF64.
 const std::array<unsigned,15> indices{1,19,2,13,21,7,9,29,17,45,30,10,14,3,43};
 const std::array<int,15> angles{-29850,-27940,5090,28664,-13154,29524,12698,-18192,-298,-25396,12370,-17688,526,-4284,-4086};
 auto zero=layout.select(0);
 for(size_t i=0;i<indices.size();++i){check(zero[i].candidateIndex==indices[i]&&zero[i].rotationUnits==angles[i],"original PPC vector (including collision probes)");check(zero[i].anchor==layout.anchors[indices[i]],"authored node offset is retained");check(std::abs(zero[i].rotationRadians-float(angles[i])*9.58738019107841e-05f)<1e-6f,"original float angle constant");}
 auto last=layout.select(255);check(last.front().candidateIndex==30&&last.front().rotationUnits==-29043&&last.back().candidateIndex==44&&last.back().rotationUnits==-22751,"unsigned generation and low32 wrap");
 for(unsigned seed=0;seed<256;++seed){auto objects=layout.select(uint8_t(seed));std::set<uint32_t> seen;for(auto&p:objects)check(seen.insert(p.anchor.sourceOffset).second,"no repeated node at any byte generation");check(objects==layout.select(uint8_t(seed)),"late participant determinism");}
 layout.count=46;check(layout.select(255).size()==46,"full candidate list terminates");
 layout.count=47;bool bad=false;try{layout.select(1);}catch(...){bad=true;}check(bad,"overfull request rejected before probe loop");
 for(auto text:{"MGO2MT.STAGE_CBOX 1 1 0","MGO2MT.STAGE_CBOX 1 0 65","MGO2MT.STAGE_CBOX 1 1 2 1 2 0 0 0 1 2 1 1 1","MGO2MT.STAGE_CBOX 1 1 1 1 2 0 0","MGO2MT.STAGE_CBOX 1 0 0 extra"}){bool rejected=false;try{std::istringstream in(text);CboxLayout::read(in);}catch(...){rejected=true;}check(rejected,"invalid/truncated CBOX layout");}
 std::istringstream empty("MGO2MT.STAGE_CBOX 1 0 0");check(CboxLayout::read(empty).select(0).empty(),"empty layout never divides by zero");
 if(argc>1){std::ifstream in(argv[1]);auto actual=CboxLayout::read(in);check(actual.count==15&&actual.anchors.size()==46,"original proc100 count and GEOM children");auto objects=actual.select(0);check(objects.front().anchor.sourceOffset==0x54d7a0&&objects.front().anchor.position==Vec3{-72287.6796875f,4000,73338.7578125f},"original authored position with repeated hash");}
 std::cout<<"CBOX PPC vectors, shared generation, unique authored nodes and bounds passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
