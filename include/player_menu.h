#pragma once
#include "player_control.h"
#include "controller_panel.h"
#include "graphics_settings.h"
#include "camera_settings.h"
#include "chat_session.h"
#include "preset_radio.h"
#include "radio_session.h"
#include "world_inventory_session.h"
#include "hold_selection.h"
#include "weapon_icons.h"
#include "equipment_icons.h"
#include <memory>
namespace mgo2win {
// In-stage menus preserve the admitted room and never run a port probe.
class PlayerMenu {
 player::Menu kind_=player::Menu::none;unsigned tab_=0,chatChannel_=0;
 bool controlsOnly_=false;
 HDC dc_=nullptr;HBITMAP bitmap_=nullptr;HGDIOBJ old_=nullptr;void* pixels_=nullptr;
 std::vector<HFONT> fonts_;std::unique_ptr<ControllerPanel> controls_;
 std::shared_ptr<GraphicsSettings> graphics_;std::shared_ptr<ControllerInput> input_;std::wstring chat_,notice_;
 std::filesystem::path gameplayPath_;bool yFirstPerson_=false,enemyNameTags_=true;unsigned gameplayFocus_=0;wchar_t pendingHigh_=0;
 std::filesystem::path cameraPath_;camera::Settings cameraSettings_;unsigned cameraFocus_=0;
 std::shared_ptr<chat::Session> chatSession_;chat::State chatState_;
 std::shared_ptr<radio::Session> radioSession_;radio::SessionState radioState_;
 mgo2::radio::Menu radioMenu_;std::optional<mgo2::radio::Selection> radioSelected_;
 std::shared_ptr<items::ClientSession> inventorySession_;items::ClientState inventoryState_;unsigned inventoryFocus_=0,inventorySlot_=0;void inventory_action();
 HWND chatHwnd_=nullptr;void cancel_composition();
 std::wstring composition_;bool composing_=false,imeEnterGuard_=false;uint64_t chatSerial_=0;
 void send_chat();void sync_chat();void append_chat(wchar_t);
 std::vector<unsigned> cues_;
 hold_selection::State holdSelection_;items::DropPolicies holdNames_;weapons::Icons holdIcons_,holdGlyphs_;
 equipment::Icons holdEquipmentIcons_;
 size_t holdSelectionFrom_=0;uint64_t holdTransitionAt_=0;
 void cue(unsigned sound){if(cues_.size()<32)cues_.push_back(sound);}
 void collect_cues();void select_tab(unsigned);void change_gameplay(bool confirm);void change_camera(bool confirm,bool reset=false,int direction=1);
public:
 PlayerMenu(std::filesystem::path,std::shared_ptr<ControllerInput>,std::shared_ptr<GraphicsSettings>);
 ~PlayerMenu();void open(player::Menu,bool controlsOnly=false);void close(bool feedback=false);
 bool visible()const{return kind_!=player::Menu::none||holdSelection_.visible();}
 // RT/action14 and LT/action15 levels are supplied independently of the old
 // modal Menu::equipment placement panel. Release returns a request, never equips.
 std::optional<hold_selection::Request> hold_selection_step(const hold_selection::Snapshot&,hold_selection::Input);
 void cancel_hold_selection(){holdSelection_.cancel();}
 bool hold_visible()const{return holdSelection_.visible();}
 uint64_t input_context()const{return uint64_t(kind_)|(uint64_t(tab_)<<4)|(uint64_t(graphics_->pending())<<8)|(uint64_t(text_entry())<<9)|(uint64_t(hold_visible())<<10);}
 bool hold_consumes_action(unsigned action)const{return hold_visible()&&action==5;}
 bool hold_blocks_gameplay()const{return holdSelection_.blocks_gameplay();}
 const hold_selection::State& hold_selection_state()const{return holdSelection_;}
 void hold_assets(const std::filesystem::path& dataRoot);
  bool chat_menu()const{return kind_==player::Menu::chat;}
  bool settings_menu()const{return kind_==player::Menu::settings;}
 bool text_entry()const{return chat_menu()&&radioMenu_.state()==mgo2::radio::MenuState::chat;}
 bool radio_visible()const{return chat_menu()&&!text_entry();}
 void select_chat_radio();void radio_direction(mgo2::radio::Direction);
 void radio_digital_mask(uint32_t mask);
 void chat_session(std::shared_ptr<chat::Session>);
 void radio_session(std::shared_ptr<radio::Session>);
 void inventory_session(std::shared_ptr<items::ClientSession>);
 bool inventory_menu()const{return kind_==player::Menu::equipment;}
 bool capturing()const{return visible()&&kind_==player::Menu::settings&&tab_==0&&controls_->capturing();}
 bool prone_y_first_person()const{return yFirstPerson_;}
 // Personal preference only. The room's enemy-name-tag policy remains an
 // independent permission that the target renderer must also check.
 bool enemy_name_tags()const{return enemyNameTags_;}
 const camera::Settings& camera_settings()const{return cameraSettings_;}
 // Mapped controller actions 8/9 mean previous/next tab, not fixed F1/F2.
 bool tab_action(unsigned);
 bool sample(const PadSample&s){return visible()&&kind_==player::Menu::settings&&tab_==0&&controls_->sample(s);}
 unsigned slot(unsigned fallback)const{return visible()&&kind_==player::Menu::settings&&tab_==0?controls_->slot():fallback;}
 bool message(HWND,UINT,WPARAM,LPARAM);
 const void* draw();std::vector<unsigned> cues();
};
}
