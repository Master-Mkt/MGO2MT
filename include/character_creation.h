#pragma once
#include <windows.h>
#include "character_catalog.h"
#include "character_client.h"
#include <string>
#include <span>
#include <optional>
#include <cmath>
#include <stdexcept>
namespace mgo2mt {
// PPC 7F74D0: pow(1.095f, (storedPitch-15)/15), clamped to reciprocal/1.095.
// Native pow is an approximation of the original math library, not bit-identical DSP.
struct CharacterVoicePreview {unsigned gender=0,voice=0;int pitch=0;};
inline float character_voice_ratio(int pitch){if(pitch < -7||pitch > 7)throw std::out_of_range("voice pitch");return std::pow(1.0950000286102295f,float(pitch)*0.06666667014360428f);}
// Native draft emits an explicit request; its owner performs network I/O.
class CharacterCreation {
 const CharacterCatalog* catalog_;
 std::array<uint8_t,28> appearance_{};
 std::wstring name_,notice_;
 // Kept separate from wire appearance[8] until original encoding is verified.
 int pitch_=0;unsigned auditions_=0;std::optional<CharacterVoicePreview> audition_;
 unsigned tab_=0,row_=0;bool color_=false,closed_=false,confirm_=false,discard_=false,yes_=false,dirty_=false;
 unsigned changes_=0,confirmations_=0;
 bool skillsRequired_=false,skillsConfigured_=false,skillRequested_=false;
 bool registrationEnabled_=false,registrationBusy_=false,registrationRequested_=false;
 bool unicodeNames_=false,imeComposing_=false,imeEnterGuard_=false;
 wchar_t pendingHigh_=0;
 std::optional<CharacterCreateRequest> registration_;
 std::vector<unsigned> cues_;
 std::vector<AppearanceRule> rules(unsigned slot)const;
 std::vector<unsigned> choices(unsigned slot,bool colors)const;
 void normalize();void step(int direction);void activate();void change_tab(unsigned);void cancel();
 void audition();
 void append_name(wchar_t);
public:
 explicit CharacterCreation(const CharacterCatalog*,bool registrationEnabled=false);
 std::optional<CharacterCreateRequest> take_registration(){auto r=std::move(registration_);registration_.reset();return r;}
 void registration_failed(std::wstring message){registrationBusy_=false;confirm_=false;yes_=false;notice_=std::move(message);}
 void require_skills(bool enabled){skillsRequired_=enabled;}
 void unicode_names(bool enabled){unicodeNames_=enabled;notice_.clear();}
 bool take_skill_request(){bool result=skillRequested_;skillRequested_=false;return result;}
 void skills_complete(bool advance){skillsConfigured_=true;dirty_=true;if(advance)activate();}
 bool registration_busy()const{return registrationBusy_;}
 bool message(HWND,UINT,WPARAM,LPARAM);
 void draw(HDC,std::span<const HFONT>)const;
 bool closed()const{return closed_;}
 bool text_entry()const{return tab_==0&&row_==0&&!confirm_&&!discard_&&!registrationBusy_;}
 const auto& appearance()const{return appearance_;}
 const auto& name()const{return name_;}
 int pitch()const{return pitch_;}
 std::optional<CharacterVoicePreview> take_audition(){auto r=audition_;audition_.reset();return r;}
 unsigned tab()const{return tab_;}unsigned row()const{return row_;}
 bool confirming()const{return confirm_;}bool discarding()const{return discard_;}bool yes()const{return yes_;}
 unsigned changes()const{return changes_;}unsigned confirmations()const{return confirmations_;}
 static std::wstring name_error(std::wstring_view,bool unicode=false);
 std::vector<unsigned> cues(){auto r=std::move(cues_);cues_.clear();return r;}
 void report()const;
};
}
