#include "menu_font.h"
#include "menu_audio.h"
#include "menu_theme.h"
#include "character_screen.h"
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <syncstream>
#include <algorithm>
namespace mgo2win {
CharacterScreen::CharacterScreen(std::function<CharacterReply(const std::atomic_bool&)>transport,std::function<uint64_t()> clock):transport_(std::move(transport)),clock_(std::move(clock)){
 dc_=CreateCompatibleDC(nullptr);if(!dc_)throw std::runtime_error("Character DC failure");BITMAPINFO i{};i.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);i.bmiHeader.biWidth=1280;i.bmiHeader.biHeight=-720;i.bmiHeader.biPlanes=1;i.bmiHeader.biBitCount=32;
 bitmap_=CreateDIBSection(dc_,&i,DIB_RGB_COLORS,&pixels_,nullptr,0);if(!bitmap_){DeleteDC(dc_);throw std::runtime_error("Character surface failure");}old_=SelectObject(dc_,bitmap_);
 for(int size:{30,23,20,17})fonts_.push_back(create_menu_font(size,FW_NORMAL));start();
}
namespace {
std::wstring skill_text(std::string_view value){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);if(!n)return {};std::wstring result(n,L' ');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),result.data(),n);return result;}
bool skill_profile_valid(const skills::RemoteState& state,const skills::Catalog* catalog,uint32_t id){
 if(!catalog||!id||state.character!=id||!state.profile||state.profile->character!=id||state.profile->status)return false;
 const auto check=skills::validate(*catalog,state.profile->loadout,state.profile->capacity);return check&&check.used==state.profile->used;
}
}
void CharacterScreen::skill_catalog(const std::filesystem::path& path){
 auto catalog=std::make_shared<skills::Catalog>();std::string error;
 if(!catalog->load(path,error)){skillCatalog_.reset();skillNotice_=L"スキル一覧を読み込めませんでした。";return;}
 skillCatalog_=std::move(catalog);if(creation_)creation_->require_skills(true);
}
bool CharacterScreen::skills_editable()const{
 if(creation_)return !creation_->registration_busy()&&!creation_->confirming()&&!creation_->discarding();
 if(!selected_character_id()||pending_)return false;
 if(detailReply_.join_status==RoomJoinStatus::joined){const auto&p=detailReply_.preparation;return !matchVisible_&&p&&p->phase==combat::wire::RoundPhase::waiting;}
 return !matchVisible_;
}
skills::Loadout CharacterScreen::active_skills()const{
 if(!skillCatalog_||!roomRequests_.skills)return {};
 const auto state=roomRequests_.skills->state();if(!state.profile||state.profile->character!=selected_character_id()||state.profile->status)return {};
 return ([&]{auto check=skills::validate(*skillCatalog_,state.profile->loadout,state.profile->capacity);return check&&check.used==state.profile->used;})()?state.profile->loadout:skills::Loadout{};
}
std::vector<std::wstring> CharacterScreen::skill_hud_labels()const{
 std::vector<std::wstring> result;if(!skillCatalog_)return result;
 for(const auto&choice:active_skills().entries)if(auto entry=skillCatalog_->find(choice.id,choice.level))result.push_back(skill_text(entry->display_name.empty()?entry->name:entry->display_name)+L"  LV "+std::to_wstring(choice.level));
 return result;
}
void CharacterScreen::open_skills(){
 if(skill_visible())return;
 if(!skills_editable()){skillNotice_=L"スキルはPC選択後、ラウンド開始前に変更できます。";lobbyNotice_=detailNotice_=skillNotice_;return;}
 if(!skillCatalog_){skillNotice_=L"スキル一覧を読み込めませんでした。データを確認してください。";lobbyNotice_=detailNotice_=skillNotice_;return;}
 if(!skillMenu_){skillMenu_=std::make_unique<SkillMenu>();skillMenu_->icons(skillIcons_);}
 skills::Loadout selected;unsigned capacity=skills::base_capacity;
 if(creation_){selected=creationSkills_;skillNotice_=L"作成内容に追加します。PC選択・ロビー接続後にサーバーへ保存します。";}
 else{
  auto state=roomRequests_.skills->state();selected=active_skills();
  if(state.profile&&state.profile->character==selected_character_id()&&!state.profile->status){auto check=skills::validate(*skillCatalog_,state.profile->loadout,state.profile->capacity);if(check&&check.used==state.profile->used)capacity=state.profile->capacity;}
  if(auto pending=skillDrafts_.find(selected_character_id());pending!=skillDrafts_.end())selected=pending->second.value;
  if(state.status==skills::RemoteStatus::ready)skillNotice_=L"適用するとサーバーへ保存します。利用できるレベルはサーバーが確認します。";
  else if(state.status==skills::RemoteStatus::saving)skillNotice_=L"スキルを保存しています。結果を待ってください。";
  else if(state.status==skills::RemoteStatus::unsupported)skillNotice_=L"サーバーのスキル設定対応を確認できません。変更は下書きになります。";
  else if(state.status==skills::RemoteStatus::rejected||state.status==skills::RemoteStatus::conflict||state.status==skills::RemoteStatus::outcome_unknown){skillNotice_=L"前回の変更の保存結果を確認できていません。最新の設定を取得しています。";roomRequests_.skills->fetch();}
  else skillNotice_=L"変更は下書きです。ロビーへ接続して設定を取得後、サーバーへ保存します。";
 }
 skillMenu_->open(skillCatalog_,selected,capacity);skillMenu_->notice(skillNotice_);
 skillSerial_=~uint64_t(0);if(!creation_)update_skills();if(cues_.size()<32)cues_.push_back(menu_audio::Confirm);
}
void CharacterScreen::update_skills(){
 if(!roomRequests_.skills)return;
 if(skill_visible()&&!creation_&&!skills_editable()){skillMenu_->close(true);for(auto cue:skillMenu_->cues())if(cues_.size()<32)cues_.push_back(cue);skillNotice_=L"ラウンドが開始されたためスキル編集を終了しました。";}
 auto state=roomRequests_.skills->state();if(state.serial==skillSerial_){auto pending=skillDrafts_.find(selected_character_id());if(pending==skillDrafts_.end()||!pending->second.queued||state.status!=skills::RemoteStatus::ready||!skills_editable())return;}skillSerial_=state.serial;
 const auto id=selected_character_id();if(!id||state.character!=id)return;
 if(skill_visible()&&!creation_)skillMenu_->apply_allowed(state.status==skills::RemoteStatus::offline||(state.status==skills::RemoteStatus::ready&&skill_profile_valid(state,skillCatalog_.get(),id)));
 auto pending=skillDrafts_.find(id);
 if(state.status==skills::RemoteStatus::ready&&state.profile&&state.profile->character==id&&skillCatalog_){
  auto verified=skills::validate(*skillCatalog_,state.profile->loadout,state.profile->capacity);if(!verified||verified.used!=state.profile->used){skillNotice_=lobbyNotice_=detailNotice_=L"サーバーのスキル設定を確認できません。保存を停止しました。";if(skill_visible())skillMenu_->notice(skillNotice_);return;}
  if(skill_visible()&&!creation_)skillMenu_->synchronize(state.profile->loadout,state.profile->capacity,pending!=skillDrafts_.end());
  skillNotice_=L"適用するとサーバーへ保存します。利用できるレベルはサーバーが確認します。";
  if(pending!=skillDrafts_.end()&&pending->second.sending){if(state.profile->loadout==pending->second.value){skillDrafts_.erase(pending);pending=skillDrafts_.end();skillNotice_=L"選択したスキルがサーバーの設定に反映されています。";}else{pending->second.sending=pending->second.queued=false;skillNotice_=L"サーバーの設定が下書きと異なります。確認してから再度適用してください。";}}
  if(pending!=skillDrafts_.end()&&pending->second.queued&&skillCatalog_){
   // Only an explicitly applied draft may replace the newly read server profile.
   if(!skills::validate(*skillCatalog_,pending->second.value,state.profile->capacity)){pending->second.queued=false;skillNotice_=L"現在のスキル枠では下書きを保存できません。選び直してください。";}
   else if(skills_editable()&&roomRequests_.skills->save(pending->second.value)){pending->second.queued=false;pending->second.sending=true;skillNotice_=L"スキルをサーバーへ保存しています…";}
  }
 }else if(state.status==skills::RemoteStatus::reading){skillNotice_=L"サーバーの設定を取得しています。取得が完了するまで適用できません。";
 }else if(state.status==skills::RemoteStatus::saving){skillNotice_=L"スキルを保存しています。結果を待ってください。";
 }else if(state.status==skills::RemoteStatus::unsupported){skillNotice_=L"サーバーのスキル設定対応を確認できません。保存できませんでした。";
 }else if(state.status==skills::RemoteStatus::rejected||state.status==skills::RemoteStatus::conflict||state.status==skills::RemoteStatus::outcome_unknown||state.status==skills::RemoteStatus::offline){
  if(pending!=skillDrafts_.end()&&pending->second.sending){pending->second.sending=false;pending->second.queued=false;}
  if(state.status==skills::RemoteStatus::rejected){
   if(state.error==6)skillNotice_=L"選択したレベルはサーバーで利用可能と確認されていません。レベルを下げて再度適用してください。";
   else if(state.error==5)skillNotice_=L"サーバーのスキル保存機能を利用できません。登録は完了していません。";
   else if(state.error==2)skillNotice_=L"スキルの保存に必要なPC接続を確認できません。ロビーへ接続し直してください。";
   else skillNotice_=L"スキルを保存できませんでした。レベル・使用枠を確認し、選び直してください。";
  }
  else if(state.status==skills::RemoteStatus::conflict)skillNotice_=L"サーバーの設定が変更されています。スキル画面を開き直して確認してください。";
  else if(state.status==skills::RemoteStatus::outcome_unknown)skillNotice_=L"保存結果を確認できません。再送せず、スキル画面を開き直して確認してください。";
  else if(pending!=skillDrafts_.end())skillNotice_=L"スキルの下書きがあります。ロビー接続後に保存状態を確認してください。";
 }
 if(skill_visible()&&!creation_&&roomRequests_.skills->state().status==skills::RemoteStatus::saving)skillMenu_->apply_allowed(false);
 if(!skillNotice_.empty()){lobbyNotice_=skillNotice_;if(detailVisible_)detailNotice_=skillNotice_;if(skill_visible())skillMenu_->notice(skillNotice_);}
}
bool CharacterScreen::skill_message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 update_skills();if(!skill_visible())return true;
 bool handled=skillMenu_->message(hwnd,msg,wp,lp);
 if(auto chosen=skillMenu_->take_saved()){
  if(creation_){creationSkills_=std::move(*chosen);creation_->skills_complete(creationSkillContinue_);creationSkillContinue_=false;for(auto cue:creation_->cues())if(cues_.size()<32)cues_.push_back(cue);}
  else if(auto id=selected_character_id()){
   skillDrafts_[id]={std::move(*chosen),true,false};skillNotice_=L"スキルの下書きを適用しました。サーバーへの保存は未完了です。";
   skillSerial_=~uint64_t(0);update_skills();
  }
 }
 for(auto cue:skillMenu_->cues())if(cues_.size()<32)cues_.push_back(cue);
 if(!skill_visible())creationSkillContinue_=false;
 return handled;
}
void CharacterScreen::stop(){cancel_=true;if(worker_.joinable())worker_.join();pending_=false;}
CharacterScreen::~CharacterScreen(){stop_rooms();stop();SelectObject(dc_,old_);DeleteObject(bitmap_);DeleteDC(dc_);for(auto f:fonts_)DeleteObject(f);}
void CharacterScreen::start(){if(pending_||GetTickCount64()<retryAt_)return;stop();slots_.load({});notice_.clear();reply_={};cancel_=false;done_=false;pending_=true;focus_=0;++requests_;cues_.push_back(menu_audio::Confirm);
 try{worker_=std::thread([this]{try{reply_=transport_(cancel_);}catch(...){reply_={};}done_=true;});}catch(...){pending_=false;reply_.status=CharacterStatus::network_error;}
 std::osyncstream(std::cout)<<"{\"character_request_started\":true}"<<std::endl;
}
void CharacterScreen::begin_registration(CharacterCreateRequest request){
 if(pending_||!createTransport_||registrationState_->unresolved){creation_->registration_failed(L"前回の登録結果を一覧で確認してください。");return;}
 stop();registrationState_->unresolved=true;registrationState_->expected_id=0;registrationState_->expected_name=request.name;if(skillCatalog_)registrationState_->skills=creationSkills_;
 cancel_=false;done_=false;pending_=registering_=true;createReply_={};
 try{worker_=std::thread([this,request=std::move(request)]{try{createReply_=createTransport_(request,cancel_);}catch(...){createReply_.status=CharacterCreateStatus::outcome_unknown;createReply_.request_may_have_been_sent=true;}done_=true;});}
 catch(...){pending_=registering_=false;*registrationState_={};creation_->registration_failed(L"登録処理を開始できませんでした。もう一度お試しください。");}
}
void CharacterScreen::update(){
 update_rooms();update_skills();
 selectionPresentation_.observe(!creation_&&!lobbyVisible_&&!skill_visible()?preview_character_id():0,clock_());
 if(!pending_||!done_||(selecting_&&selectionPresentation_.selecting(clock_())))return;worker_.join();pending_=false;
 if(selecting_){selecting_=false;auto status=selectionReply_.status;
  if((status==CharacterSelectionStatus::success&&(!selectionReply_.request_may_have_been_sent||selectionReply_.character.id!=selectionTarget_))||(selectionReply_.request_may_have_been_sent&&status!=CharacterSelectionStatus::success&&status!=CharacterSelectionStatus::rejected))status=CharacterSelectionStatus::outcome_unknown;
  selectionReply_.status=status;selectionState_->unresolved=status==CharacterSelectionStatus::outcome_unknown;
  if(status==CharacterSelectionStatus::success){lobbyVisible_=true;lobbyCategories_=false;lobbyGroup_=0;lobbyFocus_=0;lobbyNotice_.clear();roomRequests_.skills->character(selectionReply_.character.id);skillSerial_=~uint64_t(0);report_lobby_group();}
  else notice_=status==CharacterSelectionStatus::unavailable?L"PC選択はサーバー側の対応確認後に利用できます。":status==CharacterSelectionStatus::missing?L"選んだPCが一覧に見つかりません。一覧を再取得してください。":status==CharacterSelectionStatus::rejected?L"PC選択が受け付けられませんでした。ログインからやり直してください。":status==CharacterSelectionStatus::outcome_unknown?L"PC選択の結果を確認できませんでした。再送せずログインからやり直してください。":status==CharacterSelectionStatus::cancelled?L"PC選択を中止しました。":L"PC選択前に通信を確認できませんでした。もう一度お試しください。";
  if(status==CharacterSelectionStatus::success)cues_.push_back(menu_audio::Confirm);std::osyncstream(std::cout)<<"{\"character_selection_result\":true,\"status\":"<<int(status)<<",\"error\":"<<selectionReply_.error<<",\"may_have_been_sent\":"<<(selectionReply_.request_may_have_been_sent?"true":"false")<<",\"lobby_visible\":"<<(lobbyVisible_?"true":"false")<<",\"lobby_count\":"<<selectionReply_.lobbies.size()<<"}"<<std::endl;return;
 }
 if(registering_){registering_=false;
  auto status=createReply_.status;
  // A successful response without an ID or an inconsistent transport result is ambiguous.
  if((status==CharacterCreateStatus::success&&!createReply_.created_id)||(createReply_.request_may_have_been_sent&&status!=CharacterCreateStatus::success&&status!=CharacterCreateStatus::rejected))status=CharacterCreateStatus::outcome_unknown;
  std::osyncstream(std::cout)<<"{\"character_registration_result\":true,\"status\":"<<int(status)<<",\"error\":"<<createReply_.error<<",\"may_have_been_sent\":"<<(createReply_.request_may_have_been_sent?"true":"false")<<"}"<<std::endl;
  if(status==CharacterCreateStatus::success||status==CharacterCreateStatus::outcome_unknown){
   registrationState_->expected_id=status==CharacterCreateStatus::success?createReply_.created_id:0;
   registrationNotice_=status==CharacterCreateStatus::success?L"登録が完了しました。一覧を更新しています。":L"登録結果を確認できませんでした。再送せず一覧を確認します。";
   creation_.reset();retryAt_=0;start();return;
  }
  *registrationState_={};
  auto message=status==CharacterCreateStatus::rejected?L"登録が受け付けられませんでした。名前や選択内容を確認してください。":status==CharacterCreateStatus::full?L"空きスロットがありません。一覧を再取得してください。":status==CharacterCreateStatus::cancelled?L"登録前に処理を中止しました。":L"登録前に接続できませんでした。通信設定を確認してください。";
  if(creation_)creation_->registration_failed(std::wstring(message)+(createReply_.error?L" ("+std::to_wstring(createReply_.error)+L")":L""));
  return;
 }
 retryAt_=GetTickCount64()+2000;slots_.load(reply_.status==CharacterStatus::success?reply_.list:CharacterList{});focus_=slots_.count()?0:1;
 if(registrationState_->unresolved){
  bool confirmed=false;if(reply_.status==CharacterStatus::success)for(unsigned i=0;i<reply_.list.entries.size();++i){const auto&e=reply_.list.entries[i];if(registrationState_->expected_id?e.id==registrationState_->expected_id:e.name==registrationState_->expected_name){slots_.select(i);focus_=int(i);if(registrationState_->skills)skillDrafts_[e.id]={*registrationState_->skills,true,false};confirmed=true;break;}}
  if(confirmed){*registrationState_={};registrationNotice_=L"登録されたPCを一覧で確認しました。";}
  else registrationNotice_=L"前回の登録を一覧で確認できていません。再取得して確認してください。新規登録は一時停止中です。";
 }
 notice_=registrationNotice_;if(reply_.status==CharacterStatus::success)cues_.push_back(menu_audio::Confirm);report();
}
void CharacterScreen::begin_selection(){
 if(pending_||!slots_.occupied())return;
 if(!selectTransport_){notice_=L"PC選択はサーバー側の対応確認後に利用できます。";return;}
 if(selectionState_->unresolved){notice_=L"前回のPC選択結果が不明です。ログインからやり直してください。";return;}
 stop();slots_.reset_hold();slots_.cancel_dialog();selectionTarget_=slots_.preview_id();selectionPresentation_.observe(selectionTarget_,clock_());selectionPresentation_.select(clock_());selectionState_->unresolved=true;
 selectionReply_={};cancel_=false;done_=false;pending_=selecting_=true;
 try{worker_=std::thread([this,id=selectionTarget_]{try{selectionReply_=selectTransport_(id,cancel_);}catch(...){selectionReply_.status=CharacterSelectionStatus::outcome_unknown;selectionReply_.request_may_have_been_sent=true;}done_=true;});}
 catch(...){pending_=selecting_=false;selectionState_->unresolved=false;notice_=L"PC選択を開始できませんでした。";}
}
void CharacterScreen::stop_rooms(){
 roundIntro_.reset();
 roomCancel_=true;if(roomWorker_.joinable())roomWorker_.join();std::lock_guard lock(roomMutex_);roomInbox_.clear();roomRequests_.clear();combatEvents_.clear();detailVisible_=false;detailBusy_=false;update_weapons();SecureZeroMemory(detailAction_.password.data(),sizeof(detailAction_.password));
}
void CharacterScreen::begin_rooms(){
 auto rows=lobby_group_rows(selectionReply_.lobbies,lobbyGroup_);if(roomVisible_||lobbyFocus_>=rows.size())return;
 if(!roomTransport_){lobbyNotice_=L"このプレビューではロビーへ接続しません。";return;}
 stop_rooms();roomLobby_=selectionReply_.lobbies[rows[lobbyFocus_]];roomReply_={};roomFocus_=0;roomNotice_.clear();roomCancel_=false;roomRefresh_=false;roomVisible_=true;cues_.push_back(menu_audio::Confirm);
 try{roomWorker_=std::thread([this,id=selectionReply_.character.id,lobby=roomLobby_]{
  auto publish=[this](RoomReply reply){std::lock_guard lock(roomMutex_);if(reply.host_roster&&!roomInbox_.empty()){auto&last=roomInbox_.back();if(last.host_roster&&last.event==reply.event&&last.requested_room==reply.requested_room&&last.join_status==reply.join_status&&last.status==reply.status){if(last.combat_offer==reply.combat_offer){if(last.combat_events.size()+reply.combat_events.size()>128){roomCancel_=true;return;}reply.combat_events.insert(reply.combat_events.begin(),last.combat_events.begin(),last.combat_events.end());}last=std::move(reply);return;}}roomInbox_.push_back(std::move(reply));};
  try{roomTransport_(id,lobby,roomCancel_,roomRefresh_,publish,roomRequests_);}catch(...){if(!roomCancel_)publish({RoomStatus::network_error,{},0});}
 });}catch(...){roomReply_.status=RoomStatus::network_error;}
 std::osyncstream(std::cout)<<"{\"game_lobby_started\":true,\"lobby_id\":"<<roomLobby_.id<<",\"port\":"<<roomLobby_.port<<"}"<<std::endl;
}
void CharacterScreen::update_rooms(){
 if(!roomVisible_)return;std::optional<RoomReply> next;{std::lock_guard lock(roomMutex_);if(!roomInbox_.empty()){next=std::move(roomInbox_.front());roomInbox_.pop_front();}}if(!next)return;
 if(next->event!=RoomEvent::list){
  if(!detailVisible_||next->requested_room!=detailAction_.id)return;
  if(!next->detail)next->detail=detailReply_.detail;
  auto previousPreparation=detailReply_.preparation;auto previousStage=detailReply_.join_status;auto previousRequest=detailReply_.host_match?detailReply_.host_match->request:std::optional<host::LoadRequest>{};
  if(previousPreparation&&next->preparation&&previousPreparation->phase!=next->preparation->phase){briefingPanel_=briefing::Panel::none;briefingYes_=false;}
  if(previousPreparation&&!next->preparation){briefingPanel_=briefing::Panel::none;briefingYes_=false;}
  if(previousPreparation&&next->preparation){const auto& a=*previousPreparation;const auto& b=*next->preparation;
   const auto before=a.self.slot<a.players.size()?a.players[a.self.slot]:std::nullopt,after=b.self.slot<b.players.size()?b.players[b.self.slot]:std::nullopt;
   if(bool(before)!=bool(after)||(before&&after&&(before->id!=after->id||before->life!=after->life||before->deployed!=after->deployed||(briefingPanel_==briefing::Panel::ready&&before->ready!=after->ready)))){briefingPanel_=briefing::Panel::none;briefingYes_=false;}
  }
  if((next->host_match?next->host_match->request:std::optional<host::LoadRequest>{})!=previousRequest){briefingPanel_=briefing::Panel::none;briefingFocus_=0;}
  if(next->combat_offer!=detailReply_.combat_offer){combatEvents_.clear();combatCommandSequence_=loadoutPending_=0;combatLoaded_.reset();combatEntered_=false;matchVisible_=false;weaponsVisible_=false;briefingPanel_=briefing::Panel::none;briefingFocus_=0;roomRequests_.clear_combat();}
  if(combatEvents_.size()+next->combat_events.size()>128){roomCancel_=true;combatEvents_.clear();}else combatEvents_.insert(combatEvents_.end(),next->combat_events.begin(),next->combat_events.end());
  next->combat_events.clear();
  detailBusy_=next->status==RoomStatus::connecting;detailReply_=std::move(*next);detailNotice_.clear();update_weapons();update_round();
  if(detailReply_.preparation&&detailReply_.preparation->phase!=combat::wire::RoundPhase::waiting&&detailReply_.preparation->phase!=combat::wire::RoundPhase::ended&&(!previousPreparation||previousPreparation->phase==combat::wire::RoundPhase::waiting)&&!detailReply_.preparation->players[detailReply_.preparation->self.slot]->deployed)open_weapons();
  if(detailReply_.join_status!=RoomJoinStatus::joined){matchVisible_=false;briefingPanel_=briefing::Panel::none;briefingFocus_=0;}
  if(detailReply_.join_status==RoomJoinStatus::permission_checked)detailNotice_=L"参加許可の確認が完了しました。この確認モードではホストへ接続せず、参加予約を解除します。";
  else if(detailReply_.join_status==RoomJoinStatus::outcome_unknown)detailNotice_=L"参加要求の結果を確認できません。再送せず、ログインし直してください。";
  else if(detailReply_.join_status==RoomJoinStatus::invalid_input)detailNotice_=L"パスワードは3〜16バイトで入力してください（日本語は1文字3バイトが目安）。";
  else if(detailReply_.join_status==RoomJoinStatus::host_connecting)detailNotice_=L"接続情報を確認し、ゲームホストに接続しています…（Escで中止）";
  else if(detailReply_.join_status==RoomJoinStatus::host_profile)detailNotice_=L"ホストへキャラクター情報を送信しています…（Escで中止）";
  else if(detailReply_.join_status==RoomJoinStatus::host_sync)detailNotice_=L"ホストからルーム情報を取得しています…（Escで中止）";
  else if(detailReply_.join_status==RoomJoinStatus::joined)detailNotice_=L"ホストに接続しました。参加者一覧は自動で更新されます。対戦操作は準備中です。";
  else if(detailReply_.join_status==RoomJoinStatus::host_cancelled)detailNotice_=L"接続を終了しました。サーバーへ参加解除を通知しました。";
  else if(detailReply_.join_status==RoomJoinStatus::host_timeout)detailNotice_=L"ホスト接続または同期が時間切れになりました。接続先・通信許可を確認してください。";
  else if(detailReply_.join_status==RoomJoinStatus::host_rejected)detailNotice_=L"ホストが接続を拒否しました。ルームの状態を確認してください。";
  else if(detailReply_.join_status==RoomJoinStatus::host_disconnected)detailNotice_=L"ホストとの通信が途切れました。ルーム一覧から接続し直してください。";
  else if(detailReply_.join_status==RoomJoinStatus::host_network_error)detailNotice_=L"UDP通信に失敗しました。Windowsの通信許可とポート設定を確認してください。";
  else if(detailReply_.join_status==RoomJoinStatus::host_protocol_error)detailNotice_=L"ホストの応答形式・バージョンを確認できませんでした。参加を中止しました。";
  else if(detailReply_.join_status==RoomJoinStatus::host_unavailable)detailNotice_=L"接続に必要なUDPポートが未確保か、自分がホストのルームです。ポート設定を確認してください。";
  else if(detailReply_.status==RoomStatus::rejected)detailNotice_=detailReply_.error==0xc0ffee10?L"満員のため参加できません。":detailReply_.error==0xc0ffee11?L"ホストにより参加が制限されています。":detailReply_.error==0xc0ffee04?L"ホストの接続情報を取得できません。":L"ルームが見つからないか、パスワード・参加条件が一致しません。";
  else if(detailReply_.status!=RoomStatus::ready)detailNotice_=L"ルームの応答を確認できません。ロビー一覧へ戻って接続し直してください。";
  if(detailReply_.status==RoomStatus::protocol_error&&detailReply_.join_status!=RoomJoinStatus::invalid_input)roomReply_.status=RoomStatus::protocol_error;
  if(detailReply_.detail&&detailReply_.event==RoomEvent::detail)detailFocus_=detailReply_.detail->password?0:1;
  if(detailReply_.status==RoomStatus::ready&&(detailReply_.join_status==RoomJoinStatus::none||detailReply_.join_status==RoomJoinStatus::permission_checked||detailReply_.join_status==RoomJoinStatus::joined)&&(previousStage!=detailReply_.join_status||detailReply_.event==RoomEvent::detail||(detailReply_.host_match&&detailReply_.host_match->request!=previousRequest)))cues_.push_back(menu_audio::Confirm);
  std::osyncstream(std::cout)<<"{\"room_action_result\":true,\"event\":"<<int(detailReply_.event)<<",\"status\":"<<int(detailReply_.status)<<",\"join_status\":"<<int(detailReply_.join_status)<<",\"error\":"<<detailReply_.error<<",\"room_id\":"<<detailReply_.requested_room<<",\"host_roster_complete\":"<<(detailReply_.host_roster&&detailReply_.host_roster->complete?"true":"false")<<",\"host_roster_count\":"<<(detailReply_.host_roster?detailReply_.host_roster->count():0)<<",\"host_roster_revision\":"<<(detailReply_.host_roster?detailReply_.host_roster->revision:0)<<",\"host_match_revision\":"<<(detailReply_.host_match?detailReply_.host_match->revision:0)<<",\"host_match_request\":"<<(detailReply_.host_match&&detailReply_.host_match->request?detailReply_.host_match->request->sequence:0)<<"}"<<std::endl;return;
 }
 if(detailVisible_&&next->status!=RoomStatus::ready){detailReply_.host_match.reset();detailReply_.host_roster.reset();matchVisible_=false;detailBusy_=false;detailNotice_=L"ロビー接続が終了しました。一覧へ戻って接続し直してください。";}
 auto old=roomFocus_<roomReply_.rooms.size()?roomReply_.rooms[roomFocus_].id:0;auto was=roomReply_.status;roomReply_=std::move(*next);roomFocus_=0;
 for(size_t i=0;i<roomReply_.rooms.size();++i)if(roomReply_.rooms[i].id==old)roomFocus_=i;
 roomNotice_.clear();if(was!=roomReply_.status&&roomReply_.status==RoomStatus::ready&&cues_.size()<32)cues_.push_back(menu_audio::Confirm);
 std::osyncstream(std::cout)<<"{\"room_list_result\":true,\"status\":"<<int(roomReply_.status)<<",\"error\":"<<roomReply_.error<<",\"count\":"<<roomReply_.rooms.size()<<",\"lobby_id\":"<<roomLobby_.id<<"}"<<std::endl;
}
bool CharacterScreen::room_message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 if(detailVisible_)return detail_message(hwnd,msg,wp,lp);
 auto n=roomReply_.rooms.size();auto leave=[&]{stop_rooms();roomVisible_=false;roomReply_={};cues_.push_back(menu_audio::Cancel);std::osyncstream(std::cout)<<"{\"game_lobby_left\":true}"<<std::endl;};
 auto refresh=[&]{if(roomReply_.status==RoomStatus::ready&&GetTickCount64()>=roomRefreshAt_){roomRefresh_=true;roomRefreshAt_=GetTickCount64()+1000;roomNotice_=L"一覧を更新しています…";cues_.push_back(menu_audio::Confirm);}};
 auto activate=[&]{if(roomFocus_==n+1)leave();else if(roomFocus_==n)refresh();else open_room_detail();};
 if(msg==WM_CHAR||msg==WM_KEYUP)return true;
 if(msg==WM_KEYDOWN){if((lp&(1LL<<30))&&(wp==VK_RETURN||wp==VK_SPACE||wp==VK_ESCAPE||wp==VK_F5))return true;
  auto previous=roomFocus_;if(wp==VK_ESCAPE)leave();else if(wp==VK_F5)refresh();
  else if(wp==VK_HOME)roomFocus_=0;else if(wp==VK_END)roomFocus_=n+1;
  else if(wp==VK_UP)roomFocus_=(roomFocus_+n+1)%(n+2);else if(wp==VK_DOWN||wp==VK_TAB)roomFocus_=(roomFocus_+1)%(n+2);
  else if(wp==VK_NEXT)roomFocus_=std::min(n,roomFocus_+7);else if(wp==VK_PRIOR)roomFocus_=roomFocus_>=7?roomFocus_-7:0;
  else if(wp==VK_RETURN||wp==VK_SPACE)activate();if(previous!=roomFocus_&&cues_.size()<32)cues_.push_back(menu_audio::Cursor);return true;
 }
 if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(!r.right||!r.bottom)return true;int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;
  if(y>=617&&y<662){if(x>=850&&x<1160)leave();else if(x>=510&&x<820)refresh();}
  else if(x>=120&&x<1160&&y>=285&&y<572){size_t page=roomFocus_<n?roomFocus_/7:n?(n-1)/7:0,row=page*7+(y-285)/41;if(row<n){roomFocus_=row;open_room_detail();}}
  SetFocus(hwnd);return true;
 }return false;
}
void CharacterScreen::draw_rooms(){
 if(detailVisible_){draw_room_detail();return;}
 auto fill=[&](int x,int y,int w,int h,COLORREF c){menu_fill(dc_,x,y,w,h,c);};
 auto text=[&](std::wstring_view s,int x,int y,int w,int h,int font,COLORREF c,UINT flags=DT_LEFT){RECT r{x,y,x+w,y+h};SelectObject(dc_,fonts_[font]);SetTextColor(dc_,menu_text_color(c));SetBkMode(dc_,TRANSPARENT);DrawTextW(dc_,s.data(),int(s.size()),&r,flags|DT_NOPREFIX);};
 menu_heading(dc_,fonts_[0],L"ROOM LIST");
 auto light=RGB(237,231,218);text(L"OpenMGO2",850,82,310,30,1,light,DT_RIGHT);text(L"ルーム一覧",120,160,1040,42,0,light);
 text(std::wstring(lobby_group_name(unsigned(lobby_group(roomLobby_.subtype))))+L"  /  "+roomLobby_.name,120,211,1040,35,1,RGB(255,208,150));
 auto n=roomReply_.rooms.size(),page=roomFocus_<n?roomFocus_/7:n?(n-1)/7:0;
 text(L"ルーム名",138,258,700,25,3,light);text(L"パスワード",800,258,160,25,3,light,DT_CENTER);text(L"人数",980,258,150,25,3,light,DT_RIGHT);
 for(size_t i=page*7;i<std::min(n,(page+1)*7);++i){auto&r=roomReply_.rooms[i];int y=285+int(i%7)*41;menu_row(dc_,120,y,1040,39,i,i==roomFocus_);text(r.name,138,y+8,650,30,1,light);text(r.password?L"あり":L"なし",800,y+10,160,25,3,light,DT_CENTER);text(std::to_wstring(r.players)+L" / "+std::to_wstring(r.capacity),980,y+8,150,30,2,light,DT_RIGHT);}
 if(roomReply_.status!=RoomStatus::ready){auto message=roomReply_.status==RoomStatus::connecting?L"ロビーへ接続し、一覧を取得しています…":roomReply_.status==RoomStatus::rejected?L"ロビーへの接続が受け付けられませんでした。":roomReply_.status==RoomStatus::protocol_error?L"サーバーの応答を確認できませんでした。":L"ロビーとの接続が切れました。通信設定を確認してください。";text(message,138,320,980,80,1,light,DT_WORDBREAK);
  if(roomReply_.status!=RoomStatus::connecting)text(L"ロビー一覧へ戻って選び直してください。  エラー "+std::to_wstring(roomReply_.error),138,420,980,60,2,light,DT_WORDBREAK);
 }else if(!n)text(roomLobby_.subtype==3||roomLobby_.subtype==4||roomLobby_.subtype==10?L"通常ルームはありません。大会・チームの受付画面は今後対応します。":L"現在、このロビーにルームはありません。",138,320,980,90,1,light,DT_WORDBREAK);
 text(std::to_wstring(n)+L" ルーム",138,585,450,25,3,light);text(std::to_wstring(page+1)+L" / "+std::to_wstring(std::max(size_t(1),(n+6)/7)),980,585,150,25,3,light,DT_RIGHT);
 text(roomNotice_.empty()?L"Enter：ルーム詳細を開く":roomNotice_,120,617,370,55,3,RGB(255,208,150),DT_WORDBREAK);
 for(int i=0;i<2;++i){int x=510+i*340;bool active=roomFocus_==n+i;fill(x,617,310,45,active?RGB(151,168,126):RGB(50,66,52));text(i?L"ロビー一覧へ戻る":L"一覧を更新 [F5]",x,628,310,30,1,active?RGB(30,20,12):light,DT_CENTER);}
 text(L"↑ ↓：項目    PageUp / PageDown：ページ    F5：更新    Esc：ロビー一覧",83,690,1120,25,3,light);
 if(roomFocus_<n)menu_focus_guides(dc_,120,285+int(roomFocus_%7)*41);else menu_focus_guides(dc_,roomFocus_==n?510:850,617);
}
void CharacterScreen::report_lobby_group()const{
 std::osyncstream(std::cout)<<"{\"lobby_group\":"<<lobbyGroup_<<",\"row_count\":"<<lobby_group_rows(selectionReply_.lobbies,lobbyGroup_).size()<<",\"group_count\":"<<lobby_group_count(selectionReply_.lobbies)<<"}"<<std::endl;
}
void CharacterScreen::set_lobby_group(unsigned group){
 if(group>=lobby_group_count(selectionReply_.lobbies)||group==lobbyGroup_)return;
 lobbyGroup_=group;lobbyFocus_=0;lobbyNotice_.clear();if(cues_.size()<32)cues_.push_back(menu_audio::Cursor);report_lobby_group();
}
namespace {
int lobby_list_top(unsigned group,size_t count){return std::min(219+int(group)*42,548-int(std::clamp(count,size_t(1),size_t(6)))*42);}
const wchar_t* lobby_description(unsigned group){
 static constexpr const wchar_t* descriptions[]={
 L"対戦相手を自動的に探すロビーです。",
 L"既に作成されたゲームを選んで参加できます。ホストはルールを設定してゲームを開催できます。",
 L"ゲームプレイの練習ができるロビーです。操作の練習や戦闘トレーニングを選択できます。",
 L"チームで連続した対戦に挑戦するロビーです。",
 L"トーナメントの対戦に参加するロビーです。",
 L"トーナメントへの参加を受け付けるロビーです。",
 L"所属するゲーム種別がまだ確認されていないロビーです。"};
 return descriptions[std::min(group,6u)];
}
}
bool CharacterScreen::lobby_message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 if(roomVisible_)return room_message(hwnd,msg,wp,lp);
 const size_t n=lobby_group_rows(selectionReply_.lobbies,lobbyGroup_).size();
 const unsigned groups=lobby_group_count(selectionReply_.lobbies);
 auto leave=[&]{lobbyVisible_=false;notice_=L"PCを選び直せます。";cues_.push_back(menu_audio::Cancel);};
 auto activate=[&]{if(lobbyCategories_){lobbyCategories_=false;lobbyFocus_=0;cues_.push_back(menu_audio::Confirm);}else if(lobbyFocus_==n)leave();else begin_rooms();};
 if(msg==WM_CHAR||msg==WM_KEYUP)return true;
 if(msg==WM_KEYDOWN){if(lp&(1LL<<30)){if(wp==VK_RETURN||wp==VK_SPACE||wp==VK_ESCAPE)return true;}
  auto previous=lobbyFocus_;bool oldColumn=lobbyCategories_;const auto before=cues_.size();
  if(wp==VK_ESCAPE)leave();
  else if(wp==VK_LEFT)lobbyCategories_=true;
  else if(wp==VK_RIGHT)lobbyCategories_=false;
  else if(wp==VK_TAB)lobbyCategories_=!lobbyCategories_;
  else if(wp==VK_RETURN||wp==VK_SPACE)activate();
  else if(lobbyCategories_){
   if(wp==VK_UP)set_lobby_group((lobbyGroup_+groups-1)%groups);
   else if(wp==VK_DOWN)set_lobby_group((lobbyGroup_+1)%groups);
   else if(wp==VK_HOME)set_lobby_group(0);else if(wp==VK_END)set_lobby_group(groups-1);
  }else{
   if(wp==VK_HOME)lobbyFocus_=0;else if(wp==VK_END)lobbyFocus_=n;
   else if(wp==VK_UP)lobbyFocus_=(lobbyFocus_+n)%(n+1);
   else if(wp==VK_DOWN)lobbyFocus_=(lobbyFocus_+1)%(n+1);
   else if(wp==VK_NEXT)lobbyFocus_=std::min(n,lobbyFocus_+6);else if(wp==VK_PRIOR)lobbyFocus_=lobbyFocus_>=6?lobbyFocus_-6:0;
  }
  if((previous!=lobbyFocus_||oldColumn!=lobbyCategories_)&&cues_.size()==before&&cues_.size()<32)cues_.push_back(menu_audio::Cursor);return true;
 }
 if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(!r.right||!r.bottom)return true;int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;
  int top=lobby_list_top(lobbyGroup_,n);
  if(x>=120&&x<635&&y>=219&&y<219+int(groups)*42){const bool oldColumn=lobbyCategories_;const auto before=cues_.size();lobbyCategories_=true;set_lobby_group(unsigned(y-219)/42);if(!oldColumn&&cues_.size()==before)cues_.push_back(menu_audio::Cursor);}
  else if(x>=640&&x<1160&&y>=top&&y<top+int(std::min(n,size_t(6)))*42){size_t page=lobbyFocus_<n?lobbyFocus_/6:n?(n-1)/6:0;size_t row=page*6+(y-top)/42;
   if(row<n){const bool moved=row!=lobbyFocus_||lobbyCategories_;lobbyFocus_=row;lobbyCategories_=false;if(x>=1110)begin_rooms();else if(moved&&cues_.size()<32)cues_.push_back(menu_audio::Cursor);}}
  else if(n>6&&y>=552&&y<585){const auto previous=lobbyFocus_;const bool oldColumn=lobbyCategories_;if(x>=955&&x<1000)lobbyFocus_=lobbyFocus_>=6?lobbyFocus_-6:0;else if(x>=1120&&x<1160)lobbyFocus_=std::min(n-1,(lobbyFocus_<n?lobbyFocus_/6+1:0)*6);lobbyCategories_=false;if(previous!=lobbyFocus_||oldColumn)cues_.push_back(menu_audio::Cursor);}
  else if(x>=830&&x<1160&&y>=111&&y<139)open_skills();
  else if(x>=850&&x<1160&&y>=617&&y<662)leave();SetFocus(hwnd);return true;
 }
 return false;
}
void CharacterScreen::draw_lobbies(){
 if(roomVisible_){draw_rooms();return;}
 auto text=[&](std::wstring_view s,int x,int y,int w,int h,int font,COLORREF c,UINT flags=DT_LEFT){RECT r{x,y,x+w,y+h};SelectObject(dc_,fonts_[font]);SetTextColor(dc_,menu_text_color(c));SetBkMode(dc_,TRANSPARENT);DrawTextW(dc_,s.data(),int(s.size()),&r,flags|DT_NOPREFIX);};
 menu_heading(dc_,fonts_[0],L"MAIN MENU");
 const auto&games=selectionReply_.lobbies;auto rows=lobby_group_rows(games,lobbyGroup_);size_t n=rows.size(),page=lobbyFocus_<n?lobbyFocus_/6:n?(n-1)/6:0;
 text(L"OpenMGO2",850,82,310,30,1,RGB(215,227,200),DT_RIGHT);
 text(selectionReply_.character.name,640,139,520,26,3,RGB(186,200,173),DT_RIGHT);
 text(L"スキル設定 [F6]",830,112,330,25,3,RGB(248,204,133),DT_RIGHT);
 text(L"GAME TYPE",138,145,470,25,3,RGB(187,215,214));
 menu_cursor(dc_,120,172,515,42);text(L"ロビー選択",140,178,470,35,0,RGB(240,244,232));
 text(L"LOBBY",660,177,340,25,3,RGB(187,215,214));text(L"PC",1050,177,85,25,3,RGB(187,215,214),DT_RIGHT);
 auto groups=lobby_group_count(games);int selectedY=219+int(lobbyGroup_)*42;
 menu_rect(dc_,112,172,7,42+int(groups)*42,RGB(255,178,88));
 for(unsigned g=0;g<groups;++g){int y=219+int(g)*42;bool active=g==lobbyGroup_;
  menu_row(dc_,120,y,515,40,g,active);text(lobby_group_name(g),140,y+7,480,30,1,active?RGB(249,245,230):RGB(190,199,190));}
 int top=lobby_list_top(lobbyGroup_,n);
 for(size_t i=page*6;i<std::min(n,(page+1)*6);++i){const auto&game=games[rows[i]];int y=top+int(i%6)*42;bool active=!lobbyCategories_&&i==lobbyFocus_;
  menu_row(dc_,640,y,520,40,i,active);if(active)menu_rect(dc_,640,y,7,40,RGB(255,178,88));
  text(game.name,660,y+8,340,30,1,RGB(233,240,220),DT_SINGLELINE|DT_END_ELLIPSIS);
  text(std::to_wstring(game.players),1010,y+9,90,28,2,RGB(233,240,220),DT_RIGHT);
  if(active)text(L"→",1110,y+5,42,32,1,RGB(238,242,232),DT_CENTER);
 }
 if(!n)text(L"現在ロビーがありません。",660,top+9,480,70,1,RGB(237,221,181),DT_WORDBREAK);
 text(std::to_wstring(n)+L" ロビー",660,560,280,25,3,RGB(186,200,173));
 text(std::to_wstring(page+1)+L" / "+std::to_wstring(std::max(size_t(1),(n+5)/6)),1000,560,110,25,3,RGB(186,200,173),DT_CENTER);
 if(n>6){text(L"◀",955,558,45,28,2,RGB(233,240,220),DT_CENTER);text(L"▶",1120,558,40,28,2,RGB(233,240,220),DT_CENTER);}
 menu_description(dc_,fonts_[3],120,586,1040);
 text(lobbyNotice_.empty()?lobby_description(lobbyGroup_):lobbyNotice_,120,617,700,62,2,RGB(237,238,225),DT_WORDBREAK);
 bool active=!lobbyCategories_&&lobbyFocus_==n;menu_row(dc_,850,617,310,45,0,active);text(L"PC一覧へ戻る",850,627,310,32,1,RGB(233,239,222),DT_CENTER);
 text(L"← → / Tab：列   ↑ ↓：選択   Enter / 右端の矢印：入場   PgUp / PgDn：ページ   Esc：戻る",83,690,1120,25,3,RGB(174,185,165));
 if(lobbyCategories_)menu_focus_guides(dc_,120,selectedY);
 else if(lobbyFocus_<n)menu_focus_guides(dc_,640,top+int(lobbyFocus_%6)*42);
 else menu_focus_guides(dc_,850,617);
}
void CharacterScreen::tick_hold(){if(!pending_&&!lobbyVisible_&&slots_.tick(clock_())){++deleteDialogs_;cues_.push_back(menu_audio::Confirm);}}
void CharacterScreen::confirm_delete(){const bool accepted=slots_.confirm();if(accepted){++deleteYes_;notice_=L"PC削除の通信は準備中です。キャラクターは削除していません。";}cues_.push_back(accepted?menu_audio::Confirm:menu_audio::Cancel);}
void CharacterScreen::focus(int i,bool audible){if(i!=focus_){slots_.reset_hold();focus_=i;if(i<int(slots_.count())){slots_.select(unsigned(i));notice_.clear();}if(audible&&cues_.size()<32)cues_.push_back(menu_audio::Cursor);}}
void CharacterScreen::activate(){int n=int(slots_.count());if(focus_<n||focus_==n){if(n){if(slots_.occupied()){begin_selection();if(pending_)cues_.push_back(menu_audio::Confirm);}else if(slots_.purchase_required())notice_=L"追加のPCスロット購入は準備中です。購入方法・価格は後日ご案内します。";else if(slots_.can_create()){slots_.reset_hold();creation_=std::make_unique<CharacterCreation>(catalog_,bool(createTransport_)&&!registrationState_->unresolved);creation_->unicode_names(reply_.list.unicode_names);creation_->require_skills(bool(skillCatalog_));creationSkills_={};creationSkillContinue_=false;notice_.clear();cues_.push_back(menu_audio::Confirm);}}}else if(focus_==n+1)start();else if(focus_==n+2){stop();back_=true;cues_.push_back(menu_audio::Cancel);}}
bool CharacterScreen::message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 const auto previousContext=input_context();update();
 // A worker can complete after the outer menu gate sampled this screen.
 // The old screen's decision must not activate the newly arrived screen.
 if(previousContext!=input_context()&&(msg==WM_KEYDOWN||msg==WM_LBUTTONUP||msg==WM_MOUSEWHEEL))return true;
 if(!lobbyVisible_&&!creation_&&!skill_visible()&&(msg==WM_KEYDOWN||msg==WM_LBUTTONDOWN||msg==WM_MOUSEWHEEL))selectionPresentation_.activity(clock_());
 if(selecting_&&selectionPresentation_.selecting(clock_()))return true;
 if(skill_visible())return skill_message(hwnd,msg,wp,lp);
 if(msg==WM_KEYDOWN&&wp==VK_F6&&(creation_||lobbyVisible_)){if(!(lp&(1LL<<30)))open_skills();return true;}
 if(lobbyVisible_)return lobby_message(hwnd,msg,wp,lp);
 if(creation_){if(creation_->registration_busy())return true;if(msg==WM_KEYDOWN&&(wp==VK_PRIOR||wp==VK_NEXT)&&!creation_->confirming()&&!creation_->discarding()){modelYaw_+=(wp==VK_PRIOR?-.12f:.12f);if(modelYaw_>3.141593f)modelYaw_-=6.283186f;if(modelYaw_< -3.141593f)modelYaw_+=6.283186f;return true;}bool handled=creation_->message(hwnd,msg,wp,lp);if(creation_->take_skill_request()){creationSkillContinue_=true;open_skills();}if(auto request=creation_->take_registration())begin_registration(std::move(*request));for(auto c:creation_->cues())if(cues_.size()<32)cues_.push_back(c);if(creation_->closed()){creation_->report();creation_.reset();}return handled;}
 if(msg==WM_KILLFOCUS||(msg==WM_ACTIVATEAPP&&!wp)){slots_.reset_hold();slots_.cancel_dialog();return false;}
 if(msg==WM_KEYUP&&wp==VK_BACK){slots_.release();return true;}
 if(msg==WM_CHAR)return true;
 if(msg==WM_KEYDOWN){
  if((lp&(1LL<<30))&&(wp==VK_ESCAPE||wp==VK_RETURN||wp==VK_SPACE))return true;
  if(wp==VK_BACK){if(!pending_&&focus_<=int(slots_.count())&&!(lp&(1LL<<25)))slots_.press(clock_(),(lp&(1LL<<30))!=0);return true;}
  // Navigation/confirmation cancels a partial hold; key repeats cannot rearm it.
  slots_.reset_hold();
  if(slots_.dialog()){if(wp==VK_ESCAPE){slots_.cancel_dialog();cues_.push_back(menu_audio::Cancel);}else if(wp==VK_LEFT||wp==VK_RIGHT||wp==VK_TAB){const bool next=wp==VK_TAB?!slots_.yes():wp==VK_LEFT;if(next!=slots_.yes()){slots_.choose(next);cues_.push_back(menu_audio::Cursor);}}else if((wp==VK_RETURN||wp==VK_SPACE)&&!(lp&(1LL<<30)))confirm_delete();return true;}
  if(modelAvailable_&&slots_.occupied()&&(wp==VK_LEFT||wp==VK_RIGHT)){modelYaw_+=(wp==VK_LEFT?-.12f:.12f);if(modelYaw_>3.141593f)modelYaw_-=6.283186f;if(modelYaw_< -3.141593f)modelYaw_+=6.283186f;return true;}
  if(wp==VK_ESCAPE){stop();back_=true;cues_.push_back(menu_audio::Cancel);}else if(!pending_){int first=slots_.count()?0:1,last=int(slots_.count())+2,n=last-first+1;if(wp==VK_DOWN||wp==VK_TAB){bool prev=wp==VK_TAB&&(GetKeyState(VK_SHIFT)&0x8000);focus(first+(focus_-first+(prev?n-1:1))%n);}else if(wp==VK_UP)focus(first+(focus_-first+n-1)%n);else if(wp==VK_HOME)focus(first);else if(wp==VK_END)focus(last);else if((wp==VK_RETURN||wp==VK_SPACE)&&!(lp&(1LL<<30)))activate();}return true;
 }
 if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(!r.right||!r.bottom)return true;int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;slots_.reset_hold();
  if(slots_.dialog()){if(y>=395&&y<449){if(x>=390&&x<610){slots_.choose(true);confirm_delete();}else if(x>=670&&x<890){slots_.choose(false);confirm_delete();}}return true;}
  if(y>=617&&y<662&&x>=850&&x<1160){stop();back_=true;cues_.push_back(menu_audio::Cancel);}else if(!pending_){int n=int(slots_.count());if(y>=225&&y<225+n*39&&x>=120&&x<690)focus((y-225)/39);else if(y>=617&&y<662){if(x>=120&&x<480){focus(n,false);activate();}else if(x>=510&&x<820){focus(n+1,false);activate();}}}SetFocus(hwnd);return true;
 }return false;
}
const void* CharacterScreen::draw(){update();tick_hold();drawClanImage_=false;if(skill_visible())return skillMenu_->draw();memset(pixels_,0,1280*720*4);
 if(lobbyVisible_){draw_lobbies();finish_menu_surface(pixels_);if(weaponsVisible_)draw_weapon_icons();else if(detailVisible_&&detailReply_.join_status==RoomJoinStatus::joined&&!matchVisible_)draw_briefing_icons();if(drawClanImage_&&clanBitmap_)hud::paint_clan_image({static_cast<uint32_t*>(pixels_),1280*720},1280,720,clanBitmap_->get());return pixels_;}
 if(creation_){creation_->draw(dc_,fonts_);finish_menu_surface(pixels_);return pixels_;}
 auto fill=[&](int x,int y,int w,int h,COLORREF c){menu_fill(dc_,x,y,w,h,c);};
 auto text=[&](std::wstring_view s,int x,int y,int w,int h,int font,COLORREF c,UINT flags=DT_LEFT){RECT r{x,y,x+w,y+h};SelectObject(dc_,fonts_[font]);SetTextColor(dc_,menu_text_color(c));SetBkMode(dc_,TRANSPARENT);DrawTextW(dc_,s.data(),int(s.size()),&r,flags|DT_NOPREFIX);};
 menu_heading(dc_,fonts_[0],L"PC SELECT");menu_character_frame(dc_,fonts_[3]);
 text(L"OpenMGO2",850,82,310,30,1,RGB(215,227,200),DT_RIGHT);text(L"プレイヤースロット",120,160,1000,42,0,RGB(229,238,214));
 text(L"4キャラまで無料 ／ 5キャラ目以降はPCスロットを購入",120,202,1040,22,3,RGB(192,204,178));
 // Worker owns reply_ until done/join. Never read it while pending.
 int n=pending_&&!selection_preview_active()?0:int(slots_.count());
 if((!pending_||selection_preview_active())&&reply_.status==CharacterStatus::success){for(int i=0;i<n;++i){int y=225+39*i;bool occupied=i<int(reply_.list.entries.size());menu_row(dc_,120,y,570,37,i,i==focus_);text(std::to_wstring(i+1),130,y+8,35,28,2,RGB(192,211,173));text(occupied?reply_.list.entries[i].name:i>=int(slots_.capacity())?L"PCスロットを購入":i<4?L"PC新規登録（無料）":L"PC新規登録（追加枠）",180,y+7,420,31,1,RGB(233,240,220));if(occupied&&reply_.list.entries[i].main)text(L"MAIN",604,y+9,76,27,3,RGB(224,213,164),DT_RIGHT);}if(!n)text(L"利用可能なプレイヤースロットはありません。",120,245,1040,42,1,RGB(228,235,213));
  // Occupancy gates the selected account appearance.
  if(slots_.occupied())text(modelAvailable_?(modelPartial_?L"外見表示（一部の装備・色は未対応）":L"キャラクター表示"):L"3Dモデル未読込",720,196,440,24,3,RGB(186,200,173),DT_CENTER);
  menu_description(dc_,fonts_[3],120,545,570);
  text(notice_.empty()?(slots_.occupied()?L"このPCでゲームを開始します。":slots_.purchase_required()?L"5キャラ目以降はPCスロットを購入します。":L"新しいプレイヤーキャラクターを作成します。"):notice_,120,575,570,38,3,RGB(237,221,181),DT_WORDBREAK);
  if(n){bool active=focus_==n;fill(120,617,360,45,active?RGB(151,168,126):RGB(50,66,52));text(slots_.action(),120,627,360,32,1,active?RGB(18,28,19):RGB(228,234,215),DT_CENTER);}
 }
 else{std::wstring message=selecting_?L"OpenMGO2でPCを選択しています…":L"OpenMGO2からキャラクター一覧を取得しています…";if(!pending_){message=reply_.status==CharacterStatus::server_error?L"サーバーに受け付けられませんでした。ログインからやり直してください。":reply_.status==CharacterStatus::protocol_error?L"サーバーの応答を確認できませんでした。":L"OpenMGO2に接続できませんでした。通信設定を確認してください。";const wchar_t*stages[]={L"接続データ",L"入口への接続",L"接続先の取得",L"アカウントへの接続",L"セッション確認",L"一覧取得"};text(std::wstring(L"確認箇所：")+stages[unsigned(reply_.stage)]+L"  /  エラー "+std::to_wstring(reply_.error),120,350,1040,55,2,RGB(192,204,178));}text(message,120,249,1040,85,1,RGB(237,221,181),DT_WORDBREAK);}
 for(int i=0;i<2;++i){int x=510+i*340;bool active=!pending_&&focus_==n+1+i;fill(x,617,310,45,active?RGB(151,168,126):RGB(50,66,52));text(i?L"設定へ戻る":pending_?L"取得中…":L"再取得",x,627,310,32,1,active?RGB(18,28,19):RGB(228,234,215),DT_CENTER);}
 if(!pending_&&slots_.occupied()){text(L"Backspace 3秒長押し：PC削除の確認",720,582,440,32,3,RGB(212,220,195),DT_CENTER);auto elapsed=slots_.held_ms(clock_());if(elapsed){fill(750,611,380,5,RGB(52,67,47));fill(750,611,int(380*elapsed/3000),5,RGB(222,231,202));}}
 if(slots_.dialog()){fill(300,252,680,235,RGB(135,156,113));fill(303,255,674,229,RGB(25,37,28));menu_section(dc_,fonts_[3],L"CONFIRM",303,255,674);text(L"PCを削除しますか？",330,286,620,43,0,RGB(237,241,226),DT_CENTER);text(reply_.list.entries[slots_.selected()].name,330,339,620,36,1,RGB(237,221,181),DT_CENTER);for(int i=0;i<2;++i){int x=i?670:390;bool active=slots_.yes()==!i;fill(x,395,220,54,active?RGB(151,168,126):RGB(50,66,52));text(i?L"NO":L"YES",x,409,220,34,1,active?RGB(18,28,19):RGB(228,234,215),DT_CENTER);}}
 text(slots_.dialog()?L"← →：YES / NO    Enter：決定    Esc：取り消し":L"↑ ↓：項目移動    ← →：モデル回転    Enter：決定    Esc：設定へ戻る",83,690,1120,25,3,RGB(174,185,165));
 if(slots_.dialog())menu_focus_guides(dc_,slots_.yes()?390:670,395);
 else if(!pending_){if(focus_<n)menu_focus_guides(dc_,120,225+focus_*39);else menu_focus_guides(dc_,focus_==n?120:focus_==n+1?510:850,617);}
 finish_menu_surface(pixels_);return pixels_;
}
void CharacterScreen::report()const{if(pending_)return;std::osyncstream(std::cout)<<"{\"character_list_result\":true,\"status\":"<<int(reply_.status)<<",\"stage\":"<<int(reply_.stage)<<",\"error\":"<<reply_.error<<",\"count\":"<<reply_.list.entries.size()<<",\"slots\":"<<slots_.capacity()<<",\"display_rows\":"<<slots_.count()<<",\"server_slots\":"<<reply_.list.slots<<",\"purchase_required\":"<<(slots_.purchase_required()?"true":"false")<<",\"occupied_slot\":"<<(slots_.occupied()?"true":"false")<<",\"delete_dialogs\":"<<deleteDialogs_<<",\"delete_yes\":"<<deleteYes_<<",\"requests\":"<<requests_<<",\"selection_sent\":"<<(selectionReply_.request_may_have_been_sent?"true":"false")<<",\"deletion_sent\":false,\"model_rendered\":"<<(modelRendered_?"true":"false")<<"}"<<std::endl;}
}
