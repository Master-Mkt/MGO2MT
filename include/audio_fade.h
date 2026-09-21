#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace mgo2mt {
// Reviewed normal BGM stop route: 4FB50 -> 4EE20, updates at 4E8D8.
// Source ELF 1a55a41ee5afdebd89075bd205e412c8311f569ba9dc022f629bb63fb4bfd13a.
// One already-playing voice, exponent 1, native nominal frame delta; no crossfade.
class AudioFade {
 float gain_=1,step_=0;
 unsigned remaining_=0;
public:
 void stop(int tenths){
  if(tenths<0||tenths>6000)throw std::runtime_error("Unsupported BGM fade range");
  float seconds=static_cast<float>(tenths)*0.1f;
  float distance=-gain_;
  remaining_=static_cast<unsigned>(std::abs(static_cast<int>(distance*(seconds*60.f))));
  if(!remaining_){gain_=0;step_=0;}else step_=distance/static_cast<float>(remaining_);
 }
 void advance(unsigned delta){auto n=std::min({delta,3u,remaining_});remaining_-=n;gain_=std::max(0.f,gain_+static_cast<float>(n)*step_);if(n&&!remaining_)gain_=0;}
 float gain()const{return gain_;}
 unsigned remaining()const{return remaining_;}
};
}
