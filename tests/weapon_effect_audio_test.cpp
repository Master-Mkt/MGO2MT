#include "weapon_effect_audio.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstring>
using namespace mgo2mt;namespace fs=std::filesystem;
static unsigned checks=0;
static void check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
static void wave(const fs::path&p){std::vector<uint8_t>b(44+160);auto put=[&](size_t at,uint32_t v){for(int i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));};std::memcpy(b.data(),"RIFF",4);put(4,uint32_t(b.size()-8));std::memcpy(b.data()+8,"WAVEfmt ",8);put(16,16);put(20,0x10001);put(24,8000);put(28,16000);put(32,0x100002);std::memcpy(b.data()+36,"data",4);put(40,160);std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());}
int main(int argc,char**argv){try{
 check(argc==2,"fixture path");fs::path root=argv[1];fs::create_directories(root);wave(root/"custom.wav");{std::ofstream f(root/"combat.txt");f<<"MGO2MT.COMBAT_AUDIO 1 1\n42 custom.wav\n";}
 combat::Effects original;check(original.load_manifest(root/"combat.txt"),"manifest loads");std::string error;
 auto config=std::make_shared<weapon_effect::Config>();check(config->load_text(R"({"format":"MGO2MT.WeaponEffects","version":1,"weapons":[{"id":25,"sounds":{"shot":{"path":"custom.wav","delayMs":50,"gain":0.5},"reload":{"cue":42,"delayMs":80},"click":{"cue":42},"casing":{"cue":42,"delayMs":20},"explosion":{"enabled":false}}}]})",error),"config parses");
 weapon_effect::Audio audio;check(audio.configure(config,root,original,error),"valid sound registers");combat::Snapshot state;state.epoch=1;state.eventWatermark=10;combat::Player p;p.identity={0,1,1};p.life=1;p.alive=true;p.weapon=25;p.aiming=true;state.players[0]=p;audio.synchronize(1,1,0,1000);
 combat::Event shot;shot.epoch=1;shot.id=1;shot.kind=combat::EventKind::shot;shot.source=p.identity;shot.sourceLife=1;shot.weapon=25;shot.cue=42;shot.position={0,100,0};
 std::vector<combat::Sound> played;auto sink=[&](const combat::Sound&s){played.push_back(s);};
 check(audio.dispatch({&shot,1},state,1000).empty()&&audio.pending()==1,"custom suppresses original once");check(audio.dispatch({&shot,1},state,1000).empty()&&audio.pending()==1,"duplicate not scheduled");audio.sample(state,1049,shot.position,original,sink);check(played.empty(),"delay respected");audio.sample(state,1050,shot.position,original,sink);check(played.size()==1&&played[0].gain==.5f,"file gain applied");
 auto impact=shot;impact.id=2;impact.kind=combat::EventKind::damage;check(audio.dispatch({&impact,1},state,1060).size()==1,"body impact keeps original sound");
 auto explosion=shot;explosion.id=3;explosion.kind=combat::EventKind::explosion;check(audio.dispatch({&explosion,1},state,1070).empty()&&!audio.pending(),"disabled explosion silent");
 audio.casing(shot,state,1100);audio.casing(shot,state,1100);check(audio.pending()==1,"one sound per physical casing contact");audio.sample(state,1120,shot.position,original,sink);check(played.size()==2,"casing cue plays");
 audio.click(p,1200);audio.click(p,1250);check(audio.pending()==1,"dry click throttles");audio.sample(state,1250,shot.position,original,sink);check(played.size()==3,"dry click plays");p.ammo=1;audio.click(p,1400);check(!audio.pending(),"loaded gun no dry click");
 state.players[0]->reloadUntil=5000;state.players[0]->reloadElapsedMs=0;audio.reloads(state,1500);audio.reloads(state,1510);check(audio.pending()==1&&audio.replaces_reload(25),"one custom reload start");audio.sample(state,1580,shot.position,original,sink);check(played.size()==4,"reload delay plays");
 state.players[0]->reloadUntil=6000;audio.reloads(state,1600);state.players[0]->reloadUntil=0;audio.sample(state,1680,shot.position,original,sink);check(played.size()==4,"cancelled reload pending discarded");
 shot.id=4;audio.dispatch({&shot,1},state,1700);state.players[0]->life=2;audio.sample(state,1750,shot.position,original,sink);check(played.size()==4&&!audio.pending(),"respawn discards pending old life");state.players[0]->life=1;
 shot.id=5;audio.dispatch({&shot,1},state,1800);audio.synchronize(1,2,10,1810);audio.sample(state,1900,shot.position,original,sink);check(played.size()==4&&!audio.pending(),"scene change discards sounds");check(audio.dispatch({&shot,1},state,1910).empty(),"historical event suppressed");
 audio.synchronize(1,3,0,2000);shot.id=6;audio.dispatch({&shot,1},state,2000);audio.sample(state,2400,shot.position,original,sink);check(played.size()==4,"stall does not burst delayed stale audio");
 auto missing=std::make_shared<weapon_effect::Config>();check(missing->load_text(R"({"format":"MGO2MT.WeaponEffects","version":1,"defaults":{"sounds":{"shot":{"path":"missing.wav"}}}})",error),"missing path schema valid");check(!audio.configure(missing,root,original,error),"missing WAV rejects atomic configuration");shot.id=7;check(audio.dispatch({&shot,1},state,2500).empty()&&audio.pending()==1,"failed config retains previous mapping");
 auto cue=std::make_shared<weapon_effect::Config>();check(cue->load_text(R"({"format":"MGO2MT.WeaponEffects","version":1,"defaults":{"sounds":{"shot":{"cue":9999}}}})",error),"cue schema");check(!audio.configure(cue,root,original,error),"unregistered cue rejects");
 std::ofstream(root/"bad.wav")<<"not wave";check(!weapon_effect::validate_sound_file(root/"bad.wav",error),"malformed PCM rejects");
 auto world=std::make_shared<weapon_effect::Config>();check(world->load_text(R"({"format":"MGO2MT.WeaponEffects","version":1,"defaults":{"sounds":{"explosion":{"cue":42,"delayMs":100}}}})",error)&&audio.configure(world,root,original,error),"world explosion sound");audio.synchronize(1,4,0,3000);explosion.id=8;explosion.sourceLife=1;state.players[0]->life=2;check(audio.dispatch({&explosion,1},state,3000).empty()&&audio.pending()==1,"accepted explosion survives thrower respawn");state.players[0].reset();audio.sample(state,3100,shot.position,original,sink);check(played.size()==5,"world explosion survives thrower departure");explosion.id=9;audio.dispatch({&explosion,1},state,3200);audio.synchronize(1,5,10,3210);audio.sample(state,3300,shot.position,original,sink);check(played.size()==5,"world audio still scoped to scene");
 std::cout<<checks<<" weapon audio timing/lifecycle checks passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
