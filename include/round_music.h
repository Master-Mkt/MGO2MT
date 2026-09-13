#pragma once
#include "stage_music.h"
#include "audio_layers.h"
#include "combat_wire.h"
#include "host_match.h"
#include "stage_profiles.h"

namespace mgo2win::stage {
// Current n022a GCX group 1, registered by 627E8 as SD phases 0/3/2/1.
// The filenames identify the reviewed original family; additional music and
// other original groups retain their selected single-track behavior.
enum class RoundMusicPhase { normal, action, urgent };
struct RoundMusic {
 const Track* track=nullptr;
 std::filesystem::path alternateWave;
 bool alternate=false;
 RoundMusicPhase phase=RoundMusicPhase::normal;
};
inline constexpr AudioLayerMix action01_mix{93.f*0.007874f,98.f*0.007874f,500,2000};

// Original TDM 7A7198: 180000/90000 round ticks at 3000 ticks/second.
// Native GWCB supplies milliseconds, independently of the briefing countdown.
// Ticket-ratio branches (60%/30%) await authoritative native ticket state.
inline std::optional<RoundMusicPhase> tdm_round_music_phase(
 const std::optional<host::LoadRequest>& request,
 const std::optional<combat::wire::Preparation>& preparation){
 if(!request||!runtime_stage_supported(request->rotation.map)||request->rotation.rule!=1||request->rotation.flags)return std::nullopt;
 if(!preparation||preparation->generation!=request->generation||!preparation->runtimeReady||!preparation->roundClock)
  return RoundMusicPhase::normal;
 const auto& p=*preparation;
 // Preserve the terminal preset through the native result interval. A fresh
 // epoch's waiting/selecting state returns to normal, with no per-life reset.
 if(p.phase!=combat::wire::RoundPhase::active&&p.phase!=combat::wire::RoundPhase::ended)
  return RoundMusicPhase::normal;
 if(p.roundRemainingMs<=30000)return RoundMusicPhase::urgent;
 if(p.roundRemainingMs<=60000)return RoundMusicPhase::action;
 return RoundMusicPhase::normal;
}

inline RoundMusic resolve_round_music(const MusicLibrary& library,const Track* selected,RoundMusicPhase phase){
 RoundMusic result{selected,{},false,phase};
 if(!selected||selected->additional||selected->id!="original:bgm_mgo_action01")return result;
 if(phase==RoundMusicPhase::normal){
  std::error_code ec;
  if(auto sneak=library.find("original:bgm_mgo_sneak01");sneak&&std::filesystem::is_regular_file(sneak->path,ec)){
   result.track=sneak;return result;
  }
 }
 // Optional original music may be removed. The audio reader validates both
 // layers and falls back to the selected normal WAV if pairing is impossible.
 result.alternateWave=selected->path.parent_path()/"bgm_mgo_action01_layer1.wav";
 result.alternate=phase==RoundMusicPhase::urgent;
 return result;
}
}
