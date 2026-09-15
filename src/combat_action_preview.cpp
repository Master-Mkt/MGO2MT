#include "stage_normals.h"
#include "combat_action_preview.h"
#include "combat_presentation.h"
#include "combat_death.h"
#include "motion_blend_presentation.h"
#include "combat_particle_effects.h"
#include "combat_runtime_effects_overlay.h"
#include "combat_light_effects.h"
#include "game_hud.h"
#include "enemy_name_tag.h"
#include "menu_font.h"
#include "character_renderer.h"
#include "weapon_hand_renderer.h"
#include "character_catalog.h"
#include "player_motion.h"
#include "stage_lighting.h"
#include "stage_navigation.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
using namespace mgo2win;
namespace {
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
void ok(HRESULT value){check(SUCCEEDED(value),"Player render D3D11 failure");}
std::vector<char> read(const std::filesystem::path& path){std::ifstream file(path,std::ios::binary);check(bool(file),"Player render input missing");return {(std::istreambuf_iterator<char>(file)),{}};}
struct Frame {unsigned width=0,height=0;std::vector<unsigned char> rgba;};
Frame frame(ID3D11Device* device,ID3D11DeviceContext* context,const CharacterRenderer& renderer){
 ComPtr<ID3D11Resource> resource;renderer.view()->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;ok(resource.As(&texture));D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
 desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> copy;ok(device->CreateTexture2D(&desc,nullptr,&copy));context->CopyResource(copy.Get(),texture.Get());
 D3D11_MAPPED_SUBRESOURCE mapped{};ok(context->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped));Frame result{desc.Width,desc.Height,std::vector<unsigned char>(size_t(desc.Width)*desc.Height*4)};
 for(unsigned y=0;y<desc.Height;++y)std::copy_n(static_cast<unsigned char*>(mapped.pData)+y*mapped.RowPitch,desc.Width*4,result.rgba.data()+size_t(y)*desc.Width*4);
 context->Unmap(copy.Get(),0);return result;
}
void write(const std::filesystem::path& path,const Frame& image){
 auto pixels=image.rgba;for(size_t i=0;i<pixels.size();i+=4)std::swap(pixels[i],pixels[i+2]);
 BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=LONG(image.width);info.biHeight=-LONG(image.height);info.biPlanes=1;info.biBitCount=32;info.biSizeImage=DWORD(pixels.size());file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+info.biSizeImage;
 std::ofstream output(path,std::ios::binary);output.write(reinterpret_cast<const char*>(&file),sizeof(file));output.write(reinterpret_cast<const char*>(&info),sizeof(info));output.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());check(bool(output),"Player render image write failed");
}
}

