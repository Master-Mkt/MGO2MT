#include "first_person_transition.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){try{
 const WorldView third{{280,1550,-2600},{0,0,1},16.f/9.f},first{{0,1550,0},{0,0,1},16.f/9.f};
 for(unsigned step:{1,5,15,30,60}){
  FirstPersonTransition t;t.update(third,false,0);auto start=t.update(first,true,100);
  check(start.eye==third.eye&&t.body_opacity()==1,"entry starts at visible third-person camera without teleport");
  float previous=start.eye[2];for(unsigned ms=step;ms<180;ms+=step){auto view=t.update(first,true,100+ms);check(view.eye[2]>=previous&&view.eye[2]<=0,"camera moves monotonically to first person");previous=view.eye[2];if(ms>=70)check(t.body_opacity()==0,"non-arm body disappears before camera reaches head");}
  check(t.update(first,true,280).eye==first.eye&&t.body_opacity()==0&&t.split_body(),"arrival keeps only arms at any frame rate");
  t.update(third,false,300);check(t.body_opacity()==0,"exit does not reveal torso inside camera");t.update(third,false,390);check(t.body_opacity()==0,"exit waits until camera has moved away");
  check(t.update(third,false,480).eye==third.eye&&t.body_opacity()==1&&!t.split_body(),"return restores third person");
 }
 FirstPersonTransition t;t.update(third,false,0);t.update(first,true,10);auto mid=t.update(first,true,45);auto opacity=t.body_opacity();check(opacity>0&&opacity<1,"body fades instead of snapping");
 check(t.update(third,false,45).eye==mid.eye&&t.body_opacity()==opacity,"rapid reversal preserves actual camera and alpha");
 auto moving=first;moving.eye[0]+=100;t.update(moving,true,60);auto moved=t.update(moving,true,240);check(moved.eye==moving.eye,"moving aim target is followed");
 t.reset();check(t.update(third,false,1).eye==third.eye&&t.body_opacity()==1,"new life clears old first person state");
 t.update(first,true,5);check(t.update(third,false,0).eye==third.eye,"clock reset clears old transition");
 std::cout<<"First person travel, early fade, reversal, frame rates, moving target and lifecycle PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
