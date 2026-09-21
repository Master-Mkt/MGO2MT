#include "weapon_reticle.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace mgo2mt;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
bool close_float(float a,float b){return std::abs(a-b)<.0001f;}
reticle::Model model(){reticle::Model m;m.scope={1,2,{1,3,4},1,25};m.spreadRadians=.003f;m.shotWatermark=5;m.gameplay=m.active=m.alive=true;return m;}
void core(){
 auto m=model();reticle::Presentation p;auto base=p.update(m,.016);check(base.has_value(),"eligible equipped weapon");
 check(close_float(base->radiusX,base->radiusY)&&close_float(base->radiusY,float(std::tan(.003)/std::tan(.5)*360)),"vertical FOV1 and matching physical aspect project a circle");
 m.spreadRadians=.03f;m.shotWatermark++;auto wide=p.update(m,.016);
 check(wide&&close_float(wide->radiusY,float(std::tan(.03)/std::tan(.5)*360))&&wide->radiusY>base->radiusY,"HOST spread immediately expands with exact half-angle projection");
 m.spreadRadians=.003f;auto narrow=p.update(m,.016);check(narrow&&*narrow==*base,"HOST convergence has no stale interpolation or extra bloom");
 ++m.shotWatermark;check(p.update(m,.016)==narrow,"accepted edge with unchanged angle never invents spread");
 auto stale=m;--stale.shotWatermark;stale.spreadRadians=.04f;check(!p.update(stale,.016)&&p.update(m,.016)==narrow,"old watermark hidden without replacing current angle");
 for(unsigned field=0;field<7;++field){auto changed=m;switch(field){case 0:++changed.scope.epoch;break;case 1:++changed.scope.scene;break;case 2:++changed.scope.identity.slot;break;case 3:++changed.scope.identity.instance;break;case 4:++changed.scope.identity.character;break;case 5:++changed.scope.life;break;case 6:changed.scope.weapon=3;break;}changed.shotWatermark=0;changed.spreadRadians=0.f;auto g=p.update(changed,0);check(g&&g->radiusX==0&&g->radiusY==0,"every full scope change starts exact new angle with no prior shot replay");p.reset();check(p.update(m,.016).has_value(),"scope fixture re-established");}
 for(unsigned gate=0;gate<12;++gate){auto bad=m;double dt=.016;switch(gate){case 0:bad.gameplay=false;break;case 1:bad.active=false;break;case 2:bad.alive=false;break;case 3:bad.menuOpen=true;break;case 4:bad.eligible=false;break;case 5:bad.scope.weapon=0;break;case 6:bad.scope.life=0;break;case 7:bad.spreadRadians.reset();break;case 8:bad.spreadRadians=-1.f;break;case 9:bad.spreadRadians=std::numeric_limits<float>::quiet_NaN();break;case 10:dt=-1;break;case 11:dt=.251;break;}check(!p.update(bad,dt),"ineligible or invalid input hides and resets presentation");auto fresh=m;fresh.shotWatermark=0;fresh.spreadRadians=0.f;auto g=p.update(fresh,.016);check(g&&g->radiusX==0,"reset cannot retain prior spread or sequence floor");p.reset();p.update(m,.016);}
 m.scope.weapon=3;m.spreadRadians=0.f;check(p.update(m,.016).has_value(),"caller-approved held-only gun gets static reticle without ammo input");
 auto altered=reticle::geometry(.03f,300,250,{100,100,800,500,4.f/3.f});
 check(altered&&close_float(altered->radiusX/altered->radiusY,1.2f)&&altered->centerX==300&&altered->centerY==250,"physical camera aspect is independent from HUD destination aspect and off-center ray is retained");
 for(float x:{-1.f,1280.f,std::numeric_limits<float>::infinity()})check(!reticle::geometry(.03f,x,360,{}),"off-screen/invalid center never clamps into a false target");
 check(!reticle::geometry(.03f,640,720,{})&&!reticle::geometry(.03f,640,360,{0,0,1280,720,0}),"bottom edge and invalid aspect are hidden");
 for(float fov:{.15f,.4f,1.f,1.5f}){auto zoomed=reticle::geometry(.03f,640,360,{0,0,1280,720,16.f/9,fov});check(zoomed&&close_float(zoomed->radiusY,float(std::tan(.03)/std::tan(double(fov)*.5)*360)),"reticle cone follows active lens");}
 for(float fov:{0.f,-1.f,3.2f,std::numeric_limits<float>::quiet_NaN()})check(!reticle::geometry(.03f,640,360,{0,0,1280,720,16.f/9,fov}),"invalid lens hides reticle");
 reticle::Presentation independent;check(independent.update(model(),0)==base,"independent presentation has no shared scope");
}
void save(const std::filesystem::path& path,const BITMAPINFOHEADER& info,const void* pixels){BITMAPFILEHEADER h{};h.bfType=0x4d42;h.bfOffBits=sizeof(h)+sizeof(info);h.bfSize=h.bfOffBits+1280*720*4;std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(&h),sizeof(h));f.write(reinterpret_cast<const char*>(&info),sizeof(info));f.write(static_cast<const char*>(pixels),1280*720*4);check(bool(f),"reticle BMP saved");}
void pixels(const std::filesystem::path& out){
 HDC dc=CreateCompatibleDC(nullptr);check(dc!=nullptr,"memory GDI DC");BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=1280;info.bmiHeader.biHeight=-720;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* data=nullptr;
 auto dib=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&data,nullptr,0);check(dib&&data,"actual top-down 32-bit GDI surface");auto old=SelectObject(dc,dib);std::span<uint32_t> surface(static_cast<uint32_t*>(data),1280*720);constexpr uint32_t background=0xff20252b;
 auto render=[&](const reticle::Geometry& g){GdiFlush();std::fill(surface.begin(),surface.end(),background);reticle::paint(surface,1280,720,g);};
 auto wide=*reticle::geometry(.03f,640,360,{});render(wide);
 size_t n=std::count(surface.begin(),surface.end(),reticle::orange);check(n>100&&n<200,"bounded four brackets and center produce actual orange pixels");
 check(surface[360*1280+640]==reticle::orange&&surface[360*1280+620]==reticle::orange&&surface[360*1280+660]==reticle::orange,"center and cone-aligned left/right brackets are literal orange");
 check(std::all_of(surface.begin(),surface.end(),[](uint32_t x){return x==background||x==reticle::orange;}),"no theme color conversion or alpha corruption");
 if(!out.empty())save(out/"wide.bmp",info.bmiHeader,data);
 auto minimum=*reticle::geometry(0,640,360,{});render(minimum);check(surface[360*1280+646]==reticle::orange&&surface[360*1280+660]==background,"zero angle keeps six-pixel native visual gap without invented cone");
 if(!out.empty())save(out/"equipped_empty_or_static.bmp",info.bmiHeader,data);
 auto corner=*reticle::geometry(.03f,100,100,{100,100,800,500,4.f/3.f});render(corner);
 for(int y=0;y<720;++y)for(int x=0;x<1280;++x)if(x<100||y<100||x>=900||y>=600)check(surface[size_t(y)*1280+x]==background,"clipped strokes never escape the actual viewport");
 if(!out.empty())save(out/"off_center.bmp",info.bmiHeader,data);
 for(unsigned gate=0;gate<4;++gate){auto m=model();if(gate==0)m.alive=false;if(gate==1)m.menuOpen=true;if(gate==2)m.eligible=false;if(gate==3)m.scope.weapon=0;reticle::Presentation p;std::fill(surface.begin(),surface.end(),background);if(auto g=p.update(m,.016))reticle::paint(surface,1280,720,*g);check(std::all_of(surface.begin(),surface.end(),[](uint32_t x){return x==background;}),"dead/menu/ineligible/unequipped frame leaves no reticle pixels");}
 render(wide);std::vector<uint32_t> unchanged(surface.begin(),surface.end());auto invalid=wide;invalid.radiusX=std::numeric_limits<float>::infinity();reticle::paint(surface,1280,720,invalid);reticle::paint(surface.first(3),1280,720,wide);check(std::equal(surface.begin(),surface.end(),unchanged.begin()),"invalid geometry and short buffer preserve surface");
 weapon_effect::Reticle style;style.enabled=false;std::fill(surface.begin(),surface.end(),background);reticle::paint(surface,1280,720,wide,style);check(std::all_of(surface.begin(),surface.end(),[](uint32_t x){return x==background;}),"editor disabled reticle leaves surface intact");
 style.enabled=true;style.centerDot=false;style.color={0,1,0,.5f};style.length=20;style.thickness=4;reticle::paint(surface,1280,720,wide,style);check(surface[360*1280+640]==background&&surface[360*1280+660]!=background,"style changes strokes and hides dot without changing cone");check((surface[360*1280+660]>>24)==255&&((surface[360*1280+660]>>8)&255)>((background>>8)&255),"style alpha blends over opaque HUD correctly");
 std::fill(surface.begin(),surface.end(),0);style.centerDot=true;style.color={1,0,0,.25f};reticle::paint(surface,1280,720,wide,style);check(surface[360*1280+640]==0x40ff0000,"style straight alpha preserved on transparent HUD");
 SelectObject(dc,old);DeleteObject(dib);DeleteDC(dc);
}
}
int main(int argc,char** argv){try{core();std::filesystem::path out;if(argc==2){out=argv[1];std::filesystem::create_directories(out);}pixels(out);std::cout<<"weapon reticle PASS: exact cone/aspect, full scope, watermark, empty/static gun, gates, literal orange DIB, viewport and bounds\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
