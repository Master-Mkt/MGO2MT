#include "host_match.h"
#include <iostream>
#include <fstream>
#include <map>
using namespace mgo2win::host;
void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
std::vector<uint8_t> delta(std::map<unsigned,std::vector<uint8_t>>fields){
 std::vector<uint8_t>b(9);b[0]=11;for(auto&[id,value]:fields){b[2+id/8]|=uint8_t(1u<<(id%8));b.insert(b.end(),value.begin(),value.end());}return b;
}
int main(int argc,char**argv){try{
 std::vector<uint8_t>rotation(48);rotation[0]=20;rotation[1]=1;rotation[2]=34;rotation[45]=15;rotation[46]=17;rotation[47]=2;
 MatchState s;update_match(s,delta({{51,{255}}}));check(s.pending&&!s.request,"generation alone is not a map request");
 update_match(s,delta({{2,rotation},{9,{15}}}));check(s.pending&&!s.request,"round must be known before snapshot");
 update_match(s,delta({{13,{0}}}));check(s.request&&s.request->rotation==Rotation{15,17,2}&&s.request->generation==255&&s.request->transition==MatchTransition::initial,"partial initial snapshot joins correctly including last slot");
 auto original=*s.request;auto revision=s.revision;update_match(s,delta({{51,{255}}}));check(s.revision==revision&&s.request==original,"duplicate generation does not restart loading");
 update_match(s,delta({{51,{0}}}));check(s.request->transition==MatchTransition::round_restart&&s.request->sequence==2,"generation rollover and initial partial fields do not leak dirty state");
 update_match(s,delta({{13,{1}}}));check(s.request->round==0,"uncommitted round does not mutate active snapshot");
 update_match(s,delta({{51,{1}}}));check(s.request->transition==MatchTransition::next_round&&s.request->round==1,"round change committed at generation");
 update_match(s,delta({{9,{0}}}));check(s.request->index==15,"new index waits for generation");
 update_match(s,delta({{51,{2}}}));check(s.request->transition==MatchTransition::map_change&&s.request->rotation==Rotation{20,1,34},"map IDs above 15 remain valid and map/rule order preserved");
 update_match(s,delta({{9,{15}},{13,{0}},{51,{3}}}));check(s.request->transition==MatchTransition::map_and_round_change,"combined transition retains original kind 4");
 revision=s.revision;update_match(s,delta({{0,{7}}}));check(s.revision==revision+1&&s.phase==7&&s.request->generation==3,"phase change published without inventing load completion");
 auto before=s;auto malformed=delta({{9,{0}},{51,{4}}});malformed.pop_back();try{update_match(s,malformed);check(false,"expected truncation");}catch(const Invalid&){}check(s==before,"malformed delta commits no fields");
 for(auto b:{delta({{9,{16}},{51,{4}}}),delta({{2,{1,2,3}}}),delta({{55,{1}}})}){try{update_match(s,b);check(false,"expected bounds rejection");}catch(const Invalid&){}check(s==before,"bounds failures preserve prior snapshot");}
 auto extra=delta({{51,{4}}});extra.push_back(0);try{update_match(s,extra);check(false,"expected trailing bytes rejection");}catch(const Invalid&){}check(s==before,"trailing data not committed");
 check(global_generation(delta({{16,{1,2,3,4}},{51,{123}}}))==123,"group descriptor 16 consumes four bytes, not count multiplied");
 update_match(s,std::vector<uint8_t>{11,25,9,8,7});check(s==before,"non-global caches cannot change stage state");
 MatchState empty;update_match(empty,delta({{2,std::vector<uint8_t>(48)},{9,{0}},{13,{0}},{51,{0}}}));check(empty.request&&empty.request->rotation.map==0,"empty map sentinel is exposed, never reported loaded");
 if(argc>1){
  std::ifstream in(argv[1],std::ios::binary);MatchState retail;
  for(unsigned i=0;i<4;++i){int lo=in.get(),hi=in.get();check(lo>=0&&hi>=0,"retail delta length");unsigned n=unsigned(lo)|(unsigned(hi)<<8);check(n<=2048,"retail delta bound");std::vector<uint8_t>b(n);in.read(reinterpret_cast<char*>(b.data()),n);check(bool(in),"retail delta read");update_match(retail,b);
   check(retail.request&&retail.request->rotation==Rotation{15,1,2}&&retail.request->index==5&&retail.request->round==0,"retail selected rotation remains stable");
   check(retail.request->generation==(i?106:105)&&retail.request->transition==(i?MatchTransition::round_restart:MatchTransition::initial),"retail restart generation/classification");
   check(retail.phase==std::array<uint8_t,4>{3,7,0,1}[i],"retail phase sequence");
  }
  check(in.peek()==EOF,"retail trailing bytes");
 }
 std::cout<<"host match atomic snapshots, boundaries and map/round transitions passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
