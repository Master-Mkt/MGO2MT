#include "game_hud.h"
#include "menu_theme.h"
#include "name_text_fit.h"
#include <algorithm>
namespace mgo2win::hud {
std::wstring utf8(std::string_view s){
 if(s.empty())return {};if(s.size()>4096)return L"？";
 int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0);
 if(!n)return L"？";std::wstring out(n,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),out.data(),n);return out;
}
std::wstring time_label(std::optional<uint32_t> ms){if(!ms)return L"--:--";auto seconds=(*ms+uint64_t(999))/1000;auto m=seconds/60,s=seconds%60;return (m<10?L"0":L"")+std::to_wstring(m)+L":"+(s<10?L"0":L"")+std::to_wstring(s);}
std::wstring mode_label(uint8_t rule,bool shortName){
 // DM/TDM match the reviewed room-rule IDs. Other rule numbers remain explicit.
 if(rule==0)return shortName?L"DM":L"Deathmatch";
 if(rule==1)return shortName?L"TDM":L"Team Deathmatch";
 return L"RULE "+std::to_wstring(rule);
}
unsigned Intro::opacity(uint64_t now)const {if(!epoch_||now<start_)return 0;auto t=now-start_;if(t>=4000)return 0;if(t<250)return unsigned(t*255/250);if(t<=3000)return 255;return unsigned((4000-t)*255/1000);}
void draw(HDC dc,std::span<const HFONT> fonts,const Model&m,unsigned intro){
 if(fonts.size()<4)return;
 auto saved=SaveDC(dc);
 auto text=[&](std::wstring_view s,RECT r,unsigned font,COLORREF color,UINT flags=DT_LEFT){SelectObject(dc,fonts[font]);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);DrawTextW(dc,s.data(),int(s.size()),&r,flags|DT_NOPREFIX|DT_SINGLELINE|DT_END_ELLIPSIS);};
 const auto amber=RGB(255,209,131),white=RGB(237,237,220);
 menu_fill(dc,20,24,328,69,RGB(35,40,37));
 {NameTextFit fit(dc,fonts[2],m.name,247);RECT r{96,25,343,49};SetTextColor(dc,white);SetBkMode(dc,TRANSPARENT);
  DrawTextW(dc,m.name.data(),int(m.name.size()),&r,DT_LEFT|DT_NOPREFIX|DT_SINGLELINE|DT_END_ELLIPSIS);}
 text(m.clan,{96,49,343,67},3,amber);
 menu_fill(dc,96,74,246,10,RGB(54,42,30));if(m.maxHp)menu_fill(dc,96,74,int(uint64_t(246)*std::min(m.hp,m.maxHp)/m.maxHp),10,amber);
 menu_rect(dc,96,88,246,8,RGB(40,44,30));if(m.maxStamina)menu_rect(dc,96,88,int(uint64_t(246)*std::min(m.stamina,m.maxStamina)/m.maxStamina),8,RGB(205,207,134));
 if(m.faceSubmerged||m.oxygen<10000){menu_rect(dc,96,88,246,8,RGB(30,44,56));menu_rect(dc,96,88,int(uint64_t(246)*std::min<uint16_t>(m.oxygen,10000)/10000),8,RGB(72,156,225));text(m.oxygen?L"O2":L"O2  0",{350,80,440,103},3,m.oxygen?RGB(97,184,244):RGB(255,125,94));}
 menu_fill(dc,550,18,180,78,RGB(67,50,25));text(mode_label(m.rule,true),{550,21,730,43},3,amber,DT_CENTER);text(time_label(m.remainingMs),{550,44,730,82},0,amber,DT_CENTER);
 if(m.rule==0&&m.rank)text(std::to_wstring(m.rank)+L"位  K "+std::to_wstring(m.kills)+L" / D "+std::to_wstring(m.deaths),{770,30,1250,65},2,amber,DT_RIGHT);
 int y=465;if(m.dpKnown){text(L"DP : "+std::to_wstring(m.dp),{24,y,350,y+25},3,amber);y+=25;}
 for(const auto&skill:m.skills){if(y>668)break;text(L"★ "+skill,{24,y,425,y+23},3,amber);y+=23;}
 menu_fill(dc,945,606,310,87,RGB(45,38,28));text(m.weapon,{958,613,1246,640},2,amber,DT_RIGHT);text(m.infiniteAmmo?L"∞":std::to_wstring(m.ammo)+L" / "+std::to_wstring(m.reserve),{958,644,1246,681},0,amber,DT_RIGHT);
 if(m.ended){menu_fill(dc,0,228,1280,100,RGB(16,20,28));text(L"時間終了",{0,233,1280,263},1,amber,DT_CENTER);text(L"次のラウンドを準備しています",{0,266,1280,298},1,white,DT_CENTER);text(m.rule==0&&m.rank?(m.rank==1?(m.tied?L"同率1位":L"1位"):std::to_wstring(m.rank)+L"位"):L"チーム勝敗集計は未対応",{0,300,1280,326},3,white,DT_CENTER);}
 else if(!m.alive){menu_fill(dc,360,486,560,113,RGB(27,31,31));text(L"戦闘不能",{370,495,910,532},0,amber,DT_CENTER);text(m.respawnWaiting?L"再出撃まで "+std::to_wstring((m.respawnRemainingMs+999)/1000)+L" 秒":L"再出撃を準備しています",{370,540,910,570},1,white,DT_CENTER);text(L"武器選択後に再出撃できます",{370,574,910,596},3,white,DT_CENTER);}
 else if(m.reloading)text(L"RELOADING",{948,582,1250,607},3,amber,DT_RIGHT);
 if(!m.actionNotice.empty())text(m.actionNotice,{300,685,1250,716},3,amber,DT_RIGHT);
 if(intro&&!m.ended&&m.alive){menu_fill(dc,0,228,1280,76,RGB(16,20,28));auto c=RGB(80*intro/255,143*intro/255,255*intro/255);text(mode_label(m.rule),{0,233,1280,263},1,c,DT_CENTER);text(m.rule==0?L"出会うPCを倒せ！":m.rule==1?L"敵チームのPCを倒せ！":L"ROUND START",{0,266,1280,298},1,c,DT_CENTER);}
 RestoreDC(dc,saved);
}
void paint_clan_image(std::span<uint32_t> surface,int width,int height,HBITMAP image){
 if(!image||width<=0||height<=0||surface.size()<size_t(width)*size_t(height))return;
 DIBSECTION bitmap{};
 if(GetObjectW(image,sizeof(bitmap),&bitmap)!=sizeof(bitmap)||!bitmap.dsBm.bmBits
    ||bitmap.dsBm.bmWidth!=64||bitmap.dsBm.bmHeight!=64||bitmap.dsBm.bmBitsPixel!=32||bitmap.dsBm.bmWidthBytes!=64*4)return;
 const auto* pixels=static_cast<const uint32_t*>(bitmap.dsBm.bmBits);
 for(int y=0;y<64&&25+y<height;++y)for(int x=0;x<64&&24+x<width;++x){
  const uint32_t source=pixels[y*64+x],sourceAlpha=source>>24;if(!sourceAlpha)continue;
  auto& destination=surface[size_t(25+y)*size_t(width)+24+x];const uint32_t destinationAlpha=destination>>24;
  const uint32_t inverse=255-sourceAlpha,alpha=sourceAlpha*255+destinationAlpha*inverse;
  uint32_t result=((alpha+127)/255)<<24;
  for(unsigned shift:{0u,8u,16u}){
   const uint32_t sourcePremult=std::min((source>>shift)&255,sourceAlpha);
   const uint32_t destinationColor=(destination>>shift)&255;
   const uint32_t numerator=sourcePremult*65025+destinationColor*destinationAlpha*inverse;
   result|=std::min(255u,(numerator+alpha/2)/alpha)<<shift;
  }
  destination=result;
 }
}
}
