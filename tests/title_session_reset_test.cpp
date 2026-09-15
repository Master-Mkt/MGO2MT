#include "title_animation.h"
#include "title_gcx.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace {
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
std::vector<char> read(const char* path){std::ifstream stream(path,std::ios::binary);check(bool(stream),"asset open");return {std::istreambuf_iterator<char>(stream),{}};}
}
int main(int argc,char** argv){try{
 check(argc==3,"usage: title_session_reset_test animated.m2an scenerio.gcx");
 const auto animationBytes=read(argv[1]),gcxBytes=read(argv[2]);
 std::ostringstream log;
 auto bridge=std::make_unique<mgo2win::TitleGcx>(gcxBytes,log);bridge->start(18);
 const mgo2win::TitleAnimation initial(animationBytes,bridge->timeout());
 auto animation=initial;
 unsigned selected=0,completed=0,cues=0,fades=0;
 auto bind=[&]{
  bridge->set_se_handler([&](uint32_t cue){check(cue==18999,"original START cue");++cues;});
  bridge->set_fade_handler([&](int duration){check(duration>=0,"original fade");++fades;});
  animation.set_callbacks([&](uint32_t result){check(result==1,"only explicit START selected");++selected;animation.wait_for_start(false);bridge->callback(false,result);},
    [&](uint32_t result){check(result==1,"only explicit START completed");++completed;bridge->callback(true,result);});
 };
 auto finish=[&]{for(unsigned i=0;i<1000&&animation.state()!=4;++i)animation.tick(5,0);check(animation.state()==4,"selection completes");};
 bind();for(unsigned t=0;t<900;t+=5)animation.tick(5,t==895?8:0);finish();bridge->loading_ready();
 check(selected==1&&completed==1&&cues==1&&fades>0,"first original title lifecycle");
 // Fatal session return uses fresh original VM state and a pristine actor copy.
 for(unsigned cycle=0;cycle<3;++cycle){
  bridge=std::make_unique<mgo2win::TitleGcx>(gcxBytes,log);bridge->start(18);
  animation=initial;animation.wait_for_start(true);bind();
  check(!bridge->loading_requested()&&animation.ticks()==0&&animation.accepted()==0,"no previous session latch");
  animation.tick(5,8);check(animation.rejected()==1,"held START does not bypass original initial gate");
  for(unsigned t=5;t<600;t+=5)animation.tick(5,0);
  const auto before=animation.node_values(642);
  bool changed=false;
  // Two complete original 18000-tick timeout periods pass without auto-login.
  for(unsigned t=600;t<40000;t+=5){animation.tick(5,0);changed=changed||animation.node_values(642)!=before;}
  check(animation.state()==1&&!animation.result()&&!bridge->loading_requested(),"fatal wait cannot auto-advance");
  check(changed&&!animation.geometry().empty(),"original title motion continues while waiting");
  check(selected==cycle+1&&completed==cycle+1,"wait has no callbacks");
  animation.tick(5,8);check(animation.accepted()==1&&animation.result()==1,"fresh explicit START accepted");finish();
  check(bridge->loading_requested(),"fresh original callback requests loading");bridge->loading_ready();
  check(selected==cycle+2&&completed==cycle+2&&cues==cycle+2,"each reconstructed session callbacks exactly once");
  for(unsigned i=0;i<100;++i)animation.tick(5,8);
  check(selected==cycle+2&&completed==cycle+2,"completed actor ignores repeat input");
 }
 // Disabling the extension resumes the remaining original timeout, not a reset.
 mgo2win::TitleAnimation timeout(animationBytes,1000);timeout.tick(5,0);
 for(unsigned i=0;i<100;++i)timeout.tick(5,0);
 timeout.wait_for_start(true);for(unsigned i=0;i<1000;++i)timeout.tick(5,0);
 check(timeout.state()==1,"pause preserves timeout");timeout.wait_for_start(false);
 for(unsigned i=0;i<99;++i)timeout.tick(5,0);
 check(timeout.state()==1,"remaining timeout preserved exactly");timeout.tick(5,8);
 check(timeout.state()==3&&timeout.result()==2&&timeout.accepted()==0,"original timeout priority preserved after resume");
 std::cout<<"PASS: original GCX/title reset, three explicit START cycles, animated fatal wait, unchanged timeout priority\n";
 return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
