#include "stage_music.h"
#include "stage_debug.h"
#include "pcm_wave.h"
#include <fstream>
#include <iostream>
#include <chrono>
using namespace mgo2mt;
static void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
static std::vector<unsigned char> wave(unsigned identity,bool loop=true){
 std::vector<unsigned char>b;auto str=[&](const char*s){b.insert(b.end(),s,s+4);};auto u=[&](uint32_t x){for(int i=0;i<4;++i)b.push_back(static_cast<unsigned char>(x>>(8*i)));};
 str("RIFF");u(0);str("WAVE");str("fmt ");u(16);u(0x00020001);u(48000);u(192000);u(0x00100004);
 if(loop){str("smpl");u(60);for(auto x:{0u,0u,20833u,60u,0u,0u,0u,1u,0u,0u,0u,1u,3u,0u,0u})u(x);}
 str("data");u(16);for(unsigned i=0;i<4;++i)u(identity+i);auto n=uint32_t(b.size()-8);for(int i=0;i<4;++i)b[4+i]=static_cast<unsigned char>(n>>(8*i));return b;
}
int main(int argc,char**argv){try{
 if(argc>1){auto lib=stage::MusicLibrary::scan(argv[1]);check(!lib.tracks.empty()&&!lib.rejected&&!lib.overflow,"prepared library PCM validation");std::cout<<"prepared WAV tracks="<<lib.tracks.size()<<"\n";}
 auto b=wave(1);auto w=read_pcm_wave(b);check(w.channels==2&&w.rate==48000&&w.loopBegin==1&&w.loopEnd==4,"smpl end inclusive conversion");
 for(unsigned mode=0;mode<4;++mode){auto bad=b;if(mode==0)bad.pop_back();if(mode==1)bad[96]=1;if(mode==2)bad[92]=4;if(mode==3)bad[20]=3;bool caught=false;try{read_pcm_wave(bad);}catch(...){caught=true;}check(caught,"malformed PCM / fractional loop / out of bounds / float rejected");}
 auto dir=std::filesystem::temp_directory_path()/("mgo2mt-music-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(dir/"additional");
 auto write=[&](auto p,unsigned id){auto bytes=wave(id);std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());};
 write(dir/"bgm_mgo_action01.wav",1);for(unsigned i=0;i<34;++i)write(dir/"additional"/(std::to_string(100+i)+".wav"),100+i);
 auto lib=stage::MusicLibrary::scan(dir);check(lib.tracks.size()==33&&lib.overflow==2,"32 additional, original separate");
 auto first=lib.tracks[1];std::filesystem::rename(first.path,first.path.parent_path()/L"000 日本語の曲.wav");auto renamed=stage::MusicLibrary::scan(dir);check(renamed.find(first.id)&&renamed.find(first.id)->title==L"000 日本語の曲","content identity survives Unicode rename");lib=std::move(renamed);
 stage::MusicSelection selection;auto original=lib.tracks[0].id;
 check(selection.resolve(lib).track->id==original,"available original fallback");check(!selection.choose(lib,first.id,false,false)&&selection.resolve(lib).track->id==original,"normal change queued until respawn");selection.respawn();check(selection.resolve(lib).track->id==first.id,"respawn applies pending selection");
 selection.force(original);check(selection.resolve(lib).track->id==original&&!selection.resolve(lib).missingForced,"forced known song overrides local");selection.force("additional:unavailable");check(selection.resolve(lib).track->id==first.id&&selection.resolve(lib).missingForced,"unknown forced song falls back without treating row as ID");selection.force(std::nullopt);check(selection.resolve(lib).track->id==first.id&&!selection.resolve(lib).missingForced,"force release restores selection");
 stage::MusicLibrary empty;check(!selection.resolve(empty).track,"missing everything produces silence");check(selection.choose(lib,original,true,false),"debug changes immediately");check(!selection.choose(lib,"missing",true,false)&&selection.selected()==original,"invalid selection retains current choice");
 stage::MusicPlayback playback;
 check(playback.select(selection.resolve(lib).track),"first playback starts");
 selection.choose(lib,original,false,false);selection.respawn();check(!playback.select(selection.resolve(lib).track),"same song at respawn preserves existing voice and sample position");
 selection.choose(lib,first.id,false,false);check(!playback.select(selection.resolve(lib).track),"pending different song keeps playing until respawn");selection.respawn();check(playback.select(selection.resolve(lib).track),"different song at respawn replaces voice once");
 selection.choose(lib,first.id,false,true);check(!playback.select(selection.resolve(lib).track),"same immediate respawn selection also continues");
 {std::ofstream f(dir/"playlist1.txt",std::ios::binary);f<<"\xef\xbb\xbf# titles\r\nBGM_MGO_ACTION01.WAV=原曲の表示名\r\n";}
 {std::ofstream f(dir/"additional/playlist2.txt",std::ios::binary);f<<"# extra\n000 日本語の曲.wav=追加の曲名\n000 日本語の曲.wav\t日本語・한글 = remix\n../bgm_mgo_action01.wav=not allowed\ninvalid line\n";}
 lib.reload_titles(dir);check(lib.find(original)->title==L"原曲の表示名"&&lib.find(first.id)->title==L"日本語・한글 = remix","BOM CRLF UTF8 bilingual titles, tab and last-entry-wins");check(lib.playlistErrors==2,"invalid records counted without rejecting valid titles");check(!playback.select(selection.resolve(lib).track),"title-only edit never restarts audio");
 {std::ofstream f(dir/"playlist1.txt",std::ios::binary);f<<"bgm_mgo_action01.wav="<<std::string(129,'x')<<"\n";}
 lib.reload_titles(dir);check(lib.find(original)->title==L"bgm_mgo_action01","overlong title falls back to stem");
 {std::ofstream f(dir/"playlist1.txt",std::ios::binary);f<<"\xff";}
 lib.reload_titles(dir);check(lib.find(original)->title==L"bgm_mgo_action01"&&lib.playlistErrors==3,"invalid UTF8 falls back independently of other playlist");
 std::filesystem::remove(dir/"playlist1.txt");lib.reload_titles(dir);check(lib.find(original)->title==L"bgm_mgo_action01","missing playlist uses filename");
 stage::DebugControls keys;check(!keys.key(0x74,false,true)&&!keys.reset,"F5 normal mode untouched");keys.key(0x7b,false,false);check(keys.enabled&&!keys.key(0x74,false,false),"F12 global, reset requires a stage");keys.key(0x74,true,true);check(!keys.confirmReset&&!keys.reset,"held F5 ignored");
 keys.key(0x74,false,true);check(keys.confirmReset&&!keys.resetYes&&!keys.reset,"F5 asks, default NO, no immediate reset");keys.clear_actions();keys.key(13,false,true);check(!keys.confirmReset&&!keys.reset,"Enter on default NO cancels");
 keys.key(0x74,false,true);keys.key(0x25,false,true);keys.key(13,true,true);check(keys.confirmReset&&!keys.reset,"held Enter cannot confirm");keys.key(13,false,true);check(keys.reset&&!keys.confirmReset,"explicit YES triggers reset once");keys.clear_actions();
 keys.key(0x74,false,true);keys.key(0x77,false,true);check(keys.musicStep==0&&keys.confirmReset,"modal blocks underlying BGM actions");keys.click(500,640);check(keys.confirmReset&&!keys.reset,"outside click cannot activate underlying room");keys.click(700,420);check(!keys.confirmReset&&!keys.reset,"mouse NO cancels");
 keys.key(0x74,false,true);keys.click(400,420);check(keys.reset&&!keys.confirmReset,"mouse YES confirms");keys.clear_actions();keys.key(0x74,false,true);keys.key(27,false,true);check(!keys.confirmReset&&!keys.reset,"Escape cancels");
 keys.key(0x74,false,true);keys.focus_lost();check(!keys.confirmReset&&!keys.reset,"focus loss cancels");keys.key(0x74,false,true);keys.key(13,false,false);check(!keys.confirmReset&&!keys.reset,"leaving the stage cancels");
 keys.key(0x74,false,true);keys.key(0x7b,false,true);check(!keys.enabled&&!keys.confirmReset&&!keys.reset,"closing debug cancels modal");
 check(!keys.key(0x73,false,false)&&!keys.openMotionBlend,"F4 remains untouched outside debug");
 keys.key(0x7b,false,false);check(keys.key(0x73,false,false)&&keys.openMotionBlend,"F4 opens blend settings without a stage in debug");
 keys.clear_actions();keys.key(0x73,true,false);check(!keys.openMotionBlend,"held F4 cannot reopen blend settings");
 keys.key(0x73,false,true);check(keys.openMotionBlend,"F4 stage action");keys.focus_lost();check(!keys.openMotionBlend,"focus loss clears pending blend action");
 keys.key(0x73,false,true);keys.cancel_reset();check(!keys.openMotionBlend,"cancel clears pending blend action");
 keys.key(0x73,false,true);keys.key(0x74,false,true);check(keys.confirmReset&&!keys.openMotionBlend,"F5 reset confirmation cancels unconsumed F4 action");
 check(keys.key(0x73,false,true)&&!keys.openMotionBlend&&keys.confirmReset,"F5 confirmation blocks F4 modal overlap");
 keys.key(27,false,true);keys.key(0x73,false,true);check(keys.openMotionBlend,"F4 available after reset confirmation closes");
 keys.key(0x7b,false,true);check(!keys.enabled&&!keys.openMotionBlend,"closing debug clears pending blend action");
 // Every deletion target was created under this fresh test-only directory.
 for(auto it=std::filesystem::directory_iterator(dir/"additional");it!=std::filesystem::directory_iterator();++it)std::filesystem::remove(it->path());std::filesystem::remove(dir/"additional");std::filesystem::remove(dir/"bgm_mgo_action01.wav");std::filesystem::remove(dir);
 std::cout<<"PCM loop bounds, 32 custom songs, stable IDs, forced fallback, respawn and debug keys passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
