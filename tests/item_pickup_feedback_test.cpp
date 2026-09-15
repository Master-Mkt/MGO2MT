#include "item_pickup_feedback.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){try{items::PickupFeedback cue;combat::Snapshot s;s.epoch=1;s.eventWatermark=10;combat::Player p;p.identity={1,2,3};p.life=4;p.alive=true;s.players[1]=p;combat::Event e;e.epoch=1;e.id=10;e.kind=combat::EventKind::itemPickup;e.source=e.target=p.identity;e.sourceLife=e.targetLife=4;e.weapon=25;e.object=1;
 check(!cue.update(s,p.identity,{&e,1}),"first snapshot historical pickup silent");e.id=11;check(!cue.update(s,p.identity,{&e,1}),"ahead of snapshot waits");s.eventWatermark=11;check(cue.update(s,p.identity,{}),"accepted contact plays once when snapshot catches up");check(!cue.update(s,p.identity,{&e,1}),"duplicate silent");e.id=13;s.eventWatermark=13;check(cue.update(s,p.identity,{&e,1}),"fresh contact");e.id=12;check(cue.update(s,p.identity,{&e,1}),"late event chunk not lost");
 e.id=14;s.eventWatermark=14;e.sourceLife=3;check(!cue.update(s,p.identity,{&e,1}),"old life silent");e.sourceLife=4;e.target.character=9;check(!cue.update(s,p.identity,{&e,1}),"other identity silent");e.target=p.identity;e.object=3;check(!cue.update(s,p.identity,{&e,1}),"world namespace is not equipment");e.object=2;check(cue.update(s,p.identity,{&e,1}),"equipment contact cue");
 s.players[1]->alive=false;check(!cue.update(s,p.identity,{&e,1}),"dead cancels");s.players[1]->alive=true;++s.players[1]->life;check(!cue.update(s,p.identity,{&e,1}),"new life historical event silent");cue.clear();check(!cue.update(s,p.identity,{}),"disconnect baseline silent");std::cout<<"contact pickup receipt scope/deferred snapshot/once-only/namespace PASS\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
