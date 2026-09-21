#pragma once
#include "menu_theme.h"
#include <array>
namespace mgo2mt::briefing {
enum class Panel { none,map,rules,host,ready,quit };
inline constexpr std::array<const wchar_t*,7> labels{L"GAME START",L"MAP",L"RULES",L"SKILLS",L"HOST",L"OPTIONS",L"QUIT"};
inline constexpr int toolbarX=48,toolbarY=90,toolbarStep=58,toolbarSize=48;
struct Fonts {
 std::array<HFONT,4> values{};
 Fonts(){unsigned i=0;for(int size:{30,23,20,17})values[i++]=CreateFontW(-size,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"MS Gothic");}
 ~Fonts(){for(auto font:values)DeleteObject(font);}
};
inline const Fonts& fonts(){static Fonts value;return value;}
inline void panel(HDC dc,int x,int y,int w,int h){
 menu_rect(dc,x,y,w,h,RGB(58,36,18));
 for(int i=4;i<h;i+=4)menu_rect(dc,x+1,y+i,w-2,1,RGB(76,50,27));
 menu_rect(dc,x,y,w,2,RGB(172,163,139));menu_rect(dc,x,y+h-2,w,2,RGB(172,163,139));
}
inline std::wstring rule_description(uint8_t rule){
 switch(rule){
 case 0:return L"個人戦です。他のプレイヤーと戦い、スコアを競います。";
 case 1:return L"2つのチームに分かれて戦います。味方と協力して相手チームに挑みましょう。";
 default:return L"ルームに設定されたルールで対戦します。開始前にルールとマップを確認してください。";
 }
}
}
