#include "character_client.h"
#include "character_screen.h"
#include "gameplay_fingerprint.h"
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
using namespace mgo2mt;
void require(bool b){if(!b)throw std::runtime_error("character contract failed");}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(...){rejected=true;}require(rejected);}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v,unsigned n=4){while(n){b.at(at+--n)=uint8_t(v);v>>=8;}}
void name(std::vector<uint8_t>&b,size_t at,const std::string&s){std::copy(s.begin(),s.end(),b.begin()+at);}
CharacterList fixture(unsigned n){std::vector<uint8_t>b(471);b[4]=8;b[5]=uint8_t(n);size_t at=7;for(unsigned i=0;i<n;++i){if(i)put(b,at,i);at+=i?4:17;put(b,at,100+i);name(b,at+4,i?"Character":"*Main");at+=48;}return parse_characters(b);}
std::vector<uint8_t> read(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);require(bool(f));return {std::istreambuf_iterator<char>(f),{}};}
void frames(CharacterScreen&s,unsigned ms){auto until=GetTickCount64()+ms;do{s.draw();Sleep(5);}while(GetTickCount64()<until);}
std::string report(CharacterScreen&s){std::ostringstream out;auto old=std::cout.rdbuf(out.rdbuf());s.report();std::cout.rdbuf(old);return out.str();}
void frozen_configuration(){
 const auto root=std::filesystem::temp_directory_path()/("mgo2mt-client-config-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
 require(std::filesystem::create_directory(root));const auto gameplayFile=root/"gameplay.json",mounted=root/"mounted_weapons.json";
 auto write=[](const auto& path,const char* value){std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<value;require(bool(out));};
 require(client_gameplay_configuration(root)==std::optional<uint64_t>(0));
 write(gameplayFile,"{\"loaded\":1}");auto initial=gameplay::fingerprint(root);require(initial&&*initial&&!client_gameplay_configuration(root));
 require(!freeze_client_gameplay_configuration(root,*initial+1));require(freeze_client_gameplay_configuration(root,*initial));require(freeze_client_gameplay_configuration(root/".",*initial));require(client_gameplay_configuration(root)==initial);
 write(gameplayFile,"{\"loaded\":2}");auto changed=gameplay::fingerprint(root);require(changed&&changed!=initial&&!client_gameplay_configuration(root));require(!freeze_client_gameplay_configuration(root,*changed));
 write(gameplayFile,"{\"loaded\":1}");require(client_gameplay_configuration(root)==initial);write(mounted,"{}");require(!client_gameplay_configuration(root));
 require(std::filesystem::remove(mounted));require(client_gameplay_configuration(root)==initial);
 const auto shapeDirectory=root/"character",shape=shapeDirectory/"hit_geometry.gwhit";require(std::filesystem::create_directory(shapeDirectory));write(shape,"synthetic-shape-a");
 const auto shapeFingerprint=gameplay::fingerprint(root);require(shapeFingerprint&&shapeFingerprint!=initial&&!client_gameplay_configuration(root));
 write(shape,"synthetic-shape-b");require(gameplay::fingerprint(root)!=shapeFingerprint);require(std::filesystem::remove(shape));require(std::filesystem::remove(shapeDirectory));require(client_gameplay_configuration(root)==initial);
 require(std::filesystem::remove(gameplayFile));require(!client_gameplay_configuration(root));require(std::filesystem::remove(root));
}
int main(int argc,char**argv){
 frozen_configuration();
 NetworkKeys k;for(size_t i=0;i<1042;++i){k.packet[i]=uint32_t(i*0x1234567u);k.auth[i]=~k.packet[i];}k.hmac.fill(0x3a);k.wire={1,3,5,7};k.salt.fill(42);
 if(argc>1){k=NetworkKeys::load(argv[1]);auto oracle=read(std::filesystem::path(argv[1]).parent_path()/"crypto_vectors.bin");require(oracle.size()==32*24);for(size_t i=0;i<oracle.size();i+=24){std::vector<uint8_t> b(oracle.begin()+i,oracle.begin()+i+8);network_block(b,k.auth,false);require(std::equal(b.begin(),b.end(),oracle.begin()+i+8));network_block(b,k.auth,true);require(std::equal(b.begin(),b.end(),oracle.begin()+i));network_block(b,k.packet,true);require(std::equal(b.begin(),b.end(),oracle.begin()+i+16));network_block(b,k.packet,false);require(std::equal(b.begin(),b.end(),oracle.begin()+i));}require(encode_lobby(k,0x2005,1,{})==read(std::filesystem::path(argv[1]).parent_path()/"gate_request.bin"));}
 AuthReply a;a.status=AuthStatus::success;a.user=123;a.session={0x12,0xab,0x09,0xfe,0,0,0,0};auto encrypted=session_payload(k,a);require(encrypted.size()==24);network_block(encrypted,k.packet,false);require(encrypted[3]==123);std::vector<uint8_t>s(encrypted.begin()+4,encrypted.begin()+12);for(unsigned i=0;i<8;++i)s[i]^=k.salt[i];network_block(s,k.auth,true);require(std::string(s.begin(),s.end())=="12ab09fe");a.session[7]=1;rejects([&]{session_payload(k,a);});
 for(size_t n:{0u,4u,471u,1023u}){std::vector<uint8_t>p(n,17);auto wire=encode_lobby(k,0x3049,7,p);auto q=decode_lobby(k,wire,7);require(q.payload==p&&q.command==0x3049);rejects([&]{decode_lobby(k,wire,8);});wire[9]^=1;rejects([&]{decode_lobby(k,wire,7);});wire.pop_back();rejects([&]{decode_lobby(k,wire,7);});}
 rejects([&]{encode_lobby(k,0,1,{});});rejects([&]{encode_lobby(k,1,1,std::vector<uint8_t>(1024));});rejects([&]{NetworkKeys::load("missing-test-network.gnk");});
 std::vector<uint8_t>gate(46);put(gate,4,1);name(gate,24,"49.212.132.180");put(gate,39,5732,2);require(account_endpoint(gate)==5732);auto duplicate=gate;duplicate.insert(duplicate.end(),gate.begin(),gate.end());rejects([&]{account_endpoint(duplicate);});gate[24]='5';rejects([&]{account_endpoint(gate);});gate[24]='4';put(gate,39,443,2);rejects([&]{account_endpoint(gate);});gate.pop_back();rejects([&]{account_endpoint(gate);});
 require(fixture(0).entries.empty());auto list=fixture(8);require(list.entries.size()==8&&list.entries[0].main&&list.entries[0].name==L"Main"&&list.entries[7].id==107);
 std::vector<uint8_t>b(471);b[5]=1;put(b,24,1);name(b,28,"\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");require(parse_characters(b).entries[0].name==L"テスト");b[28]=0xff;rejects([&]{parse_characters(b);});b[28]=10;rejects([&]{parse_characters(b);});b[5]=9;rejects([&]{parse_characters(b);});b.resize(470);rejects([&]{parse_characters(b);});
 b.assign(471,0);b[5]=2;put(b,24,7);name(b,28,"One");put(b,72,1);put(b,76,7);name(b,80,"Two");rejects([&]{parse_characters(b);});put(b,76,8);require(parse_characters(b).entries.size()==2);put(b,72,2);rejects([&]{parse_characters(b);});
 std::atomic_int calls=0;
 {CharacterScreen ui([&](const std::atomic_bool&cancel){++calls;while(!cancel)Sleep(2);CharacterReply r;r.status=CharacterStatus::cancelled;return r;});frames(ui,30);for(int i=0;i<10;++i)ui.message(nullptr,WM_KEYDOWN,VK_RETURN,0);require(calls==1);auto started=GetTickCount64();ui.message(nullptr,WM_KEYDOWN,VK_ESCAPE,0);require(ui.back()&&GetTickCount64()-started<250);}
 {CharacterScreen ui([&](const std::atomic_bool&){CharacterReply r;r.status=CharacterStatus::success;r.stage=CharacterStage::list;r.list=list;return r;});frames(ui,50);ui.message(nullptr,WM_KEYDOWN,VK_DOWN,0);ui.message(nullptr,WM_KEYDOWN,VK_RETURN,0);require(!ui.back());auto text=report(ui);require(text.find("\"count\":8")!=std::string::npos&&text.find("Main")==std::string::npos&&text.find("\"selection_sent\":false")!=std::string::npos);ui.message(nullptr,WM_KEYDOWN,VK_END,0);ui.message(nullptr,WM_KEYDOWN,VK_RETURN,0);require(ui.back());}
 {CharacterScreen ui([](const std::atomic_bool&){CharacterReply r;r.status=CharacterStatus::server_error;r.stage=CharacterStage::session;r.error=0xc0ffee01;return r;});frames(ui,50);require(report(ui).find("\"status\":3")!=std::string::npos);ui.message(nullptr,WM_KEYDOWN,VK_ESCAPE,0);require(ui.back());}
 std::atomic_bool cancelled=true;require(probe_character_gate("unused",cancelled).status==CharacterStatus::cancelled);
 std::cout<<"Character codec, candidate crypto oracle, session inverse, endpoint pinning, 0/8 records, malformed data, cancellation and offline UI passed. No external authentication.\n";
}
