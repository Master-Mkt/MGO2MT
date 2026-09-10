#include "title_gcx.h"
#include <stdexcept>
#include <syncstream>
namespace mgo2win {
void TitleGcx::trace(const char* event,uint32_t procedure){
 log_<<"{\"gcx_event\":\""<<event<<"\",\"entry_procedure\":"<<procedure<<",\"calls\":[";
 bool first=true;for(auto p:runtime_.calls()){if(!first)log_<<',';first=false;log_<<p;}log_<<"]}"<<std::endl;
}
void TitleGcx::start(uint32_t entry){
 if(timeout_)throw std::runtime_error("GCX title already started");
 runtime_.bind(0x19000000,0); // Explicit normal-edition bootstrap input.
 runtime_.bind(0x12000006,2); // Title dispatch state; full boot is not executed.
 runtime_.execute(entry);trace("start",entry);
 for(const auto& r:runtime_.requests()){
  const char* disposition="debug_log";
  if(r.code==0x82bc9&&r.argument(0)==0x3041cb&&r.option(0x3348e5)==23){
   if(bgm_)throw std::runtime_error("Duplicate title BGM request");bgm_=true;disposition="bgm23_adapter_requested";
  }else if(r.code==0x6592a7&&r.argument(0)==0xb019a7){
   if(timeout_||r.option(0x1bd06)!=0xbfa95f)throw std::runtime_error("Unsupported title layout/duplicate actor");
   auto t=r.option(0x468ed),s=r.option(0x983d71),c=r.option(0x39d643);
   if(t<=0||t>180000||s<=0||c<=0||uint64_t(s)>runtime_.procedure_count()||uint64_t(c)>runtime_.procedure_count())throw std::runtime_error("GCX title option range");
   timeout_=static_cast<uint32_t>(t);selected_=static_cast<uint32_t>(s);completed_=static_cast<uint32_t>(c);disposition="native_title_actor";
  }else if(r.code!=0x3ab23b)throw std::runtime_error("Unsupported GCX bootstrap effect");
  std::osyncstream(log_)<<gcx_request_json(r,disposition)<<std::endl;
 }
 if(!timeout_||!bgm_)throw std::runtime_error("GCX entry did not configure title and BGM");
 std::osyncstream(log_)<<"{\"gcx_title_config\":true,\"timeout\":"<<timeout_<<",\"selected_proc\":"<<selected_<<",\"completed_proc\":"<<completed_<<"}"<<std::endl;
}
void TitleGcx::callback(bool completed,uint32_t result){
 if(!timeout_)throw std::runtime_error("GCX title not started");auto proc=completed?completed_:selected_;
 runtime_.execute(proc,{{GcxValue::Kind::integer,result,{}}});trace(completed?"completed":"selected",proc);
 for(const auto& r:runtime_.requests()){
  const char* disposition="deferred_unimplemented_effect";
  if(r.code==0x3ab23b)disposition="debug_log";
  else if(r.code==0x82bc9&&r.argument(0)==0xb03162){
   auto cue=r.option(0x3348e5);if(cue!=18999)throw std::runtime_error("Unreviewed sound cue");
   disposition="deferred_original_se";if(se_){se_(static_cast<uint32_t>(cue));disposition="native_start_sound_requested";}
  }
  else if(r.code==0x82bc9&&r.argument(0)==0x35b952){
   disposition="deferred_bgm_fade";
   if(fade_){auto n=r.option(0x3bb205);if(n<0||n>6000)throw std::runtime_error("BGM fade option range");fade_(static_cast<int>(n));disposition="native_bgm_fade_requested";}
  }
  else if(r.code==0x6592a7&&r.argument(0)==0x767e15){
   if(r.option(0x4e4bf2)!=0xe4fa0f||r.option(0x3d11ec)!=1||r.option(0x39d643)!=5||loading_requested_)throw std::runtime_error("Unreviewed loading request");
   loading_callback_=5;loading_requested_=true;disposition="native_loading_requested";
  }else if(r.code==0x6592a7)disposition="deferred_actor_no_callback_scheduled";
  std::osyncstream(log_)<<gcx_request_json(r,disposition)<<std::endl;
 }
}
void TitleGcx::loading_ready(){
 if(!loading_requested_||loading_completed_)throw std::runtime_error("Loading callback lifecycle");
 loading_completed_=true;runtime_.execute(loading_callback_);trace("loading_ready",loading_callback_);
 bool stage=false;
 for(const auto&r:runtime_.requests()){
  const char* disposition="deferred_unimplemented_effect";
  if(r.code==0x37c884){
   if(r.arguments.size()!=1||r.arguments[0].text!="lobby")throw std::runtime_error("Unreviewed next stage");
   stage=true;disposition="lobby_requested_not_implemented";
  }
  std::osyncstream(log_)<<gcx_request_json(r,disposition)<<std::endl;
 }
 if(!stage)throw std::runtime_error("Loading callback did not request lobby");
}
}
