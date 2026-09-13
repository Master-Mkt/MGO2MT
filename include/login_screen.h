#pragma once
#include <windows.h>
#include <vector>
#include "login_form.h"
#include <filesystem>
#include <thread>
#include <functional>
#include <memory>
#include "port_screen.h"
#include "authentication.h"
#include "character_screen.h"
namespace mgo2win {
class LoginScreen {
 LoginForm form_;HDC dc_=nullptr;HBITMAP bitmap_=nullptr;HGDIOBJ old_=nullptr;void* pixels_=nullptr;
 std::vector<HFONT> fonts_;std::vector<unsigned> cues_;
 std::filesystem::path store_;bool restored_=false,saved_=false;unsigned persistedMode_=0;
 bool manualOnly_=false;uint16_t fixedLocalPort_=0;
 bool authEnabled_=true,pending_=false,authenticated_=false,leaving_=false,automatic_=false;
 std::thread authThread_;std::atomic_bool cancel_{false},done_{false};AuthReply reply_;
 std::function<AuthReply(const AuthCredentials&,const std::atomic_bool&)> transport_;
 ULONGLONG autoAt_=0,retryAt_=0;unsigned requests_=0;
 std::unique_ptr<PortScreen> ports_;
 std::unique_ptr<CharacterScreen> characters_;std::filesystem::path networkKeys_;
 std::shared_ptr<CharacterRegistrationState> registrationState_=std::make_shared<CharacterRegistrationState>();
 std::shared_ptr<CharacterSelectionState> selectionState_=std::make_shared<CharacterSelectionState>();
 std::shared_ptr<std::atomic_bool> roomJoinUncertain_=std::make_shared<std::atomic_bool>(false);
 const CharacterCatalog* catalog_=nullptr;
 void open_characters();
 bool externalPorts_=true;
 std::shared_ptr<ControllerInput> input_;std::shared_ptr<GraphicsSettings> graphics_;
 void cue(int c){if(c>=0&&cues_.size()<32)cues_.push_back(static_cast<unsigned>(c));}
 void key(LoginForm::Key);
 bool save();void begin_auth(bool automatic);void update();
public:
 explicit LoginScreen(std::filesystem::path store,bool authEnabled=true,
   std::function<AuthReply(const AuthCredentials&,const std::atomic_bool&)> transport=authenticate,bool externalPorts=true,std::shared_ptr<ControllerInput> input={},std::shared_ptr<GraphicsSettings> graphics={},std::filesystem::path networkKeys={},bool manualOnly=false,uint16_t fixedLocalPort=0);~LoginScreen();
 bool controller_sample(const PadSample& s){return ports_&&ports_->controller_sample(s);}
 unsigned input_slot()const{return ports_?ports_->input_slot():input_->config.slot;}
 bool message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp);
 bool back()const{return form_.back();}
 bool port_visible()const{return bool(ports_)||bool(characters_);}
 bool character_visible()const{return bool(characters_);}
 bool creation_visible()const{return characters_&&characters_->creation_visible();}
 bool personal_overlay_visible()const{return characters_&&characters_->skill_visible();}
 bool gameplay_visible()const{return characters_&&characters_->room_match_visible()&&!characters_->weapon_visible()&&!characters_->skill_visible();}
 void selection_presentation(uint32_t saluteMs,bool magazine,bool box,uint32_t soundDelayMs=0){if(characters_)characters_->selection_presentation(saluteMs,magazine,box,soundDelayMs);}
 SelectionPresentation::Frame selection_frame(){return characters_?characters_->selection_frame():SelectionPresentation::Frame{};}
 bool take_selection_sound(){return characters_&&characters_->take_selection_sound();}
 bool take_gameplay_options_request(){return characters_&&characters_->take_gameplay_options_request();}
 bool weapon_music_available()const{return characters_&&characters_->weapon_music_available();}
 bool take_weapon_music_request(){return characters_&&characters_->take_weapon_music_request();}
 void weapon_music_feedback(std::wstring title,bool ready){if(characters_)characters_->weapon_music_feedback(std::move(title),ready);}
 std::optional<CharacterVoicePreview> take_audition(){return characters_?characters_->take_audition():std::nullopt;}
 std::optional<host::LoadRequest> stage_request()const{return characters_?characters_->stage_request():std::nullopt;}
 std::shared_ptr<chat::Session> chat_session()const{return characters_?characters_->chat_session():nullptr;}
 std::shared_ptr<invitations::Session> invitation_session()const{return characters_?characters_->invitation_session():nullptr;}
 std::shared_ptr<items::ClientSession> inventory_session()const{return characters_?characters_->inventory_session():nullptr;}
 std::shared_ptr<radio::Session> radio_session()const{return characters_?characters_->radio_session():nullptr;}
 std::optional<host::LoadRequest> stage_load_request()const{return characters_?characters_->stage_load_request():std::nullopt;}
 std::optional<host::Placements> stage_placements()const{return characters_?characters_->stage_placements():std::nullopt;}
 std::optional<stage::SceneSnapshot> stage_scene()const{return characters_?characters_->stage_scene():std::nullopt;}
 std::optional<combat::wire::Offer> combat_offer()const{return characters_?characters_->combat_offer():std::nullopt;}
 std::optional<combat::wire::Preparation> combat_preparation()const{return characters_?characters_->combat_preparation():std::nullopt;}
 std::optional<combat::Snapshot> combat_state()const{return characters_?characters_->combat_state():std::nullopt;}
 combat::SopView combat_sop()const{return characters_?characters_->combat_sop():combat::SopView{};}
 bool room_enemy_name_tags()const{return characters_&&characters_->room_enemy_name_tags();}
 bool room_auto_aim()const{return characters_&&characters_->room_auto_aim();}
 clan::State enemy_clan_emblem(uint32_t id){return characters_?characters_->enemy_clan_emblem(id):clan::State{};}
 std::optional<host::Roster> room_host_roster()const{return characters_?characters_->room_host_roster():std::nullopt;}
 combat::wire::Status combat_status()const{return characters_?characters_->combat_status():combat::wire::Status::awaiting_world;}
 std::vector<combat::Event> combat_events(){return characters_?characters_->combat_events():std::vector<combat::Event>{};}
 void combat_input(const combat::wire::Input&i){if(characters_)characters_->combat_input(i);}
 stage::SceneSyncStatus scene_status()const{return characters_?characters_->scene_status():stage::SceneSyncStatus::idle;}
 void stage_feedback(const stage::Result&s){if(characters_)characters_->stage_feedback(s);}
 void stage_audio_feedback(std::wstring s){if(characters_)characters_->stage_audio_feedback(std::move(s));}
 void stage_reset_feedback(bool open,bool yes){if(characters_)characters_->stage_reset_feedback(open,yes);}
 void stage_debug_feedback(std::wstring s){if(characters_)characters_->stage_debug_feedback(std::move(s));}
 void stage_navigation_feedback(bool active){if(characters_)characters_->stage_navigation_feedback(active);}
 std::optional<std::array<uint8_t,28>> stage_appearance()const{return characters_?characters_->stage_appearance():std::nullopt;}
 bool model_preview_visible()const{return characters_&&characters_->preview_visible();}
 void character_catalog(const CharacterCatalog*c){catalog_=c;if(characters_)characters_->catalog(c);}
 uint32_t preview_character_id()const{return characters_?characters_->preview_character_id():0;}
 std::optional<std::array<uint8_t,28>> preview_appearance()const{return characters_?characters_->preview_appearance():std::nullopt;}
 void model_partial(bool v){if(characters_)characters_->model_partial(v);}
 float model_yaw()const{return characters_?characters_->model_yaw():0;}
 void model_available(bool v){if(characters_)characters_->model_available(v);}
 void model_rendered(){if(characters_)characters_->model_rendered();}
 void show_character_preview(bool slots=false,bool selection=false);
 // Only the isolated scripted UI path may open settings without authentication.
 void show_port_preview(bool external=false);
 const void* draw();
 std::vector<unsigned> cues(){auto c=std::move(cues_);cues_.clear();return c;}
 void report()const;
};
}
