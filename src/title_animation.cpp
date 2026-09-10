// Evidence: C07D88 normal-layout branch, 24AE50/249D40/24D078,
// 250A40 -> 252010 et al., 24F838/24FAB0/24ED80/250390.
// ELF SHA256 1a55a41ee5afdebd89075bd205e412c8311f569ba9dc022f629bb63fb4bfd13a.
// Forward 7/8 tracks only; GPU/color composition retains the phase-two approximation.
#include "title_animation.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace mgo2win {
namespace {
struct Reader {
 const std::vector<char>& b;size_t p=4;
 uint32_t word(){if(p+4>b.size())throw std::runtime_error("Animation truncated");uint32_t v;std::memcpy(&v,b.data()+p,4);p+=4;return v;}
 float number(){uint32_t v=word();float f;std::memcpy(&f,&v,4);if(!std::isfinite(f)||std::abs(f)>100000)throw std::runtime_error("Animation numeric range");return f;}
 Values values(){Values v;for(auto& f:v)f=number();if(v[29]!=0&&v[29]!=1)throw std::runtime_error("Animation blend range");return v;}
};
struct Transform {float a=1,b=0,x=0,c=0,d=1,y=0;std::array<float,4> color{255,255,255,128};bool visible=true;};
}
TitleAnimation::TitleAnimation(const std::vector<char>& bytes,uint32_t timeout,bool foreground):foreground_(foreground),remaining_(timeout){
 if(!timeout||timeout>180000)throw std::runtime_error("Title timeout range");
 if(bytes.size()<24||std::memcmp(bytes.data(),"M2AN",4))throw std::runtime_error("Animation magic");Reader r{bytes};
 auto version=r.word();if(version!=1&&version!=2&&version!=3)throw std::runtime_error("Animation version");loading_=version==2;background_=version==3;auto count=r.word(),quads=r.word(),events=r.word(),textures=r.word();textures_=textures;
 if(!count||count>10000||!quads||quads>10000||(background_?(count!=125||quads!=83||events!=17||textures!=8):loading_?(count!=11||events!=1||textures!=2):(events!=5||textures!=10)))throw std::runtime_error("Animation counts");
 nodes_.resize(count+1);
 for(uint32_t i=1;i<=count;++i){auto& n=nodes_[i];n.parent=r.word();n.type=r.word();n.flags=r.word();n.initial=n.value=r.values();if(n.parent>=i||(n.type!=0&&n.type!=2&&n.type!=3&&n.type!=4&&n.type!=6&&n.type!=10)||(n.flags&0x8000))throw std::runtime_error("Animation node contract");}
 templates_.resize(quads);
 for(auto&q:templates_){q.atlas=static_cast<int32_t>(r.word());q.blend=r.word();q.node=r.word();if(q.atlas< -1||q.atlas>=static_cast<int>(textures)||!q.node||q.node>count||q.blend<0||q.blend>1)throw std::runtime_error("Animation quad contract");for(auto&v:q.vertices){float f[8];for(auto&x:f)x=r.number();std::memcpy(&v,f,sizeof(v));}}
 for(uint32_t i=0;i<events;++i){Event e;e.id=r.word();auto loop=r.word();if(loop>1||(loop&&!background_))throw std::runtime_error("Animation loop unsupported");e.loop=loop!=0;auto nt=r.word();if(e.loop&&nt!=1)throw std::runtime_error("Background loop must have one track");if(!nt||nt>1000)throw std::runtime_error("Animation tracks");
  if(std::any_of(events_.begin(),events_.end(),[&](const Event& a){return a.id==e.id;}))throw std::runtime_error("Duplicate event");
  for(uint32_t j=0;j<nt;++j){Track t;t.node=r.word();auto nc=r.word();if(t.node>count||(!t.node&&e.id!=0x298bf7)||!nc||nc>1000)throw std::runtime_error("Animation track contract");
   uint64_t duration=0;
   for(uint32_t k=0;k<nc;++k){Command c;c.op=r.word();c.duration=r.word();c.mask=r.word();c.target=r.values();duration+=c.duration;
    if((c.op!=0&&c.op!=3&&c.op!=7&&c.op!=8)||c.mask>>30||duration>100000||((c.op==0||c.op==3)&&k+1!=nc))throw std::runtime_error("Animation command contract");t.commands.push_back(c);}
   if(e.loop&&!duration)throw std::runtime_error("Zero-duration animation loop");
   if(t.commands.back().op!=0&&t.commands.back().op!=3)throw std::runtime_error("Missing track terminator");e.tracks.push_back(t);}
  events_.push_back(e);}
 if(r.p!=bytes.size()||(!loading_&&!background_&&count<642))throw std::runtime_error("Animation extent");
 if(background_){
  for(auto id:{0x94eaa6u,0x9aacfau,0x43478du,0x2c517du})if(event(id).loop)throw std::runtime_error("Background setup must not loop");
  for(uint32_t id=0x883ce6;id<=0x883ceb;++id)if(!event(id).loop)throw std::runtime_error("Missing background loop");
  for(uint32_t id=0x8854e6;id<=0x8854ec;++id)if(!event(id).loop)throw std::runtime_error("Missing background loop");
 }else if(loading_)event(0x9ca2fa);else for(auto id:{0xf7c4bcu,0x298bf7u,0xf8b0f2u,0x850634u,0xf877cau})event(id);
}
const Event& TitleAnimation::event(uint32_t id)const{auto it=std::find_if(events_.begin(),events_.end(),[=](const Event&e){return e.id==id;});if(it==events_.end())throw std::runtime_error("Unknown event");return *it;}
float TitleAnimation::interpolate(float a,float b,float t){float inverse=1.0f-t;float value=t*b+inverse*a;return std::trunc(value+(value<0?-.5f:.5f));}
void TitleAnimation::start(uint32_t id){
 if(std::any_of(active_.begin(),active_.end(),[=](const Active&a){return a.id==id;}))return;
 Active a;a.id=id;for(const auto&t:event(id).tracks){Cursor c;c.node=t.node?t.node:642;c.snapshot=nodes_[c.node].value;a.cursors.push_back(c);}active_.push_back(a);
}
void TitleAnimation::advance(uint32_t delta){
 for(auto&a:active_){a.time+=delta;const auto&e=event(a.id);
  for(size_t i=0;i<a.cursors.size();++i){auto&c=a.cursors[i];const auto&t=e.tracks[i];
   unsigned steps=0;while(!c.done){if(++steps>10000)throw std::runtime_error("Animation work limit");const auto&cmd=t.commands.at(c.index);if(cmd.op==0||cmd.op==3){if(e.loop){a.time=0;c.start=0;c.index=0;c.snapshot=nodes_[c.node].value;++loop_restarts_;break;}c.done=true;break;}
    float weight=cmd.op==7||!cmd.duration?1.f:std::min(1.f,float(a.time-c.start)/float(cmd.duration));auto&n=nodes_[c.node];
    if(!(n.flags&0x200000))for(unsigned f=0;f<30;++f)if(cmd.mask&(1u<<f))n.value[f]=f>=28?cmd.target[f]:interpolate(c.snapshot[f],cmd.target[f],weight);
    if(a.time<c.start+cmd.duration)break;
    c.start+=cmd.duration;++c.index;c.snapshot=n.value;
   }
  }
 }
 std::erase_if(active_,[](const Active&a){return std::all_of(a.cursors.begin(),a.cursors.end(),[](const Cursor&c){return c.done;});});
}
void TitleAnimation::tick(uint32_t delta,uint32_t pressed){
 if(!delta||delta>1000)throw std::runtime_error("Animation delta range");ticks_+=delta;
 // A2DE84 starts two bg_anime instances and thirteen independently repeating
 // events. 24D078 opcode4 waits are compiled to zero-mask timed commands.
 if(background_){if(state_==0){start(foreground_?0x9aacfa:0x94eaa6);start(0x43478d);if(!foreground_)start(0x2c517d);for(uint32_t id=0x8854e6;id<=0x8854ec;++id)start(id);for(uint32_t id=0x883ce6;id<=0x883ceb;++id)start(id);state_=2;}advance(delta);return;}
 if(loading_){if(state_==0){start(0x9ca2fa);state_=2;}advance(delta);return;}
 if(state_==0){start(0xf7c4bc);state_=1;if(pressed&8)++rejected_;}
 else if(state_==1){wait_=std::min(621u,wait_+delta);remaining_=remaining_>delta?remaining_-delta:0;
  if(wait_<=620){if(pressed&8)++rejected_;}
  else if(!remaining_){std::erase_if(active_,[](const Active&a){return a.id==0x298bf7;});start(0x850634);state_=3;result_=2;if(selected_)selected_(result_);}
  else if(pressed&8){std::erase_if(active_,[](const Active&a){return a.id==0x298bf7;});start(0xf8b0f2);state_=3;result_=1;++accepted_;accepted_tick_=ticks_;if(selected_)selected_(result_);}
  else start(0x298bf7);
 }else if(state_==3&&active_.empty()){state_=4;++callbacks_;callback_tick_=ticks_;if(completed_)completed_(result_);}
 if(state_!=4)advance(delta);
}
std::vector<Quad> TitleAnimation::geometry()const{
 std::vector<Transform> transforms(nodes_.size());std::vector<std::array<float,16>> colors(nodes_.size());
 for(size_t i=1;i<nodes_.size();++i){const auto&n=nodes_[i];const auto&v=n.value;auto&p=transforms[n.parent];auto&t=transforms[i];t=p;t.visible=p.visible&&v[28]!=0;
  for(unsigned k=0;k<16;++k)colors[i][k]=std::min(255.f,std::round(v[10+k]*p.color[k%4]/(k%4==3?128.f:255.f)));
  for(unsigned k=0;k<4;++k)t.color[k]=colors[i][k];
  if(n.type==0){float angle=v[27]*6.28318530718f/4096,scale=v[26]/256,co=std::cos(angle)*scale,si=std::sin(angle)*scale;t.a=p.a*co+p.b*si;t.b=-p.a*si+p.b*co;t.c=p.c*co+p.d*si;t.d=-p.c*si+p.d*co;t.x=p.a*v[0]/16+p.b*v[1]/16+p.x;t.y=p.c*v[0]/16+p.d*v[1]/16+p.y;}
 }
 std::vector<Quad> result;result.reserve(templates_.size());
 for(auto q:templates_){const auto&n=nodes_[q.node];if(!transforms[q.node].visible)continue;const auto&v=n.value;const auto&p=transforms[n.parent];q.blend=static_cast<int>(v[29]);
  for(unsigned k=0;k<4;++k){float x,y;if(n.type==4||n.type==6){x=v[k*2]/16;y=v[k*2+1]/16;}
   else if(n.type==10){x=q.vertices[k].x+(v[0]-n.initial[0])/16;y=q.vertices[k].y+(v[1]-n.initial[1])/16;}
   else{x=(v[0]+((k==1||k==2)?v[8]:0))/16;y=(v[1]+(k>=2?v[9]:0))/16;}
   auto&out=q.vertices[k];out.x=p.a*x+p.b*y+p.x;out.y=p.c*x+p.d*y+p.y;out.r=colors[q.node][k*4]/255;out.g=colors[q.node][k*4+1]/255;out.b=colors[q.node][k*4+2]/255;out.a=std::min(1.f,colors[q.node][k*4+3]/128);
  }result.push_back(q);
 }return result;
}
}
