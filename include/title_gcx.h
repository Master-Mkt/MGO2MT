#pragma once
#include "gcx_runtime.h"
#include <ostream>
namespace mgo2win {
// Host bridge for the reviewed normal-title/loading route. The renderer signals
// loading_ready after presentation; remaining unimplemented effects are logged.
class TitleGcx {
 GcxRuntime runtime_;
 std::ostream& log_;
 uint32_t selected_=0,completed_=0,timeout_=0;
 bool bgm_=false;
 std::function<void(int)> fade_;
 std::function<void(uint32_t)> se_;
 uint32_t loading_callback_=0;
 bool loading_requested_=false,loading_completed_=false;
 void trace(const char* event,uint32_t procedure);
public:
 TitleGcx(std::vector<char> bytes,std::ostream& log):runtime_(std::move(bytes)),log_(log){}
 void start(uint32_t entry);
 void callback(bool completed,uint32_t result);
 void set_fade_handler(std::function<void(int)> handler){fade_=std::move(handler);}
 void set_se_handler(std::function<void(uint32_t)> handler){se_=std::move(handler);}
 bool loading_requested()const{return loading_requested_;}
 void loading_ready();
 uint32_t timeout()const{return timeout_;}
 bool bgm_requested()const{return bgm_;}
};
}
