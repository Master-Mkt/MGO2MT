#include "mortar_shell_renderer.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace mgo2mt;
namespace {
unsigned checks=0;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);++checks;}
bool near(float a,float b,float tolerance=.1f){return std::abs(a-b)<=tolerance;}
bool near(stage::Vec3 a,stage::Vec3 b,float tolerance=.1f){for(unsigned j=0;j<3;++j)if(!near(a[j],b[j],tolerance))return false;return true;}
template<class F>void rejects(F action,const char* why){bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed,why);}
const mortar_shells::Flight& first(const mortar_shells::Simulation& s){for(const auto& f:s.flights())if(f)return *f;throw std::runtime_error("Expected visible shell");}
struct Fixture {
 mounted::Registry registry;
 combat::Snapshot snapshot;
 std::shared_ptr<const stage::Collision> world=std::make_shared<stage::Collision>(stage::Collision::make({},{}));
 mortar_shells::Simulation simulation{300000};
 Fixture(){mounted::Type type;type.id="mortar";type.weapon=103;type.kind=mounted::Kind::mortar;type.launchSpeed=25000;type.gravity=9800;type.maxFlightMs=10000;type.blastRadius=6000;
  registry.types.push_back(type);registry.placements.push_back({20,7,"mortar",{},0});
  snapshot.epoch=5;snapshot.eventWatermark=10000;combat::Player p;p.identity={0,1,11};p.life=3;p.alive=true;p.mountedId=7;snapshot.players[0]=p;
 }
 combat::Event shot(uint64_t id=1)const{combat::Event e;e.epoch=5;e.id=id;e.kind=combat::EventKind::shot;e.source=snapshot.players[0]->identity;e.sourceLife=3;e.weapon=103;e.object=7;e.position={0,1000,0};e.normal={0,.6f,.8f};return e;}
 void update(std::span<const combat::Event> e={},uint64_t now=1000,uint64_t scene=1,uint8_t map=20){simulation.update(e,&snapshot,registry,map,world,now,scene);}
 void send(combat::Event e,uint64_t now=1000){update(std::span(&e,1),now);}
};
}
int main(){try{
 for(float bad:{0.f,-1.f,1000001.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})rejects([&]{mortar_shells::Simulation s(bad);},"Invalid configured range is rejected");
 Fixture f;auto shot=f.shot();f.send(shot);check(f.simulation.size()==1,"One accepted mortar shot displays one shell");
 check(first(f.simulation).position==shot.position&&near(first(f.simulation).velocity,{0,15000,20000}),"Accepted event origin and direction drive original speed");
 check(first(f.simulation).gravity==9800&&first(f.simulation).life==3,"Gravity and owner life retained");
 auto projected=shot;projected.id=2;projected.kind=combat::EventKind::projectile;f.send(projected);f.send(shot);check(f.simulation.size()==1,"Companion projectile and repeated shot do not duplicate shell");
 f.update({},1500);check(near(first(f.simulation).position,{0,7275,10000}),"Half-second ballistic position matches analytic trajectory");
 check(near(first(f.simulation).velocity,{0,10100,20000}),"Velocity changes only by configured gravity");
 const auto& flight=first(f.simulation);check(flight.at==1500&&near(flight.traceTo,flight.position)&&!near(flight.traceFrom,flight.traceTo),"Debug endpoints describe last real point sweep");
 // Compare native cosmetic motion against the actual HOST pool over several
 // irregular frame intervals, using its independent public simulation API.
 Fixture parity;const auto accepted=parity.shot();projectile::Pool host(32);host.reset({5,1});
 projectile::Profile profile{103,25000,9800,300000,10000,0,true,0,600,6000};
 check(host.spawn({{5,1},{0,1,11,3},1,103,accepted.position,accepted.normal},profile,1000)==projectile::Submit::accepted,"HOST accepts comparison shot");parity.send(accepted);
 for(uint64_t t:{1017u,1033u,1050u,1274u,1800u,2500u,3001u}){host.advance(t,[](auto,auto,float,auto)->std::optional<projectile::Contact>{return {};});parity.update({},t);
  check(host.size()==parity.simulation.size(),"Cosmetic and HOST live counts agree");check(near(host.debug_flights()[0].position,first(parity.simulation).position,.15f),"Cosmetic trajectory agrees with HOST at irregular intervals");}
 // Double-sided original collision rays remove the shell at a finite wall.
 Fixture wall;wall.world=std::make_shared<stage::Collision>(stage::Collision::make({{-10000,-10000,500},{10000,-10000,500},{0,10000,500}},{{{0,1,2},stage::attribute::bomb}}));
 shot=wall.shot();shot.normal={0,0,1};wall.send(shot);wall.update({},1020);check(wall.simulation.size()==0,"Finite high-speed sweep hits thin wall without tunneling");
 wall.send(shot,1030);check(wall.simulation.size()==0,"Wall-completed shot cannot be replayed");
 Fixture floor;floor.world=std::make_shared<stage::Collision>(stage::Collision::make({{-10000,0,-10000},{10000,0,-10000},{0,0,10000}},{{{0,1,2},stage::attribute::floor|stage::attribute::bomb}}));
 shot=floor.shot();shot.position={0,100,0};shot.normal={0,-1,0};floor.send(shot);floor.update({},1010);check(floor.simulation.size()==0,"Falling shell disappears on original floor geometry");
 Fixture replacement;replacement.send(replacement.shot());replacement.world=std::make_shared<stage::Collision>(stage::Collision::make({},{}));replacement.update({},1010);check(replacement.simulation.size()==1,"Normal shared Collision refresh does not reset scene");
 Fixture explosions;explosions.send(explosions.shot());auto explosion=explosions.shot(3);explosion.kind=combat::EventKind::explosion;explosion.position={100000,0,0};explosions.send(explosion);check(explosions.simulation.size()==1,"Distant explosion does not remove unrelated flight");
 explosion.id=4;explosion.position={0,1000,0};explosion.source.character=22;explosions.send(explosion);check(explosions.simulation.size()==1,"Another player's explosion cannot erase shell");
 explosion.id=5;explosion.source=explosions.snapshot.players[0]->identity;explosion.sourceLife=2;explosions.send(explosion);check(explosions.simulation.size()==1,"Previous-life explosion cannot erase new shell");
 explosion.id=6;explosion.sourceLife=3;explosions.send(explosion);check(explosions.simulation.size()==0,"Nearby HOST explosion removes matching owner's shell");
 explosions.send(explosions.shot());check(explosions.simulation.size()==0,"Explosion-completed accepted shot cannot replay");
 Fixture ordered;auto a=ordered.shot(8),b=ordered.shot(9);b.kind=combat::EventKind::explosion;const std::array batch{b,a};ordered.update(batch);check(ordered.simulation.size()==0,"Event order within a batch follows authoritative event IDs");
 Fixture bound;std::vector<combat::Event> many;for(uint64_t i=1;i<=40;++i)many.push_back(bound.shot(i));bound.update(many);check(bound.simulation.size()==32,"Renderer simulation is bounded to 32 original shells");
 bound.snapshot.players[0]->life=4;bound.update({},1010);check(bound.simulation.size()==0,"New life clears all previous incarnation flights");
 bound.update(many,1010);check(bound.simulation.size()==0,"Previous-life events cannot be reaccepted");
 Fixture detached;detached.send(detached.shot());detached.snapshot.players[0].reset();detached.update({},1010);check(detached.simulation.size()==0,"Disconnected firing owner clears its shell");
 for(unsigned mode=0;mode<4;++mode){Fixture reset;reset.send(reset.shot());auto e=reset.shot(2);if(mode==0)reset.snapshot.epoch=6;
  const auto generation=reset.simulation.generation();reset.update(std::span(&e,1),mode==3?999:1010,mode==1?2:1,mode==2?4:20);
  check(reset.simulation.size()==0&&reset.simulation.generation()!=generation,"Epoch, scene, map or clock rollback clears flights and pending events");}
 Fixture paused;paused.send(paused.shot());paused.update({},2001);check(paused.simulation.size()==0,"Over-one-second catch-up discards without teleportation");
 Fixture lifetime;lifetime.registry.types[0].maxFlightMs=100;lifetime.send(lifetime.shot());lifetime.update({},1100);check(lifetime.simulation.size()==0,"Configured maximum flight life is enforced");
 Fixture limited;limited.simulation=mortar_shells::Simulation(100);shot=limited.shot();shot.normal={0,0,1};limited.send(shot);limited.update({},1020);check(limited.simulation.size()==0,"Configured HOST range bounds cosmetic travel");
 Fixture missing;missing.send(missing.shot());missing.world.reset();missing.update({},1010);check(missing.simulation.size()==0,"Missing world clears flight instead of ignoring collision");
 Fixture invalidSnapshot;invalidSnapshot.send(invalidSnapshot.shot());invalidSnapshot.simulation.update({},nullptr,invalidSnapshot.registry,20,invalidSnapshot.world,1010,1);check(invalidSnapshot.simulation.size()==0,"Missing authoritative snapshot clears flight");
 for(unsigned mode=0;mode<10;++mode){Fixture bad;auto e=bad.shot();if(mode==0)e.weapon=104;if(mode==1)e.epoch=4;if(mode==2)e.sourceLife=2;if(mode==3)e.source.slot=24;
  if(mode==4)e.normal={};if(mode==5)e.normal={0,0,2};if(mode==6)e.position[0]=std::numeric_limits<float>::quiet_NaN();if(mode==7)e.id=0;if(mode==8)e.id=10001;if(mode==9)e.object=65536;
  bad.send(e);check(bad.simulation.size()==0,"Unrelated, stale or invalid accepted-event data is rejected");}
 Fixture explicitType;explicitType.snapshot.players[0]->mountedId=0;explicitType.send(explicitType.shot());check(explicitType.simulation.size()==1,"Explicit event emplacement survives operator dismount");
 Fixture legacy;legacy.snapshot.players[0]->mountedId=0;shot=legacy.shot();shot.object=0;legacy.send(shot);check(legacy.simulation.size()==1,"Legacy post-dismount event accepts one unambiguous flight policy");
 Fixture ambiguous;auto other=ambiguous.registry.types[0];other.id="other";other.launchSpeed=10000;ambiguous.registry.types.push_back(other);ambiguous.registry.placements.push_back({20,8,"other",{},0});ambiguous.snapshot.players[0]->mountedId=0;
 shot=ambiguous.shot();shot.object=0;ambiguous.send(shot);check(ambiguous.simulation.size()==0,"Ambiguous legacy flight settings never pick a guessed emplacement");
 shot.id=2;shot.object=7;ambiguous.send(shot);check(ambiguous.simulation.size()==1&&near(first(ambiguous.simulation).velocity,{0,15000,20000}),"Explicit emplacement selects exact policy among different mortars");
 Fixture invalidType;invalidType.registry.types[0].gravity=std::numeric_limits<float>::infinity();invalidType.send(invalidType.shot());check(invalidType.simulation.size()==0,"Invalid mounted physics is rejected before simulation");
 Fixture huge;std::vector<combat::Event> over(4097,huge.shot());huge.update(over);check(huge.simulation.size()==0,"Oversized event batches are bounded");
 huge.send(huge.shot());check(huge.simulation.size()==0,"Discarded oversized batch cannot later replay same snapshot events");
 Fixture overflow;overflow.send(overflow.shot(),UINT64_MAX-10);check(overflow.simulation.size()==0,"Flight expiry timestamp cannot wrap");
 ModelVertex original{};original.y=1;original.ny=1;original.u=.25f;original.v=.75f;original.ar=.123f;original.aa=.8f;original.lr=.7f;
 std::array<ModelVertex,1> input{original},output{};
 for(const stage::Vec3 direction:std::array<stage::Vec3,7>{{{0,1,0},{0,-1,0},{1,0,0},{0,0,1},{0,.6f,.8f},{-.8f,-.6f,0},{.000001f,-1,0}}}){
  mortar_shells::orient_vertices(input,direction,output);const auto& v=output[0];check(near({v.x,v.y,v.z},direction,.0001f),"Original shell +Y points along its velocity including downward apex");
  check(near({v.nx,v.ny,v.nz},direction,.0001f),"Original normal is rotated with the geometry");check(v.u==original.u&&v.v==original.v&&v.ar==original.ar&&v.aa==original.aa&&v.lr==original.lr,"Orientation preserves UV and original material/vertex data");}
 rejects([&]{mortar_shells::orient_vertices(input,{},output);},"Zero-vector orientation is rejected");
 rejects([&]{mortar_shells::orient_vertices(input,{0,1,0},{});},"Mesh extent mismatch is rejected");
 rejects([&]{mortar_shells::orient_vertices(input,{0,std::numeric_limits<float>::infinity(),0},output);},"Nonfinite orientation is rejected");
 std::cout<<"PASS "<<checks<<" mortar shell trajectory, scope, collision and original-axis checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
