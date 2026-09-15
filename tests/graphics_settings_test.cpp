#include "graphics_settings.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
void require(bool b){if(!b)throw std::runtime_error("Graphics settings contract failed");}
int main(){auto dir=std::filesystem::current_path()/(L"graphics-test-"+std::to_wstring(GetCurrentProcessId()));require(std::filesystem::create_directory(dir));auto path=dir/L"graphics.cfg";
 try{
  GraphicsConfig cfg;require(valid_graphics(cfg));cfg={1,1920,1080,60000,1001,4096,0};save_graphics(path,cfg);GraphicsConfig loaded;require(load_graphics(path,loaded)&&loaded==cfg);
  for(unsigned shadow:{512u,1024u,2048u,4096u,8192u}){loaded.shadow=shadow;require(valid_graphics(loaded));}loaded.refresh_den=0;require(!valid_graphics(loaded));loaded=cfg;loaded.shadow=3000;require(!valid_graphics(loaded));
  {std::ofstream f(path);f<<"MGO2WIN.GRAPHICS 1 1 1920 1080 60000 0 4096 1";}bool bad=false;try{load_graphics(path,loaded);}catch(...){bad=true;}require(bad);std::filesystem::remove(path);
  {std::ofstream f(path);f<<"MGO2WIN.GRAPHICS 1 0 3840 2160 0 1 2048 1";}require(load_graphics(path,loaded)&&loaded.renderScale==100&&!loaded.anisotropy&&!loaded.mipmaps&&!loaded.linearColor);
  loaded.renderScale=150;loaded.anisotropy=16;loaded.mipmaps=loaded.linearColor=1;save_graphics(path,loaded);GraphicsConfig enhanced;require(load_graphics(path,enhanced)&&loaded==enhanced);enhanced.anisotropy=3;require(!valid_graphics(enhanced));enhanced=loaded;enhanced.renderScale=201;require(!valid_graphics(enhanced));std::filesystem::remove(path);
  GraphicsSettings s(path);unsigned calls=0;GraphicsConfig applied;bool reject=false;s.apply=[&](const GraphicsConfig& c){++calls;if(reject&&c.width==1600)return false;applied=c;return true;};
  s.draft.width=1600;s.draft.height=900;s.request();s.tick(100,true);require(s.pending()&&applied.width==1600&&!std::filesystem::exists(path));s.tick(15099,true);require(s.pending());s.tick(15100,true);require(!s.pending()&&applied.width==1280&&!std::filesystem::exists(path));
  s.draft.width=1600;s.request();s.tick(20000,true);s.tick(20001,false);require(!s.pending()&&applied.width==1280);
  s.draft.width=1600;s.request();s.tick(21000,true);s.cancel();s.tick(21001,true);require(!s.pending()&&applied.width==1280);
  reject=true;s.draft.width=1600;s.request();s.tick(22000,true);require(!s.pending()&&applied.width==1280);reject=false;
  s.draft.width=1920;s.draft.height=1080;s.draft.shadow=8192;s.request();s.tick(23000,true);require(s.confirm()&&load_graphics(path,loaded)&&loaded.width==1920&&loaded.shadow==8192);require(!s.pending());
  GraphicsSettings navigation(dir/L"not-saved.cfg");for(unsigned i=0;i<5;++i)navigation.message(nullptr,WM_KEYDOWN,VK_DOWN,0);navigation.message(nullptr,WM_KEYDOWN,VK_RIGHT,0);require(navigation.draft.anisotropy==2);navigation.message(nullptr,WM_KEYDOWN,VK_DOWN,0);navigation.message(nullptr,WM_KEYDOWN,VK_RIGHT,0);require(navigation.draft.mipmaps==1);navigation.message(nullptr,WM_KEYDOWN,VK_DOWN,0);navigation.message(nullptr,WM_KEYDOWN,VK_RIGHT,0);require(navigation.draft.linearColor==1);navigation.message(nullptr,WM_KEYDOWN,VK_DOWN,0);navigation.apply=[](const auto&){return true;};navigation.message(nullptr,WM_KEYDOWN,VK_RETURN,0);navigation.tick(24000,true);require(navigation.pending());navigation.cancel();navigation.tick(24001,true);require(!navigation.pending()&&!navigation.active.linearColor);
  navigation.message(nullptr,WM_KEYDOWN,VK_NEXT,0);navigation.message(nullptr,WM_KEYDOWN,VK_RIGHT,0);require(navigation.draft.shadowEnabled==1);navigation.message(nullptr,WM_KEYDOWN,VK_DOWN,0);navigation.message(nullptr,WM_KEYDOWN,VK_RIGHT,0);require(navigation.draft.shadow==4096);navigation.message(nullptr,WM_KEYDOWN,VK_DOWN,0);navigation.message(nullptr,WM_KEYDOWN,VK_RIGHT,0);require(navigation.draft.shadowCascades==5);
  loaded=navigation.draft;loaded.shadowPcf=2;loaded.shadowBias=100;loaded.shadowNormal=80;loaded.shadowSlope=5;loaded.shadowDebug=1;save_graphics(dir/L"shadow.cfg",loaded);require(load_graphics(dir/L"shadow.cfg",enhanced)&&loaded==enhanced);require(enhanced.shadows().enabled&&enhanced.shadows().cascades==5);std::filesystem::remove(dir/L"shadow.cfg");enhanced.shadowCascades=7;require(!valid_graphics(enhanced));enhanced=loaded;enhanced.shadowBias=1001;require(!valid_graphics(enhanced));
  {std::ofstream f(dir/L"old.cfg");f<<"MGO2WIN.GRAPHICS 2 0 1280 720 0 1 2048 1 100 0 0 0";}require(load_graphics(dir/L"old.cfg",enhanced)&&!enhanced.shadowEnabled&&enhanced.shadowCascades==4);std::filesystem::remove(dir/L"old.cfg");
  GraphicsSettings restored(path);require(restored.draft==s.active);require(calls>=9);std::filesystem::remove(path);std::filesystem::remove(dir);
 }catch(...){std::error_code ec;std::filesystem::remove(path,ec);std::filesystem::remove(dir,ec);throw;}
 std::cout<<"Graphics rational refresh persistence, bounds, 15-second timeout, focus-loss/Esc rollback, apply-failure recovery and confirmed-only saving passed.\n";
}
