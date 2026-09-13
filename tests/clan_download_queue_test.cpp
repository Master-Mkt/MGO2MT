#include "clan_download_queue.h"
#include <algorithm>
#include <iostream>
#include <string_view>
using namespace mgo2win::clan;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void put(std::vector<uint8_t>&p,size_t at,uint32_t v,size_t n=4){for(size_t i=0;i<n;++i){p[at+n-i-1]=uint8_t(v);v>>=8;}}
static uint32_t get(std::span<const uint8_t>p,size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v=(v<<8)|p[at+i];return v;}
static std::vector<uint8_t> pixels(){std::vector<uint8_t>p(16384);for(size_t i=0;i<p.size();++i)p[i]=uint8_t(i*37+11);return p;}
static std::vector<uint8_t> reply(uint32_t id,uint32_t offset=0,bool empty=false){
 auto data=pixels();uint32_t count=empty?0:std::min(896u,16384-offset);std::vector<uint8_t> p(64+count);
 put(p,4,0x454d3634);put(p,8,id);put(p,12,1,2);put(p,14,64,2);put(p,16,64,2);put(p,18,1,2);
 put(p,20,empty?0:16384);put(p,24,empty?0:offset);put(p,28,count,2);
 // Independent byte fixture SHA, shared with clan_hud_test; no production digest used.
 std::string_view hash=empty?"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855":"ce61cc72a84bd3526a8f955235c5f4f67439820d6ffb910b5ab07e54741262b8";
 auto nibble=[](char c){return unsigned(c<='9'?c-'0':c-'a'+10);};
 for(size_t i=0;i<32;++i)p[32+i]=uint8_t(nibble(hash[2*i])*16+nibble(hash[2*i+1]));
 if(count)std::copy_n(data.begin()+offset,count,p.begin()+64);return p;
}
static void finish(DownloadQueue&q,uint32_t id,uint64_t&now){
 for(uint32_t off=0;off<16384;off+=896){auto p=q.take(now++);check(p&&get(*p,0)==id&&get(*p,8)==off,"19 chunks preserve identity and offset");check(!q.take(now),"only one pending wire request");q.receive(reply(id,off));}
}
int main(){try{
 auto own=std::make_shared<Cache>(),enemy=std::make_shared<Cache>();uint64_t now=0;DownloadQueue q(own,enemy);
 own->want(7);enemy->want(8);finish(q,7,now);check(own->state().image.has_value()&&!enemy->state().image,"own completes first");
 auto ownImage=own->state().image;auto p=q.take(now++);check(p&&get(*p,0)==8,"enemy follows own");
 enemy->want(9);check(!q.take(now),"changing identity must wait for old reply");q.receive(reply(8));
 check(!enemy->state().image&&enemy->wanted()==9,"partial old reply never published as new identity");
 finish(q,9,now);check(enemy->state().image==ownImage,"verified enemy image published");
 enemy->want(0);q.take(now++);enemy->want(9);check(!q.take(now++)&&enemy->state().image==ownImage,"A-zero-A restores memo without wire");
 auto serial=enemy->state().serial;for(int i=0;i<10;++i)check(!q.take(now++),"done is idle");check(enemy->state().serial==serial,"memo does not republish every pump");
 enemy->want(7);check(!q.take(now++)&&enemy->state().image==ownImage,"own memo shared with target");
 enemy->want(10);p=q.take(now++);check(p&&get(*p,0)==10,"target request");q.receive(reply(10,0,true));
 enemy->want(0);q.take(now++);enemy->want(10);check(!q.take(now++)&&!enemy->state().image,"absent image memo prevents retries");
 // Existing own HUD survives transport timeout; late reply cannot cross to new target.
 enemy->want(11);check(q.take(10000).has_value(),"timeout fixture request");check(!q.take(12999)&&!q.disabled(),"before deadline");q.tick(13000);
 check(q.disabled()&&!q.take(13001),"deadline disables entire stream");q.receive(reply(11,0,true));enemy->want(12);
 check(!q.take(14000)&&own->state().image==ownImage&&!enemy->state().image,"late timeout response ignored and own image retained");
 // Own request can preempt enemy between chunks, never during an in-flight reply.
 own=std::make_shared<Cache>();enemy=std::make_shared<Cache>();DownloadQueue priority(own,enemy);enemy->want(21);
 p=priority.take(0);own->want(22);check(!priority.take(1),"own cannot steal pending enemy reply");priority.receive(reply(21));
 p=priority.take(2);check(p&&get(*p,0)==22,"own gets next slot");priority.receive(reply(22,0,true));
 p=priority.take(3);check(p&&get(*p,0)==21&&get(*p,8)==896,"enemy partial transfer resumes");priority.receive(reply(21,896));
 // Same desired clan uses exactly one complete transfer for both caches.
 own=std::make_shared<Cache>();enemy=std::make_shared<Cache>();DownloadQueue same(own,enemy);own->want(31);enemy->want(31);now=0;
 finish(same,31,now);check(own->state().image&&own->state().image==enemy->state().image&&!same.take(now++),"same clan downloaded once");
 // Malformed/wrong identity response stops both channels without erasing own HUD.
 enemy->want(32);same.take(now++);same.receive(reply(33));check(same.disabled()&&own->state().image&&!enemy->state().image,"wrong identity cannot publish");
 // Bound memo storage; a retained result avoids requests and an evicted result restarts at zero.
 own=std::make_shared<Cache>();enemy=std::make_shared<Cache>();DownloadQueue bounded(own,enemy);now=0;
 for(uint32_t id=100;id<125;++id){enemy->want(id);p=bounded.take(now++);check(p&&get(*p,0)==id,"new memo clan requested");bounded.receive(reply(id,0,true));}
 enemy->want(101);check(!bounded.take(now++),"24th retained memo");enemy->want(100);p=bounded.take(now++);check(p&&get(*p,0)==100&&get(*p,8)==0,"25th entry evicts oldest");bounded.receive(reply(100,0,true));
 enemy->want(0x80000000u);check(!bounded.take(now++)&&!bounded.disabled(),"invalid clan is not sent or session-fatal");
 enemy->want(0);check(!bounded.take(now++),"zero clan does not send");
 std::cout<<"clan queue priority/19 chunks/pending identity/memo/timeout tests passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
