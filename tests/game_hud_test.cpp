#include "game_hud.h"
#include "menu_theme.h"
#include "name_text_fit.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
static void check(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char**argv){try{
 check(hud::utf8("日本語プレイヤー") == L"日本語プレイヤー","UTF-8 Japanese player name survives conversion");
 check(hud::utf8("\xf0\x9f\x8e\xae")==L"\xd83c\xdfae","supplementary Unicode preserves surrogate pair");
 check(hud::utf8("\xc0\xaf")==L"？"&&hud::utf8("\xed\xa0\x80")==L"？"&&hud::utf8(std::string(4097,'A'))==L"？","invalid and oversized text has bounded replacement");
 check(hud::time_label({})==L"--:--"&&hud::time_label(0)==L"00:00"&&hud::time_label(1)==L"00:01"&&hud::time_label(60000)==L"01:00"&&hud::time_label(60001)==L"01:01","clock keeps missing authority distinct and rounds boundary up");
 check(hud::time_label(UINT32_MAX)==L"71582:48","largest timer avoids overflow");
 check(hud::mode_label(1)==L"Team Deathmatch"&&hud::mode_label(1,true)==L"TDM"&&hud::mode_label(255)==L"RULE 255","unverified mode number never borrows another rule label");
 check(hud::mode_label(0)==L"Deathmatch"&&hud::mode_label(0,true)==L"DM","reviewed DM label");
 hud::Intro intro;check(!intro.opacity(1000),"no intro before deployment");intro.deployed(7,1000);
 check(!intro.opacity(999)&&!intro.opacity(1000)&&intro.opacity(1125)==127&&intro.opacity(1250)==255&&intro.opacity(4000)==255&&intro.opacity(4500)==127&&!intro.opacity(5000),"intro fade and lifetime boundaries");
 intro.deployed(7,6000);check(!intro.opacity(6100),"same round snapshot does not restart mode flash");intro.deployed(8,6000);check(intro.opacity(6250)==255,"new round triggers flash");intro.reset();check(!intro.opacity(6250),"leaving room clears intro");
 HDC dc=CreateCompatibleDC(nullptr);check(dc!=nullptr,"HUD memory DC");BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=1280;bi.bmiHeader.biHeight=-720;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;void*pixels=nullptr;
 HBITMAP bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&pixels,nullptr,0);check(bitmap&&pixels,"HUD bitmap");auto old=SelectObject(dc,bitmap);
 // Exercise the actual CharacterScreen font and sizes used by the game HUD.
 std::array<HFONT,4> fonts{};constexpr int sizes[]{30,23,20,17};for(size_t i=0;i<fonts.size();++i)fonts[i]=CreateFontW(-sizes[i],0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FIXED_PITCH,L"MS Gothic");
 auto oldFont=SelectObject(dc,fonts[2]);std::array<WORD,7>glyphs{};std::wstring Japanese=L"日本語名前技能";
 check(GetGlyphIndicesW(dc,Japanese.c_str(),int(Japanese.size()),glyphs.data(),GGI_MARK_NONEXISTING_GLYPHS)!=GDI_ERROR&&std::none_of(glyphs.begin(),glyphs.end(),[](WORD g){return g==0xffff;}),"selected native HUD font contains Japanese glyphs");
 hud::Model model;model.name=L"日本語プレイヤー";model.clan=L"クラン名";model.weapon=L"AK102";model.rule=1;model.remainingMs=174000;model.hp=750;model.maxHp=1000;model.ammo=21;model.reserve=90;model.dpKnown=true;model.dp=1000;
 model.skills={L"アサルトライフル Lv.1",L"ショットガン Lv.1",L"CQC Lv.1",L"モノマニア Lv.1"};
 std::memset(pixels,0,1280*720*4);hud::draw(dc,fonts,model,255);GdiFlush();auto data=static_cast<uint32_t*>(pixels);
 auto count=[&](int left,int top,int right,int bottom){size_t n=0;for(int y=top;y<bottom;++y)for(int x=left;x<right;++x)if(data[size_t(y)*1280+x]&0xffffff)++n;return n;};
 check(count(24,491,424,583)>300,"selected skill names and levels render at left");
 size_t nameGlyphs=0,introGlyphs=0;for(int y=25;y<49;++y)for(int x=70;x<343;++x){auto p=data[size_t(y)*1280+x];if((p&255)>200&&((p>>8)&255)>200&&((p>>16)&255)>200)++nameGlyphs;}
 for(int y=233;y<297;++y)for(int x=390;x<900;++x){auto p=data[size_t(y)*1280+x];if((p&255)>180&&((p>>8)&255)>85&&((p>>16)&255)<140)++introGlyphs;}
 check(nameGlyphs>150,"Japanese name renders foreground glyphs");check(introGlyphs>150,"mode announcement renders blue glyphs rather than only its strip");
 {
  // The original short-name operation must remain pixel-identical.
  std::vector<uint32_t> expected(data,data+1280*720);
  menu_fill(dc,96,25,247,24,RGB(35,40,37));SelectObject(dc,fonts[2]);SetTextColor(dc,RGB(237,237,220));SetBkMode(dc,TRANSPARENT);
  RECT rect{96,25,343,49};DrawTextW(dc,model.name.data(),int(model.name.size()),&rect,DT_LEFT|DT_NOPREFIX|DT_SINGLELINE|DT_END_ELLIPSIS);GdiFlush();
  check(std::equal(expected.begin(),expected.end(),data),"short HUD name unchanged from original GDI pixels");
  auto current=GetCurrentObject(dc,OBJ_FONT);NameTextFit fit(dc,fonts[2],model.name,247);
  check(fit.fits()&&!fit.adjusted()&&GetCurrentObject(dc,OBJ_FONT)==current,"short name uses identical original font object");
 }
 // Exercise the final straight-alpha HUD, after the menu color-key conversion.
 finish_menu_surface(pixels);const size_t emblemAt=25*1280+24;
 const uint32_t untouched=data[emblemAt+3];check(data[emblemAt+2]==0x6e634b2fu,"HUD backdrop has its intended menu alpha");
 BITMAPINFO emblemInfo{};emblemInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);emblemInfo.bmiHeader.biWidth=64;emblemInfo.bmiHeader.biHeight=-64;emblemInfo.bmiHeader.biPlanes=1;emblemInfo.bmiHeader.biBitCount=32;
 void* emblemPixels=nullptr;auto emblemBitmap=CreateDIBSection(nullptr,&emblemInfo,DIB_RGB_COLORS,&emblemPixels,nullptr,0);check(emblemBitmap&&emblemPixels,"synthetic clan DIB");std::memset(emblemPixels,0,64*64*4);
 auto emblem=static_cast<uint32_t*>(emblemPixels);emblem[0]=0xff000000u;emblem[1]=0xff232b2bu;emblem[2]=0x80402010u;emblem[10]=0x80402010u;data[emblemAt+10]=0;
 hud::paint_clan_image({data,1280*720},1280,720,emblemBitmap);
 check(data[emblemAt]==0xff000000u,"opaque black clan pixel is not color-keyed away");
 check(data[emblemAt+1]==0xff232b2bu,"clan colors matching menu translucent keys stay opaque");
 check(data[emblemAt+2]==0xb7774324u,"partial clan alpha source-over composes onto translucent HUD in straight alpha");
 check(data[emblemAt+3]==untouched,"transparent clan pixel preserves existing HUD");
 check(data[emblemAt+10]==0x80804020u,"premultiplied source becomes straight alpha over a transparent destination");
 auto before=data[emblemAt];hud::paint_clan_image({data,1280*720},1280,720,nullptr);check(data[emblemAt]==before,"absent image is inert");DeleteObject(emblemBitmap);
 if(argc>1){BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+1280*720*4;std::ofstream out(argv[1],std::ios::binary);out.write(reinterpret_cast<char*>(&file),sizeof(file));out.write(reinterpret_cast<char*>(&bi.bmiHeader),sizeof(bi.bmiHeader));out.write(static_cast<const char*>(pixels),1280*720*4);check(bool(out),"HUD review bitmap");}
 {
  const std::wstring sixteen=L"日本語名前十六文字表示確認用兵士";check(sixteen.size()==16,"16 Japanese scalar fixture");
  std::array<WORD,16> indices{};SelectObject(dc,fonts[2]);
  check(GetGlyphIndicesW(dc,sixteen.data(),16,indices.data(),GGI_MARK_NONEXISTING_GLYPHS)!=GDI_ERROR&&std::none_of(indices.begin(),indices.end(),[](WORD g){return g==0xffff;}),"all 16 Japanese fixture glyphs exist");
  TEXTMETRICW before{},after{};GetTextMetricsW(dc,&before);SIZE original{};GetTextExtentPoint32W(dc,sixteen.data(),16,&original);
  {NameTextFit fit(dc,fonts[2],sixteen,247);GetTextMetricsW(dc,&after);
   check(original.cx>247&&fit.adjusted()&&fit.fits()&&fit.extent().cx<=247,"full Japanese name width fits without ellipsis");
   check(before.tmHeight==after.tmHeight,"HUD font vertical height preserved");
   std::cout<<"HUD Japanese16 width "<<original.cx<<" -> "<<fit.extent().cx<<", height "<<after.tmHeight<<'\n';}
  model.name=sixteen;std::memset(pixels,0,1280*720*4);hud::draw(dc,fonts,model,255);GdiFlush();
  if(argc>1){BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+1280*720*4;std::ofstream out(std::string(argv[1])+".unicode.bmp",std::ios::binary);out.write(reinterpret_cast<char*>(&file),sizeof(file));out.write(reinterpret_cast<char*>(&bi.bmiHeader),sizeof(bi.bmiHeader));out.write(static_cast<const char*>(pixels),1280*720*4);check(bool(out),"Unicode HUD bitmap");}
  std::wstring emoji;for(int i=0;i<16;++i)emoji+=L"😀";
  std::vector<WORD> emojiGlyphs(emoji.size());SelectObject(dc,fonts[2]);
  const auto glyphResult=GetGlyphIndicesW(dc,emoji.data(),int(emoji.size()),emojiGlyphs.data(),GGI_MARK_NONEXISTING_GLYPHS);
  const bool allEmojiGlyphs=glyphResult!=GDI_ERROR&&std::none_of(emojiGlyphs.begin(),emojiGlyphs.end(),[](WORD g){return g==0xffff;});
  {NameTextFit fit(dc,fonts[2],emoji,247);std::cout<<"HUD emoji16 measured width "<<fit.extent().cx<<", fits="<<fit.fits()<<", direct-font UTF16 glyph coverage="<<allEmojiGlyphs<<" (not a color emoji/shaping guarantee)\n";}
 }
 model.skills.clear();std::memset(pixels,0,1280*720*4);hud::draw(dc,fonts,model,0);GdiFlush();check(count(24,491,424,583)==0,"empty skills do not retain prior-round labels");
 SelectObject(dc,oldFont);for(auto font:fonts)DeleteObject(font);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
 std::cout<<"HUD Japanese glyphs, selected skills, timer, intro and final clan alpha passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
