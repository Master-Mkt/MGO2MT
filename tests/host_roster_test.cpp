#include "host_roster.h"
#include <iostream>
using namespace mgo2win::host;
void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
std::vector<uint8_t> player(uint8_t slot,uint16_t instance,uint32_t id,std::string name="Player",std::string clan=""){
 std::vector<uint8_t>b(24);b[0]=7;b[4]=uint8_t(instance);b[5]=uint8_t(instance>>8);b[7]=slot;for(unsigned i=0;i<4;++i)b[8+i]=uint8_t(id>>(8*i));b.insert(b.end(),name.begin(),name.end());b.push_back(0);b.insert(b.end(),clan.begin(),clan.end());b[1]=uint8_t(b.size()-7);return b;
}
std::vector<uint8_t> remove(uint16_t instance,uint8_t flag=1){return {7,2,0,0,uint8_t(instance),uint8_t(instance>>8),flag,0,0};}
int main(){try{
 Roster r;auto invalid=[&](auto b){auto before=r;bool failed=false;try{update_roster(r,b);}catch(const Invalid&){failed=true;}check(failed&&r==before,"malformed records are atomic");};
 update_roster(r,player(0,0x100,77,"Host","Clan"));update_roster(r,player(23,0xabba,123,"\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"));check(r.count()==2&&!r.complete&&r.slots[23]->instance==0xabba,"slot and instance are independent; UTF8 preserved");
 std::vector<uint8_t>end{7,0,0,0,0,0,3};update_roster(r,end);check(r.complete&&r.revision==3,"initial roster complete");auto rev=r.revision;update_roster(r,end);update_roster(r,player(0,0x100,77,"Host","Clan"));check(r.revision==rev,"unchanged replay not published");
 update_roster(r,remove(0x100,2));check(!r.slots[0]&&r.count()==1,"both remove flags resolve instance");update_roster(r,player(0,0x200,999,"New player"));rev=r.revision;update_roster(r,remove(0x100));check(r.count()==2&&r.revision==rev,"stale removal cannot delete slot reuse");
 invalid(player(24,3,3));invalid(player(1,3,0));invalid(player(1,0x200,5));invalid(player(1,2,999));invalid(player(1,2,5,""));invalid(player(1,2,5,std::string(24,'x')));invalid(player(1,2,5,"\xc0\xaf"));invalid(player(1,2,5,"Bad\nName"));invalid(player(1,2,5,"Name",std::string(24,'x')));
 auto b=player(1,2,5);b[18]=3;invalid(b);b=player(1,2,5);b.pop_back();b[1]--;invalid(b);b=player(1,2,5);b[1]++;invalid(b);
 b=player(1,2,5);for(size_t n=0;n<b.size();++n){auto trunc=b;trunc.resize(n);invalid(trunc);}
 auto other=player(3,4,6);other[3]=2;rev=r.revision;check(!update_roster(r,other)&&r.revision==rev,"non-player class ignored");
 for(unsigned i=1;i<23;++i)update_roster(r,player(uint8_t(i),uint16_t(i),2000+i));check(r.count()==24,"24 occupied slots bounded");
 std::cout<<"host roster: initial/update/remove/reused slots/UTF8/malformed/24 slots passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
