#pragma once
#include "stage_assets.h"
#include "multi_ui_state.h"
#include <windows.h>
#include "character_client.h"
#include "lobby_groups.h"
#include "character_slots.h"
#include "character_creation.h"
#include "weapon_selection.h"
#include "mounted_weapons.h"
#include "weapon_icons.h"
#include "game_hud.h"
#include "skill_menu.h"
#include "selection_presentation.h"
#include "briefing_ui.h"
#include "briefing_map.h"
#include <map>
#include <memory>
#include <thread>
#include <optional>
#include <functional>
#include <mutex>
#include <deque>
namespace mgo2mt {
// Shared across screen navigation. An ambiguous send stays locked until a
// refreshed list confirms the requested unique name (or the returned ID).
struct CharacterRegistrationState {bool unresolved=false;uint32_t expected_id=0;std::wstring expected_name;std::optional<skills::Loadout> skills;};
struct CharacterSelectionState {bool unresolved=false;};
class CharacterScreen {
 HDC dc_=nullptr;HBITMAP bitmap_=nullptr;HGDIOBJ old_=nullptr;void*pixels_=nullptr;std::vector<HFONT>fonts_;
 std::function<CharacterReply(const std::atomic_bool&)>transport_;std::thread worker_;std::atomic_bool cancel_{false},done_{false};
 CharacterReply reply_;bool pending_=false,back_=false;int focus_=0;ULONGLONG retryAt_=0;unsigned requests_=0;std::vector<unsigned>cues_;
 CharacterSlots slots_;std::wstring notice_;unsigned deleteDialogs_=0,deleteYes_=0;
 const CharacterCatalog* catalog_=nullptr;std::unique_ptr<CharacterCreation> creation_;
 std::function<CharacterCreateReply(const CharacterCreateRequest&,const std::atomic_bool&)> createTransport_;
 std::shared_ptr<CharacterRegistrationState> registrationState_=std::make_shared<CharacterRegistrationState>();
 CharacterCreateReply createReply_;bool registering_=false;std::wstring registrationNotice_;
 std::function<CharacterSelectionReply(uint32_t,const std::atomic_bool&)> selectTransport_;
 std::shared_ptr<CharacterSelectionState> selectionState_=std::make_shared<CharacterSelectionState>();
 CharacterSelectionReply selectionReply_;bool selecting_=false,lobbyVisible_=false;uint32_t selectionTarget_=0;size_t lobbyFocus_=0;std::wstring lobbyNotice_;
 bool lobbyCategories_=false;unsigned lobbyGroup_=0;void set_lobby_group(unsigned);void report_lobby_group()const;
 RoomTransport roomTransport_;std::thread roomWorker_;std::atomic_bool roomCancel_{false},roomRefresh_{false};std::mutex roomMutex_;std::deque<RoomReply> roomInbox_;
 std::vector<combat::Event> combatEvents_;
 uint32_t combatCommandSequence_=0,loadoutPending_=0;std::optional<bool> combatLoaded_;bool combatEntered_=false;
 bool round_command(combat::wire::Command);void round_ready();void round_team();
 hud::Intro roundIntro_;unsigned briefingFocus_=0;bool gameplayOptionsRequest_=false;
 briefing::Panel briefingPanel_=briefing::Panel::none;bool briefingYes_=false;unsigned hostVoteFocus_=0;
 bool briefing_message(HWND,UINT,WPARAM,LPARAM);void draw_briefing();void toggle_gameplay_briefing();void draw_room_loading();
 std::unique_ptr<clan::Bitmap> clanBitmap_;uint64_t clanSerial_=~uint64_t(0);bool drawClanImage_=false;
 void update_round();std::wstring round_notice()const;bool supported_weapon(uint16_t)const;
 RoomRequests roomRequests_;bool detailVisible_=false,detailBusy_=false,matchVisible_=false;stage::Status stageStatus_=stage::Status::idle;unsigned detailFocus_=1;RoomReply detailReply_;RoomAction detailAction_;std::wstring detailNotice_;
 void open_room_detail();void request_room_join();bool detail_message(HWND,UINT,WPARAM,LPARAM);void draw_room_detail();void draw_room_match();
 std::shared_ptr<const weapons::Catalog> weaponCatalog_;
 mounted::Registry mountedCatalog_;
 weapons::Icons weaponIcons_,briefingIcons_;
 briefing::Map briefingMap_;bool briefingMapReady_=false;
 void draw_briefing_icons();
 std::shared_ptr<const skills::Catalog> skillCatalog_;std::unique_ptr<SkillMenu> skillMenu_;
 std::filesystem::path skillIcons_;skills::Loadout creationSkills_;bool creationSkillContinue_=false;
 struct SkillDraft {skills::Loadout value;bool queued=true,sending=false;};
 std::map<uint32_t,SkillDraft> skillDrafts_;uint64_t skillSerial_=~uint64_t(0);std::wstring skillNotice_;
 bool skills_editable()const;void update_skills();bool skill_message(HWND,UINT,WPARAM,LPARAM);
 std::unique_ptr<weapons::Selection> weaponSelection_;std::optional<host::LoadRequest> weaponRequest_;
 bool weaponsVisible_=false;unsigned weaponCategory_=0,weaponSection_=0;size_t weaponFocus_=0;
 bool weaponMusicRequest_=false,weaponMusicReady_=false;std::wstring weaponMusicTitle_;
 std::wstring weaponNotice_;void open_weapons();void update_weapons();
 bool weapon_message(HWND,UINT,WPARAM,LPARAM);void draw_room_weapons();void draw_weapon_icons();
 RoomReply roomReply_;GameLobbyEntry roomLobby_;bool roomVisible_=false;size_t roomFocus_=0;ULONGLONG roomRefreshAt_=0;std::wstring roomNotice_;
 void begin_rooms();void stop_rooms();void update_rooms();bool room_message(HWND,UINT,WPARAM,LPARAM);void draw_rooms();
 void begin_selection();bool lobby_message(HWND,UINT,WPARAM,LPARAM);void draw_lobbies();
 void begin_registration(CharacterCreateRequest);
 SelectionPresentation selectionPresentation_;
 bool selection_preview_active()const{return selecting_&&selectionPresentation_.selecting(clock_());}
 bool modelAvailable_=false,modelRendered_=false,modelPartial_=false;float modelYaw_=0.15f;
 std::function<uint64_t()> clock_;void tick_hold();void confirm_delete();
 void start();void update();void stop();void activate();void focus(int,bool audible=true);
public:
 multi_ui::Context ui_presentation()const {
  using R=multi_ui::Route;
  R route=R::characters;
  if(creation_)route=R::creation;
  if(lobbyVisible_)route=lobbyCategories_?R::lobbyGroups:R::lobbies;
  if(roomVisible_)route=R::rooms;
  if(detailVisible_)route=R::roomDetail;
  const bool joined=detailVisible_&&detailReply_.join_status==RoomJoinStatus::joined;
  if(joined)route=weaponsVisible_?R::loadout:matchVisible_?R::gameplay:R::briefing;
  if(joined&&detailReply_.preparation&&detailReply_.preparation->phase==combat::wire::RoundPhase::ended)route=R::result;
  auto c=multi_ui::presentation(route,pending_||selecting_||registering_||detailBusy_||room_loading(),
     slots_.dialog()||skill_visible()||briefingPanel_!=briefing::Panel::none);
  const auto&p=detailReply_.preparation;
  if(joined&&p){
   if(p->respawnWaiting)c.flags.insert("respawn_waiting");
   c.bindings["remaining_ms"]=p->roundClock?std::to_string(p->roundRemainingMs):"";
   c.bindings["respawn_ms"]=std::to_string(p->respawnRemainingMs);
   const auto&s=detailReply_.combat_state;
   if(s&&s->epoch==p->epoch&&p->self.slot<s->players.size())if(const auto&player=s->players[p->self.slot];player&&player->identity==p->self){
    c.bindings["hp"]=std::to_string(player->hp);c.bindings["max_hp"]=std::to_string(player->maxHp);
    c.bindings["ammo"]=std::to_string(player->ammo);c.bindings["reserve"]=std::to_string(player->reserve);
    c.bindings["weapon_id"]=std::to_string(player->weapon);
    c.flags.insert(player->alive?"alive":"dead");
    if(player->aiming)c.flags.insert("aiming");
    if(player->reloadUntil)c.flags.insert("reloading");
    if(route==R::gameplay)c.state=!player->alive?"dead":player->reloadUntil?"reloading":"active";
   }
  }
  return c;
 }
 uint64_t input_context()const{
  return uint64_t(pending_)|(uint64_t(selecting_)<<1)|(uint64_t(lobbyVisible_)<<2)|(uint64_t(lobbyCategories_)<<3)|
   (uint64_t(roomVisible_)<<4)|(uint64_t(detailVisible_)<<5)|(uint64_t(detailBusy_)<<6)|(uint64_t(matchVisible_)<<7)|
   (uint64_t(weaponsVisible_)<<8)|(uint64_t(skill_visible())<<9)|(uint64_t(bool(creation_))<<10)|
   (uint64_t(slots_.dialog())<<11)|(uint64_t(briefingPanel_)<<12)|(uint64_t(room_loading())<<20)|
   (uint64_t(creation_&&creation_->confirming())<<21)|(uint64_t(creation_&&creation_->discarding())<<22);
 }
 explicit CharacterScreen(std::function<CharacterReply(const std::atomic_bool&)>,std::function<uint64_t()> clock=[] {return GetTickCount64();});~CharacterScreen();
 LobbyMonitorState lobby_monitor()const{return roomRequests_.lobbyMonitor->state();}
 std::shared_ptr<LobbyMonitor> lobby_monitor_handle()const{return roomRequests_.lobbyMonitor;}
 std::shared_ptr<notices::Session> notification_session()const{return roomRequests_.notifications;}
 uint32_t preview_character_id()const{return (!pending_||selection_preview_active())&&!lobbyVisible_&&reply_.status==CharacterStatus::success?slots_.preview_id():0;}
 std::optional<std::array<uint8_t,28>> preview_appearance()const {if(creation_)return creation_->appearance();if(preview_character_id())return reply_.list.entries[slots_.selected()].appearance;return {};}
 bool preview_visible()const{return bool(creation_)||preview_character_id()!=0;}
 bool creation_visible()const{return bool(creation_);}
 bool busy()const{return pending_;}
 bool lobby_visible()const{return lobbyVisible_;}
 bool room_visible()const{return roomVisible_;}
 RoomStatus room_status()const{return roomReply_.status;}
 size_t room_count()const{return roomReply_.rooms.size();}
 void rooms(RoomTransport transport){roomTransport_=std::move(transport);}
 void room_guard(std::shared_ptr<std::atomic_bool> state){roomRequests_.uncertain=std::move(state);}
 bool room_detail_visible()const{return detailVisible_;}bool room_detail_busy()const{return detailBusy_;}
 RoomJoinStatus room_join_status()const{return detailReply_.join_status;}
 std::shared_ptr<chat::Session> chat_session()const{return roomRequests_.chatSession;}
 std::shared_ptr<invitations::Session> invitation_session()const{return roomRequests_.invitationSession;}
 std::shared_ptr<items::ClientSession> inventory_session()const{return roomRequests_.inventorySession;}
 std::shared_ptr<radio::Session> radio_session()const{return roomRequests_.radioSession;}
 std::optional<host::LoadRequest> stage_load_request()const {return detailVisible_&&detailReply_.join_status==RoomJoinStatus::joined&&detailReply_.host_match?detailReply_.host_match->request:std::nullopt;}
 // A local briefing overlay never tears down the admitted world or its clock.
 std::optional<host::LoadRequest> stage_request()const {return matchVisible_||combatEntered_?stage_load_request():std::nullopt;}
 bool room_loading()const;
 const std::optional<host::Placements>& stage_placements()const{return detailReply_.host_placements;}
 const std::optional<stage::SceneSnapshot>& stage_scene()const{return detailReply_.host_scene;}
 std::optional<combat::wire::Offer> combat_offer()const{return detailReply_.combat_offer;}
 const std::optional<combat::wire::Preparation>& combat_preparation()const{return detailReply_.preparation;}
 std::optional<combat::Snapshot> combat_state()const{return detailReply_.combat_state;}
 combat::SopView combat_sop()const{return detailReply_.combat_sop;}
 std::optional<combat::wire::DebugFlights> debug_flights()const{return detailReply_.debug_flights;}
 std::optional<combat::wire::Environment> environment_settings()const{return detailReply_.environment;}
 bool room_enemy_name_tags()const{return detailReply_.detail&&detailReply_.detail->environment_known&&detailReply_.detail->enemy_nametags;}
 bool room_auto_aim()const{return detailReply_.detail&&detailReply_.detail->environment_known&&detailReply_.detail->auto_aim;}
 clan::State enemy_clan_emblem(uint32_t id){roomRequests_.enemyClanEmblem->want(id);return roomRequests_.enemyClanEmblem->state();}
 combat::wire::Status combat_status()const{return detailReply_.combat_status;}
 std::vector<combat::Event> combat_events(){auto result=std::move(combatEvents_);combatEvents_.clear();return result;}
 void combat_input(const combat::wire::Input&i){if(stage_load_request())roomRequests_.combat_input(i);}
 stage::SceneSyncStatus scene_status()const{return detailReply_.scene_status;}
 void stage_feedback(const stage::Result&);
 void stage_feedback(stage::Status s){stage::Result r;r.status=s;stage_feedback(r);}
 std::wstring stageAudioNotice_;
 std::wstring stageDebugNotice_;
 bool stageInspection_=false;
 void stage_navigation_feedback(bool active){stageInspection_=active;}
 bool stageResetConfirm_=false,stageResetYes_=false;
 void stage_reset_feedback(bool open,bool yes){stageResetConfirm_=open;stageResetYes_=yes;}
 void stage_debug_feedback(std::wstring s){stageDebugNotice_=std::move(s);}
 void stage_audio_feedback(std::wstring s){stageAudioNotice_=std::move(s);}
 bool room_match_visible()const{return matchVisible_;}
 bool take_gameplay_options_request(){bool result=gameplayOptionsRequest_;gameplayOptionsRequest_=false;return result;}
 void skill_catalog(const std::filesystem::path&);
 void skill_icons(const std::filesystem::path& path){skillIcons_=path;if(skillMenu_)skillMenu_->icons(path);}
 void briefing_icons(const std::filesystem::path& path){std::string error;briefingIcons_.load(path,error);}
 void briefing_map(const std::filesystem::path& path){std::string error;briefingMapReady_=briefingMap_.load(path,error);}
 unsigned briefing_focus()const{return briefingFocus_;}
 briefing::Panel briefing_panel()const{return briefingPanel_;}
 bool briefing_confirm_yes()const{return briefingYes_;}
 unsigned briefing_host_choice()const{return hostVoteFocus_;}
 void open_skills();
  bool skill_visible()const{return skillMenu_&&skillMenu_->visible();}
  const std::wstring& skill_notice()const{return skillNotice_;}
 skills::Loadout active_skills()const;std::vector<std::wstring> skill_hud_labels()const;
 void weapon_catalog(const std::filesystem::path&);
 void weapon_icons(const std::filesystem::path& path){std::string error;weaponIcons_.load(path,error);}
 bool weapon_visible()const{return weaponsVisible_;}
 bool weapon_music_available()const{const auto&p=detailReply_.preparation;return weaponsVisible_&&stage_load_request().has_value()&&!loadoutPending_&&p&&p->self.slot<p->players.size()&&p->players[p->self.slot]&&!p->players[p->self.slot]->deployed;}
 bool take_weapon_music_request(){const bool requested=weaponMusicRequest_;weaponMusicRequest_=false;return requested&&weapon_music_available();}
 void weapon_music_feedback(std::wstring title,bool ready){weaponMusicTitle_=std::move(title);weaponMusicReady_=ready;}
 const weapons::Selection* weapon_draft()const{return weaponSelection_.get();}
 const std::optional<host::MatchState>& room_host_match()const{return detailReply_.host_match;}
 // Return a display copy. The short original HOST record stays untouched.
 std::optional<host::Roster> room_host_roster()const{auto roster=detailReply_.host_roster;if(roster)for(auto&p:roster->slots)if(p)p->name=roomRequests_.nameDirectory->display(p->character,p->name);return roster;}
 std::wstring player_display_name(uint32_t,std::wstring_view fallback)const;
 unsigned current_lobby_group()const{return lobbyGroup_;}
 uint16_t focused_lobby_id()const{auto rows=lobby_group_rows(selectionReply_.lobbies,lobbyGroup_);return lobbyVisible_&&lobbyFocus_<rows.size()?selectionReply_.lobbies[rows[lobbyFocus_]].id:0;}
 uint32_t selected_character_id()const{return lobbyVisible_?selectionReply_.character.id:0;}
 std::optional<std::array<uint8_t,28>> stage_appearance()const{return stage_load_request()&&selectionReply_.character.id?std::optional(selectionReply_.character.appearance):std::nullopt;}
 bool creation_text_entry()const{return !skill_visible()&&((creation_&&creation_->text_entry())||(detailVisible_&&detailFocus_==0));}
 std::optional<CharacterVoicePreview> take_audition(){return creation_?creation_->take_audition():std::nullopt;}
 void catalog(const CharacterCatalog*c){catalog_=c;}
 void registration(std::function<CharacterCreateReply(const CharacterCreateRequest&,const std::atomic_bool&)> fn,std::shared_ptr<CharacterRegistrationState> state){createTransport_=std::move(fn);registrationState_=std::move(state);}
 void selection(std::function<CharacterSelectionReply(uint32_t,const std::atomic_bool&)> fn,std::shared_ptr<CharacterSelectionState> state){selectTransport_=std::move(fn);selectionState_=std::move(state);}
 void model_partial(bool v){modelPartial_=v;}
 void model_available(bool v){modelAvailable_=v;}void model_rendered(){modelRendered_=true;}
 void selection_presentation(uint32_t saluteMs,bool magazine,bool box,uint32_t soundDelayMs=0){selectionPresentation_.configure(saluteMs,magazine,box,soundDelayMs);}
 SelectionPresentation::Frame selection_frame(){return selectionPresentation_.frame(clock_());}
 bool take_selection_sound(){return selectionPresentation_.take_sound(clock_());}
 float model_yaw()const{return modelYaw_;}
 bool message(HWND,UINT,WPARAM,LPARAM);const void* draw();bool back()const{return back_;}
 std::vector<unsigned> cues(){auto c=std::move(cues_);cues_.clear();return c;}void report()const;
};
}
