#include "combat_audio.h"
#include <fstream>
#include <iostream>
#include <chrono>
using namespace mgo2mt::combat;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(){auto root=std::filesystem::temp_directory_path()/("mgo2mt-combat-audio-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));try{
 std::filesystem::create_directory(root);
 // Short synthetic PCM fixture tests dispatch; no game or UI audio substitute.
 std::vector<unsigned char>b;auto text=[&](const char*s){for(unsigned i=0;i<4;++i)b.push_back(s[i]);};auto put=[&](unsigned n,unsigned width){for(unsigned i=0;i<width;++i)b.push_back(uint8_t(n>>(8*i)));};text("RIFF");put(40,4);text("WAVE");text("fmt ");put(16,4);put(1,2);put(1,2);put(48000,4);put(96000,4);put(2,2);put(16,2);text("data");put(4,4);put(0,4);{std::ofstream f(root/"hit.wav",std::ios::binary);f.write(reinterpret_cast<char*>(b.data()),b.size());}{std::ofstream f(root/"combat.txt");f<<"MGO2MT.COMBAT_AUDIO 1 1\n1369 hit.wav\n";}
 Effects local,remote;check(local.load(root)&&remote.load(root),"strict PCM effect mapping loads");Event e;e.epoch=1;e.id=1;e.cue=1369;e.kind=EventKind::impact;e.position={0,0,0};std::vector<Sound>a,c;auto addA=[&](const Sound&s){a.push_back(s);};auto addC=[&](const Sound&s){c.push_back(s);};
 local.dispatch(std::array{e},{0,0,0},addA);remote.dispatch(std::array{e},{12000,0,0},addC);check(a.size()==1&&c.size()==1&&a[0].file==c[0].file&&a[0].cue==c[0].cue&&a[0].gain==1&&c[0].gain==.5f,"both players resolve same cue, with listener distance attenuation");
 local.dispatch(std::array{e},{0,0,0},addA);check(a.size()==1,"repeated event never restarts effect");e.id=2;e.cue=9999;local.dispatch(std::array{e},{0,0,0},addA);check(a.size()==1&&local.missing()==1,"missing cue is silent and counted");
 e.id=5;e.cue=1369;local.dispatch(std::array{e},{0,0,0},addA);e.id=4;local.dispatch(std::array{e},{0,0,0},addA);local.dispatch(std::array{e},{0,0,0},addA);check(a.size()==3,"out-of-order reliable chunk plays once without dropping a gunshot");a.resize(1);
 {std::ofstream f(root/"combat.txt");f<<"MGO2MT.COMBAT_AUDIO 1 1\n1369 ../hit.wav\n";}check(!local.load(root),"network-independent catalog traversal rejected");e.epoch=2;e.id=1;e.cue=1369;local.dispatch(std::array{e},{0,0,0},addA);check(a.size()==2,"new round restarts event IDs; failed reload preserved valid catalog");
 {std::ofstream f(root/"combat.txt");f<<"MGO2MT.COMBAT_AUDIO 1 4\n12006 hit.wav\n12016 hit.wav\n12025 hit.wav\n12032 hit.wav\n";}check(local.load(root),"original explosion cues load");a.clear();e.epoch=3;e.cue=0;e.kind=EventKind::explosion;
 for(unsigned w=52;w<=55;++w){e.id=w;e.weapon=uint16_t(w);local.dispatch(std::array{e},{0,0,0},addA);}check(a.size()==4&&a[0].cue==12006&&a[1].cue==12016&&a[2].cue==12025&&a[3].cue==12032,"original explosion subtype cues route once");e.id=70;e.weapon=52;e.kind=EventKind::impact;local.dispatch(std::array{e},{0,0,0},addA);check(a.size()==4,"impact is not inferred as an explosion");
 {std::ofstream f(root/"combat.txt");f<<"MGO2MT.COMBAT_AUDIO 1 3\n12003 hit.wav\n12004 hit.wav\n12005 hit.wav\n";}check(local.load(root),"original mortar explosion cues load");a.clear();e.epoch=4;e.cue=0;e.kind=EventKind::explosion;e.weapon=103;e.position={0,0,0};
 for(float distance:{14999.f,15000.f,34999.f,35000.f}){++e.id;local.dispatch(std::array{e},{distance,0,0},addA);}check(a.size()==4&&a[0].cue==12003&&a[1].cue==12004&&a[2].cue==12004&&a[3].cue==12005,"original mortar dry explosion distance thresholds");
 std::filesystem::remove(root/"hit.wav");std::filesystem::remove(root/"combat.txt");std::filesystem::remove(root);std::cout<<"combat PCM mapping, exactly-once sound and independent listener volume passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';std::error_code ec;std::filesystem::remove(root/"hit.wav",ec);std::filesystem::remove(root/"combat.txt",ec);std::filesystem::remove(root,ec);return 1;}}
