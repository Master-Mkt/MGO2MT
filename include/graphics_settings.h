#pragma once
#include <windows.h>
#include "shadow_cascade.h"
#include <vector>
#include <filesystem>
#include <functional>
#include <string>
namespace mgo2win {
struct GraphicsConfig {
 unsigned fullscreen=0,width=1280,height=720,refresh_num=0,refresh_den=1,shadow=2048,vsync=1;
 unsigned renderScale=100,anisotropy=0,mipmaps=0,linearColor=0;
 unsigned shadowEnabled=0,shadowCascades=4,shadowPcf=1,shadowBias=30,shadowNormal=20,shadowSlope=2,shadowDebug=0;
 shadows::Settings shadows()const{return {bool(shadowEnabled),shadowCascades,(shadow<1024?1024:shadow),shadowPcf,float(shadowBias)*.00001f,float(shadowNormal),float(shadowSlope),60000,bool(shadowDebug)};}
 bool operator==(const GraphicsConfig&)const=default;
};
struct DisplayMode {unsigned width,height,num,den;};
bool valid_graphics(const GraphicsConfig&);
bool load_graphics(const std::filesystem::path&,GraphicsConfig&);
void save_graphics(const std::filesystem::path&,const GraphicsConfig&);
class GraphicsSettings {
 std::filesystem::path path_;GraphicsConfig previous_;
 bool request_=false,undo_=false;ULONGLONG until_=0;
 int focus_=0;bool back_=false,shadowPage_=false;
 std::vector<unsigned> cues_;
 void cue(unsigned sound){if(cues_.size()<32)cues_.push_back(sound);}
 void change(int);void activate();
public:
 GraphicsConfig active,draft;
 std::vector<DisplayMode> modes;
 std::function<bool(const GraphicsConfig&)> apply;
 std::wstring notice=L"項目を選び、変更後に「適用」を押してください。";
 explicit GraphicsSettings(std::filesystem::path);
 bool pending()const{return until_!=0;}
 void request(){if(!pending())request_=true;}
 void cancel(){if(pending())undo_=true;}
 bool confirm();
 void tick(ULONGLONG now,bool foreground);
 bool message(HWND,UINT,WPARAM,LPARAM);
 POINT draw(HDC,const std::vector<HFONT>&); // Active item origin for focus guides.
 bool back(){bool b=back_;back_=false;return b;}
 std::vector<unsigned> cues(){auto c=std::move(cues_);cues_.clear();return c;}
};
}
