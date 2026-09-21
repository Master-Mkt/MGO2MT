#include "bullet_decals.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt::combat::decals;
namespace {
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Impact hit(uint64_t id,Surface surface=Surface::static_solid){return {{5,7},id,{100,200,300},{0,0,2},18,surface};}
}
int main(){try{
 const Policy native{2,1000,200,25,1};Pool pool(native);
 check(!pool.emit(hit(1),0),"no pre-baseline mark");pool.synchronize({5,7},4,10);check(!pool.emit(hit(4),10),"late join watermark does not replay old impact");pool.synchronize({5,7},20,10);
 check(pool.emit(hit(5),10)&&!pool.emit(hit(5),11),"ordered event once");auto visible=pool.sample(11);check(visible.size()==1&&visible[0].position==Vec3{100,200,301}&&visible[0].normal==Vec3{0,0,1},"normalize and offset on surface");check(std::abs(dot(visible[0].tangent,visible[0].normal))<1e-5f&&std::abs(dot(visible[0].tangent,visible[0].bitangent))<1e-5f&&std::abs(dot(visible[0].tangent,visible[0].tangent)-1)<1e-5f,"stable orthonormal decal plane");check(visible[0].material==18&&visible[0].alpha==1&&visible[0].radius==25,"material retained, native size explicit");
 uint64_t sequence=6;for(auto surface:{Surface::water,Surface::sky,Surface::body,Surface::dynamic,Surface::unknown}){const auto id=sequence++;check(!pool.emit(hit(id,surface),20)&&!pool.emit(hit(id),20),"excluded surface cannot be reinterpreted/replayed");}
 auto invalid=hit(sequence++);invalid.normal={};check(!pool.emit(invalid,20),"zero normal reject");invalid=hit(sequence++);invalid.position[0]=std::numeric_limits<float>::quiet_NaN();check(!pool.emit(invalid,20),"NaN position reject");invalid=hit(sequence++);invalid.normal[1]=std::numeric_limits<float>::infinity();check(!pool.emit(invalid,20),"infinite normal reject");invalid=hit(sequence++);invalid.scope.scene=8;check(!pool.emit(invalid,20),"wrong scene rejects");check(pool.emit(hit(sequence++),100),"later admitted event");check(pool.emit(hit(sequence++),200)&&pool.size()==2,"oldest evicted at runtime capacity");visible=pool.sample(950);check(visible.size()==2&&std::abs(visible[0].alpha-.75f)<1e-5f&&visible[1].alpha==1,"fade only during configured tail");check(pool.sample(1100).size()==1&&pool.sample(1200).empty(),"lifetime exact boundary");
 pool.synchronize({5,7},100,1300);check(pool.emit(hit(99),1300),"post-expiry fresh event");const auto before=pool.policy();auto bad=native;bad.capacity=Pool::maximumCapacity+1;check(!pool.configure(bad)&&pool.policy().capacity==before.capacity&&pool.size()==1,"invalid configuration atomic");bad=native;bad.fadeMs=1001;check(!Pool::valid(bad),"fade exceeds life");bad=native;bad.radius=std::numeric_limits<float>::quiet_NaN();check(!Pool::valid(bad),"NaN radius");bool threw=false;try{Pool rejected(bad);}catch(const std::invalid_argument&){threw=true;}check(threw,"invalid constructor explicit failure");
 check(pool.sample(1299).empty()&&!pool.emit(hit(99),1299),"clock rollback clears effects but preserves replay floor");pool.synchronize({6,7},100,1400);auto changed=hit(101);check(!pool.emit(changed,1400),"old epoch rejected");changed.scope={6,7};pool.synchronize({6,7},101,1400);changed.normal={0,1,0};check(pool.emit(changed,1400),"floor-normal stable basis");pool.synchronize({6,8},101,1401);check(pool.size()==0,"stage replacement clears marks");pool.synchronize({},0,1402);check(pool.size()==0&&!pool.emit(changed,1402),"disconnect clears and disarms");
 Pool larger({4096,60000,1000,25,1});larger.synchronize({5,7},0,0);larger.synchronize({5,7},4097,0);for(uint64_t id=1;id<=4097;++id)check(larger.emit(hit(id),id),"expanded pool accepts impacts");check(larger.size()==4096&&larger.sample(4097).front().eventId==2,"capacity expanded beyond item limit with deterministic eviction");check(larger.configure({0,60000,1000,25,1})&&larger.size()==0,"zero capacity clears/disables");larger.synchronize({5,7},4098,4098);check(!larger.emit(hit(4098),4098),"disabled pool consumes no storage");
 std::cout<<"bullet_decals_test PASS: static surface, epoch/scene/watermark, finite basis, duplicate rejection, expiry/fade, capacity and clock rollback\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
