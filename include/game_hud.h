#pragma once
#include <windows.h>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>
namespace mgo2mt::hud {
std::wstring utf8(std::string_view);
std::wstring time_label(std::optional<uint32_t> milliseconds);
std::wstring mode_label(uint8_t rule, bool abbreviated=false);
struct Model {
 std::wstring name,clan,weapon; uint32_t hp=0,maxHp=0,ammo=0,reserve=0;
 uint8_t rule=0; std::optional<uint32_t> remainingMs;
 std::vector<std::wstring> skills;
 uint32_t kills=0,deaths=0;unsigned rank=0;bool tied=false;
 bool alive=true,reloading=false,infiniteAmmo=false; uint32_t dp=0; bool dpKnown=false,ended=false;
 std::wstring actionNotice;bool respawnWaiting=false;uint32_t respawnRemainingMs=0;
 uint32_t stamina=0,maxStamina=0;uint16_t oxygen=10000;bool faceSubmerged=false;
};
class Intro {
 uint64_t epoch_=0,start_=0;
public:
 void reset(){epoch_=start_=0;}
 void deployed(uint64_t epoch,uint64_t now){if(epoch&&epoch!=epoch_){epoch_=epoch;start_=now;}}
 unsigned opacity(uint64_t now)const;
};
void draw(HDC,std::span<const HFONT>,const Model&,unsigned introOpacity=0);
// Composite a decoded 64x64 top-down premultiplied clan DIB into the final
// straight-alpha menu surface, after finish_menu_surface. No GDI color key.
void paint_clan_image(std::span<uint32_t> surface,int width,int height,HBITMAP);
}
