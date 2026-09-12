#include "host_placements.h"
#include "stage_lighting.h"
#include <iostream>
using namespace mgo2win::host;
static void check(bool ok,const char*s){if(!ok)throw std::runtime_error(s);}
static void feed(PlacementReceiver&r,uint8_t generation,uint16_t id,uint8_t type=113){
 std::vector<uint8_t>a{0,0x64,uint8_t(id),uint8_t(id>>8),24,0};
 r.receive(generation,a);r.receive(generation,std::array<uint8_t,6>{1,type,0,1,0,0});
 r.receive(generation,std::array<uint8_t,10>{2,0x2e,0xfb,0x41,1,0x29,9,0,64,0});
 r.receive(generation,std::array<uint8_t,9>{3,1,0,2,3,4,5,6,7});
}
int main(){try{
 PlacementReceiver a,b;a.begin(7);b.begin(7);feed(a,7,0x102);feed(b,7,0x102);
 a.receive(7,std::array<uint8_t,6>{0,0,0,0,0,0});a.receive(7,std::array<uint8_t,6>{1,0,0,0,0,0});
 check(a.result()==b.result(),"two independent receivers reproduce identical host layout without RNG");
 auto&item=a.result().items.at(0x102);check(item.position()==std::array<float,3>{-12340,321,23450}&&item.degrees()[1]==90,"little endian signed coordinates and recovered axis scales");
 auto revision=a.result().revision;feed(a,7,0x102);check(a.result().revision==revision&&a.result().items.size()==1,"periodic replay does not duplicate instances");
 a.receive(7,std::array<uint8_t,6>{0,0x64,3,1,24,0});a.receive(7,std::array<uint8_t,6>{1,140,0,1,0,0});
 check(a.result().partial&&a.result().items.size()==1,"partial record never creates an item");
 bool failed=false;try{a.receive(7,std::array<uint8_t,9>{3});}catch(const Invalid&){failed=true;}check(failed&&!a.result().partial&&a.result().items.size()==1,"missing position rejected without mutating published layout");
 a.receive(7,std::array<uint8_t,6>{0,0xa2,2,1,24,0});a.receive(7,std::array<uint8_t,6>{1,113,0,0,0,0});check(a.result().items.empty(),"item removal");
 feed(a,7,0x102);a.begin(8);feed(a,7,0x102);check(a.result().items.empty()&&!a.result().partial,"old generation cannot restore removed objects");feed(a,8,0x102,140);check(a.result().items.at(0x102).type==140,"new generation can reuse instance ID");
 for(uint16_t id=1;id<12;++id)feed(a,8,id);failed=false;try{feed(a,8,999);}catch(const Invalid&){failed=true;}check(failed&&a.result().items.size()==12,"retail 12-item bound");
 ObjectStates objects(3,{1,3,5,1});check(!objects.complete()&&objects.snapshot_request()==std::optional{std::array<uint8_t,2>{0xe1,3}},"object snapshot initially pending with original request");
 // LSB packed widths: 1,5,19,0. This crosses the byte boundary.
 check(!objects.receive(std::array<uint8_t,4>{0xe0,4,0x3b,1}),"other player's snapshot ignored");
 failed=false;try{objects.receive(std::array<uint8_t,3>{0xe0,3,0x3b});}catch(const Invalid&){failed=true;}check(failed&&!objects.complete()&&objects.values()==std::vector<uint8_t>(4),"truncated snapshot cannot mark completion");
 check(objects.receive(std::array<uint8_t,4>{0xe0,3,0x3b,1})&&objects.complete()&&objects.values()==std::vector<uint8_t>({1,5,19,0}),"complete addressed snapshot restores all registered widths atomically");
 check(!objects.snapshot_request(),"complete snapshot stops requests");
 check(objects.initial_values()==objects.values(),"initial callback preserves both current and quiet restoration baseline");
 check(objects.receive(std::array<uint8_t,1>{3})&&objects.values()[3]==1,"single-bit destructible delta");
 check(!objects.receive(std::array<uint8_t,4>{0xe0,3,0,0})&&objects.values()[3]==1,"duplicate initial snapshot cannot resurrect destroyed actor");
 check(objects.initial_values()[3]==0,"later break remains distinguishable from initial destroyed state");
 // Reviewed factory callbacks: bottles use an 8-bit OR mask; CBOX uses a
 // 2-bit maximum. OR-ing CBOX values 1 and 2 would incorrectly produce 3.
 ObjectStates first(2,{1,8,2},{ObjectStates::Update::bits,ObjectStates::Update::bits,ObjectStates::Update::maximum});
 ObjectStates late(2,{1,8,2},{ObjectStates::Update::bits,ObjectStates::Update::bits,ObjectStates::Update::maximum});
 first.receive(std::array<uint8_t,4>{0xe0,2,0,0});
 first.receive(std::array<uint8_t,1>{0});first.receive(std::array<uint8_t,2>{1,1});first.receive(std::array<uint8_t,2>{1,128});
 first.receive(std::array<uint8_t,2>{2,1});first.receive(std::array<uint8_t,2>{2,2});first.receive(std::array<uint8_t,2>{2,1});
 late.receive(std::array<uint8_t,4>{0xe0,2,3,5});
 check(first.values()==std::vector<uint8_t>({1,129,2})&&late.values()==first.values(),"late join and live updates reproduce identical heterogeneous object states");
 check(first.initial_values()==std::vector<uint8_t>({0,0,0})&&late.initial_values()==late.values(),"late-join baseline distinguishes restoration from new destruction");
 mgo2win::stage::Lighting lights;mgo2win::stage::PointLight p;p.position={0,2,0};p.color={1,1,1};p.range=p.extendedRange=4;p.flags=0x100;p.identity={0x101,7,123,{-10,-10,-10},{10,10,10}};lights.points.push_back(p);
 check(lights.sample({0,0,0},{0,1,0}).color[0]==.5f,"fixture is visibly illuminated");
 if(objects.values()[0])lights.enable(123,~0u,false);
 check(lights.sample({0,0,0},{0,1,0}).color[0]==0,"late-join broken state disables its light");
 check(lights.enable(123,7,true)==1&&lights.enable(123,7,true)==0,"restore and repeated light updates");
 check(lights.enable_sphere({0,0,0},2,false)==0,"strict destruction radius boundary");check(lights.enable_sphere({0,0,0},2.1f,false)==1,"sphere lights disabled");
 std::cout<<"placement assembly, signed coordinates, replay, generation, snapshot completion and destructible lights passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
