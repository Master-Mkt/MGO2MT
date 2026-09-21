#include "round_music.h"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace mgo2mt;
static void check(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
int main(){try{
 namespace fs=std::filesystem;
 const auto dir=fs::temp_directory_path()/("mgo2mt-round-music-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 fs::create_directory(dir);
 {std::ofstream f(dir/"bgm_mgo_sneak01.wav");f<<"fixture existence only; PCM validated separately";}
 stage::MusicLibrary library;
 library.tracks={{"original:bgm_mgo_action01",L"action",dir/"bgm_mgo_action01.wav"},
                 {"original:bgm_mgo_sneak01",L"sneak",dir/"bgm_mgo_sneak01.wav"},
                 {"additional:fixture",L"custom",dir/"custom.wav",true},
                 {"original:bgm_mgo_action02",L"other group",dir/"bgm_mgo_action02.wav"}};
 const auto* selected=&library.tracks[0];
 using Phase=stage::RoundMusicPhase;
 std::optional<host::LoadRequest> request=host::LoadRequest{};
 request->rotation={20,1,0};request->generation=4;
 std::optional<combat::wire::Preparation> preparation=combat::wire::Preparation{};
 auto& prep=*preparation;prep.epoch=10;prep.generation=4;prep.runtimeReady=true;prep.roundClock=true;
 prep.phase=combat::wire::RoundPhase::active;
 auto phaseAt=[&](uint32_t remaining){prep.roundRemainingMs=remaining;return stage::tdm_round_music_phase(request,preparation);};
 check(phaseAt(60001)==Phase::normal&&phaseAt(60000)==Phase::action&&phaseAt(30001)==Phase::action&&phaseAt(30000)==Phase::urgent&&phaseAt(0)==Phase::urgent,"original inclusive 60/30 second boundaries");
 prep.remainingMs=0;prep.countdown=true;check(phaseAt(60001)==Phase::normal,"briefing countdown cannot trigger round music");
 prep.roundClock=false;check(phaseAt(0)==Phase::normal,"unknown round clock is not an expired round");prep.roundClock=true;
 prep.phase=combat::wire::RoundPhase::waiting;check(phaseAt(0)==Phase::normal,"new epoch waiting is normal");
 prep.phase=combat::wire::RoundPhase::selecting;check(phaseAt(0)==Phase::normal,"initial loadout selection is normal");
 prep.phase=combat::wire::RoundPhase::ended;check(phaseAt(0)==Phase::urgent,"result interval holds terminal urgency");
 prep.phase=combat::wire::RoundPhase::active;prep.epoch=11;check(phaseAt(29000)==Phase::urgent,"late joining an active round uses current time");
 prep.players[0]=combat::wire::RoundPlayer{};prep.players[0]->life=2;prep.players[0]->deployed=false;
 check(phaseAt(29000)==Phase::urgent,"death and respawn selection do not reset global phase");
 prep.players[0]->deployed=true;check(phaseAt(29000)==Phase::urgent,"redeployment preserves urgency");
 ++prep.generation;check(phaseAt(0)==Phase::normal,"old scene preparation cannot apply urgency");--prep.generation;
 for(auto rotation:{host::Rotation{19,1,0},host::Rotation{20,0,0},host::Rotation{20,1,2}}){request->rotation=rotation;check(!phaseAt(0),"unreviewed map/rule/flags do not acquire group override");}
 request->rotation={20,1,0};check(stage::tdm_round_music_phase(request,std::nullopt)==Phase::normal&&!stage::tdm_round_music_phase(std::nullopt,preparation),"absent host state and leaving stage");
 auto normal=stage::resolve_round_music(library,selected,Phase::normal);
 auto action=stage::resolve_round_music(library,selected,Phase::action);
 auto urgent=stage::resolve_round_music(library,selected,Phase::urgent);
 check(normal.track==&library.tracks[1]&&normal.alternateWave.empty(),"GCX normal phase resolves sneak01");
 check(action.track==selected&&!action.alternate&&action.alternateWave==dir/"bgm_mgo_action01_layer1.wav","action phase uses preset zero");
 check(urgent.track==selected&&urgent.alternate&&urgent.alternateWave==action.alternateWave,"urgency changes preset, not file");
 stage::MusicPlayback playback;
 check(playback.select(normal.track)&&!playback.select(normal.track),"normal respawn does not restart");
 check(playback.select(action.track,action.alternateWave)&&!playback.select(urgent.track,urgent.alternateWave),"only sneak to action replaces voice");
 check(!playback.select(urgent.track,urgent.alternateWave)&&!playback.select(action.track,action.alternateWave),"respawn or interrupted urgency keeps source position");
 check(playback.select(action.track)&&playback.select(action.track,action.alternateWave),"switching reviewed family support reconfigures immutable layout");
 // Exercise the actual host-clock -> family -> voice/preset control path.
 stage::MusicPlayback livePlayback;unsigned starts=0;
 std::optional<AudioLayerFader> fader;
 auto update=[&](uint32_t remaining,uint64_t now){
  auto phase=phaseAt(remaining);check(bool(phase),"supported live fixture");
  auto music=stage::resolve_round_music(library,selected,*phase);
  if(livePlayback.select(music.track,music.alternateWave)){
   ++starts;fader.reset();if(!music.alternateWave.empty())fader.emplace(stage::action01_mix,music.alternate);
  }
  return fader?fader->update(music.alternate,now):std::array<float,2>{};
 };
 update(61000,0);update(60000,1000);auto before=update(31000,30000);update(30000,31000);
 auto halfway=update(29500,31500);
 check(starts==2&&before[0]>0&&before[1]==0&&halfway[0]>0&&halfway[1]==stage::action01_mix.alternateGain,"60s switches file, 30s crossfades one voice using original gains");
 prep.players[0]->deployed=false;update(29000,32000);prep.players[0]->deployed=true;
 auto finished=update(28000,33000);check(starts==2&&finished[0]==0&&finished[1]==stage::action01_mix.alternateGain,"death and redeploy during fade do not restart it");
 prep.phase=combat::wire::RoundPhase::ended;update(0,61000);check(starts==2,"result delay preserves current voice");
 ++prep.epoch;prep.phase=combat::wire::RoundPhase::waiting;update(0,62000);check(starts==3&&!fader,"new round resets to normal family track");
 for(auto i:{2,3})for(auto phase:{Phase::normal,Phase::action,Phase::urgent}){
  auto custom=stage::resolve_round_music(library,&library.tracks[i],phase);
  check(custom.track==&library.tracks[i]&&custom.alternateWave.empty()&&!custom.alternate,"custom and unreviewed groups remain selected");
 }
 check(!stage::resolve_round_music(library,nullptr,Phase::urgent).track,"empty music library stays silent");
 fs::remove(dir/"bgm_mgo_sneak01.wav");
 auto missing=stage::resolve_round_music(library,selected,Phase::normal);
 check(missing.track==selected&&!missing.alternate,"removed optional sneak falls back to selected action");
 check(playback.select(normal.track),"next normal round returns to sneak when available");
 playback.clear();check(playback.select(urgent.track),"leaving and late rejoining starts a fresh voice");
 fs::remove(dir);
 std::cout<<"Original GCX group 1 selection, sample-preserving preset changes, respawn, custom music and optional assets passed\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
