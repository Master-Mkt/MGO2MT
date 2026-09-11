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
   std::function<AuthReply(const AuthCredentials&,const std::atomic_bool&)> transport=authenticate,bool externalPorts=true,std::shared_ptr<ControllerInput> input={},std::shared_ptr<GraphicsSettings> graphics={},std::filesystem::path networkKeys={});~LoginScreen();
 bool controller_sample(const PadSample& s){return ports_&&ports_->controller_sample(s);}
 unsigned input_slot()const{return ports_?ports_->input_slot():input_->config.slot;}
 bool message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp);
 bool back()const{return form_.back();}
 bool port_visible()const{return bool(ports_)||bool(characters_);}
 bool character_visible()const{return bool(characters_);}
 bool creation_visible()const{return characters_&&characters_->creation_visible();}
 std::optional<CharacterVoicePreview> take_audition(){return characters_?characters_->take_audition():std::nullopt;}
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
