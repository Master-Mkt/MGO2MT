#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <functional>
namespace mgo2win {
struct Vertex {float x,y,u,v,r,g,b,a;};
struct Quad {int32_t atlas,blend;uint32_t node;Vertex vertices[4];};
using Values=std::array<float,30>;
struct Node {uint32_t parent,type,flags;Values initial,value;};
struct Command {uint32_t op,duration,mask;Values target;};
struct Track {uint32_t node;std::vector<Command> commands;};
struct Event {uint32_t id;bool loop=false;std::vector<Track> tracks;};
struct Cursor {uint32_t node,index=0,start=0;Values snapshot{};bool done=false;};
struct Active {uint32_t id,time=0;std::vector<Cursor> cursors;};
class TitleAnimation {
 bool loading_=false,background_=false,foreground_=false;
 uint32_t loop_restarts_=0;
 uint32_t textures_=10;
 std::vector<Node> nodes_;std::vector<Quad> templates_;std::vector<Event> events_;std::vector<Active> active_;
 uint32_t ticks_=0,wait_=0,state_=0,accepted_=0,rejected_=0,callbacks_=0,accepted_tick_=0,callback_tick_=0;
 uint32_t remaining_=18000,result_=0; // nttitle proc18, option 0x468ED at GCX 0x15263B.
 bool wait_for_start_=false;
 std::function<void(uint32_t)> selected_,completed_;
 const Event& event(uint32_t id)const;
 void start(uint32_t id);void advance(uint32_t delta);
public:
 explicit TitleAnimation(const std::vector<char>& bytes,uint32_t timeout=18000,bool foreground=false);
 void tick(uint32_t delta,uint32_t pressed);
 // Native disconnect presentation: keep the original animation and START gate,
 // but suspend its attract timeout until the user explicitly resumes.
 void wait_for_start(bool enabled){wait_for_start_=enabled;}
 void set_callbacks(std::function<void(uint32_t)> selected,std::function<void(uint32_t)> completed){selected_=std::move(selected);completed_=std::move(completed);}
 std::vector<Quad> geometry()const;
 uint32_t ticks()const{return ticks_;}uint32_t state()const{return state_;}
 bool ready_for_start()const{return !loading_&&!background_&&state_==1&&wait_>620;}
 uint32_t accepted()const{return accepted_;}uint32_t rejected()const{return rejected_;}
 uint32_t callbacks()const{return callbacks_;}uint32_t accepted_tick()const{return accepted_tick_;}uint32_t callback_tick()const{return callback_tick_;}
 uint32_t result()const{return result_;}
 uint32_t loop_restarts()const{return loop_restarts_;}
 uint32_t texture_count()const{return textures_;}
 size_t active_count()const{return active_.size();}const Values& node_values(uint32_t id)const{return nodes_.at(id).value;}
 static float interpolate(float start,float target,float weight);
};
}
