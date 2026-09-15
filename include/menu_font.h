#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
namespace mgo2win {
inline const wchar_t* original_menu_font_face(int weight){return weight>=FW_BOLD?L"SCE-PS3 NewRodin JPN Bold":L"SCE-PS3 NewRodin JPN Regular";}
// Process-private original PS3 fonts. No Windows installation or registry edit.
// Selecting NewRodin for these native menus is a presentation policy; original
// per-screen fontset selection and PS3 rasterizer parity remain unconfirmed.
class MenuFontResources {
 std::vector<HANDLE> handles_;bool ready_=false;
public:
 MenuFontResources()=default;MenuFontResources(const MenuFontResources&)=delete;
 ~MenuFontResources(){for(auto h:handles_)RemoveFontMemResourceEx(h);}
 bool ready()const{return ready_;}
 bool load(const std::filesystem::path& root){
  if(ready_)return true;
  for(auto name:{L"SCE-PS3-NR-R-JPN.TTF",L"SCE-PS3-NR-B-JPN.TTF"}){
   std::ifstream f(root/name,std::ios::binary|std::ios::ate);
   const auto size=f?std::streamoff(f.tellg()):0;
   if(size<12||size>32*1024*1024){clear();return false;}
   std::vector<char> data(static_cast<size_t>(size));f.seekg(0);f.read(data.data(),size);DWORD count=0;
   auto h=f?AddFontMemResourceEx(data.data(),DWORD(data.size()),nullptr,&count):nullptr;
   if(!h||!count){if(h)RemoveFontMemResourceEx(h);clear();return false;}handles_.push_back(h);
  }
  // Win32 uses nameID 1, which includes Regular/Bold in these original TTFs;
  // the shorter typographic family (nameID 16) silently selects a substitute.
  auto dc=CreateCompatibleDC(nullptr);bool matched=dc!=nullptr;
  if(dc)for(auto weight:{FW_NORMAL,FW_BOLD}){
   auto face=original_menu_font_face(weight);auto font=CreateFontW(-23,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,face);
   auto old=SelectObject(dc,font);wchar_t actual[80]{};matched=matched&&GetTextFaceW(dc,80,actual)>0&&std::wstring(actual)==face;
   SelectObject(dc,old);DeleteObject(font);
  }
  if(dc)DeleteDC(dc);if(!matched){clear();return false;}ready_=true;return true;
 }
private:
 void clear(){for(auto h:handles_)RemoveFontMemResourceEx(h);handles_.clear();ready_=false;}
};
inline MenuFontResources& menu_font_resources(){static MenuFontResources fonts;return fonts;}
inline HFONT create_menu_font(int pixels,int weight=FW_NORMAL){
 return CreateFontW(-pixels,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
  ANTIALIASED_QUALITY,DEFAULT_PITCH,menu_font_resources().ready()?original_menu_font_face(weight):L"MS Gothic");
}
}
