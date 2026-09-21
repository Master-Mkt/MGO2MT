#include "water_surface_effects.h"
#include "water_effects_overlay.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt::stage;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char**argv){try{
 auto surface=WaterSurface::make({{{Vec3{-1000,0,0},Vec3{1000,0,0},Vec3{0,3000,0}}}});
 WaterSurfaceEffects fx;WaterSurfaceScope scope{1,1};std::array<WaterSurfaceActor,1> actor{{{10,1,{0,852,-50}}}};
 auto update=[&](uint64_t now){fx.update(scope,actor,&surface,now,true);};
 update(100);actor[0].point[2]=50;update(116);check(fx.size()==WaterSurfaceEffects::dropsPerCrossing,"crossing emits twelve bounded native drops");
 auto lines=fx.lines(150);check(lines.size()==WaterSurfaceEffects::dropsPerCrossing&&lines[0].splash&&lines[0].alpha<1,"finite lifetime and ballistic lines");
 for(auto& l:lines)for(float v:l.from)check(std::isfinite(v),"finite trajectory");
 update(116);check(fx.size()==WaterSurfaceEffects::dropsPerCrossing,"same clock cannot repeat emission");
 actor[0].point[2]=-50;update(130);check(fx.size()==WaterSurfaceEffects::dropsPerCrossing,"cooldown suppresses repeated crossing");
 actor[0].point[2]=50;update(300);check(fx.size()==2*WaterSurfaceEffects::dropsPerCrossing,"later crossing can emit");
 actor[0].life=2;update(301);check(fx.size()==0,"respawn clears old life immediately");
 actor[0].point[2]=-50;update(302);check(fx.size()==WaterSurfaceEffects::dropsPerCrossing,"new life starts its own track");
 ++scope.epoch;update(303);check(fx.size()==0,"new room epoch clears");
 actor[0].point[2]=50;update(304);check(fx.size()==WaterSurfaceEffects::dropsPerCrossing,"new epoch crossing");
 ++scope.scene;update(305);check(fx.size()==0,"scene replacement clears");
 actor[0].point[2]=-50;update(306);check(fx.size()==WaterSurfaceEffects::dropsPerCrossing,"scene crossing");
 actor[0].point[2]=50;update(600);check(fx.size()==0,"stall uses baseline rather than crossing");
 actor[0].point[2]=-50;update(599);check(fx.size()==0,"clock reversal resets");
 actor[0].point[2]=50;update(600);check(fx.size()==WaterSurfaceEffects::dropsPerCrossing,"resumed baseline");
 actor[0].point[2]=-3000;update(601);check(fx.size()==0,"teleport clears actor trail");
 actor[0].point={2000,852,-50};update(602);actor[0].point[2]=50;update(603);check(fx.size()==0,"outside finite triangle has no effect");
 fx.update(scope,{},&surface,604,true);check(fx.size()==0,"departed actors removed");
 actor[0].point={0,852,-50};update(605);actor[0].point[2]=50;update(606);check(fx.size()==WaterSurfaceEffects::dropsPerCrossing,"rejoin establishes a fresh baseline");
 fx.update(scope,actor,&surface,607,false);check(fx.size()==0,"paused or menu inactive clears");
 update(608);actor[0].point[2]=-50;update(609);check(fx.size()==WaterSurfaceEffects::dropsPerCrossing,"resume then movement");
 std::array<WaterSurfaceActor,2> duplicate{actor[0],actor[0]};fx.update(scope,duplicate,&surface,610,true);check(fx.size()==0,"duplicate identities reject whole batch");
 actor[0].point[0]=NAN;update(611);check(fx.size()==0,"invalid coordinates reset");
 std::vector<WaterSurfaceActor> crowd;for(unsigned i=0;i<24;++i)crowd.push_back({i+1,1,{0,852,-50}});
 fx.update(scope,crowd,&surface,700,true);for(auto& a:crowd)a.point[2]=50;fx.update(scope,crowd,&surface,716,true);
 check(fx.size()==WaterSurfaceEffects::maximumDrops,"24 actors fit exact particle bound");check(fx.lines(1316).empty(),"particles expire without update");
 crowd.push_back({25,1,{0,852,0}});fx.update(scope,crowd,&surface,717,true);check(fx.size()==0,"oversized actor set rejected");
 check(water_surface_identity(1,2,3)!=water_surface_identity(2,2,3),"slot is part of complete identity");
 fx.reset();actor[0]={water_surface_identity(1,2,3),1,{0,852,-50}};update(720);actor[0].point[2]=50;update(721);check(fx.size()==WaterSurfaceEffects::dropsPerCrossing,"same tick fixture");
 actor[0].life=2;update(721);check(fx.size()==0,"same clock respawn still clears old drops");actor[0].point[2]=-50;update(722);check(fx.size()==WaterSurfaceEffects::dropsPerCrossing,"new life position baseline retained");
 fx.update(scope,{},&surface,722,true);check(fx.size()==0,"same clock departure clears old drops");
 update(723);actor[0].point[2]=50;update(724);check(fx.size()==WaterSurfaceEffects::dropsPerCrossing,"replacement identity fixture");actor[0].identity=water_surface_identity(2,2,3);update(724);check(fx.size()==0,"same clock slot replacement clears old actor");
 // The actual navigation controller crosses a finite decorative water face
 // without gaining a floor, a water volume, or an invented movement penalty.
 auto floor=Collision::make({{-10000,0,-10000},{10000,0,-10000},{10000,0,10000},{-10000,0,10000}},{{{0,1,2}},{{0,2,3}}});
 Navigation nav;check(nav.place(floor,{0,500,-50}),"navigation floor setup");fx.reset();actor[0]={10,1,nav.feet()};actor[0].point[1]+=850;update(800);
 nav.advance(floor,{1,0,0,0,1000},.1f);actor[0].point=nav.feet();actor[0].point[1]+=850;update(900);
 check(fx.size()==WaterSurfaceEffects::dropsPerCrossing&&std::abs(nav.feet()[2]-50)<.01f&&!nav.water_state().level,"actual movement crosses water face at full dry speed");
 if(argc>1){std::ifstream in(argv[1],std::ios::binary);auto aa=WaterSurface::read(in);check(aa.triangles().size()==77,"authored AA 77 triangles");
  auto t=aa.triangles()[0];Vec3 center{};for(unsigned k=0;k<3;++k)center[k]=(t.vertices[0][k]+t.vertices[1][k]+t.vertices[2][k])/3;
  auto u=mgo2mt::enemy_tag::sub(t.vertices[1],t.vertices[0]),v=mgo2mt::enemy_tag::sub(t.vertices[2],t.vertices[0]);
  auto normal=mgo2mt::enemy_tag::unit(Vec3{u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]});check(bool(normal),"real face normal");
  actor[0].point=mgo2mt::enemy_tag::add(center,mgo2mt::enemy_tag::mul(*normal,-10));fx.reset();fx.update(scope,actor,&aa,1000,true);
  actor[0].point=mgo2mt::enemy_tag::add(center,mgo2mt::enemy_tag::mul(*normal,10));fx.update(scope,actor,&aa,1016,true);check(fx.size()==WaterSurfaceEffects::dropsPerCrossing,"real AA contact emits through same adapter");
 }
 // Perspective-correct occlusion: a narrow post hides only the middle of a
 // projected stroke even when its endpoints have different camera depths.
 auto post=Collision::make({{-15,1500,500},{15,1500,500},{15,1600,500},{-15,1600,500}},{{{0,1,2}},{{0,2,3}}});
 std::array<WaterEffectLine,1> trail{{{{-200,1552,1000},{800,1552,4000},1,true}}};std::vector<uint32_t> pixels(1280*720);
 for(unsigned order=0;order<2;++order){std::fill(pixels.begin(),pixels.end(),0);paint_water(pixels,1280,720,trail,{0,1552,0},{0,0,1},floor,&post);
  check(pixels[316*1280+928]==0&&pixels[316*1280+888]!=0&&pixels[316*1280+968]!=0,"post hides center while visible sides survive");
  for(auto pixel:pixels)if(pixel)check((pixel&255)==((pixel>>8)&255)&&((pixel>>8)&255)==((pixel>>16)&255),"water strokes default to neutral gray");
  for(size_t i=0;i<pixels.size();++i)if(pixels[i])check(i%1280>=620&&i%1280<1236&&i/1280>=120&&i/1280<512,"water stays in gameplay viewport");std::swap(trail[0].from,trail[0].to);
 }
 std::cout<<"finite water contact: navigation/AA geometry/lifecycle/bounds/perspective occlusion PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
