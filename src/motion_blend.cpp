#include "motion_blend.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace mgo2mt {
namespace {
using Quaternion=std::array<float,4>;
bool normalize(Quaternion& q){
 double squared=0;
 for(float v:q){if(!std::isfinite(v))return false;squared+=double(v)*v;}
 if(!(squared>0))return false;
 const double norm=std::sqrt(squared);for(float& v:q)v=static_cast<float>(v/norm);
 return true;
}
bool prepare(const MotionPose& input,MotionPose& out){
 if(!input.rootBone||input.rotations.empty()||input.rotations.size()>128||!input.rotations.contains(input.rootBone))return false;
 for(float v:input.root)if(!std::isfinite(v)||std::abs(v)>1e6f)return false;
 out=input;
 for(auto& [bone,q]:out.rotations)if(!bone||!normalize(q))return false;
 return true;
}
bool compatible(const MotionPose& a,const MotionPose& b){
 if(a.rootBone!=b.rootBone||a.rotations.size()!=b.rotations.size())return false;
 auto first=a.rotations.begin();auto second=b.rotations.begin();
 for(;first!=a.rotations.end();++first,++second)if(first->first!=second->first)return false;
 return true;
}
Quaternion slerp(const Quaternion& a,const Quaternion& b,double alpha){
 if(alpha<=0)return a;
 double product=0;for(unsigned i=0;i<4;++i)product+=double(a[i])*b[i];
 const double sign=product<0?-1:1;
 product=std::clamp(product*sign,0.,1.);
 double left=1-alpha,right=alpha;
 if(product<1-1e-12){
  const double angle=std::acos(product),divisor=std::sin(angle);
  left=std::sin((1-alpha)*angle)/divisor;right=std::sin(alpha*angle)/divisor;
 }
 Quaternion out{};for(unsigned i=0;i<4;++i)out[i]=static_cast<float>(left*a[i]+right*sign*b[i]);
 normalize(out);return out;
}
}
void MotionBlend::reset(){displayed_={};from_={};key_=0;alpha_=1;initialized_=false;accepted_=false;}
const MotionPose& MotionBlend::update(uint64_t key,const MotionPose& input,double seconds,float rate){
 MotionPose target;
 if(!key||!std::isfinite(seconds)||seconds<0||!std::isfinite(rate)||rate<=0||!prepare(input,target)||
    (initialized_&&!compatible(displayed_,target))){
  accepted_=false;
  if(!initialized_)throw std::invalid_argument("Invalid complete pose or motion blend input");
  return displayed_;
 }
 accepted_=true;
 if(!initialized_){displayed_=std::move(target);key_=key;alpha_=1;initialized_=true;return displayed_;}
 if(key!=key_){from_=displayed_;key_=key;alpha_=0;return displayed_;}
 if(alpha_>=1){displayed_=std::move(target);return displayed_;}
 const double remaining=1-alpha_;
 // Compare before multiplying so even DBL_MAX seconds cannot overflow.
 if(seconds>=remaining/double(rate))alpha_=1;
 else alpha_+=seconds*double(rate);
 // Remove at most 1e-12 of progress residue from partitioned frame deltas.
 if(1-alpha_<=1e-12)alpha_=1;
 if(alpha_>=1){displayed_=std::move(target);from_={};return displayed_;}
 for(unsigned i=0;i<3;++i)displayed_.root[i]=static_cast<float>((1-alpha_)*from_.root[i]+alpha_*target.root[i]);
 for(auto& [bone,q]:displayed_.rotations)q=slerp(from_.rotations.at(bone),target.rotations.at(bone),alpha_);
 return displayed_;
}
}
