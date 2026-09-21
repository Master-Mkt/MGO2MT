#include "character_screen.h"
#include "stage_music.h"
#include <algorithm>
#include <fstream>
#include <iostream>
using namespace mgo2mt;
namespace {void check(bool ok,const char*s){if(!ok)throw std::runtime_error(s);}}
int main(int argc,char**argv){try{
 check(argc>=2,"catalog argument");std::mutex mutex;std::vector<combat::wire::Command> commands;std::optional<RoomReply> outgoing;std::shared_ptr<clan::Cache> clanCache;
 {RoomRequests requests;combat::wire::Command command;command.epoch=1;command.kind=combat::wire::CommandKind::ready;command.enabled=true;check(requests.combat_command(command),"old room command queued");RoomAction action;action.event=RoomEvent::join;action.id=2;check(requests.submit(action)&&!requests.take_command(),"new room discards old same-epoch commands");}
 combat::Identity self{1,257,123};host::LoadRequest request{1,1,0,0,{20,1,2},host::MatchTransition::initial};
 RoomReply state;state.event=RoomEvent::join;state.requested_room=77;state.status=RoomStatus::ready;state.join_status=RoomJoinStatus::joined;
 RoomDetail detail;detail.id=77;detail.name=L"出撃テスト";detail.capacity=16;detail.players=1;detail.subtype=1;detail.roster={{456,L"HOST"}};state.detail=detail;
 state.host_roster=host::Roster{};state.host_roster->complete=true;state.host_roster->slots[0]=host::Player{0,256,456,"HOST",{}};state.host_roster->slots[1]=host::Player{1,257,123,"日本語PC","クラン名"};
 state.host_match=host::MatchState{};state.host_match->generation=1;state.host_match->request=request;state.combat_offer=combat::wire::Offer{1,self};
 state.host_scene=stage::SceneSnapshot{request,1,{}};state.scene_status=stage::SceneSyncStatus::ready;
 combat::wire::Preparation prep;prep.epoch=1;prep.revision=1;prep.self=self;prep.generation=1;prep.runtimeReady=true;prep.dpEnabled=true;prep.dpBalance=1000;prep.supported={25};prep.requiredCategories=1;prep.players[1]=combat::wire::RoundPlayer{self,1};state.preparation=prep;
 CharacterScreen ui([](const std::atomic_bool&){CharacterReply r;r.status=CharacterStatus::success;r.list.slots=4;r.list.entries.push_back({123,L"PC"});return r;});ui.weapon_catalog(argv[1]);ui.skill_catalog(std::filesystem::path(argv[1]).parent_path()/"skill_catalog.tsv");ui.briefing_icons(std::filesystem::path(argv[1]).parent_path().parent_path()/"work/skills-resources-20260913/icons/briefing.tsv");if(argc>3)ui.weapon_icons(argv[3]);
 ui.selection([](uint32_t,const std::atomic_bool&){CharacterSelectionReply r;r.status=CharacterSelectionStatus::success;r.request_may_have_been_sent=true;r.character.id=123;r.lobbies.push_back({5,5735,1,L"Test",0,2});return r;},std::make_shared<CharacterSelectionState>());
 ui.rooms([&](uint32_t,const GameLobbyEntry&,const std::atomic_bool&stop,std::atomic_bool&,const RoomPublish&publish,RoomRequests&requests){
  {std::lock_guard lock(mutex);clanCache=requests.clanEmblem;}
  RoomReply list;list.status=RoomStatus::ready;list.rooms.push_back({77,L"出撃テスト",1,16});publish(list);
  while(!stop){if(auto action=requests.take()){auto r=state;r.event=action->event;if(action->event==RoomEvent::detail){r.join_status=RoomJoinStatus::none;r.preparation.reset();}publish(r);}
   std::optional<RoomReply> next;{std::lock_guard lock(mutex);if(auto c=requests.take_command())commands.push_back(*c);next=std::move(outgoing);outgoing.reset();}if(next)publish(*next);Sleep(1);
  }
 });
 auto key=[&](WPARAM k){ui.message(nullptr,WM_KEYDOWN,k,0);};
 auto wait=[&](auto predicate){auto until=GetTickCount64()+3000;while(!predicate()&&GetTickCount64()<until){ui.draw();Sleep(1);}check(predicate(),"UI timeout");};
 auto count=[&]{std::lock_guard lock(mutex);return commands.size();};
 auto last=[&]{std::lock_guard lock(mutex);return commands.back();};
 auto publish=[&]{++state.preparation->revision;{std::lock_guard lock(mutex);outgoing=state;}wait([&]{return ui.combat_preparation()&&ui.combat_preparation()->revision==state.preparation->revision;});};
 auto capture=[&](const char*name){if(argc<3)return;std::filesystem::path dir=argv[2];std::filesystem::create_directories(dir);BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=1280;info.biHeight=-720;info.biPlanes=1;info.biBitCount=32;info.biSizeImage=1280*720*4;file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+info.biSizeImage;const auto*pixels=ui.draw();std::ofstream out(dir/name,std::ios::binary);out.write(reinterpret_cast<const char*>(&file),sizeof(file));out.write(reinterpret_cast<const char*>(&info),sizeof(info));out.write(static_cast<const char*>(pixels),info.biSizeImage);};
 ui.draw();wait([&]{return !ui.busy();});key(VK_RETURN);wait([&]{return ui.lobby_visible();});key(VK_RETURN);wait([&]{return ui.room_count()==1;});key(VK_RETURN);wait([&]{return ui.room_detail_visible()&&!ui.room_detail_busy();});key(VK_RETURN);wait([&]{return ui.room_join_status()==RoomJoinStatus::joined;});
 // A status label alone must not acknowledge an applied collision/object scene.
 ui.stage_feedback(stage::Status::preview_ready);wait([&]{return count()==1;});check(!last().enabled&&last().kind==combat::wire::CommandKind::loaded,"preview status alone is not ready");
 stage::Result loaded;loaded.status=stage::Status::preview_ready;loaded.request=request;loaded.collision=std::make_shared<stage::Collision>(stage::Collision::make({{-10000,0,-10000},{10000,0,-10000},{0,0,10000}},{{{0,1,2}}}));loaded.objectSnapshot=state.host_scene;
 ui.stage_feedback(loaded);wait([&]{return count()==2;});check(last().enabled&&last().sceneRevision==1,"applied reviewed scene acknowledged");ui.stage_feedback(loaded);Sleep(20);check(count()==2,"unchanged readiness sends once");
 state.preparation->players[1]->loaded=true;state.preparation->lastCommand=last().sequence;publish();wait([&]{return ui.weapon_draft()&&ui.weapon_draft()->current_context()->dp_balance==1000;});
 key(VK_F9);wait([&]{return count()==3;});check(last().kind==combat::wire::CommandKind::ready&&last().enabled,"START ready request");
 state.preparation->players[1]->ready=true;state.preparation->lastCommand=last().sequence;publish();for(int i=0;i<20;++i){ui.draw();Sleep(1);}key(VK_F9);wait([&]{return count()==4;});check(!last().enabled,"START ready cancellation");capture("ready.bmp");
 state.preparation->phase=combat::wire::RoundPhase::selecting;state.preparation->lastCommand=last().sequence;state.preparation->players[1]->ready=false;publish();wait([&]{return ui.weapon_visible();});
 auto choices=ui.weapon_draft()->choices(weapons::Category::primary);auto at=std::find_if(choices.begin(),choices.end(),[](auto e){return e->id==25;});check(at!=choices.end(),"AK catalog");for(auto i=choices.begin();i!=at;++i)key(VK_RIGHT);key(VK_RETURN);check(ui.weapon_draft()->selected(weapons::Category::primary)->id==25,"reviewed supported ID selected");capture("weapons.bmp");
 key(VK_DOWN);key(VK_RETURN);check(ui.weapon_draft()->selected(weapons::Category::primary)->id==25,"unsupported next grid row cannot replace host supported choice");capture("unsupported.bmp");key(VK_UP);
 key(VK_F2);capture("secondary.bmp");key(VK_F2);capture("support.bmp");key(VK_F1);key(VK_F1);
 key(VK_F10);wait([&]{return count()==5;});check(last().weapons==std::array<uint16_t,3>{25,0,0},"only supported categories requested");key(VK_F10);Sleep(20);check(count()==5&&!ui.room_match_visible(),"pending confirmation cannot duplicate grant or enter locally");
 state.preparation->error=combat::wire::CommandError::spawn;state.preparation->lastCommand=last().sequence;publish();for(int i=0;i<25;++i){ui.draw();Sleep(1);}capture("spawn_blocked.bmp");key(VK_F10);wait([&]{return count()==6;});
 state.preparation->lastCommand=last().sequence;state.preparation->error=combat::wire::CommandError::none;state.preparation->dpBalance=0;state.preparation->players[1]->deployed=true;state.preparation->phase=combat::wire::RoundPhase::active;publish();for(int i=0;i<25;++i){ui.draw();Sleep(1);}check(!ui.room_match_visible(),"grant metadata waits for authoritative player snapshot");
 state.combat_state=combat::Snapshot{1,2,0,{}};combat::Player player;player.identity=self;player.team=1;player.alive=true;player.hp=player.maxHp=player.stamina=player.maxStamina=1000;player.weapon=25;state.combat_state->players[1]=player;state.combat_status=combat::wire::Status::active;publish();wait([&]{return ui.room_match_visible()&&!ui.weapon_visible();});capture("deployed.bmp");
 // Verify the actual returned CharacterScreen surface, not only HUD's GDI pass.
 std::shared_ptr<clan::Cache> emblemCache;{std::lock_guard lock(mutex);emblemCache=clanCache;}check(bool(emblemCache),"test room shares clan cache");
 const size_t emblemAt=25*1280+24;auto surface=static_cast<const uint32_t*>(ui.draw());const std::array<uint32_t,4> withoutEmblem={surface[emblemAt],surface[emblemAt+1],surface[emblemAt+2],surface[emblemAt+3]};
 clan::Image emblem;emblem.bgra[0]=0xff000000u;emblem.bgra[1]=0xff232b2bu;emblem.bgra[2]=0x80402010u;emblemCache->want(7);emblemCache->put(7,emblem);
 surface=static_cast<const uint32_t*>(ui.draw());check(surface[emblemAt]==0xff000000u,"final HUD retains opaque black clan pixel");check(surface[emblemAt+1]==0xff232b2bu,"final HUD bypasses menu color-key alpha for clan pixels");check(surface[emblemAt+2]==0xb7774324u,"final HUD composes partial clan alpha once");check(surface[emblemAt+3]==withoutEmblem[3],"final HUD keeps transparent clan pixels unchanged");capture("clan-alpha.bmp");
 emblemCache->put(7,std::nullopt);surface=static_cast<const uint32_t*>(ui.draw());for(size_t i=0;i<4;++i)check(surface[emblemAt+i]==withoutEmblem[i],"cleared clan image does not survive into next HUD frame");emblemCache->put(7,emblem);
 key(VK_F4);check(!ui.weapon_visible()&&ui.room_match_visible(),"gameplay cannot open the deployment picker through its hidden shortcut");
 key(VK_F9);check(!ui.room_match_visible()&&ui.stage_request()==request,"START keeps deployed world while opening briefing");key(VK_F4);check(ui.weapon_visible(),"deployed player can inspect loadout through briefing");publish();for(int i=0;i<25;++i){ui.draw();Sleep(1);}check(ui.weapon_visible(),"repeated snapshots do not force close menus");
 surface=static_cast<const uint32_t*>(ui.draw());const std::array<uint32_t,4> weaponWithCache={surface[emblemAt],surface[emblemAt+1],surface[emblemAt+2],surface[emblemAt+3]};emblemCache->put(7,std::nullopt);surface=static_cast<const uint32_t*>(ui.draw());for(size_t i=0;i<4;++i)check(surface[emblemAt+i]==weaponWithCache[i],"clan HUD does not leak into weapon screen");
 loaded.objectSnapshot->revision=2;ui.stage_feedback(loaded);wait([&]{return count()==7;});check(!last().enabled,"unapplied/mismatched scene revision revokes readiness");
 // Keep the actual screen's admitted-stage lifetime across ended and a new
 // offer, which is the title renderer's BGM lifetime gate.
 stage::MusicPlayback playback;stage::Track track{"test:same",L"同じ曲",{},false};check(playback.select(&track),"initial BGM voice");
 state.preparation->phase=combat::wire::RoundPhase::ended;state.preparation->roundClock=true;state.preparation->roundRemainingMs=0;state.combat_status=combat::wire::Status::ended;publish();
 check(ui.stage_load_request()==request&&!ui.weapon_visible()&&!ui.room_match_visible(),"end holds admitted stage and returns nested picker to briefing");key(VK_F4);key(VK_F9);Sleep(20);check(!ui.weapon_visible()&&ui.room_match_visible()&&count()==7&&!playback.select(&track),"end blocks actions without restarting same BGM");capture("round-ended.bmp");
 auto nextRequest=request;nextRequest.sequence=2;nextRequest.generation=2;nextRequest.round=1;nextRequest.transition=host::MatchTransition::next_round;
 state.host_match->request=nextRequest;state.host_match->generation=2;state.host_scene=stage::SceneSnapshot{nextRequest,3,{}};state.combat_offer=combat::wire::Offer{2,self};state.combat_state=combat::Snapshot{2,1,0,{}};state.combat_status=combat::wire::Status::awaiting_spawn;
 state.preparation->epoch=2;state.preparation->generation=2;state.preparation->phase=combat::wire::RoundPhase::waiting;state.preparation->roundClock=false;state.preparation->roundRemainingMs=0;state.preparation->lastCommand=0;state.preparation->players[1]=combat::wire::RoundPlayer{self,1};publish();
 check(ui.stage_load_request()==nextRequest&&!ui.room_match_visible()&&!ui.weapon_visible()&&!playback.select(&track),"new epoch returns to READY while BGM stage remains admitted");
 ui.stage_feedback(loaded);wait([&]{return count()==8;});check(!last().enabled&&last().epoch==2,"old loaded scene cannot acknowledge new epoch");loaded.request=nextRequest;loaded.objectSnapshot=state.host_scene;ui.stage_feedback(loaded);wait([&]{return count()==9;});check(last().enabled&&last().epoch==2&&last().generation==2&&last().sceneRevision==3,"new scene explicitly applied");
 state.preparation->players[1]->loaded=true;state.preparation->lastCommand=last().sequence;publish();key(VK_F9);wait([&]{return count()==10;});check(last().epoch==2&&last().kind==combat::wire::CommandKind::ready,"new round requires fresh READY");capture("round-next-ready.bmp");
 std::cout<<"applied scene gate, READY cancellation, host DP, blocked spawn retry, authoritative entry, ended HUD/actions, fresh epoch READY and BGM lifetime passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
