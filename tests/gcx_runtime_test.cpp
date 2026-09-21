#include "gcx_runtime.h"
#include "title_gcx.h"
#include "title_animation.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace mgo2mt;
static void require(bool b){if(!b)throw std::runtime_error("GCX assertion failed");}
template<class F> void rejects(F f){bool threw=false;try{f();}catch(const std::exception&){threw=true;}require(threw);}
static GcxValue arg(int n){return {GcxValue::Kind::integer,n,{}};}
int main(int argc,char** argv){try{
 require(argc==2);std::ifstream f(argv[1],std::ios::binary);require(bool(f));std::vector<char> bytes((std::istreambuf_iterator<char>(f)),{});
 GcxRuntime vm(bytes);require(vm.procedure_count()==21);
 auto compiled=vm.compile_title_program();std::vector<char> native(compiled.begin(),compiled.end());GcxRuntime nv(native);require(nv.native_program());
 nv.bind(0x19000000,0);nv.bind(0x12000006,2);
 for(unsigned proc:{5u,18u,19u,20u})for(int value=(proc==5||proc==18?-1:0);value<=(proc==5||proc==18?-1:5);++value){
  GcxRuntime original(bytes);original.bind(0x19000000,0);original.bind(0x12000006,2);std::vector<GcxValue> args;if(value>=0)args.push_back(arg(value));original.execute(proc,args);nv.execute(proc,args);
  std::vector<std::string> expected,actual;for(auto r:original.requests())if(r.code!=0x3ab23b){r.offset=0;for(auto&v:r.arguments)if(v.kind==GcxValue::Kind::block)v.number=0;for(auto&o:r.options)for(auto&v:o.values)if(v.kind==GcxValue::Kind::block)v.number=0;expected.push_back(gcx_request_json(r,"effect"));}for(const auto&r:nv.requests())actual.push_back(gcx_request_json(r,"effect"));require(expected==actual);
 }
 rejects([&]{nv.execute(20,{arg(6)});});rejects([&]{nv.execute(18,{arg(0)});});
 auto badNative=native;badNative.push_back('X');rejects([&]{GcxRuntime bad(badNative);});badNative=native;badNative.resize(50);rejects([&]{GcxRuntime bad(badNative);});
 std::ostringstream nativeLog;TitleGcx nativeBridge(native,nativeLog);nativeBridge.start(18);unsigned nativeCue=0;int nativeFade=-1;
 nativeBridge.set_se_handler([&](uint32_t cue){nativeCue=cue;});nativeBridge.set_fade_handler([&](int n){nativeFade=n;});nativeBridge.callback(false,1);nativeBridge.callback(true,1);require(nativeCue==18999&&nativeFade>=0&&nativeBridge.loading_requested());nativeBridge.loading_ready();
 rejects([&]{vm.execute(18);});require(vm.requests().empty()); // Unknown state cannot silently default to zero.
 vm.bind(0x19000000,0);vm.bind(0x12000006,2);vm.execute(18);
 require(vm.requests().size()==3);auto title=vm.requests().back();require(title.offset==0x152614&&title.argument(0)==0xb019a7&&title.option(0x468ed)==18000&&title.option(0x1bd06)==0xbfa95f);
 vm.bind(0x19000000,1);vm.execute(18);require(vm.requests().back().option(0x1bd06)==0xe2f087); // Opposite branch, no normal actor.
 vm.execute(20,{arg(1)});require(vm.requests().size()==2&&vm.requests()[0].option(0x3348e5)==18999&&vm.requests()[1].arguments[0].number==0x35b952);
 vm.execute(20,{arg(2)});require(vm.requests().size()==1&&vm.requests()[0].options[0].values[0].number==10);
 vm.execute(20,{arg(5)});require(vm.requests().size()==1&&vm.requests()[0].option(0x3348e5)==92);
 vm.execute(20,{arg(0)});require(vm.requests().empty());
 vm.execute(19,{arg(1)});require(vm.calls()==std::vector<uint32_t>({19,4,3}));
 require(vm.requests().back().argument(0)==0x767e15&&vm.requests().back().option(0x39d643)==5); // Parent args survive nested proc3.
 vm.execute(19,{arg(2)});require(vm.calls()==std::vector<uint32_t>({19}));require(vm.requests().size()==1);
 rejects([&]{vm.execute(20);});require(vm.requests().empty());rejects([&]{vm.execute(0);});rejects([&]{vm.execute(0x8012);});
 std::ostringstream log;TitleGcx bridge(bytes,log);bridge.start(18);require(bridge.timeout()==18000&&bridge.bgm_requested());bridge.callback(false,1);bridge.callback(true,1);
 require(log.str().find("deferred_original_se")!=std::string::npos&&bridge.loading_requested());
 bridge.loading_ready();require(log.str().find("lobby_requested_not_implemented")!=std::string::npos);rejects([&]{bridge.loading_ready();});
 TitleGcx soundBridge(bytes,log);unsigned seCount=0;soundBridge.start(18);soundBridge.set_se_handler([&](uint32_t cue){require(cue==18999);++seCount;});
 soundBridge.callback(false,1);require(seCount==1);soundBridge.callback(true,1);require(soundBridge.loading_requested());
 // Mutating only the GCX timeout changes native configuration. No generated AST or text is consulted.
 auto changed=bytes;changed[0x152641]=0x34;changed[0x152642]=0x12;TitleGcx custom(changed,log);custom.start(18);require(custom.timeout()==0x1234);
 // Real LA2 actor timeout now comes from changed GCX, and feeds result 2 back to it.
 auto root=std::filesystem::path(argv[1]).parent_path().parent_path().parent_path().parent_path();
 std::ifstream af(root/"work/title/animated.m2an",std::ios::binary);require(bool(af));std::vector<char> animation((std::istreambuf_iterator<char>(af)),{});
 TitleAnimation actor(animation,custom.timeout());unsigned selections=0,completions=0;
 auto loadingPath=root/"work/loading/loading.m2an";
 if(std::filesystem::exists(loadingPath)){
  std::ifstream lf(loadingPath,std::ios::binary);std::vector<char> lb((std::istreambuf_iterator<char>(lf)),{});
  TitleAnimation loading(lb);require(loading.texture_count()==2);
  for(int i=0;i<60;++i)loading.tick(5,8);
  require(loading.state()==2&&!loading.geometry().empty()&&loading.accepted()==0);
  lb.pop_back();rejects([&]{TitleAnimation broken(lb);});
 }
 actor.set_callbacks([&](uint32_t result){require(result==2);++selections;custom.callback(false,result);},[&](uint32_t result){require(result==2);++completions;custom.callback(true,result);});
 for(unsigned tick=0;tick<6000&&actor.state()!=4;tick+=5)actor.tick(5,0);
 require(actor.state()==4&&selections==1&&completions==1&&actor.accepted_tick()==0);
 require(actor.callback_tick()<5500); // Original hardcoded 18000 could not pass.
 for(size_t n:{size_t(0),size_t(3),size_t(90),bytes.size()-1}){auto bad=bytes;bad.resize(n);rejects([&]{GcxRuntime broken(bad);broken.bind(0x19000000,0);broken.execute(18);});}
 auto invalid=bytes;invalid[7]=1;rejects([&]{GcxRuntime broken(invalid);}); // Packed locals byte must not be treated as offset.
 invalid=bytes;invalid[0x152587]=static_cast<char>(0x0e); // Existing cue token -> unsupported resource.
 rejects([&]{GcxRuntime broken(invalid);broken.bind(0x19000000,0);broken.execute(18);});
 std::cout<<"GCX title branches, callback 1/2/5/0, nested argument frames, modified binary timeout, and malformed inputs passed\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
