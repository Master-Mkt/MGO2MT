#include "stage_chunks.h"
#include "stage_assets.h"
#include <windows.h>
#include <bcrypt.h>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
using namespace mgo2mt;
void check(bool v,const char*m){if(!v)throw std::runtime_error(m);}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(...){rejected=true;}check(rejected,"expected rejection");}
void word(std::vector<char>&b,uint32_t x){for(int i=0;i<4;++i)b.push_back(char(x>>(8*i)));}
std::vector<char> fixture(){std::vector<char>b{'G','W','M','1'};for(auto v:{2,3,3,1,1})word(b,v);
 for(float x:{0,0,0,1,1,0})word(b,std::bit_cast<uint32_t>(x));
 for(auto p:{std::array<float,3>{0,0,0},{1,0,0},{0,1,0}}){for(float x:p)word(b,std::bit_cast<uint32_t>(x));for(float x:{0.f,0.f,1.f,0.f,0.f,.25f,.5f,.75f,.5f})word(b,std::bit_cast<uint32_t>(x));}
 for(auto v:{0u,1u,2u,0u,3u,0u,0u,0x120000u})word(b,v);for(int i=0;i<3;++i)word(b,std::bit_cast<uint32_t>(1.f));for(auto v:{4,4,9,8})word(b,v);b.resize(b.size()+8);return b;
}
std::array<unsigned char,32> hash(const std::vector<char>&b){std::array<unsigned char,32>d{};check(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<char*>(b.data())),ULONG(b.size()),d.data(),32)>=0,"hash");return d;}
std::string hex(const std::vector<char>&b){std::string s;for(auto c:hash(b)){s+="0123456789abcdef"[c>>4];s+="0123456789abcdef"[c&15];}return s;}
void write(const std::filesystem::path&p,const std::vector<char>&b){std::ofstream f(p,std::ios::binary);f.write(b.data(),b.size());}
int main(int argc,char**argv){try{
 if(argc==2){auto m=stage::load_stage_chunks(argv[1]);check(!m->vertices.empty()&&m->parts.size()>1,"original split stage");std::cout<<m->vertices.size()<<" vertices "<<m->parts.size()<<" parts\n";return 0;}
 auto root=std::filesystem::temp_directory_path()/("mgo2mt-chunks-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(root);
 const auto b=fixture();write(root/"n022aa.gwm",b);write(root/"n022ab.gwm",b);auto cfg=root/"n022a.chunks.cfg";
 auto text=[&](std::string tail="",std::string file="n022ab.gwm",std::string digest=""){std::ofstream f(cfg);f<<"MGO2MT.STAGE_CHUNKS 1\nCOUNT 2\nCHUNK n022aa.gwm "<<hex(b)<<" 0 0 0 1 1 0\nCHUNK "<<file<<" "<<(digest.empty()?hex(b):digest)<<" 0 0 0 1 1 0\nEND\n"<<tail;};text();
 // Each sidecar binds the hash and pre-merge indices of its own chunk.
 std::vector<char>gsa{'G','S','A','1'};for(auto x:{1,1,1})word(gsa,x);for(auto x:hash(b))gsa.push_back(char(x));for(auto x:{0u,0u,3u,0u,0x120000u,1u})word(gsa,x);write(root/"n022ab.gsa",gsa);
 std::vector<char>gwn{'G','W','N','1'};for(auto x:{3,1})word(gwn,x);for(auto x:hash(b))gwn.push_back(char(x));word(gwn,0);word(gwn,1023);write(root/"n022ab.gwn",gwn);
 auto m=stage::load_stage_chunks(cfg);check(m->vertices.size()==6&&m->indices[3]==3&&m->parts[1].first==3&&m->parts[1].texture==1,"rebased geometry");check(m->parts[0].surfaceAlpha==0&&m->parts[1].surfaceAlpha==1&&m->vertices[3].nx==1,"chunk-specific sidecars");check(m->vertices[3].aa==.5f&&m->vertices[3].ar==.25f,"authored color preserved");
 // Direct merge also preserves original records and slot indices while rebasing images.
 CharacterModel a(b),c(b);c.parts[0].original.present=true;c.parts[0].original.mdnPath="original/path";c.parts[0].original.raw[9]=73;c.parts[0].original.normalSlot=0;c.parts[0].original.textures.push_back({{},0,"original image"});c.parts[0].floorNormal=0;c.parts[0].floorBlend=0;c.parts[0].surfaceAlpha=1;stage::append_stage_chunk(a,std::move(c));auto&p=a.parts[1];check(p.original.raw[9]==73&&p.original.mdnPath=="original/path"&&p.original.normalSlot==0&&p.original.textures[0].image==1&&p.floorNormal==1&&p.floorBlend==1&&p.surfaceAlpha==1,"all original material metadata preserved");
 text("junk");rejects([&]{stage::load_stage_chunks(cfg);});text("","../n022ab.gwm");rejects([&]{stage::load_stage_chunks(cfg);});text("","n022aa.gwm");rejects([&]{stage::load_stage_chunks(cfg);});text("","n022ab.gwm",std::string(64,'0'));rejects([&]{stage::load_stage_chunks(cfg);});text();
 stage::ChunkLimits limits;limits.totalBytes=b.size()*2;rejects([&]{stage::load_stage_chunks(cfg,{},limits);});limits={};limits.count=1;rejects([&]{stage::load_stage_chunks(cfg,{},limits);});limits={};limits.decodedBytes=1;rejects([&]{stage::load_stage_chunks(cfg,{},limits);});
 std::stop_source cancel;cancel.request_stop();rejects([&]{stage::load_stage_chunks(cfg,cancel.get_token());});
 gsa.back()^=1;write(root/"n022ab.gsa",gsa);rejects([&]{stage::load_stage_chunks(cfg);});gsa.back()^=1;write(root/"n022ab.gsa",gsa);
 {stage::Assets assets(root);assets.select(host::LoadRequest{1,1,0,0,{20,1,2},host::MatchTransition::initial});auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(assets.result().status==stage::Status::loading&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));check(assets.result().status==stage::Status::preview_ready&&assets.result().model&&assets.result().model->parts.size()==2,"Assets consumes split stage");}
 write(root/"n022a.gwm",b);{stage::Assets assets(root);assets.select(host::LoadRequest{1,1,0,0,{20,1,2},host::MatchTransition::initial});while(assets.result().status==stage::Status::loading)std::this_thread::sleep_for(std::chrono::milliseconds(1));check(assets.result().model&&assets.result().model->vertices.size()==3,"legacy single file priority");}
 std::filesystem::remove_all(root);std::cout<<"stage chunks passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
