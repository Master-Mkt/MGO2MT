#include "native_name_directory.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2mt::names;
void check(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> response(uint64_t nonce,std::vector<Record> records){
 std::vector<uint8_t> out{'G','W','N','M',1,0,1,1};detail::put(out,nonce,8);detail::put(out,records.size(),2);out.push_back(16);out.push_back(64);detail::put(out,1,4);
 for(auto&r:records){detail::put(out,r.id,4);out.push_back(r.main?1:0);out.push_back(0);detail::put(out,r.name.size(),2);out.insert(out.end(),r.name.begin(),r.name.end());}return out;
}
int main(){try{
 Directory d;check(!d.take(1,0),"no request before authenticated connection");d.connect(1);
 std::vector<uint32_t> ids;for(uint32_t id=1;id<=24;++id)ids.push_back(id);d.want(ids);
 auto request=d.take(10,0);check(request&&request->size()==52&&(*request)[17]==8,"maximum eight IDs, payload below 1023");
 check(!d.take(11,1),"single flight");check(!d.receive(response(11,{{1,"WRONG",false}}),1),"nonce mismatch");
 check(!d.receive(response(10,{{9,"OUTSIDE",false}}),1),"unsolicited valid character refused atomically");
 auto bytes=response(10,{{1,"日本語名前表示試験壱弐参四伍六七",true},{2,"同一短縮名の本人その一",false},{3,"同一短縮名の本人その二",false}});
 check(d.receive(bytes,2)&&d.display(1,"old")=="日本語名前表示試験壱弐参四伍六七","full Japanese display retained by ID");
 check(d.display(2,"same")!=d.display(3,"same"),"legacy alias collision never merges IDs");
 check(!d.receive(bytes,3),"completed reply cannot replay");
 request=d.take(11,3);check(request&&detail::read(*request,20,4)==9,"missing records are attempted once; next batch starts ninth");
 check(d.receive(response(11,{}),4),"empty success settles unavailable names");
 request=d.take(12,5);check(request&&detail::read(*request,20,4)==17,"third batch serves last eight");
 check(d.receive(response(12,{{24,"最後の相手",false}}),6)&&!d.needs_request(7),"bounded completed batches");
 const std::vector<uint32_t> changed{1,25};d.want(changed);check(d.display(24,"legacy")=="legacy","departed peer cache removed");
 check(d.take(13,8).has_value(),"new peer queried");d.connect(2);check(!d.receive(response(13,{{25,"old room",false}}),9)&&d.display(1,"fallback")=="fallback","connection reset drops names and pending nonce");
 check(d.take(14,10).has_value()&&!d.receive(response(14,{{2,"TOO LATE",false}}),3010)&&!d.needs_request(3011),"deadline equality stops unsupported-session retries");
 d.connect(3);check(d.needs_request(4000),"fresh connection can negotiate again");
 std::cout<<"Native name batches, nonce, missing IDs, identity collisions, lifecycle and legacy fallback PASS\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
