#include "title_animation.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <cstring>
#include <stdexcept>
static void require(bool x){if(!x)throw std::runtime_error("Background animation check failed");}
static uint32_t word(const std::vector<char>& b,size_t p){uint32_t n;std::memcpy(&n,b.data()+p,4);return n;}
int main(int argc,char**argv){try{
 require(argc==2);std::ifstream f(argv[1],std::ios::binary);require(bool(f));std::vector<char>b((std::istreambuf_iterator<char>(f)),{});
 mgo2win::TitleAnimation rear(b),front(b,18000,true);
 rear.tick(5,0);const auto first=rear.node_values(56);
 for(int t=10;t<=400;t+=5)rear.tick(5,0);
 require(rear.node_values(56)[0]!=first[0]||rear.node_values(56)[1]!=first[1]);
 for(int t=405;t<=2825;t+=5)rear.tick(5,0);
 // Source event 0x883CE6 takes 2818 ticks. 24D078 resets its clock to zero
 // on the completing update (2820 at a five-tick cadence), then restarts.
 require(rear.node_values(56)==first);require(rear.loop_restarts()>0);
 for(int t=2830;t<=15000;t+=5)rear.tick(5,0);
 require(rear.loop_restarts()>40&&rear.active_count()==13&&rear.state()==2&&rear.callbacks()==0);
 for(int t=5;t<=300;t+=5)front.tick(5,0);
 require(front.active_count()==13);require(front.node_values(3)!=rear.node_values(3));
 // Reject a zero-duration looping event instead of spinning forever.
 auto bad=b;size_t p=24+size_t(word(b,8))*132+size_t(word(b,12))*140;bool changed=false;
 for(unsigned e=0;e<word(b,16);++e){auto loop=word(b,p+4),tracks=word(b,p+8);p+=12;
  for(unsigned t=0;t<tracks;++t){auto count=word(b,p+4);p+=8;
   for(unsigned c=0;c<count;++c){if(loop&&!changed){uint32_t zero=0;std::memcpy(bad.data()+p+4,&zero,4);}p+=132;}
  }if(loop&&!changed){changed=true;break;}
 }
 bool rejected=false;try{mgo2win::TitleAnimation invalid(bad);}catch(const std::exception&){rejected=true;}require(changed&&rejected);
 std::cout<<"original movement, two layers, 13 loops, clock reset, zero-duration loop rejection: passed\n";
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
