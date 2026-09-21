#pragma once
#include <windows.h>
#include <span>
#include <cstdint>
#include <string_view>
namespace mgo2mt::cover_hud {
struct Model {bool available=false,attached=false,pending=false;int lean=0;bool left=false,right=false,firstPerson=false;std::wstring_view action=L"Y";};
void draw(HDC,std::span<const HFONT>,const Model&);
class Renderer {
 HDC dc_=nullptr;HBITMAP bitmap_=nullptr;HGDIOBJ old_=nullptr;HFONT font_=nullptr;uint32_t* pixels_=nullptr;
public:
 Renderer();~Renderer();Renderer(const Renderer&)=delete;Renderer& operator=(const Renderer&)=delete;
 void paint(std::span<uint32_t>,const Model&);
};
}
