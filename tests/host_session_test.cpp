#include "host_session.h"
#include <iostream>
using namespace mgo2win::host;
void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
void be(std::vector<uint8_t>&b,size_t at,uint32_t v,unsigned n=4){while(n){b[at+--n]=uint8_t(v);v>>=8;}}
std::vector<uint8_t> generation(){std::vector<uint8_t>b(10);b[0]=11;b[8]=8;b[9]=3;return b;}
int main(){try{
 std::vector<uint8_t>info(579),personal(245),skills(12);be(info,0,123);info[4]='P';info[5]='C';be(info,28,0x12345678);be(info,574,0x87654321);be(personal,0,234);personal[4]='C';personal[20]=1;personal[239]=12;personal[240]=3;be(personal,241,0xa7000d);for(unsigned i=49;i<76;++i)personal[i]=uint8_t(i-49);personal[76]=1;personal[81]=3;be(skills,0,2);skills[4]=1;skills[5]=0x60;skills[8]=25;skills[9]=0x20;
 auto profile=profile_payload(123,info,personal,skills);check(profile[0]==2&&profile[1]==0x78&&profile[2]==0x56&&profile[3]==0x21&&profile[4]==0x43,"actual profile counters endian");check(profile[6]==12&&profile[7]==3&&profile[8]==234&&profile[12]==13&&profile[16]==13&&profile[28]==0&&profile[36]==8&&profile[37]==25&&profile[38]==26,"actual profile appearance/clan/level");check(profile[39]==1&&profile[40]==3&&profile[45]==25&&profile[46]==0&&profile[47]==0x60,"actual equipped skills and registry");check(profile.size()==100,"profile variable size");
 auto state=generation();check(global_generation(state)==3,"explicit global generation field");auto partial=state;partial[8]=0;partial.pop_back();check(!global_generation(partial),"no acceptance without field");
 Hello local{123,0x01234567,2,2,{{{192,0,2,1},5730},{{10,0,0,1},5730}}};Hello peer{456,0x76543210,2,1,{{{192,0,2,2},5730},{{10,0,0,2},5730}}};Keys keys{local.seed^peer.seed,local.seed^peer.seed^initial_mac};
 auto helloPacket=[&](uint16_t seq){Message m;m.payload=encode_hello(peer);return encode({seq,{m}},{});};
 auto app=[&](uint16_t seq,uint8_t serial,std::vector<uint8_t>b){Message m;m.channel=1;m.serial=serial;m.payload=std::move(b);return encode({seq,{m}},keys);};
 Machine machine(local,456,profile,0);check(machine.poll(0).size()==1,"initial handshake sent");machine.receive(encode({0,{{0,true,true,false,0,{}}}}),10);check(machine.result().stage==Stage::connecting,"ACK alone never means connected");machine.receive(helloPacket(1),20);check(machine.result().stage==Stage::profile,"hello identity and both halves");check(machine.poll(20).size()==2,"handshake ACK uses established key and profile sends");
 machine.receive(app(2,1,generation()),30);check(machine.result().stage==Stage::profile,"out of order snapshot held");machine.receive(app(3,0,{7,0,0,0,0,0,3}),40);check(machine.result().stage==Stage::joined&&machine.result().was_joined,"ordered terminator then explicit sync");machine.receive(app(4,0,{7,0,0,0,0,0,3}),50);check(machine.result().stage==Stage::joined,"duplicate application not replayed");machine.cancel();check(machine.result().stage==Stage::cancelled&&machine.leave_packet().has_value(),"cancel retains profile/entry cleanup state");
 Machine timeout(local,456,profile,0);timeout.poll(8000);check(timeout.result().stage==Stage::timeout&&!timeout.result().profile_sent,"handshake deadline");
 Machine mismatch(local,456,profile,0);auto wrong=peer;wrong.character=457;Message bad;bad.payload=encode_hello(wrong);mismatch.receive(encode({0,{bad}}),1);check(mismatch.result().stage==Stage::protocol_error,"wrong PC rejected");
 Machine rejected(local,456,profile,0);Message refusal;refusal.payload={0xc8,1,0,0,0};rejected.receive(encode({0,{refusal}}),1);check(rejected.result().stage==Stage::rejected,"host short rejection");
 Machine silent(local,456,profile,0);silent.receive(encode({0,{{0,true,true,false,0,{}}}}),1);silent.receive(helloPacket(1),2);silent.poll(2);silent.receive(app(2,0,{7,0,0,0,0,0,3}),3);check(silent.result().stage==Stage::synchronizing,"roster alone not admission");silent.poll(8003);check(silent.result().stage==Stage::timeout&&silent.result().profile_sent&&!silent.result().was_joined,"sync timeout cleanup needed");

 auto object=[](uint8_t slot,uint16_t instance,uint32_t id){std::vector<uint8_t>b(26);b[0]=7;b[1]=19;b[4]=uint8_t(instance);b[5]=uint8_t(instance>>8);b[7]=slot;for(unsigned i=0;i<4;++i)b[8+i]=uint8_t(id>>(8*i));b[24]='P';return b;};
 Machine roster(local,456,profile,0);roster.receive(encode({0,{{0,true,true,false,0,{}}}}),1);roster.receive(helloPacket(1),2);
 roster.receive(app(2,0,{7,0,0,2,0,0,3}),3);check(roster.result().stage==Stage::profile,"other object class terminator cannot admit");
 roster.receive(app(3,1,object(3,0x103,123)),4);roster.receive(app(4,2,object(0,0x100,456)),5);roster.receive(app(5,3,{7,0,0,0,0,0,3}),6);roster.receive(app(6,4,generation()),7);
 check(roster.result().stage==Stage::joined&&roster.result().roster.count()==2,"roster propagated through real admission machine");auto revision=roster.result().roster.revision;
 roster.receive(app(7,6,{7,2,0,0,0x22,2,1,0,0}),8);check(roster.result().roster.revision==revision,"out of order removal held");roster.receive(app(8,5,object(8,0x222,777)),9);check(roster.result().roster.count()==2&&roster.result().roster.revision==revision+2,"ordered join and removal within joined stage");
 roster.receive(app(9,7,{7,2,0,0,3,1,2,0,0}),10);check(roster.result().stage==Stage::disconnected&&!roster.result().roster.complete&&roster.result().roster.count()==0,"local removal ends session and clears live data");
 std::cout<<"host profile, admission, ordering, cancellation and failure checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
