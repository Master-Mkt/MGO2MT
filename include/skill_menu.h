#pragma once
#include <windows.h>
#include "skill_settings.h"
#include "weapon_icons.h"

namespace mgo2win {
class SkillMenu {
 HDC dc_=nullptr;HBITMAP bitmap_=nullptr;HGDIOBJ old_=nullptr;HFONT font_=nullptr;void* pixels_=nullptr;
 std::shared_ptr<const skills::Catalog> catalog_;std::unique_ptr<skills::Editor> editor_;weapons::Icons icons_;
 std::vector<uint16_t> ids_;size_t focus_=0;bool visible_=false,editable_=true;
 std::optional<skills::Loadout> saved_;std::vector<unsigned> cues_;std::wstring notice_,contextNotice_;
 void cue(unsigned);void move(int);void change(int,bool toggle=false);void apply();void activate();
public:
 SkillMenu();~SkillMenu();SkillMenu(const SkillMenu&)=delete;SkillMenu&operator=(const SkillMenu&)=delete;
 void icons(const std::filesystem::path&);
 void open(std::shared_ptr<const skills::Catalog>,const skills::Loadout&,unsigned capacity=skills::base_capacity,bool editable=true);
 void close(bool feedback=false);
 bool visible()const{return visible_;}size_t focus()const{return focus_;}
 void notice(std::wstring value){contextNotice_=std::move(value);}
 const skills::Loadout& draft()const;
 bool message(HWND,UINT,WPARAM,LPARAM);
 const void* draw();
 std::optional<skills::Loadout> take_saved(){auto result=std::move(saved_);saved_.reset();return result;}
 std::vector<unsigned> cues(){std::vector<unsigned> out;out.swap(cues_);return out;}
};
}
