#pragma once
#include "multi_ui.h"
#include <optional>
namespace mgo2mt::multi_ui {
// Optional local layout. Its explicit click actions affect local overlay state
// and report named events; the caller decides which game events it accepts.
class Layer {
 Runtime runtime_;std::filesystem::path path_;
 std::optional<std::filesystem::file_time_type> modified_;
 bool observed_=false,painted_=false;uint64_t nextPoll_=0,paintedRevision_=0;
 Context context_;std::vector<uint8_t> rgba_;
public:
 explicit Layer(std::filesystem::path path):path_(std::move(path)){}
 const std::vector<uint8_t>* frame(uint64_t now,const Context&,bool focused=true,bool soundEnabled=true);
 bool click(float designX,float designY){return runtime_.click(designX,designY,context_);}
 bool pointer(float designX,float designY,bool down){return runtime_.pointer(designX,designY,down,context_);}
 bool key(unsigned virtualKey){return runtime_.key(virtualKey,context_);}
 bool trigger(std::string_view id){return runtime_.trigger(id,context_);}
 void cancel_actions(std::string reason="cancelled"){runtime_.cancel_actions(std::move(reason));}
 std::vector<ActionEvent> take_events(){return runtime_.take_events();}
 bool ready()const{return runtime_.ready();}
 bool busy()const{return runtime_.busy();}
 const std::string& error()const{return runtime_.error();}
};
}
