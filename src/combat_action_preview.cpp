#include "stage_sky.h"
#include "original_first_person_lens.h"
#include "stage_surface_alpha.h"
#include "first_person_transition.h"
#include "first_person_sight.h"
#include "stage_weather_renderer.h"
#include "stage_environment.h"
#include "stage_precipitation.h"
#include "tracer_renderer.h"
#include "physics_debug_render.h"
#include "stage_normals.h"
#include "stage_floor_blend.h"
#include "combat_action_preview.h"
#include "foot_ik.h"
#include "combat_presentation.h"
#include "combat_death.h"
#include "motion_blend_presentation.h"
#include "combat_particle_effects.h"
#include "combat_particle_renderer.h"
#include "combat_runtime_effects_overlay.h"
#include "combat_light_effects.h"
#include "game_hud.h"
#include "enemy_name_tag.h"
#include "menu_font.h"
#include "character_renderer.h"
#include "weapon_hand_renderer.h"
#include "installed_weapon_model.h"
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
using namespace mgo2mt;
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

namespace mgo2mt {
int run_combat_action_preview(const std::filesystem::path&data,const std::filesystem::path&output,bool shadowCapture,bool hemisphereCapture){try{
 std::filesystem::create_directories(output);CharacterCatalog catalog(read(data/"character/appearance.gwc"));PlayerMotionBank motions(read(data/"character/player.gwmot"));weapon_hand::Bank bank(read(data/"weapons/hands.gwh"));weapon_hand::Models models(data/"weapons");
 auto stageBytes=read(data/"stage/n022a.gwm");CharacterModel stageModel(stageBytes);stage::load_original_normals(stageModel,stageBytes,data/"stage/n022a.gwn");stage::load_floor_blend(stageModel,stageBytes,data/"stage/n022a.gfb");const auto alphaParts=stage::load_surface_alpha(stageModel,stageBytes,data/"stage/n022a.gsa");std::ifstream cf(data/"stage/n022a.collision.cfg"),lf(data/"stage/n022a.lighting.cfg");auto world=stage::Collision::read(cf);auto light=stage::Lighting::read(lf);for(auto&v:stageModel.vertices){auto l=light.sample({v.x,v.y,v.z},{v.nx,v.ny,v.nz});v.lr=l.color[0];v.lg=l.color[1];v.lb=l.color[2];v.lit=1;}
 stage::Navigation nav;check(nav.place(world,{-42878.8359f,3000,29781.7969f}),"Death fixture floor");const auto feet=nav.feet();constexpr float yaw=2.2f;
 std::array<uint8_t,28>a{};a[2]=11;a[3]=22;a[15]=46;a[17]=57;auto body=catalog.assemble(a);check(body.ready(),"Original body required");
 ComPtr<ID3D11Device>d;ComPtr<ID3D11DeviceContext>c;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c));CharacterRenderer stageRenderer(d.Get(),stageModel),renderer(d.Get(),body.model);stageRenderer.resize_target(d.Get(),1280,720);weapon_hand::Actor held;
 std::unique_ptr<combat::particles::Renderer> originalEffects;if(std::filesystem::exists(data/"fx/original.gwfx"))originalEffects=std::make_unique<combat::particles::Renderer>(d.Get(),data/"fx/original.gwfx");
 combat::Snapshot snapshot;snapshot.epoch=12;snapshot.revision=1;snapshot.eventWatermark=1;combat::Player p;p.identity={0,5,123};p.life=1;p.alive=true;p.hp=p.maxHp=p.stamina=p.maxStamina=1000;p.weapon=25;p.ammo=29;p.reserve=150;p.pose.feet=feet;p.pose.yaw=yaw;snapshot.players[0]=p;
 player::Ragdoll rag;combat::presentation::Death death;death.scope(rag,12,1,p.identity,p.life);combat::presentation::Reload reload;
 combat::particles::Pool particles;particles.synchronize(12,1,1,0);combat::LightEffects lights;lights.synchronize(12,1,1,0);
 auto casing=[&](const combat::Event&e)->std::optional<combat::particles::CasingEmission>{if(held.weapon_id()!=e.weapon)return {};auto axis=held.connection(0x443037,yaw,feet);if(!axis)return {};auto direction=axis->front;for(unsigned i=0;i<3;++i)direction[i]-=axis->rear[i];return combat::particles::CasingEmission{axis->rear,direction};};
 menu_font_resources().load(data/"fonts");check(menu_font_resources().ready(),"Original Japanese font required");std::array<HFONT,4>fonts{create_menu_font(30,FW_BOLD),create_menu_font(23,FW_BOLD),create_menu_font(20,FW_BOLD),create_menu_font(16,FW_NORMAL)};
 unsigned rendered=0,totalCues=0;MotionPose lastLive;double seconds=0;
 std::ofstream report(output/"capture.json");report<<"{\"offlineRenderFixture\":true,\"originalStage\":\"n022a\",\"frames\":[";
 for(unsigned step=0;step<4;++step){
  if(step==0){lastLive=*motions.sample(PlayerMotion::Aim,0);auto hand=*bank.select(25,PlayerMotion::Aim,0,true);weapon_hand::upper_body(lastLive,hand,catalog.skeleton(0));catalog.pose(body,lastLive);held.update(d.Get(),c.Get(),models,body,hand,.1);combat::Event shot;shot.epoch=12;shot.id=2;shot.kind=combat::EventKind::shot;shot.source=p.identity;shot.sourceLife=1;shot.weapon=25;shot.position=*held.muzzle(yaw,feet);shot.normal={std::sin(yaw),0,std::cos(yaw)};snapshot.eventWatermark=2;particles.dispatch({&shot,1},snapshot,0,casing);lights.dispatch({&shot,1},snapshot,0);}
  if(step==1){p.reloadUntil=4000;reload.update(12,&p,0);for(unsigned ms=10;ms<=700;ms+=10){p.reloadElapsedMs=uint16_t(ms/25*25);totalCues+=unsigned(reload.update(12,&p,ms).size());seconds=reload.seconds();lastLive=*motions.sample(PlayerMotion::Reload,seconds);auto hand=*bank.select(25,PlayerMotion::Reload,seconds);weapon_hand::upper_body(lastLive,hand,catalog.skeleton(0));catalog.pose(body,lastLive);held.update(d.Get(),c.Get(),models,body,hand,.01);++rendered;}check(held.magazine_visible(),"Detached original magazine visible");}
  if(step==2){p.alive=false;p.hp=0;p.reloadUntil=0;p.reloadElapsedMs=0;snapshot.players[0]=p;reload.update(12,&p,900);check(death.update(rag,catalog,0,lastLive,feet,yaw,p,snapshot),"Death handoff");held.clear();for(unsigned i=0;i<150;++i){rag.step(world,1.f/120);++rendered;}check(rag.active(),"Corpse physics remains active");catalog.pose(body,motion_blend::local_physics_pose(rag.pose(),yaw));}
  if(step==3){p.alive=true;p.hp=p.maxHp;p.life=2;p.ammo=30;death.scope(rag,12,1,p.identity,p.life);check(!rag.active(),"Respawn clears corpse");lastLive=*motions.sample(PlayerMotion::Aim,0);auto hand=*bank.select(25,PlayerMotion::Aim,0,true);weapon_hand::upper_body(lastLive,hand,catalog.skeleton(0));catalog.pose(body,lastLive);held.update(d.Get(),c.Get(),models,body,hand,.1);snapshot.players[0]=p;}
  renderer.update_vertices(c.Get(),body.model.vertices);auto origin=rag.active()?rag.origin():feet;float angle=yaw+1.57079632679f;WorldView camera;camera.eye={feet[0]-std::sin(angle)*3700,feet[1]+1900,feet[2]-std::cos(angle)*3700};camera.direction={feet[0]-camera.eye[0],feet[1]+750-camera.eye[1],feet[2]-camera.eye[2]};camera.aspect=16.f/9.f;
  auto pointLights=lights.sample(step==0?0:1000);stageRenderer.render(c.Get(),0,false,&camera,nullptr,nullptr,pointLights);renderer.render(c.Get(),yaw,false,&camera,&stageRenderer,&origin,pointLights);held.draw(c.Get(),yaw,camera,&stageRenderer,origin,pointLights);if(originalEffects&&step==0){auto sprites=particles.sprites(snapshot,0);check(sprites.size()==2,"Original AK flash and smoke sprites");check(originalEffects->render(c.Get(),stageRenderer,camera,sprites),"Original AK sprite draw");}auto image=frame(d.Get(),c.Get(),stageRenderer);
  HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=1280;bi.bmiHeader.biHeight=-720;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;void* raw=nullptr;auto bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&raw,nullptr,0);auto old=SelectObject(dc,bitmap);auto pixels=std::span<uint32_t>(static_cast<uint32_t*>(raw),1280*720);
  for(size_t i=0;i<pixels.size();++i)pixels[i]=0xff000000u|(uint32_t(image.rgba[i*4])<<16)|(uint32_t(image.rgba[i*4+1])<<8)|image.rgba[i*4+2];
  if(step==0){auto parts=particles.sample(snapshot,0);auto lines=combat::runtime_effects::lines(parts,snapshot,0,bool(originalEffects));combat::material_effects::paint(pixels,lines,camera.eye,camera.direction,world,nullptr,{0,0,1280,720,camera.aspect});check(parts.size()==5,"AK flash/smoke/casing sampled");}
  hud::Model hud;hud.name=L"プレイヤー";hud.weapon=L"AK102";hud.hp=p.hp;hud.maxHp=1000;hud.stamina=p.stamina;hud.maxStamina=1000;hud.ammo=p.ammo;hud.reserve=150;hud.alive=p.alive;hud.reloading=step==1;hud.rule=1;hud.remainingMs=178000;hud.respawnWaiting=step==2;hud.respawnRemainingMs=1750;
  hud::draw(dc,fonts,hud,0);GdiFlush();if(step==3){hud::EnemyNameTagRenderer tag;check(tag.paint(pixels,1280,720,640,205,"ターゲット表示テスト",nullptr,hud::EnemyVitals{22,630,1000}),"Target level and HP fixture");}for(size_t i=0;i<pixels.size();++i){auto v=pixels[i];image.rgba[i*4]=uint8_t(v>>16);image.rgba[i*4+1]=uint8_t(v>>8);image.rgba[i*4+2]=uint8_t(v);image.rgba[i*4+3]=255;}SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
  const char* names[]={"01-ak-fire.bmp","02-ak-reload.bmp","03-death.bmp","04-respawn.bmp"};write(output/names[step],image);if(step)report<<',';report<<"{\"file\":\""<<names[step]<<"\",\"life\":"<<p.life<<",\"alive\":"<<(p.alive?"true":"false")<<",\"physicsActive\":"<<(rag.active()?"true":"false")<<"}";++rendered;
 }
 report<<"]";
 if(originalEffects){
  report<<",\"originalEffectTextures\":"<<originalEffects->textures()<<",\"weaponEffectFrames\":[";
  for(unsigned scenario=0;scenario<3;++scenario){
   p.weapon=scenario==0?24:scenario==1?50:56;snapshot.players[0]=p;lastLive=*motions.sample(PlayerMotion::Aim,0);auto hand=bank.select(p.weapon,PlayerMotion::Aim,0,true);held.clear();if(hand){weapon_hand::upper_body(lastLive,*hand,catalog.skeleton(0));catalog.pose(body,lastLive);held.update(d.Get(),c.Get(),models,body,*hand,.1);}else catalog.pose(body,lastLive);renderer.update_vertices(c.Get(),body.model.vertices);
   particles.clear();const uint64_t watermark=10+scenario;particles.synchronize(12,1,watermark,5000);combat::Event e;e.epoch=12;e.id=watermark+1;e.source=p.identity;e.sourceLife=p.life;e.weapon=p.weapon;e.normal={std::sin(yaw),0,std::cos(yaw)};e.kind=scenario==0?combat::EventKind::shot:scenario==1?combat::EventKind::explosion:combat::EventKind::smoke;e.position={feet[0]+e.normal[0]*1200,feet[1]+300,feet[2]+e.normal[2]*1200};if(scenario==0){auto muzzle=held.muzzle(yaw,feet);check(bool(muzzle),"Original M4 muzzle fixture");e.position=*muzzle;}snapshot.eventWatermark=e.id;particles.dispatch({&e,1},snapshot,5000,casing);
   WorldView camera;float angle=yaw+1.57079632679f;camera.eye={feet[0]-std::sin(angle)*5000,feet[1]+2200,feet[2]-std::cos(angle)*5000};camera.direction={feet[0]-camera.eye[0],feet[1]+850-camera.eye[1],feet[2]-camera.eye[2]};camera.aspect=16.f/9.f;const auto env=light.environment(feet);stageRenderer.render(c.Get(),0,false,&camera);renderer.render(c.Get(),yaw,false,&camera,&stageRenderer,&feet,{},nullptr,nullptr,&env);held.draw(c.Get(),yaw,camera,&stageRenderer,feet,{},nullptr,&env);auto sprites=particles.sprites(snapshot,scenario==0?5000:scenario==1?5200:7000);check(!sprites.empty()&&originalEffects->render(c.Get(),stageRenderer,camera,sprites),"Original expanded weapon effect draw");auto image=frame(d.Get(),c.Get(),stageRenderer);const char* names[]={"05-m4-fire.bmp","06-rpg-explosion.bmp","07-smoke-grenade.bmp"};write(output/names[scenario],image);if(scenario)report<<',';report<<"{\"file\":\""<<names[scenario]<<"\",\"weapon\":"<<p.weapon<<",\"sprites\":"<<sprites.size()<<",\"originalTextures\":true}";++rendered;
  }report<<"]";
  // Preserve the optional AK lighting comparison contract after extra fixtures.
  p.weapon=25;snapshot.players[0]=p;lastLive=*motions.sample(PlayerMotion::Aim,0);auto hand=*bank.select(25,PlayerMotion::Aim,0,true);weapon_hand::upper_body(lastLive,hand,catalog.skeleton(0));catalog.pose(body,lastLive);held.update(d.Get(),c.Get(),models,body,hand,.1);renderer.update_vertices(c.Get(),body.model.vertices);
 }
 {
  const uint16_t weapon=models.find(24)?24:25;auto hand=bank.select(weapon,PlayerMotion::Aim,0,true);check(bool(hand),"First person weapon pose");auto pose=*motions.sample(PlayerMotion::Aim,0);weapon_hand::upper_body(pose,*hand,catalog.skeleton(0));catalog.pose(body,pose);held.clear();check(held.update(d.Get(),c.Get(),models,body,*hand,.1),"First person original weapon");
  auto arms=weapon_hand::first_person_arms(body,catalog.skeleton(0));check(!arms.indices.empty()&&arms.indices.size()<body.model.indices.size(),"Original arm subset excludes head and torso");CharacterRenderer armRenderer(d.Get(),arms);WorldView camera{nav.eye(),{std::sin(yaw),0,std::cos(yaw)},16.f/9.f,original_first_person::vertical_fov_degrees(weapon,0,16.f/9.f)*.0174532925199433f};const auto env=light.environment(feet);
  stageRenderer.render(c.Get(),0,false,&camera);const auto base=frame(d.Get(),c.Get(),stageRenderer);armRenderer.render(c.Get(),yaw,false,&camera,&stageRenderer,&feet,{},nullptr,nullptr,&env);held.draw(c.Get(),yaw,camera,&stageRenderer,feet,{},nullptr,&env);auto image=frame(d.Get(),c.Get(),stageRenderer);size_t changed=0;for(size_t i=0;i<image.rgba.size();i+=4)if(!std::equal(image.rgba.begin()+i,image.rgba.begin()+i+3,base.rgba.begin()+i))++changed;check(changed>100,"First person arms and weapon visible");write(output/"08-first-person.bmp",image);report<<",\"firstPerson\":{\"weapon\":"<<weapon<<",\"armTriangles\":"<<arms.indices.size()/3<<",\"changedPixels\":"<<changed<<",\"file\":\"08-first-person.bmp\"}";++rendered;
  auto remainder=weapon_hand::first_person_body(body,catalog.skeleton(0));CharacterRenderer fadingBody(d.Get(),remainder);FirstPersonTransition transition;WorldView third=camera;third.verticalFov=1.f;third.eye[0]-=std::sin(yaw)*2600;third.eye[2]-=std::cos(yaw)*2600;third.eye[0]+=source_screen_x*std::cos(yaw)*280;third.eye[2]-=source_screen_x*std::sin(yaw)*280;transition.update(third,false,0);transition.update(camera,true,1000);
  report<<",\"firstPersonTransition\":[";unsigned shot=0;
  for(uint64_t ms:{0ull,35ull,180ull}){auto travel=transition.update(camera,true,1000+ms);stageRenderer.render(c.Get(),0,false,&travel);if(transition.body_opacity()>0)fadingBody.render(c.Get(),yaw,false,&travel,&stageRenderer,&feet,{},nullptr,nullptr,&env,transition.body_opacity());armRenderer.render(c.Get(),yaw,false,&travel,&stageRenderer,&feet,{},nullptr,nullptr,&env);held.draw(c.Get(),yaw,travel,&stageRenderer,feet,{},nullptr,&env);auto image=frame(d.Get(),c.Get(),stageRenderer);const char* names[]={"10-fp-transition-start.bmp","11-fp-transition-fade.bmp","12-fp-transition-arrived.bmp"};write(output/names[shot],image);if(shot)report<<',';report<<"{\"milliseconds\":"<<ms<<",\"bodyOpacity\":"<<transition.body_opacity()<<",\"file\":\""<<names[shot]<<"\"}";++shot;++rendered;}report<<"]";
  report<<",\"hawkeye\":[";for(unsigned level:{0u,3u}){camera.verticalFov=original_first_person::vertical_fov_degrees(weapon,level,camera.aspect)*.0174532925199433f;stageRenderer.render(c.Get(),0,false,&camera);armRenderer.render(c.Get(),yaw,false,&camera,&stageRenderer,&feet,{},nullptr,nullptr,&env);held.draw(c.Get(),yaw,camera,&stageRenderer,feet,{},nullptr,&env);auto image=frame(d.Get(),c.Get(),stageRenderer);const char* name=level?"14-hawkeye-level3.bmp":"13-hawkeye-level0.bmp";write(output/name,image);if(level)report<<',';report<<"{\"level\":"<<level<<",\"verticalFov\":"<<camera.verticalFov<<",\"magnification\":"<<original_first_person::aim_zoom(weapon,level)<<",\"file\":\""<<name<<"\"}";++rendered;}report<<"]";
  report<<",\"sightAlignment\":[";unsigned sightIndex=0;
  for(uint16_t id:{24,25}){
   const auto* source=models.find(id);if(!source||!first_person_sight::local_axis(id,*source,models.connection_points()))continue; // Older small fixtures lack original CNP provenance.
   auto h=bank.select(id,PlayerMotion::Aim,0,true);check(bool(h),"CNP fixture hold");auto posed=*motions.sample(PlayerMotion::Aim,0);weapon_hand::upper_body(posed,*h,catalog.skeleton(0));catalog.pose(body,posed);held.clear();check(held.update(d.Get(),c.Get(),models,body,*h,.1),"CNP fixture original weapon");armRenderer.update_vertices(c.Get(),body.model.vertices);
   WorldView original{nav.eye(),{std::sin(yaw),0,std::cos(yaw)},16.f/9.f,original_first_person::vertical_fov_degrees(id,0,16.f/9.f)*.0174532925199433f};auto axis=held.sight_axis(yaw,feet);check(bool(axis),"Original CNP sight pair");auto aligned=first_person_sight::Controller{}.update(original,id,axis);check(aligned.calibrated,"CNP eye calibration");check(aligned.view.direction==original.direction&&aligned.view.verticalFov==original.verticalFov,"CNP preserves aiming direction and lens");
   stageRenderer.render(c.Get(),0,false,&aligned.view);armRenderer.render(c.Get(),yaw,false,&aligned.view,&stageRenderer,&feet,{},nullptr,nullptr,&env);held.draw(c.Get(),yaw,aligned.view,&stageRenderer,feet,{},nullptr,&env);auto image=frame(d.Get(),c.Get(),stageRenderer);const char* name=id==24?"17-m4-cnp-sight.bmp":"18-ak-cnp-sight.bmp";write(output/name,image);
   if(sightIndex++)report<<',';report<<"{\"weapon\":"<<id<<",\"displacement\":"<<aligned.displacement<<",\"axisAngleDegrees\":"<<aligned.axisAngleRadians*57.2957795f<<",\"file\":\""<<name<<"\"}";++rendered;
  }report<<"]";
  lastLive=*motions.sample(PlayerMotion::Aim,0);auto ak=*bank.select(25,PlayerMotion::Aim,0,true);weapon_hand::upper_body(lastLive,ak,catalog.skeleton(0));catalog.pose(body,lastLive);held.clear();held.update(d.Get(),c.Get(),models,body,ak,.1);renderer.update_vertices(c.Get(),body.model.vertices);
 }
 if(const auto* source=models.find(66)){
  std::optional<stage::CollisionHit> wall;
  for(unsigned i=0;i<32&&!wall;++i){const float angle=float(i)*6.283185307f/32;auto hit=world.ray(nav.eye(),{std::sin(angle),0,std::cos(angle)},10000);if(hit&&std::abs(hit->normal[1])<.2f)wall=hit;}
  if(wall){const auto normal=wall->normal;auto placed=installed_weapon_model(*source,normal,0);CharacterRenderer object(d.Get(),placed);auto origin=wall->position;WorldView camera;for(unsigned i=0;i<3;++i){origin[i]+=normal[i]*4;camera.eye[i]=origin[i]+normal[i]*700;camera.direction[i]=-normal[i];}camera.aspect=16.f/9.f;const auto env=light.environment(origin);stageRenderer.render(c.Get(),0,false,&camera);const auto before=frame(d.Get(),c.Get(),stageRenderer);object.render(c.Get(),0,false,&camera,&stageRenderer,&origin,{},nullptr,nullptr,&env);auto image=frame(d.Get(),c.Get(),stageRenderer);size_t changed=0;for(size_t i=0;i<image.rgba.size();i+=4)if(!std::equal(image.rgba.begin()+i,image.rgba.begin()+i+3,before.rgba.begin()+i))++changed;check(changed>20,"Original C4 visible on QQ wall");write(output/"09-wall-c4.bmp",image);report<<",\"wallPlacement\":{\"weapon\":66,\"changedPixels\":"<<changed<<",\"file\":\"09-wall-c4.bmp\"}";++rendered;}
 }
 if(std::filesystem::exists(data/"stage/n022a.sky.cfg")){
  auto skyModel=CharacterModel(read(data/"stage/n022a.sky.gwm"),ModelExtent::sky);std::ifstream settingsFile(data/"stage/n022a.sky.cfg");auto settings=stage::SkySettings::read(settingsFile);settings.prepare(skyModel);CharacterRenderer sky(d.Get(),skyModel,true);WorldView camera{nav.eye(),{std::sin(yaw)*.35f,1.5f,std::cos(yaw)*.35f},16.f/9.f};Frame previous;report<<",\"skyMotion\":[";unsigned variant=0;
  for(double seconds:{0.,60.}){auto pose=settings.sample(seconds);SkyFrame draw{pose.position,pose.degrees,settings.color,settings.fogColor,settings.fog,pose.cloudU};stageRenderer.render(c.Get(),0,false,&camera);sky.render(c.Get(),0,false,&camera,&stageRenderer,nullptr,{},nullptr,nullptr,nullptr,1.f,CharacterPass::all,&draw);auto image=frame(d.Get(),c.Get(),stageRenderer);size_t changed=0;if(variant){for(size_t i=0;i<image.rgba.size();i+=4)if(!std::equal(image.rgba.begin()+i,image.rgba.begin()+i+3,previous.rgba.begin()+i))++changed;check(changed>100,"Original cloud rotation changes sky pixels");}else previous=image;const char* name=variant?"16-sky-after60seconds.bmp":"15-sky-start.bmp";write(output/name,image);if(variant)report<<',';report<<"{\"seconds\":"<<seconds<<",\"yawDegrees\":"<<pose.degrees[1]<<",\"changedPixels\":"<<changed<<",\"file\":\""<<name<<"\"}";++variant;++rendered;}report<<"]";
 }
 {
  WorldView camera;const float angle=yaw+1.57079632679f;camera.eye={feet[0]-std::sin(angle)*4200,feet[1]+2200,feet[2]-std::cos(angle)*4200};camera.direction={feet[0]-camera.eye[0],feet[1]+750-camera.eye[1],feet[2]-camera.eye[2]};camera.aspect=16.f/9.f;const auto env=light.environment(feet);
  auto scene=[&](){stageRenderer.render(c.Get(),0,false,&camera);renderer.render(c.Get(),yaw,false,&camera,&stageRenderer,&feet,{},nullptr,nullptr,&env);held.draw(c.Get(),yaw,camera,&stageRenderer,feet,{},nullptr,&env);};scene();auto clear=frame(d.Get(),c.Get(),stageRenderer);write(output/"19-qq-clear.bmp",clear);++rendered;
  if(std::filesystem::exists(data/"fx/weather.gwfx")){
   const auto bodyCamera=camera;camera.eye={feet[0],feet[1]+2800,feet[2]};float longest=0;
   for(unsigned i=0;i<64;++i){const float angle=float(i)*6.283185307f/64;stage::Vec3 direction{std::sin(angle),-.08f,std::cos(angle)};if(auto hit=world.ray(camera.eye,direction,90000);hit&&hit->distance>longest){longest=hit->distance;camera.direction=direction;}}
   scene();std::unique_ptr<CharacterRenderer> sky;
   if(std::filesystem::exists(data/"stage/n022a.sky.cfg")){auto m=CharacterModel(read(data/"stage/n022a.sky.gwm"),ModelExtent::sky);std::ifstream file(data/"stage/n022a.sky.cfg");auto s=stage::SkySettings::read(file);s.prepare(m);sky=std::make_unique<CharacterRenderer>(d.Get(),m,true);auto p=s.sample(12);SkyFrame f{p.position,p.degrees,s.color,s.fogColor,s.fog,p.cloudU};sky->render(c.Get(),0,false,&camera,&stageRenderer,nullptr,{},nullptr,nullptr,nullptr,1.f,CharacterPass::all,&f);}
   clear=frame(d.Get(),c.Get(),stageRenderer);write(output/"19-qq-clear.bmp",clear);
   stage::weather::Renderer weather(d.Get(),data/"fx/weather.gwfx");const auto wind=stage::weather::Controller{}.sample("n022a",12,camera.eye,&world);check(wind.active,"QQ weather enabled");check(weather.render(c.Get(),stageRenderer,camera,wind),"QQ original dust/fog render");auto storm=frame(d.Get(),c.Get(),stageRenderer);size_t changed=0;for(size_t i=0;i<storm.rgba.size();i+=4)if(!std::equal(storm.rgba.begin()+i,storm.rgba.begin()+i+3,clear.rgba.begin()+i))++changed;check(changed>100,"QQ requested fog changes distant scene");write(output/"20-qq-fog-sandstorm.bmp",storm);report<<",\"qqWeather\":{\"seconds\":12,\"strength\":"<<wind.strength<<",\"outdoors\":"<<(wind.outdoors?"true":"false")<<",\"dustSprites\":"<<wind.dust.size()<<",\"changedPixels\":"<<changed<<",\"file\":\"20-qq-fog-sandstorm.bmp\"}";++rendered;camera=bodyCamera;
  }
  scene();auto debug=frame(d.Get(),c.Get(),stageRenderer);physics_debug::Frame wires;wires.standing(feet,nav.capsule());wires.hit_regions(feet,yaw,host_hit::Stance::standing);wires.skeleton(body,catalog.skeleton(0),feet,yaw);if(auto axis=held.sight_axis(yaw,feet))wires.line(axis->rear,axis->front,physics_debug::Kind::attachment);for(const auto& [key,connection]:held.connection_frames(yaw,feet))wires.line(connection.rear,connection.front,physics_debug::Kind::attachment);wires.terrain(world,camera.eye);
  std::vector<uint32_t> pixels(size_t(debug.width)*debug.height);for(size_t i=0;i<pixels.size();++i)pixels[i]=0xff000000u|(uint32_t(debug.rgba[i*4])<<16)|(uint32_t(debug.rgba[i*4+1])<<8)|debug.rgba[i*4+2];const auto stats=physics_debug::paint(pixels,debug.width,debug.height,wires,{camera.eye,camera.direction,0,0,1280,720,camera.aspect,camera.verticalFov});check(stats.pixels>1000&&stats.triangles>0,"F12 original terrain, capsule and bone overlay");hud::EnemyNameTagRenderer label;label.paint(pixels,1280,720,640,45,"F12  PHYSICS / BONES / CNP",nullptr);for(size_t i=0;i<pixels.size();++i){debug.rgba[i*4]=uint8_t(pixels[i]>>16);debug.rgba[i*4+1]=uint8_t(pixels[i]>>8);debug.rgba[i*4+2]=uint8_t(pixels[i]);}write(output/"21-f12-physics-bones.bmp",debug);report<<",\"physicsDebug\":{\"lines\":"<<stats.lines<<",\"terrainTriangles\":"<<stats.triangles<<",\"pixels\":"<<stats.pixels<<",\"omitted\":"<<stats.omitted<<",\"file\":\"21-f12-physics-bones.bmp\"}";++rendered;
 }

 {
  auto source=std::make_shared<const CharacterModel>(stageModel);auto authored=std::make_shared<const stage::Lighting>(light);stage::EnvironmentCache environmentCache;
  WorldView camera;const float angle=yaw+1.57079632679f;camera.eye={feet[0]-std::sin(angle)*4200,feet[1]+2200,feet[2]-std::cos(angle)*4200};camera.direction={feet[0]-camera.eye[0],feet[1]+750-camera.eye[1],feet[2]-camera.eye[2]};camera.aspect=16.f/9.f;
  Frame initial;report<<",\"hostEnvironment\":[";
  for(unsigned variant=0;variant<3;++variant){environment::Config cfg;cfg.time=variant==1?environment::Time::night:environment::Time::day;if(variant==2){cfg.manualHemisphere=true;cfg.upper={80,160,255};cfg.lower={190,65,28};cfg.gainMilli=1250;}
   environmentCache.update(source,authored,cfg);CharacterRenderer changedStage(d.Get(),*environmentCache.model());changedStage.resize_target(d.Get(),1280,720);auto env=environmentCache.lighting()->environment(feet);changedStage.render(c.Get(),0,false,&camera);renderer.render(c.Get(),yaw,false,&camera,&changedStage,&feet,{},nullptr,nullptr,&env);held.draw(c.Get(),yaw,camera,&changedStage,feet,{},nullptr,&env);auto image=frame(d.Get(),c.Get(),changedStage);size_t changed=0;if(variant){for(size_t i=0;i<image.rgba.size();i+=4)if(!std::equal(image.rgba.begin()+i,image.rgba.begin()+i+3,initial.rgba.begin()+i))++changed;check(changed>1000,"HOST environment affects visible terrain and actor");}else initial=image;
   const char* names[]={"22-host-day.bmp","23-host-night.bmp","24-host-manual-hemisphere.bmp"};write(output/names[variant],image);if(variant)report<<',';report<<"{\"file\":\""<<names[variant]<<"\",\"nativePreset\":true,\"manualHemisphere\":"<<(cfg.manualHemisphere?"true":"false")<<",\"changedPixels\":"<<changed<<"}";++rendered;
  }report<<"]";
  if(std::filesystem::exists(data/"fx/weather.gwfx")){
   // Move the fixture above a real exposed QQ surface, never invent a floor.
   auto exposed=world.ray({feet[0],feet[1]+25000,feet[2]},{0,-1,0},50000);check(bool(exposed),"weather fixture exposed surface");camera.eye={exposed->position[0],exposed->position[1]+2500,exposed->position[2]};camera.direction={std::sin(yaw),-.4f,std::cos(yaw)};
   float longestWeatherView=0;for(unsigned i=0;i<64;++i){const float angle=i*6.283185307f/64;stage::Vec3 direction{std::sin(angle),-.25f,std::cos(angle)};auto hit=world.ray(camera.eye,direction,90000);if(hit&&hit->distance>longestWeatherView){longestWeatherView=hit->distance;camera.direction=direction;}}
   stage::weather::Renderer weather(d.Get(),data/"fx/weather.gwfx");combat::tracers::Renderer precipitation(d.Get());auto collision=std::make_shared<const stage::Collision>(world);report<<",\"hostPrecipitation\":[";
   for(unsigned variant=0;variant<2;++variant){stage::weather::SurfaceController surface;auto settings=stage::weather::Settings::defaults("n022a");settings.preset=stage::weather::Preset::clear;settings.rainEnabled=variant==0;settings.snowEnabled=variant==1;settings.wetSeconds=settings.snowSeconds=1;std::shared_ptr<const stage::weather::SurfaceGrid> grid;
    for(unsigned step=0;step<110;++step)grid=surface.sample(settings,step*.025,camera.eye,collision,1);check(bool(grid),"weather ground accumulated");stageRenderer.render(c.Get(),0,false,&camera);auto before=frame(d.Get(),c.Get(),stageRenderer);stage::weather::Frame effect;effect.surface=grid;weather.render(c.Get(),stageRenderer,camera,effect);auto surfaceImage=frame(d.Get(),c.Get(),stageRenderer);size_t surfaceChanged=0;for(size_t i=0;i<surfaceImage.rgba.size();i+=4)if(!std::equal(surfaceImage.rgba.begin()+i,surfaceImage.rgba.begin()+i+3,before.rgba.begin()+i))++surfaceChanged;check(surfaceChanged>100,"QQ exposed wet/snow ground pixels visible independently of particles");auto drops=stage::weather::precipitation(settings,12,camera.eye,&world);check(!drops.empty(),"native rain/snow visible over exposed QQ surface");precipitation.render(c.Get(),stageRenderer,camera,drops);auto image=frame(d.Get(),c.Get(),stageRenderer);size_t changed=0;for(size_t i=0;i<image.rgba.size();i+=4)if(!std::equal(image.rgba.begin()+i,image.rgba.begin()+i+3,before.rgba.begin()+i))++changed;check(changed>100,"rain/snow changes QQ image");const char* name=variant?"26-host-snow.bmp":"25-host-rain.bmp";write(output/name,image);if(variant)report<<',';report<<"{\"file\":\""<<name<<"\",\"nativePrecipitation\":true,\"acceleratedAccumulationFixture\":true,\"particles\":"<<drops.size()<<",\"surfaceChangedPixels\":"<<surfaceChanged<<",\"changedPixels\":"<<changed<<"}";++rendered;
   }report<<"]";
  }
 }
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
 // Compare the same original skin/animation/collision before and after IK.
 // Search only admitted positions near the existing QQ fixture, never move a
 // live player or change source vertices to construct a more dramatic result.
 {
  auto base=*motions.sample(PlayerMotion::Run,0);float lowest=1e9f;
  for(unsigned f=0;f<60;++f){auto sampled=*motions.sample(PlayerMotion::Run,double(f)/60);catalog.pose(body,sampled);float low=1e9f;
   for(size_t i=0;i<body.model.vertices.size();++i){float foot=0;for(unsigned j=0;j<4;++j){auto key=catalog.skeleton(0)[body.skin[i].bones[j]].key;if(key==0xfb4232||key==0xf81206||key==0x5b4a33||key==0xfb1246)foot+=body.skin[i].weights[j];}if(foot>.5f)low=std::min(low,body.model.vertices[i].y);}
   if(low<lowest){lowest=low;base=std::move(sampled);}
  }
  weapon_hand::upper_body(base,*bank.select(25,PlayerMotion::Run,.23,false),catalog.skeleton(0));
  stage::Vec3 chosen=feet;float best=-1;unsigned admitted=0;
  for(int x=-4;x<=4;++x)for(int z=-4;z<=4;++z){auto hint=feet;hint[0]+=float(x)*300;hint[2]+=float(z)*300;hint[1]+=600;stage::Navigation candidate;
   if(!candidate.place(world,hint,1200))continue;++admitted;foot_ik::Solver trial;
   trial.solve(catalog,body,base,&world,candidate.feet(),yaw,1.f/60,true,{1,1,1,1,1});auto r=trial.result();
   float score=0;for(const auto&f:r.feet)if(f.supported)score=std::max(score,f.correction);
   if(r.active&&score>best){best=score;chosen=candidate.feet();}
  }
  foot_ik::Solver solver;MotionPose fixed;
  for(unsigned i=0;i<60;++i)fixed=solver.solve(catalog,body,base,&world,chosen,yaw,1.f/60,true,{1,1,1,1,1});
  check(solver.result().active,"Original QQ foot IK contacts");
  report<<",\"footIk\":{\"admittedPositions\":"<<admitted<<",\"origin\":["<<chosen[0]<<','<<chosen[1]<<','<<chosen[2]<<"],\"maximumLift\":"<<best<<",\"pelvis\":"<<solver.result().pelvis<<",\"frames\":[";
  Frame before;for(unsigned variant=0;variant<3;++variant){catalog.pose(body,variant?fixed:base);renderer.update_vertices(c.Get(),body.model.vertices);
   auto hand=*bank.select(25,PlayerMotion::Run,.23,false);held.update(d.Get(),c.Get(),models,body,hand,.1);
   float angle=yaw+1.57079632679f;float distance=variant==2?3300.f:1600.f;float height=variant==2?800.f:180.f;WorldView camera;
   camera.eye={chosen[0]-std::sin(angle)*distance,chosen[1]+height+300,chosen[2]-std::cos(angle)*distance};camera.direction={chosen[0]-camera.eye[0],chosen[1]+height-camera.eye[1],chosen[2]-camera.eye[2]};camera.aspect=16.f/9.f;
   auto environment=light.environment(chosen);stageRenderer.render(c.Get(),0,false,&camera);renderer.render(c.Get(),yaw,false,&camera,&stageRenderer,&chosen,{},nullptr,nullptr,&environment);held.draw(c.Get(),yaw,camera,&stageRenderer,chosen,{},nullptr,&environment);
   auto image=frame(d.Get(),c.Get(),stageRenderer);const char* names[]={"27-foot-ik-before.bmp","28-foot-ik-after.bmp","29-foot-ik-full.bmp"};write(output/names[variant],image);size_t changed=0;
   if(!variant)before=image;else if(variant==1){for(size_t p=0;p<image.rgba.size();p+=4)if(!std::equal(image.rgba.begin()+p,image.rgba.begin()+p+4,before.rgba.begin()+p))++changed;check(changed>20,"Foot correction changes actual rendered pixels");}
   if(variant)report<<',';report<<"{\"file\":\""<<names[variant]<<"\",\"changedPixels\":"<<changed<<'}';++rendered;
  }report<<"]}";
 }
 for(auto font:fonts)DeleteObject(font);report<<",\"surfaceAlphaParts\":"<<alphaParts<<",\"simulatedFrames\":"<<rendered<<",\"reloadCues\":"<<totalCues<<",\"passed\":true}\n";std::cout<<"AK/death/respawn render fixture PASS "<<rendered<<" frames\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
}
