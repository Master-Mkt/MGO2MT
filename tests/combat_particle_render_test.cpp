#include "combat_particle_renderer.h"
#include <algorithm>
#include <fstream>
#include <iostream>
using namespace mgo2mt;using Microsoft::WRL::ComPtr;
namespace {
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
void ok(HRESULT h){check(SUCCEEDED(h),"WARP call");}
std::vector<uint8_t> pixels(ID3D11Device*d,ID3D11DeviceContext*c,const CharacterRenderer&r){ComPtr<ID3D11Resource> resource;r.view()->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;ok(resource.As(&texture));D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> staging;ok(d->CreateTexture2D(&desc,nullptr,&staging));c->CopyResource(staging.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE m{};ok(c->Map(staging.Get(),0,D3D11_MAP_READ,0,&m));std::vector<uint8_t> result(desc.Width*desc.Height*4);for(size_t y=0;y<desc.Height;++y)std::copy_n(static_cast<uint8_t*>(m.pData)+y*m.RowPitch,desc.Width*4,result.data()+y*desc.Width*4);c->Unmap(staging.Get(),0);return result;}
CharacterModel panel(float z){CharacterModel m;m.bounds={-10000,-10000,z,10000,10000,z};for(auto p:std::array<std::array<float,2>,4>{{{-10000,-10000},{10000,-10000},{10000,10000},{-10000,10000}}}){ModelVertex v{};v.x=p[0];v.y=p[1];v.z=z;v.nz=-1;v.lr=v.lg=v.lb=.1f;v.lit=1;m.vertices.push_back(v);}m.indices={0,1,2,0,2,3};m.parts.push_back({0,6,0,0});m.textures.push_back({4,4,9,{255,255,255,255,0,0,0,0}});return m;}
}
int main(int argc,char**argv){try{
 check(argc>=2,"original bundle path required");for(auto requested:{D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_0}){ComPtr<ID3D11Device>d;ComPtr<ID3D11DeviceContext>c;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,&requested,1,D3D11_SDK_VERSION,&d,&level,&c));combat::particles::Renderer fx(d.Get(),argv[1]);check(fx.textures()==26,"all26 original effect images load");CharacterRenderer surface(d.Get(),panel(10000)),wall(d.Get(),panel(2000));WorldView view{{0,0,0},{0,0,1}};
 combat::particles::Sprite a{{0,0,3000},600,0,{1,1,1,.7f},0x510d60,{0,0,.5f,.5f},true};auto b=a;b.position[2]=5000;b.texture=0xcaa2b5;b.additive=false;
 auto base=[&]{surface.render(c.Get(),0,false,&view);return pixels(d.Get(),c.Get(),surface);};auto before=base();check(fx.render(c.Get(),surface,view,std::array{a,b}),"original textured explosion draw");auto after=pixels(d.Get(),c.Get(),surface);check(before!=after,"original pixels visible");base();fx.render(c.Get(),surface,view,std::array{b,a});check(after==pixels(d.Get(),c.Get(),surface),"back to front sort independent input order");
 wall.render(c.Get(),0,false,&view);before=pixels(d.Get(),c.Get(),wall);fx.render(c.Get(),wall,view,std::array{a,b});check(before==pixels(d.Get(),c.Get(),wall),"opaque wall hides transparent sprites");
 base();fx.render(c.Get(),surface,view,std::array{a,b});CharacterRenderer cover(d.Get(),panel(6000));cover.render(c.Get(),0,false,&view,&surface);after=pixels(d.Get(),c.Get(),surface);base();cover.render(c.Get(),0,false,&view,&surface);check(after==pixels(d.Get(),c.Get(),surface),"sprites do not poison depth");
 before=base();a.texture=0xdeadbeef;check(!fx.render(c.Get(),surface,view,std::array{a}),"unknown texture rejected");check(before==pixels(d.Get(),c.Get(),surface),"rejected sprite no draw");a.texture=0x090aec;a.position[2]=-1;check(!fx.render(c.Get(),surface,view,std::array{a}),"behind camera rejected");
 // Smoke within100mm of the wall should fade continuously instead of cutting
 // a hard rectangle. Real source texture, same camera/depth and original bytes.
 b.position={0,0,9900};b.radius=2200;b.rgba={1,1,1,.8f};b.additive=false;
 before=base();fx.render(c.Get(),surface,view,std::array{b});auto hard=pixels(d.Get(),c.Get(),surface);
 render_backend::Options soft;soft.softParticles=true;render_backend::Device(d.Get()).configure(soft);
 base();fx.render(c.Get(),surface,view,std::array{b});auto softened=pixels(d.Get(),c.Get(),surface);
 uint64_t hardEnergy=0,softEnergy=0;for(size_t i=0;i<before.size();i+=4)for(unsigned k=0;k<3;++k){hardEnergy+=std::abs(int(hard[i+k])-int(before[i+k]));softEnergy+=std::abs(int(softened[i+k])-int(before[i+k]));}
 check(hardEnergy>1000&&softEnergy>0&&softEnergy<hardEnergy/2,"soft smoke fades near opaque geometry without vanishing");
 b.position[2]=11000;base();fx.render(c.Get(),surface,view,std::array{b});check(before==pixels(d.Get(),c.Get(),surface),"soft smoke cannot bleed through wall");
 render_backend::Device(d.Get()).configure({});b.position[2]=9900;base();fx.render(c.Get(),surface,view,std::array{b});check(hard==pixels(d.Get(),c.Get(),surface),"soft OFF exactly restores original smoke");
 if(argc>=3){
  fx.add_bundle(argv[2]);const auto total=fx.textures();check(total>26&&total<=36,"additional original blood images load without replacing combat images");fx.add_bundle(argv[2]);check(fx.textures()==total,"identical shared original images append idempotently");
  bool missingRejected=false;try{fx.add_bundle(std::filesystem::path(argv[2])/"not-a-directory.gwfx");}catch(const std::runtime_error&){missingRejected=true;}check(missingRejected&&fx.textures()==total,"failed additional bundle leaves existing images intact");
  combat::Snapshot snapshot;snapshot.epoch=7;snapshot.eventWatermark=1;combat::Player victim;victim.identity={1,12,123};victim.life=2;victim.alive=false;snapshot.players[1]=victim;
  combat::Event event;event.epoch=7;event.id=1;event.kind=combat::EventKind::damage;event.target=victim.identity;event.targetLife=2;event.hpDamage=100;event.position={0,0,3000};event.normal={0,0,-1};
  for(bool gekko:{false,true}){snapshot.players[1]->specialPc.kind=gekko?special_pc::Kind::gekko:special_pc::Kind::human;combat::particles::Pool blood;blood.synchronize(7,1,0,0);blood.dispatch({&event,1},snapshot,0);auto sprites=blood.sprites(snapshot,100);check(sprites.size()==3,"accepted lethal body contact supplies three textured blood sprites");
   before=base();check(fx.render(c.Get(),surface,view,sprites),"original blood texture draws for target kind");check(before!=pixels(d.Get(),c.Get(),surface),"original body blood produces visible pixels");
   wall.render(c.Get(),0,false,&view);before=pixels(d.Get(),c.Get(),wall);fx.render(c.Get(),wall,view,sprites);check(before==pixels(d.Get(),c.Get(),wall),"blood behind solid wall remains occluded");
   snapshot.players[1]->life=3;check(blood.sprites(snapshot,101).empty(),"render stream clears previous life after respawn");snapshot.players[1]->life=2;
  }
 std::cout<<"Original blood append, lethal contact pixels, wall occlusion, respawn and failed-load preservation PASS, FL "<<unsigned(requested)<<'\n';
 }
 if(argc>=4){
  weapon_effect::Config edited;std::string error;check(edited.load_text(R"({"format":"MGO2MT.WeaponEffects","version":1,"defaults":{"particles":{"muzzle":[{"texture":"alpha.png","radius":400,"stretch":[3,0.5]}]}}})",error),"import PNG configuration");auto originalCount=fx.textures();fx.add_textures(edited,argv[3]);check(fx.textures()==originalCount+1,"PNG added alongside originals");fx.add_textures(edited,argv[3]);check(fx.textures()==originalCount+1,"same imported image is idempotent");
  auto samples=weapon_effect::sample(*edited.particles(25,"muzzle"),{0,0,3000},{0,0,1},1,0);check(samples.size()==1,"edited runtime sample");combat::particles::Sprite rectangle;rectangle.position=samples[0].position;rectangle.radius=samples[0].radius;rectangle.rgba=samples[0].rgba;rectangle.texture=samples[0].texture;rectangle.stretch=samples[0].stretch;
  before=base();check(fx.render(c.Get(),surface,view,std::array{rectangle}),"imported rectangular board draws");auto wide=pixels(d.Get(),c.Get(),surface);check(wide!=before,"PNG alpha pixels visible");rectangle.stretch={1,1};base();fx.render(c.Get(),surface,view,std::array{rectangle});check(wide!=pixels(d.Get(),c.Get(),surface),"non-square stretch changes actual rasterized geometry");rectangle.rotation=1.5707963f;rectangle.stretch={3,.5f};base();fx.render(c.Get(),surface,view,std::array{rectangle});check(wide!=pixels(d.Get(),c.Get(),surface),"billboard rotation and stretch both applied");
  weapon_effect::Config invalidImport;check(invalidImport.load_text(R"({"format":"MGO2MT.WeaponEffects","version":1,"defaults":{"particles":{"muzzle":[{"texture":"missing.png"}]}}})",error),"missing image parser remains independent of deployment");bool rejected=false;try{fx.add_textures(invalidImport,argv[3]);}catch(const std::exception&){rejected=true;}check(rejected&&fx.textures()==originalCount+1,"failed texture transaction preserves originals and imports");rectangle.stretch={NAN,1};check(!fx.render(c.Get(),surface,view,std::array{rectangle}),"invalid stretch cannot reach GPU");
  std::cout<<"Imported PNG alpha, actual rectangular rotation/stretch and atomic resource failure PASS, FL "<<unsigned(requested)<<'\n';
 }
 std::cout<<"Original effect26, sorting/no depth writes, soft intersection fade, wall occlusion and exact OFF restoration PASS, FL "<<unsigned(requested)<<'\n';}return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
