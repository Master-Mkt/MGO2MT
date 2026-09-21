#include "multi_ui_layer.h"
#include <algorithm>
#include <iostream>
#include <syncstream>
namespace mgo2mt::multi_ui {
const std::vector<uint8_t>* Layer::frame(uint64_t now,const Context& context,bool focused,bool soundEnabled){
 if(!observed_||now>=nextPoll_){
  nextPoll_=now+1000;
  std::error_code ec;std::optional<std::filesystem::file_time_type> stamp;
  if(std::filesystem::is_regular_file(path_,ec)){
   auto value=std::filesystem::last_write_time(path_,ec);if(!ec)stamp=value;
  }
  if(!observed_||stamp!=modified_){
   const bool hadFile=modified_.has_value();observed_=true;modified_=stamp;painted_=false;
   if(stamp){
    if(runtime_.load(path_,path_.parent_path()))std::osyncstream(std::cout)<<"multi_ui: layout loaded\n";
    else std::osyncstream(std::cout)<<"multi_ui: default UI retained: "<<runtime_.error()<<'\n';
   }else{runtime_=Runtime{};if(hadFile)std::osyncstream(std::cout)<<"multi_ui: layout removed; default UI retained\n";}
  }
 }
 if(!runtime_.ready())return nullptr;
 runtime_.update(context,focused,soundEnabled,now);
 if(!painted_||paintedRevision_!=runtime_.revision()||context.screen!=context_.screen||context.state!=context_.state||context.flags!=context_.flags||context.bindings!=context_.bindings){
  rgba_.assign(1280*720*4,0);
  try{if(!runtime_.paint(rgba_,1280,720,context)){
   std::osyncstream(std::cout)<<"multi_ui: paint failed; default UI retained: "<<runtime_.error()<<'\n';
   runtime_=Runtime{};painted_=false;return nullptr;
  }}
  catch(const std::exception&e){
   std::osyncstream(std::cout)<<"multi_ui: paint failed; default UI retained: "<<e.what()<<'\n';
   runtime_=Runtime{};painted_=false;return nullptr;
  }
  context_=context;painted_=true;paintedRevision_=runtime_.revision();
 }
 return &rgba_;
}
}
