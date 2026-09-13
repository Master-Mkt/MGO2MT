#include "player_menu.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
namespace {
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
void bitmap(const void* pixels,const std::filesystem::path& path){BITMAPFILEHEADER h{};BITMAPINFOHEADER i{};h.bfType=0x4d42;h.bfOffBits=sizeof(h)+sizeof(i);h.bfSize=h.bfOffBits+1280*720*4;i.biSize=sizeof(i);i.biWidth=1280;i.biHeight=-720;i.biPlanes=1;i.biBitCount=32;std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<char*>(&h),sizeof(h));f.write(reinterpret_cast<char*>(&i),sizeof(i));f.write(static_cast<const char*>(pixels),1280*720*4);check(bool(f),"capture");}
const items::ClientContext context{{9,2},{1,11,101,1},{0,2,0,0},true};
const items::wire::Header header{context.scope,991,context.actor,0};
items::wire::Held holdings(bool occupied){items::wire::Held h;h.header=header;if(occupied){h.selectedSlot=0;h.slots[0].contents={25,1,17,93,0,items::Resource::ammunition,items::Domain::weapon};}return h;}
items::Entity entity(uint64_t id,items::PlacementKind kind,float z){items::Entity e;e.key={context.scope,id};e.kind=kind;e.owner=context.actor;e.contents={25,1,17,93,0,items::Resource::ammunition,items::Domain::weapon};e.position={0,2,z,0};return e;}
std::vector<uint8_t> fixture(items::wire::Record r){auto bytes=items::wire::encode(r);check(bool(bytes),"synthetic fixture shape");return *bytes;}
std::shared_ptr<items::ClientSession> ready(bool occupied){auto session=std::make_shared<items::ClientSession>();check(session->pump(context,0,header.token,true,{}).size()==1,"probe");items::wire::Page page;page.header=header;page.revision=1;page.total=3;page.capacity={64,64};page.entities={entity(1,items::PlacementKind::installed,100),entity(2,items::PlacementKind::dropped,200),entity(3,items::PlacementKind::dropped,1600)};
 std::vector<std::vector<uint8_t>> incoming{fixture(items::wire::Offer{header,31,{64,64}}),fixture(holdings(occupied)),fixture(page)};check(session->pump(context,1,0,true,incoming).empty()&&session->state().status==items::ClientStatus::ready&&session->state().world,"ready synthetic admitted fixture");return session;}
}
int main(int argc,char**argv){try{
 const auto temp=std::filesystem::temp_directory_path()/("MGO2WIN-equipment-menu-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));std::filesystem::create_directory(temp);
 auto input=std::make_shared<ControllerInput>(temp/"input.cfg");auto graphics=std::make_shared<GraphicsSettings>(temp/"graphics.cfg");PlayerMenu menu(temp/"input.cfg",input,graphics);
 auto capture=[&](const char* name){check(menu.draw()!=nullptr,"equipment render");if(argc>1){std::filesystem::create_directories(argv[1]);bitmap(menu.draw(),std::filesystem::path(argv[1])/name);}};
 auto key=[&](unsigned code,LPARAM flags=0){check(menu.message(nullptr,WM_KEYDOWN,code,flags),"menu consumes key");};
 menu.open(player::Menu::equipment);check(menu.inventory_menu(),"equipment open");key(VK_RETURN);capture("equipment_unavailable.bmp");key(VK_ESCAPE);check(!menu.visible(),"cancel unavailable");
 auto probing=std::make_shared<items::ClientSession>();probing->pump(context,0,991,true,{});menu.inventory_session(probing);menu.open(player::Menu::equipment);capture("equipment_probing.bmp");key(VK_RETURN);check(probing->state().delivery==items::Delivery::none,"cannot mutate before offer");
 for(unsigned index=0;index<5;++index){auto session=ready(index<2);menu.inventory_session(session);menu.open(player::Menu::equipment);
  if(index==0)capture("equipment_held.bmp");if(index==2)capture("equipment_empty.bmp");
  for(unsigned n=0;n<index;++n)key(VK_DOWN);
  key(VK_RETURN,1LL<<30);check(session->state().delivery==items::Delivery::none,"held Enter never submits");
  key(VK_RETURN);check(session->state().delivery==items::Delivery::pending,"selected action queued");key(VK_RETURN);auto out=session->pump(context,2,0,true,{});check(out.size()==1,"repeat confirm cannot create duplicate operation");auto parsed=items::wire::decode(out.front());check(parsed&&std::holds_alternative<items::wire::Command>(*parsed),"command wire");const auto command=std::get<items::wire::Command>(*parsed);
  check(unsigned(command.action)==index+1,"all five menu rows map to distinct operations");check(command.header.actor==context.actor&&command.header.scope==context.scope&&command.header.token==header.token,"only session-bound identity used");
  if(index==2)check(command.entity==2,"pickup selects nearest dropped item, ignores closer installed and far dropped");
  if(index>=3)check(command.entity==1,"recover/use selects nearest installed item");
  if(index<2)check(command.entity==0&&command.heldRevision==1,"drop/install uses authoritative slot revision");
  if(index==4)check(command.heldSlot==255&&command.heldRevision==0,"use carries no fabricated client holdings");
  check(session->pump(context,3,0,true,{}).empty(),"UI never reissues pending operation");
  items::wire::Reply reply;reply.header=command.header;reply.action=command.action;reply.heldSlot=command.heldSlot;reply.result=items::ResultCode::unauthorized;reply.worldRevision=1;std::vector<std::vector<uint8_t>> incoming{fixture(reply)};session->pump(context,4,0,true,incoming);menu.inventory_session(session);check(session->state().delivery==items::Delivery::rejected,"HOST rejection visible to UI");if(index==0)capture("equipment_rejected.bmp");
  key(VK_ESCAPE);check(!menu.visible()&&session->pump(context,5,0,true,{}).empty(),"cancel does not send operation or leave room");
 }
 auto timeout=ready(true);menu.inventory_session(timeout);menu.open(player::Menu::equipment);key(VK_RETURN);timeout->pump(context,2,0,true,{});timeout->pump(context,5002,0,true,{});menu.inventory_session(timeout);capture("equipment_unconfirmed.bmp");key(VK_RETURN);check(timeout->state().delivery==items::Delivery::unconfirmed&&timeout->pump(context,5003,0,true,{}).empty(),"unknown outcome locks further menu operations");
 timeout->disconnect();menu.inventory_session(timeout);capture("equipment_disconnected.bmp");key(VK_RETURN);check(timeout->state().delivery==items::Delivery::none,"disconnected equipment never submits");menu.close();
 std::filesystem::remove_all(temp);std::cout<<"equipment_menu_test PASS: synthetic GWIV, five actions, closest compatible target, repeat/unknown-result guards, offline rendering\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
