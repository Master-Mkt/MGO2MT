#include "gekko_greeting.h"
#include "character_catalog.h"
#include "ladder_input.h"
#include "item_menu_shortcut.h"
#include <fstream>
#include <iostream>
using namespace mgo2mt;
void check(bool x,const char*s){if(!x)throw std::runtime_error(s);}
std::vector<char> read(const char*p){std::ifstream in(p,std::ios::binary);check(bool(in),"asset missing");return {(std::istreambuf_iterator<char>(in)),{}};}
int main(int argc,char**argv){try{
 check(argc==3,"Greeting test bank catalog");auto data=read(argv[1]);special_pc::GekkoGreeting greeting(data);CharacterCatalog catalog(read(argv[2]));auto body=catalog.assemble({});check(body.ready(),"Gekko model");float movement=0;std::array<float,3> first{};
 for(unsigned i=0;i<=140;++i){auto s=greeting.sample(double(i)/60);check(bool(s)&&s->sourceIndex==132&&s->sourceKey==0xAA3B67,"source identity");catalog.pose(body,catalog.complete_pose(0,s->pose));for(const auto&v:body.model.vertices)check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::abs(v.x)<10000&&std::abs(v.y)<10000&&std::abs(v.z)<10000,"skin finite bounds");auto p=body.bone_position(0xFB7DF0);check(bool(p),"original right toe");if(!i)first=s->pose.root;movement=(std::max)(movement,std::abs(s->pose.root[1]-first[1]));}check(movement>100,"real animated greeting");
 auto bad=data;bad[16]^=1;bool reject=false;try{special_pc::GekkoGreeting wrong(bad);}catch(...){reject=true;}check(reject,"wrong source key rejected");check(!greeting.sample(-1),"negative time rejected");
 special_pc::GekkoFootsteps feet;check(!feet.update(1,1,0xC06DF3,0,true),"first sample silent");unsigned count=0;for(unsigned i=1;i<=102;++i)count+=feet.update(1,1,0xC06DF3,double(i)/60,true);check(count==2,"two native contacts per original walk loop");check(!feet.update(1,1,0xC06DF3,9,true),"stall no backlog");check(!feet.update(2,1,0xC06DF3,9.1,true),"scope silent");check(!feet.update(2,2,0xC06DF3,9.2,true),"life silent");check(!feet.update(2,2,0xAA3B67,10,true),"salute has no walking contact");
 ladder::Input input;input.scope(1,2,3);check(!input.step(true,true,0,1,0),"held at activation blocked");input.step(false,true,0,1,1);check(input.step(true,true,0,1,2),"fresh contextual Y");check(input.intent(0,0,true).action==ladder::Action::enter,"entry intent");input.sent(7);input.acknowledge(6,true);check(input.pending(),"old ack does not clear");input.acknowledge(7,true);check(!input.pending(),"matching ack clears");check(!input.step(true,true,1,0,3),"held cannot leave");input.step(false,true,1,0,4);check(input.step(true,true,1,0,5)&&input.intent(1,0,true).action==ladder::Action::leave,"fresh Y exits");input.step(false,false,1,0,6);check(!input.pending()&&input.intent(1,1,false)==ladder::Intent{},"modal clears pending and movement");
 ItemMenuShortcut shortcut;check(!shortcut.step(1,2,3,false,false,true),"menu scope initial");check(shortcut.step(1,2,3,true,false,true),"F7 menu entry");check(!shortcut.step(1,2,3,true,false,true),"held F7 no reopen");shortcut.step(1,2,3,false,false,true);check(shortcut.step(1,2,3,false,true,true),"LT plus X entry");shortcut.step(1,2,3,false,false,false);check(!shortcut.step(1,2,3,true,true,false)&&!shortcut.step(1,2,3,true,true,true),"modal held controls stay consumed");check(!shortcut.step(2,2,3,true,true,true),"new epoch cannot inherit menu press");
 std::cout<<"Original Gekko greeting 141 posed frames / native footstep / ladder input PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
