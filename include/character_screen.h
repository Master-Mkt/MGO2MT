#pragma once
#include "stage_assets.h"
#include <windows.h>
#include "character_client.h"
#include "lobby_groups.h"
#include "character_slots.h"
#include "character_creation.h"
#include "weapon_selection.h"
#include <memory>
#include <thread>
#include <optional>
#include <functional>
#include <mutex>
#include <deque>
namespace mgo2win {
// Shared across screen navigation. An ambiguous send stays locked until a
// refreshed list confirms the requested unique name (or the returned ID).
struct CharacterRegistrationState {bool unresolved=false;uint32_t expected_id=0;std::wstring expected_name;};
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
 RoomRequests roomRequests_;bool detailVisible_=false,detailBusy_=false,matchVisible_=false;stage::Status stageStatus_=stage::Status::idle;unsigned detailFocus_=1;RoomReply detailReply_;RoomAction detailAction_;std::wstring detailNotice_;
 void open_room_detail();void request_room_join();bool detail_message(HWND,UINT,WPARAM,LPARAM);void draw_room_detail();void draw_room_match();
 std::shared_ptr<const weapons::Catalog> weaponCatalog_;
 std::unique_ptr<weapons::Selection> weaponSelection_;std::optional<host::LoadRequest> weaponRequest_;
 bool weaponsVisible_=false;unsigned weaponCategory_=0;size_t weaponFocus_=0;
 std::wstring weaponNotice_;void open_weapons();void update_weapons();
 bool weapon_message(HWND,UINT,WPARAM,LPARAM);void draw_room_weapons();
 RoomReply roomReply_;GameLobbyEntry roomLobby_;bool roomVisible_=false;size_t roomFocus_=0;ULONGLONG roomRefreshAt_=0;std::wstring roomNotice_;
 void begin_rooms();void stop_rooms();void update_rooms();bool room_message(HWND,UINT,WPARAM,LPARAM);void draw_rooms();
 void begin_selection();bool lobby_message(HWND,UINT,WPARAM,LPARAM);void draw_lobbies();
 void begin_registration(CharacterCreateRequest);
 bool modelAvailable_=false,modelRendered_=false,modelPartial_=false;float modelYaw_=0.15f;
 std::function<uint64_t()> clock_;void tick_hold();void confirm_delete();
 void start();void update();void stop();void activate();void focus(int);
public:
 explicit CharacterScreen(std::function<CharacterReply(const std::atomic_bool&)>,std::function<uint64_t()> clock=[] {return GetTickCount64();});~CharacterScreen();
 uint32_t preview_character_id()const{return !pending_&&!lobbyVisible_&&reply_.status==CharacterStatus::success?slots_.preview_id():0;}
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
 std::optional<host::LoadRequest> stage_load_request()const {return detailVisible_&&detailReply_.join_status==RoomJoinStatus::joined&&detailReply_.host_match?detailReply_.host_match->request:std::nullopt;}
 std::optional<host::LoadRequest> stage_request()const {return matchVisible_?stage_load_request():std::nullopt;}
 const std::optional<host::Placements>& stage_placements()const{return detailReply_.host_placements;}
 void stage_feedback(stage::Status s){stageStatus_=s;}
 std::wstring stageAudioNotice_;
 std::wstring stageDebugNotice_;
 bool stageResetConfirm_=false,stageResetYes_=false;
 void stage_reset_feedback(bool open,bool yes){stageResetConfirm_=open;stageResetYes_=yes;}
 void stage_debug_feedback(std::wstring s){stageDebugNotice_=std::move(s);}
 void stage_audio_feedback(std::wstring s){stageAudioNotice_=std::move(s);}
 bool room_match_visible()const{return matchVisible_;}
 void weapon_catalog(const std::filesystem::path&);
 bool weapon_visible()const{return weaponsVisible_;}
 const weapons::Selection* weapon_draft()const{return weaponSelection_.get();}
 const std::optional<host::MatchState>& room_host_match()const{return detailReply_.host_match;}
 const std::optional<host::Roster>& room_host_roster()const{return detailReply_.host_roster;}
 unsigned current_lobby_group()const{return lobbyGroup_;}
 uint16_t focused_lobby_id()const{auto rows=lobby_group_rows(selectionReply_.lobbies,lobbyGroup_);return lobbyVisible_&&lobbyFocus_<rows.size()?selectionReply_.lobbies[rows[lobbyFocus_]].id:0;}
 uint32_t selected_character_id()const{return lobbyVisible_?selectionReply_.character.id:0;}
 bool creation_text_entry()const{return (creation_&&creation_->text_entry())||(detailVisible_&&detailFocus_==0);}
 std::optional<CharacterVoicePreview> take_audition(){return creation_?creation_->take_audition():std::nullopt;}
 void catalog(const CharacterCatalog*c){catalog_=c;}
 void registration(std::function<CharacterCreateReply(const CharacterCreateRequest&,const std::atomic_bool&)> fn,std::shared_ptr<CharacterRegistrationState> state){createTransport_=std::move(fn);registrationState_=std::move(state);}
 void selection(std::function<CharacterSelectionReply(uint32_t,const std::atomic_bool&)> fn,std::shared_ptr<CharacterSelectionState> state){selectTransport_=std::move(fn);selectionState_=std::move(state);}
 void model_partial(bool v){modelPartial_=v;}
 void model_available(bool v){modelAvailable_=v;}void model_rendered(){modelRendered_=true;}
 float model_yaw()const{return modelYaw_;}
 bool message(HWND,UINT,WPARAM,LPARAM);const void* draw();bool back()const{return back_;}
 std::vector<unsigned> cues(){auto c=std::move(cues_);cues_.clear();return c;}void report()const;
};
}