namespace mgo2win {
int run_combat_action_preview(const std::filesystem::path&data,const std::filesystem::path&output,bool shadowCapture,bool hemisphereCapture){try{
 std::filesystem::create_directories(output);CharacterCatalog catalog(read(data/"character/appearance.gwc"));PlayerMotionBank motions(read(data/"character/player.gwmot"));weapon_hand::Bank bank(read(data/"weapons/hands.gwh"));weapon_hand::Models models(data/"weapons");
 auto stageBytes=read(data/"stage/n022a.gwm");CharacterModel stageModel(stageBytes);stage::load_original_normals(stageModel,stageBytes,data/"stage/n022a.gwn");std::ifstream cf(data/"stage/n022a.collision.cfg"),lf(data/"stage/n022a.lighting.cfg");auto world=stage::Collision::read(cf);auto light=stage::Lighting::read(lf);for(auto&v:stageModel.vertices){auto l=light.sample({v.x,v.y,v.z},{v.nx,v.ny,v.nz});v.lr=l.color[0];v.lg=l.color[1];v.lb=l.color[2];v.lit=1;}
 stage::Navigation nav;check(nav.place(world,{-42878.8359f,3000,29781.7969f}),"Death fixture floor");const auto feet=nav.feet();constexpr float yaw=2.2f;
 std::array<uint8_t,28>a{};a[2]=11;a[3]=22;a[15]=46;a[17]=57;auto body=catalog.assemble(a);check(body.ready(),"Original body required");
 ComPtr<ID3D11Device>d;ComPtr<ID3D11DeviceContext>c;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c));CharacterRenderer stageRenderer(d.Get(),stageModel),renderer(d.Get(),body.model);stageRenderer.resize_target(d.Get(),1280,720);weapon_hand::Actor held;
 combat::Snapshot snapshot;snapshot.epoch=12;snapshot.revision=1;snapshot.eventWatermark=1;combat::Player p;p.identity={0,5,123};p.life=1;p.alive=true;p.hp=p.maxHp=p.stamina=p.maxStamina=1000;p.weapon=25;p.ammo=29;p.reserve=150;p.pose.feet=feet;p.pose.yaw=yaw;snapshot.players[0]=p;
 player::Ragdoll rag;combat::presentation::Death death;death.scope(rag,12,1,p.identity,p.life);combat::presentation::Reload reload;
 combat::particles::Pool particles;particles.synchronize(12,1,1,0);combat::LightEffects lights;lights.synchronize(12,1,1,0);
 menu_font_resources().load(data/"fonts");check(menu_font_resources().ready(),"Original Japanese font required");std::array<HFONT,4>fonts{create_menu_font(30,FW_BOLD),create_menu_font(23,FW_BOLD),create_menu_font(20,FW_BOLD),create_menu_font(16,FW_NORMAL)};
 unsigned rendered=0,totalCues=0;MotionPose lastLive;double seconds=0;
 std::ofstream report(output/"capture.json");report<<"{\"offlineRenderFixture\":true,\"originalStage\":\"n022a\",\"frames\":[";
 for(unsigned step=0;step<4;++step){
  if(step==0){lastLive=*motions.sample(PlayerMotion::Aim,0);auto hand=*bank.select(25,PlayerMotion::Aim,0,true);weapon_hand::upper_body(lastLive,hand,catalog.skeleton(0));catalog.pose(body,lastLive);held.update(d.Get(),c.Get(),models,body,hand,.1);combat::Event shot;shot.epoch=12;shot.id=2;shot.kind=combat::EventKind::shot;shot.source=p.identity;shot.sourceLife=1;shot.weapon=25;shot.position=*held.muzzle(yaw,feet);shot.normal={std::sin(yaw),0,std::cos(yaw)};snapshot.eventWatermark=2;particles.dispatch({&shot,1},snapshot,0);lights.dispatch({&shot,1},snapshot,0);}
  if(step==1){p.reloadUntil=4000;reload.update(12,&p,0);for(unsigned ms=10;ms<=700;ms+=10){p.reloadElapsedMs=uint16_t(ms/25*25);totalCues+=unsigned(reload.update(12,&p,ms).size());seconds=reload.seconds();lastLive=*motions.sample(PlayerMotion::Reload,seconds);auto hand=*bank.select(25,PlayerMotion::Reload,seconds);weapon_hand::upper_body(lastLive,hand,catalog.skeleton(0));catalog.pose(body,lastLive);held.update(d.Get(),c.Get(),models,body,hand,.01);++rendered;}check(held.magazine_visible(),"Detached original magazine visible");}
  if(step==2){p.alive=false;p.hp=0;p.reloadUntil=0;p.reloadElapsedMs=0;snapshot.players[0]=p;reload.update(12,&p,900);check(death.update(rag,catalog,0,lastLive,feet,yaw,p,snapshot),"Death handoff");held.clear();for(unsigned i=0;i<150;++i){rag.step(world,1.f/120);++rendered;}check(rag.active(),"Corpse physics remains active");catalog.pose(body,motion_blend::local_physics_pose(rag.pose(),yaw));}
  if(step==3){p.alive=true;p.hp=p.maxHp;p.life=2;p.ammo=30;death.scope(rag,12,1,p.identity,p.life);check(!rag.active(),"Respawn clears corpse");lastLive=*motions.sample(PlayerMotion::Aim,0);auto hand=*bank.select(25,PlayerMotion::Aim,0,true);weapon_hand::upper_body(lastLive,hand,catalog.skeleton(0));catalog.pose(body,lastLive);held.update(d.Get(),c.Get(),models,body,hand,.1);snapshot.players[0]=p;}
  renderer.update_vertices(c.Get(),body.model.vertices);auto origin=rag.active()?rag.origin():feet;float angle=yaw+1.57079632679f;WorldView camera;camera.eye={feet[0]-std::sin(angle)*3700,feet[1]+1900,feet[2]-std::cos(angle)*3700};camera.direction={feet[0]-camera.eye[0],feet[1]+750-camera.eye[1],feet[2]-camera.eye[2]};camera.aspect=16.f/9.f;
  auto pointLights=lights.sample(step==0?0:1000);stageRenderer.render(c.Get(),0,false,&camera,nullptr,nullptr,pointLights);renderer.render(c.Get(),yaw,false,&camera,&stageRenderer,&origin,pointLights);held.draw(c.Get(),yaw,camera,&stageRenderer,origin,pointLights);auto image=frame(d.Get(),c.Get(),stageRenderer);
  HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=1280;bi.bmiHeader.biHeight=-720;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;void* raw=nullptr;auto bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&raw,nullptr,0);auto old=SelectObject(dc,bitmap);auto pixels=std::span<uint32_t>(static_cast<uint32_t*>(raw),1280*720);
  for(size_t i=0;i<pixels.size();++i)pixels[i]=0xff000000u|(uint32_t(image.rgba[i*4])<<16)|(uint32_t(image.rgba[i*4+1])<<8)|image.rgba[i*4+2];
  if(step==0){auto parts=particles.sample(snapshot,0);auto lines=combat::runtime_effects::lines(parts,snapshot,0);combat::material_effects::paint(pixels,lines,camera.eye,camera.direction,world,nullptr,{0,0,1280,720,camera.aspect});check(parts.size()==5,"AK flash/smoke/casing rendered");}
  hud::Model hud;hud.name=L"プレイヤー";hud.weapon=L"AK102";hud.hp=p.hp;hud.maxHp=1000;hud.stamina=p.stamina;hud.maxStamina=1000;hud.ammo=p.ammo;hud.reserve=150;hud.alive=p.alive;hud.reloading=step==1;hud.rule=1;hud.remainingMs=178000;hud.respawnWaiting=step==2;hud.respawnRemainingMs=1750;
  hud::draw(dc,fonts,hud,0);GdiFlush();if(step==3){hud::EnemyNameTagRenderer tag;check(tag.paint(pixels,1280,720,640,205,"ターゲット表示テスト",nullptr,hud::EnemyVitals{22,630,1000}),"Target level and HP fixture");}for(size_t i=0;i<pixels.size();++i){auto v=pixels[i];image.rgba[i*4]=uint8_t(v>>16);image.rgba[i*4+1]=uint8_t(v>>8);image.rgba[i*4+2]=uint8_t(v);image.rgba[i*4+3]=255;}SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
  const char* names[]={"01-ak-fire.bmp","02-ak-reload.bmp","03-death.bmp","04-respawn.bmp"};write(output/names[step],image);if(step)report<<',';report<<"{\"file\":\""<<names[step]<<"\",\"life\":"<<p.life<<",\"alive\":"<<(p.alive?"true":"false")<<",\"physicsActive\":"<<(rag.active()?"true":"false")<<"}";++rendered;
 }
 report<<"]";
 if(hemisphereCapture){const auto env=light.environment(feet);report<<",\"actorHemisphereVolumes\":"<<env.volumes<<",\"actorHemisphereWeight\":"<<env.weight;check(env.volumes>0,"QQ actor samples original LT3 volumes");}
 if(shadowCapture||hemisphereCapture){
  WorldView camera;const float angle=yaw+1.57079632679f;camera.eye={feet[0]-std::sin(angle)*3700,feet[1]+1900,feet[2]-std::cos(angle)*3700};camera.direction={feet[0]-camera.eye[0],feet[1]+750-camera.eye[1],feet[2]-camera.eye[2]};camera.aspect=16.f/9.f;
  shadows::Renderer shadowPass;shadows::Settings settings;settings.cascades=4;settings.resolution=2048;
  std::vector<shadows::Caster> casters{{&stageRenderer,0,{}},{&renderer,yaw,feet}};held.shadow_casters(casters,yaw,feet);const auto environment=light.environment(feet);Frame original;report<<",\"shadowComparison\":[";
  for(unsigned variant=0;variant<3;++variant){settings.enabled=hemisphereCapture?variant==2:variant!=0;settings.visualize=!hemisphereCapture&&variant==2;check(shadowPass.configure(d.Get(),settings),"Original stage shadow allocation");
   const shadows::Renderer* receiver=nullptr;if(settings.enabled){check(shadowPass.render(c.Get(),camera,light.direction,light.direct,stageRenderer.bounds(),casters),"Original stage shadow depth");receiver=&shadowPass;}
   stageRenderer.render(c.Get(),0,false,&camera,nullptr,nullptr,{},nullptr,receiver);const auto* env=hemisphereCapture&&variant?&environment:nullptr;renderer.render(c.Get(),yaw,false,&camera,&stageRenderer,&feet,{},nullptr,receiver,env);held.draw(c.Get(),yaw,camera,&stageRenderer,feet,{},receiver,env);auto image=frame(d.Get(),c.Get(),stageRenderer);
   const char* names[]={hemisphereCapture?"hemisphere-off.bmp":"shadow-off.bmp",hemisphereCapture?"hemisphere-on.bmp":"shadow-on.bmp",hemisphereCapture?"hemisphere-shadow.bmp":"shadow-cascades.bmp"};write(output/names[variant],image);size_t changed=0;if(variant){for(size_t i=0;i<image.rgba.size();i+=4)if(!std::equal(image.rgba.begin()+i,image.rgba.begin()+i+3,original.rgba.begin()+i))++changed;check(changed>100,"Original stage shadow changes visible pixels");}else original=image;
   const auto stats=shadowPass.statistics();if(variant)report<<',';report<<"{\"file\":\""<<names[variant]<<"\",\"changedPixels\":"<<changed<<",\"cascades\":"<<stats.cascades<<",\"resolution\":"<<stats.resolution<<",\"allocatedBytes\":"<<stats.allocatedBytes<<",\"drawCalls\":"<<stats.drawCalls<<",\"triangles\":"<<stats.triangles<<"}";
  }report<<"]";
 }
 for(auto font:fonts)DeleteObject(font);report<<",\"simulatedFrames\":"<<rendered<<",\"reloadCues\":"<<totalCues<<",\"passed\":true}\n";std::cout<<"AK/death/respawn render fixture PASS "<<rendered<<" frames\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
}
