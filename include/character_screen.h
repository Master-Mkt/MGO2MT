#pragma once
#include <windows.h>
#include "character_client.h"
#include "character_slots.h"
#include "character_creation.h"
#include <memory>
#include <thread>
#include <optional>
#include <functional>
namespace mgo2win {
// Shared across screen navigation. An ambiguous send stays locked until a
// refreshed list confirms the requested unique name (or the returned ID).
struct CharacterRegistrationState {bool unresolved=false;uint32_t expected_id=0;std::wstring expected_name;};
class CharacterScreen {
 HDC dc_=nullptr;HBITMAP bitmap_=nullptr;HGDIOBJ old_=nullptr;void*pixels_=nullptr;std::vector<HFONT>fonts_;
 std::function<CharacterReply(const std::atomic_bool&)>transport_;std::thread worker_;std::atomic_bool cancel_{false},done_{false};
 CharacterReply reply_;bool pending_=false,back_=false;int focus_=0;ULONGLONG retryAt_=0;unsigned requests_=0;std::vector<unsigned>cues_;
 CharacterSlots slots_;std::wstring notice_;unsigned deleteDialogs_=0,deleteYes_=0;
 const CharacterCatalog* catalog_=nullptr;std::unique_ptr<CharacterCreation> creation_;
 std::function<CharacterCreateReply(const CharacterCreateRequest&,const std::atomic_bool&)> createTransport_;
 std::shared_ptr<CharacterRegistrationState> registrationState_=std::make_shared<CharacterRegistrationState>();
 CharacterCreateReply createReply_;bool registering_=false;std::wstring registrationNotice_;
 void begin_registration(CharacterCreateRequest);
 bool modelAvailable_=false,modelRendered_=false,modelPartial_=false;float modelYaw_=0.15f;
 std::function<uint64_t()> clock_;void tick_hold();void confirm_delete();
 void start();void update();void stop();void activate();void focus(int);
public:
 explicit CharacterScreen(std::function<CharacterReply(const std::atomic_bool&)>,std::function<uint64_t()> clock=[] {return GetTickCount64();});~CharacterScreen();
 uint32_t preview_character_id()const{return !pending_&&reply_.status==CharacterStatus::success?slots_.preview_id():0;}
 std::optional<std::array<uint8_t,28>> preview_appearance()const {if(creation_)return creation_->appearance();if(preview_character_id())return reply_.list.entries[slots_.selected()].appearance;return {};}
 bool preview_visible()const{return bool(creation_)||preview_character_id()!=0;}
 bool creation_visible()const{return bool(creation_);}
 bool busy()const{return pending_;}
 bool creation_text_entry()const{return creation_&&creation_->text_entry();}
 std::optional<CharacterVoicePreview> take_audition(){return creation_?creation_->take_audition():std::nullopt;}
 void catalog(const CharacterCatalog*c){catalog_=c;}
 void registration(std::function<CharacterCreateReply(const CharacterCreateRequest&,const std::atomic_bool&)> fn,std::shared_ptr<CharacterRegistrationState> state){createTransport_=std::move(fn);registrationState_=std::move(state);}
 void model_partial(bool v){modelPartial_=v;}
 void model_available(bool v){modelAvailable_=v;}void model_rendered(){modelRendered_=true;}
 float model_yaw()const{return modelYaw_;}
 bool message(HWND,UINT,WPARAM,LPARAM);const void* draw();bool back()const{return back_;}
 std::vector<unsigned> cues(){auto c=std::move(cues_);cues_.clear();return c;}void report()const;
};
}
