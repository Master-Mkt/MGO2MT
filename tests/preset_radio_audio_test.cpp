#include "preset_radio_audio.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2win::radio_audio;
static void check(bool ok) {if(!ok)throw std::runtime_error("radio audio contract");}
int main() {
 const std::array<unsigned,16> types{7,8,9,10,11,12,13,14,16,17,18,19,20,21,22,23};
 const std::array<unsigned,16> expected{30639,30789,30939,31089,36780,36930,37080,37230,38410,38560,38710,38860,39010,39160,39310,39460};
 unsigned cases=0;
 for(unsigned i=0;i<types.size();++i)for(unsigned id=0;id<256;++id) {
  auto cue=resolve(types[i],id);const bool valid=id<=7||(id>=9&&id<=16);check(bool(cue)==valid);
  if(cue) {check(cue->self==expected[i]+id&&cue->remote==expected[i]+81+id);check(asset_path("data/radio",cue->self).filename()==std::to_string(cue->self)+".gwa");check(!asset_path("data/radio",cue->remote).empty());++cases;}
 }
 check(cases==256);check(!resolve(6,0)&&!resolve(15,0)&&!resolve(24,0)&&!resolve(255,0));check(!resolve(7,std::numeric_limits<unsigned>::max()));check(!resolve(23,0,true));check(resolve(7,0,true).has_value());
 check(asset_path("data/radio",0).empty()&&asset_path("data/radio",30647).empty()&&asset_path("data/radio",0xffffffff).empty());
 std::array<uint8_t,28>a{};a[7]=7;a[8]=15;check(appearance_voice(a)->type==7);
 for(unsigned gender=0;gender<2;++gender)for(unsigned v=0;v<8;++v)for(unsigned p=0;p<=30;++p){a[0]=uint8_t(gender);a[7]=uint8_t((gender?16:7)+v);a[8]=uint8_t(p);const auto x=appearance_voice(a);check(x&&x->type==a[7]&&x->pitchByte==p);check(appearance_voice(std::span(a).first(27)).has_value());}
 a[0]=0;a[7]=16;check(!appearance_voice(a));a[0]=2;check(!appearance_voice(a));a[0]=1;a[8]=31;check(!appearance_voice(a));a[8]=255;check(!appearance_voice(a));check(!appearance_voice({}));check(!appearance_voice(std::span(a).first(9)));
 for(auto type:types) {
  float previous=0;
  for(unsigned p=0;p<=30;++p){auto ratio=pitch_ratio(type,p);check(ratio&&std::isfinite(*ratio)&&*ratio>=previous&&*ratio>=.9132419f&&*ratio<=1.095001f);previous=*ratio;}
  check(*pitch_ratio(type,15)==1.0f);check(std::abs(*pitch_ratio(type,0)-.913241982f)<1e-7f);check(std::abs(*pitch_ratio(type,30)-1.0950000286f)<1e-7f);check(std::abs(*pitch_ratio(type,8)-.9585325f)<1e-5f);check(*pitch_ratio(type,22)<1.05f);
 }
 for(unsigned t=0;t<=6;++t)check(*pitch_ratio(t,0)==1&&*pitch_ratio(t,30)==1);
 check(*pitch_ratio(25,0)==1);check(!pitch_ratio(26,15)&&!pitch_ratio(7,31));
 std::cout<<"PASS 16 voices x 16 presets, wire appearance bounds, original pitch range, special branch rejection\n";
}
