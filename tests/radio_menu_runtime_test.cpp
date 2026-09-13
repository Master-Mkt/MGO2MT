// Offline integration: real menus and sessions, in-memory native HOST delivery.
// No window, controller device, login or socket is opened.
#include "player_menu.h"
#include "chat_view.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
constexpr size_t pixels=1280*720;
using Surface=std::vector<uint32_t>;
Surface snapshot(PlayerMenu& menu){const auto* p=static_cast<const uint32_t*>(menu.draw());check(p!=nullptr,"real menu surface");return {p,p+pixels};}
void capture(const Surface& p,const std::filesystem::path& path){
 BITMAPFILEHEADER h{};BITMAPINFOHEADER i{};h.bfType=0x4d42;h.bfOffBits=sizeof(h)+sizeof(i);h.bfSize=h.bfOffBits+1280*720*4;
 i.biSize=sizeof(i);i.biWidth=1280;i.biHeight=-720;i.biPlanes=1;i.biBitCount=32;
 std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(&h),sizeof(h));f.write(reinterpret_cast<const char*>(&i),sizeof(i));f.write(reinterpret_cast<const char*>(p.data()),p.size()*sizeof(uint32_t));check(bool(f),"BMP save");
}
size_t differences(const Surface& a,const Surface& b,int top,int bottom){size_t n=0;for(int y=top;y<bottom;++y)for(int x=140;x<1100;++x)n+=a[size_t(y)*1280+x]!=b[size_t(y)*1280+x];return n;}
const radio::Identity self{0,11,101},peer{1,12,102};
const radio::Filter team=[](const radio::Member&a,const radio::Member&b,uint8_t){return a.team==b.team;};
struct Rig {
 radio::Context context{71,{{self,1,1,true},{peer,1,1,true}}};radio::Service host{{1000}};
 std::shared_ptr<radio::Session> session=std::make_shared<radio::Session>();
 std::shared_ptr<chat::Session> chat=std::make_shared<chat::Session>();
 Rig(bool supported=true){
  chat->connect(self.character,chat::EndpointProfile::nomad_jp);chat->enter(20);
  chat->roster({{self.character,"日本語の自分",1},{peer.character,"日本語の味方",1}},true);
  host.reset(context.epoch);auto out=session->pump(context,self,GetTickCount64(),111,true,{},team);
  check(out.size()==1&&radio::decode(out[0])->kind==radio::Kind::probe,"native probe");
  if(supported){auto offers=host.receive(self,out[0],context,GetTickCount64(),team);check(offers.size()==1,"native offer");session->pump(context,self,GetTickCount64(),111,true,{offers[0].body},team);check(session->state().status==radio::Status::ready,"negotiated HOST");}
 }
 void attach(PlayerMenu& menu){menu.close();menu.chat_session(chat);menu.radio_session(session);}
 std::vector<radio::Body> pump(){return session->pump(context,self,GetTickCount64(),111,true,{},team);}
 void no_chat(){check(!chat->take(GetTickCount64())&&chat->state().serial==0&&chat->state().delivery==chat::Delivery::none,"radio never queues ordinary chat");}
};
// title_preview's physical held/pressed adapter, supplied synthetic samples.
// This test exercises the real menu consumer, not the desktop polling loop.
void physical(PlayerMenu& menu,uint32_t held,uint32_t pressed,bool active=true,bool armed=true){
 const auto directions=held&15;
 if(active&&armed&&menu.radio_visible()&&(pressed&directions)==directions)menu.radio_digital_mask(directions);
}
void open_radio(PlayerMenu& menu){menu.select_chat_radio();check(menu.text_entry(),"first SELECT is chat");menu.select_chat_radio();check(menu.radio_visible()&&!menu.text_entry(),"second SELECT is radio");}
}
int main(int argc,char** argv){try{
 const auto scratch=std::filesystem::temp_directory_path()/("MGO2WIN-radio-menu-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
 std::filesystem::create_directories(scratch);
 std::filesystem::path output;if(argc>1){output=std::filesystem::path(argv[1]);std::filesystem::create_directories(output);}
 const auto save=[&](const Surface& s,const char* name){if(!output.empty())capture(s,output/name);};
 auto input=std::make_shared<ControllerInput>(scratch/"input.cfg");auto graphics=std::make_shared<GraphicsSettings>(scratch/"graphics.cfg");
 PlayerMenu menu(scratch/"input.cfg",input,graphics);Rig rig;rig.attach(menu);open_radio(menu);
 const auto categories=snapshot(menu);save(categories,"01_categories.bmp");
 physical(menu,9,9);physical(menu,3,3);physical(menu,1,1,false);physical(menu,1,1,true,false);physical(menu,0,0);
 check(snapshot(menu)==categories&&rig.pump().empty(),"diagonal/inactive/unarmed/analog cannot pick category");
 physical(menu,1,1);const auto messages=snapshot(menu);save(messages,"02_attack_messages.bmp");check(differences(categories,messages,150,540)>100,"first fresh direction changes actual category rendering");
 physical(menu,1,0);physical(menu,9,8);physical(menu,9,9);
 menu.message(nullptr,WM_KEYDOWN,VK_UP,LPARAM(1ULL<<30));
 check(snapshot(menu)==messages&&rig.pump().empty(),"held keyboard/physical and staggered/simultaneous diagonal cannot submit");rig.no_chat();
 physical(menu,8,8);check(rig.session->state().delivery==radio::DeliveryState::queued,"second fresh edge queues native radio");
 save(snapshot(menu),"03_sending.bmp");
 const auto requests=rig.pump();check(requests.size()==1,"exactly one request");const auto request=radio::decode(requests[0]);
 check(request&&request->kind==radio::Kind::request&&request->identity==self&&request->life==1&&request->preset==1,"request binds self/life and original preset");
 physical(menu,8,0);physical(menu,8,8);check(rig.pump().empty(),"selection consumes once without retransmit");rig.no_chat();
 std::vector<radio::Body> echo;for(const auto& delivery:rig.host.receive(self,requests[0],rig.context,GetTickCount64(),team))if(delivery.recipient==self)echo.push_back(delivery.body);
 check(echo.size()==1,"HOST produces self notification");rig.session->pump(rig.context,self,GetTickCount64(),111,true,echo,team);
 auto events=rig.session->drain();check(events.size()==1&&events[0].sender==self&&events[0].preset==1,"one validated notification");
 // Same catalogue-to-history bridge as the desktop; validation already performed
 // by the real Session against Context. No generic chat send API is used.
 for(const auto& event:events)for(const auto& category:mgo2::radio::defaultCategories())for(const auto& message:category.messages)if(message.presetId==event.preset)check(rig.chat->receive_radio(event.sender.character,std::string(message.text),GetTickCount64()),"radio history insertion");
 menu.radio_session(rig.session);save(snapshot(menu),"04_confirmed.bmp");check(rig.session->state().delivery==radio::DeliveryState::confirmed,"self notify confirms flight");
 auto history=rig.chat->state();check(history.lines.size()==1&&history.lines[0].radio&&history.lines[0].name=="日本語の自分"&&history.lines[0].text=="敵がいるぞ!!","Japanese attributed RADIO history");
 menu.select_chat_radio();check(menu.text_entry(),"SELECT returns to chat");save(snapshot(menu),"05_radio_history.bmp");rig.no_chat();
 rig.session->pump(rig.context,self,GetTickCount64(),111,true,echo,team);check(rig.session->drain().empty()&&rig.chat->state().lines.size()==1,"duplicate self notification adds no history");
 // Real menu remembers an old generation while the worker observes respawn.
 Rig stale;stale.attach(menu);open_radio(menu);physical(menu,1,1);const auto generation=stale.session->state().generation;
 ++stale.context.members[0].life;check(stale.pump().empty()&&stale.session->state().generation!=generation,"worker advances life generation");
 physical(menu,8,8);check(stale.pump().empty()&&stale.session->state().delivery==radio::DeliveryState::none,"stale on-screen life cannot enqueue");stale.no_chat();
 menu.radio_session(stale.session);check(!menu.visible(),"life refresh closes stale radio modal");
 // A choice already queued before respawn is discarded before it reaches HOST.
 Rig queued;queued.attach(menu);open_radio(menu);physical(menu,1,1);physical(menu,8,8);check(queued.session->state().delivery==radio::DeliveryState::queued,"queued before respawn");
 ++queued.context.members[0].life;check(queued.pump().empty()&&queued.session->state().delivery==radio::DeliveryState::none,"queued old-life choice never sends");queued.no_chat();
 Rig unsupported(false);unsupported.attach(menu);open_radio(menu);physical(menu,1,1);physical(menu,8,8);const auto probing=snapshot(menu);check(unsupported.session->state().status==radio::Status::probing,"probing UI");
 unsupported.session->pump(unsupported.context,self,GetTickCount64()+radio::probe_timeout_ms,111,true,{},team);menu.radio_session(unsupported.session);
 check(unsupported.session->state().status==radio::Status::unavailable,"old HOST bounded timeout");
 menu.select_chat_radio();menu.select_chat_radio();physical(menu,1,1);physical(menu,8,8);const auto unavailable=snapshot(menu);save(unavailable,"06_host_unsupported.bmp");
 check(differences(probing,unavailable,628,671)>20,"real menu renders distinct unsupported HOST notice");
 check(unsupported.session->state().delivery==radio::DeliveryState::none,"unsupported HOST cannot queue");unsupported.no_chat();
 menu.message(nullptr,WM_KILLFOCUS,0,0);check(!menu.visible(),"focus loss clears radio");
 menu.select_chat_radio();menu.select_chat_radio();unsupported.chat->leave();menu.chat_session(unsupported.chat);check(!menu.visible()&&unsupported.chat->state().lines.empty(),"room leave closes radio and history");
 check(!std::filesystem::exists(scratch/"input.cfg")&&!std::filesystem::exists(scratch/"player.cfg")&&!std::filesystem::exists(scratch/"camera.cfg"),"radio does not mutate settings");std::filesystem::remove(scratch);
 std::cout<<"radio_menu_runtime_test PASS: real PlayerMenu + chat/radio Session + Service, SELECT/Dpad one request, self notify RADIO history, stale life, held/diagonal, unsupported HOST; no sockets/windows\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
