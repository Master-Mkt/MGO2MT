#include "menu_audio.h"
#include "character_screen.h"
#include <algorithm>
namespace mgo2mt {
namespace {
const wchar_t* round_error(combat::wire::CommandError error){using E=combat::wire::CommandError;switch(error){
 case E::none:return L"";
 case E::unavailable:return L"ホストの武器・出撃データが準備できていません。";
 case E::not_loaded:return L"ステージの読み込みとチーム選択を完了してください。";
 case E::phase:return L"現在のラウンド状態では、この操作はできません。";
 case E::team:return L"チームを変更できません。";
 case E::weapon:return L"ホストで動作に対応している武器を選択してください。";
 case E::restricted:return L"この部屋では選択した武器を使用できません。";
 case E::insufficient_dp:return L"DPが足りません。武器を選び直してください。";
 case E::spawn:return L"出撃位置に障害物か他のPCがいます。少し待って再度出撃してください。DPは消費していません。";
 case E::already_deployed:return L"出撃済みです。再出撃の受付までお待ちください。";
 default:return L"操作の応答を確認できませんでした。";
}}
}
bool CharacterScreen::round_command(combat::wire::Command command){
 if(!stage_load_request()||!detailReply_.preparation)return false;
 command.epoch=detailReply_.preparation->epoch;command.sequence=combatCommandSequence_+1;
 const auto&self=detailReply_.preparation->players[detailReply_.preparation->self.slot];if(!self)return false;command.life=self->life;
 if(!roomRequests_.combat_command(command)){detailNotice_=L"送信待ちです。少し待って操作してください。";return false;}
 combatCommandSequence_=command.sequence;return true;
}
bool CharacterScreen::supported_weapon(uint16_t id)const{
 if(!detailReply_.preparation)return true;const auto&ids=detailReply_.preparation->supported;return std::find(ids.begin(),ids.end(),id)!=ids.end();
}
void CharacterScreen::stage_feedback(const stage::Result& result){
 stageStatus_=result.status;const auto&p=detailReply_.preparation;if(!stage_load_request()||!p)return;
 bool loaded=result.status==stage::Status::preview_ready&&result.request==stage_load_request()&&result.collision&&result.objectSnapshot==detailReply_.host_scene&&detailReply_.scene_status==stage::SceneSyncStatus::ready&&detailReply_.host_scene&&detailReply_.host_scene->request.generation==p->generation;
 if(combatLoaded_&&*combatLoaded_==loaded)return;
 combat::wire::Command command;command.kind=combat::wire::CommandKind::loaded;command.generation=p->generation;command.enabled=loaded;
 if(loaded)command.sceneRevision=detailReply_.host_scene->revision;
 if(round_command(command))combatLoaded_=loaded;
}
void CharacterScreen::round_ready(){
 if(combatEntered_){toggle_gameplay_briefing();return;}
 auto&p=detailReply_.preparation;if(!p){detailNotice_=L"このホストは出撃操作に対応していません。";return;}
 if(p->phase==combat::wire::RoundPhase::ended){detailNotice_=L"時間終了。次のラウンドを準備しています。";return;}
 if(p->phase!=combat::wire::RoundPhase::waiting){open_weapons();return;}
 const auto&self=p->players[p->self.slot];if(!self)return;
 combat::wire::Command command;command.kind=combat::wire::CommandKind::ready;command.enabled=!self->ready;
 if(round_command(command))cues_.push_back(command.enabled?menu_audio::Confirm:menu_audio::Cancel);
}
void CharacterScreen::round_team(){
 auto&p=detailReply_.preparation;if(!p||p->autoAssign)return;const auto&self=p->players[p->self.slot];if(!self)return;
 combat::wire::Command command;command.kind=combat::wire::CommandKind::team;command.team=self->team==1?2:1;
 if(round_command(command))cues_.push_back(menu_audio::Cursor);
}
void CharacterScreen::update_round(){
 const auto&p=detailReply_.preparation;if(!p){loadoutPending_=0;return;}
 if(loadoutPending_&&uint32_t(p->lastCommand-loadoutPending_)<0x80000000u){loadoutPending_=0;weaponNotice_=round_error(p->error);}
 const auto&self=p->players[p->self.slot];const auto&state=detailReply_.combat_state;
 if(p->phase==combat::wire::RoundPhase::ended){loadoutPending_=0;weaponsVisible_=false;roomRequests_.clear_combat();return;}
 if(self&&!self->deployed&&combatEntered_){combatEntered_=false;loadoutPending_=0;open_weapons();}
 if(!combatEntered_&&self&&self->deployed&&state&&state->epoch==p->epoch&&state->players[p->self.slot]&&state->players[p->self.slot]->identity==p->self&&state->players[p->self.slot]->life==self->life){weaponsVisible_=false;matchVisible_=true;combatEntered_=true;roundIntro_.deployed(p->epoch,clock_());}
}
std::wstring CharacterScreen::round_notice()const{
 const auto&p=detailReply_.preparation;if(!p)return L"";
 if(p->phase==combat::wire::RoundPhase::ended)return p->freeForAll?L"時間終了。個人順位を表示し、次のラウンドを準備しています。":L"時間終了。次のラウンドを準備しています（チーム勝敗集計は未対応）。";
 const auto&self=p->players[p->self.slot];if(!self)return L"参加者の状態を確認しています…";
 if(p->error!=combat::wire::CommandError::none)return round_error(p->error);
 if(!p->runtimeReady){if(detailReply_.combat_status==combat::wire::Status::awaiting_world)return L"ホストのステージデータを確認できないため、出撃できません。";if(detailReply_.combat_status==combat::wire::Status::awaiting_spawn)return L"このステージ・ルールの出撃位置は対応準備中です。";return L"ホストの武器の動作データが確認中のため、出撃を待っています。ステージ情報と武器一覧は確認できます。";}
 if(!self->loaded)return L"ステージと配置を読み込んでいます…";
 std::wstring team=p->freeForAll?L"DM":self->team==1?L"RED":self->team==2?L"BLUE":L"未選択";
 if(!p->autoAssign)team+=L" [F7で変更]";
 if(p->respawnWaiting)return L"戦闘不能 / 再出撃まで "+std::to_wstring((p->respawnRemainingMs+999)/1000)+L" 秒";
 if(self->deployed)return L"ラウンド進行中 / "+team+L"。移動・射撃は操作設定に従います。";
 if(p->phase!=combat::wire::RoundPhase::waiting)return L"ラウンド開始 / "+team+L"。F4で武器を選び、出撃してください。";
 auto seconds=(p->remainingMs+999)/1000;
 return team+L" / "+(self->ready?L"出撃OK（F9 / STARTで取消）":L"F9 / STARTで出撃OK")+(p->countdown?L" / 準備開始まで "+std::to_wstring(seconds)+L"秒":L"");
}
}
