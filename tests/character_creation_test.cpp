#include "character_creation.h"
#include "character_screen.h"
#include <fstream>
#include <stdexcept>
#include <iostream>
using namespace mgo2mt;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(int argc,char**argv){try{
 for(auto name:{L"四文字名",L"한글이름",L"Test!",L"abcdefghijklmnop",L"日本語名前",L"!!**",L"😀😀😀😀"})check(CharacterCreation::name_error(name).empty(),"valid multilingual name");
 for(auto name:{L"",L"abc",L"日本語",L"한글명",L"日本語名前六",L"abcdefghijklmnopq",L" abcd",L"abcd ",L"OpenMGO2",L"GM_test",L"abc\\",L"ab\u200bcd",L"😀😀😀😀a"})check(!CharacterCreation::name_error(name).empty(),"invalid name/byte boundary");
 auto repeated=[](std::wstring_view s,unsigned n){std::wstring out;while(n--)out+=s;return out;};
 const auto japanese16=repeated(L"日",16),emoji16=repeated(L"😀",16);
 for(auto name:{repeated(L"日",4),japanese16,emoji16,repeated(L"日😀A",5)+L"字"})
  check(CharacterCreation::name_error(name,true).empty(),"native four to sixteen scalars including supplementary plane");
 for(auto name:{repeated(L"日",3),repeated(L"日",17),repeated(L"😀",17),std::wstring(1,wchar_t(0xd800))+L"ABC",L"ABC"+std::wstring(1,wchar_t(0xdc00)),std::wstring(L" abcd"),std::wstring(L"GM_test")})
  check(!CharacterCreation::name_error(name,true).empty(),"native scalar count, strict UTF16 and existing policy");
 check(!CharacterCreation::name_error(japanese16).empty()&&!CharacterCreation::name_error(emoji16).empty(),"capability off retains legacy byte limit");
 for(auto name:{repeated(L"日",4),japanese16,emoji16,repeated(L"日",17)}){
  CharacterCreation draft(nullptr);draft.unicode_names(true);
  for(auto ch:name)draft.message(nullptr,WM_CHAR,ch,0);
  check(draft.name()==name,"Unicode draft preserves all UTF16 input units");
  for(auto k:{VK_END,VK_UP,VK_RETURN})draft.message(nullptr,WM_KEYDOWN,k,0);
  check(draft.confirming()==(name!=repeated(L"日",17)),"native UI review obeys scalar limit");
 }
 {
  CharacterCreation input(nullptr);input.unicode_names(true);
  for(auto ch:emoji16)input.message(nullptr,WM_CHAR,ch,0);
  check(input.name().size()==32,"16 emoji occupy 32 UTF16 units");
  input.message(nullptr,WM_CHAR,VK_BACK,0);check(input.name()==repeated(L"😀",15),"backspace removes entire pair");
  input.message(nullptr,WM_CHAR,0xd83d,0);check(input.name()==repeated(L"😀",15),"pending high surrogate is not visible name");
  input.message(nullptr,WM_KILLFOCUS,0,0);input.message(nullptr,WM_CHAR,0xde00,0);check(input.name()==repeated(L"😀",15),"focus loss cannot combine stale pair");
  input.message(nullptr,WM_IME_STARTCOMPOSITION,0,0);input.message(nullptr,WM_KEYDOWN,VK_RETURN,0);
  check(input.text_entry()&&!input.confirming(),"IME Enter while composing cannot advance or register");
  input.message(nullptr,WM_IME_ENDCOMPOSITION,0,0);input.message(nullptr,WM_KEYDOWN,VK_RETURN,0);
  check(input.text_entry(),"IME commit Enter remains guarded until release");
  input.message(nullptr,WM_KEYUP,VK_RETURN,0);input.message(nullptr,WM_KEYDOWN,VK_RETURN,0);
  check(input.row()==1&&!input.confirming()&&!input.take_registration(),"fresh Enter after release may advance only one row");
 }
 CharacterCreation voice(nullptr);auto vk=[&](unsigned k,LPARAM lp=0){voice.message(nullptr,WM_KEYDOWN,k,lp);};
 check(voice.pitch()==0&&character_voice_ratio(0)==1.f,"neutral pitch");
 for(int i=0;i<4;++i)vk(VK_DOWN);
 for(int i=0;i<30;++i)vk(VK_LEFT);
 check(voice.pitch()==-7&&voice.appearance()[8]==0,"pitch clamps and does not invent a wire encoding");
 auto sample=voice.take_audition();check(sample&&sample->pitch==-7,"low pitch audition snapshot");
 check(!voice.take_audition(),"audition is consumed once");
 for(int i=0;i<30;++i)vk(VK_RIGHT);
 check(voice.pitch()==7,"upper pitch clamp");voice.take_audition();
 vk(VK_F4,1LL<<30);check(!voice.take_audition(),"repeat cannot retrigger audition button");
 vk(VK_F4);sample=voice.take_audition();check(sample&&sample->pitch==7&&sample->voice==0,"F4 snapshots high pitch");
 vk(VK_F4);voice.message(nullptr,WM_KILLFOCUS,0,0);check(!voice.take_audition(),"focus loss clears pending voice");
 check(std::abs(character_voice_ratio(-7)*character_voice_ratio(7)-1.f)<.00001f,"original pitch curve symmetric");
 check(std::abs(character_voice_ratio(-7)-0.9585322831f)<.000001f&&std::abs(character_voice_ratio(7)-1.0432616800f)<.000001f,"original coefficient pitch endpoints");
 for(int bad:{-8,8}){bool rejected=false;try{character_voice_ratio(bad);}catch(const std::out_of_range&){rejected=true;}check(rejected,"invalid audio pitch rejected");}
 vk(VK_END);vk(VK_UP);vk(VK_RETURN);check(!voice.confirming()&&voice.text_entry(),"pitch does not bypass name validation");
 // Creating a PC visits the explicit skill step before any registration review.
 CharacterCreation skilled(nullptr);skilled.require_skills(true);
 for(auto ch:std::wstring(L"SkillPC"))skilled.message(nullptr,WM_CHAR,ch,0);
 for(auto k:{VK_END,VK_UP,VK_RETURN})skilled.message(nullptr,WM_KEYDOWN,k,0);
 check(skilled.take_skill_request()&&!skilled.take_skill_request()&&!skilled.confirming()&&!skilled.take_registration(),"final skill step emits one request and never registers");
 skilled.skills_complete(true);check(skilled.confirming()&&!skilled.yes(),"applied skill draft advances to safe NO review");
 skilled.message(nullptr,WM_KEYDOWN,VK_ESCAPE,0);check(!skilled.confirming()&&!skilled.take_registration(),"cancel review keeps skills without registering");
 if(argc==1){std::cout<<"Unicode name validation passed\n";return 0;}
 check(argc==2,"catalog required");std::ifstream in(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(in)),{});CharacterCatalog catalog(bytes);
 for(unsigned gender=0;gender<2;++gender)for(unsigned kind:{100,200,300,400,500}){auto options=catalog.creation_choices(gender,kind);check(!options.empty(),"basic model choices available");for(const auto&r:options)check(r.gender==gender&&r.kind==kind&&!r.flags,"gender/kind/quality filter");}
 // Screen/remote integration: explicit draft, server authority, stale reconnect and HUD.
 {
  auto file=std::filesystem::temp_directory_path()/(L"mgo2mt-skill-screen-"+std::to_wstring(GetCurrentProcessId())+L".tsv");
  struct Cleanup{std::filesystem::path file;~Cleanup(){std::error_code ec;std::filesystem::remove(file,ec);}}cleanup{file};
  {std::ofstream out(file);out<<"MGO2MT_SKILLS\t1\n";for(int id=1;id<=8;++id)out<<"SKILL\t"<<id<<"\t1\t1\tSkill "<<id<<"\tスキル "<<id<<'\n';}
  uint64_t now=100;CharacterScreen skillsFlow([](const std::atomic_bool&){CharacterReply r;r.status=CharacterStatus::success;r.list.slots=4;CharacterEntry e;e.id=101;e.name=L"SkillPC";r.list.entries.push_back(e);return r;},[&]{return now;});
  skillsFlow.skill_catalog(file);skillsFlow.selection([](uint32_t id,const std::atomic_bool&){CharacterSelectionReply r;r.status=CharacterSelectionStatus::success;r.request_may_have_been_sent=true;r.character.id=id;r.character.name=L"SkillPC";r.lobbies={{1,5733,0,L"Synthetic lobby",0,2}};return r;},std::make_shared<CharacterSelectionState>());
  std::atomic<skills::Remote*> remote=nullptr;
  skillsFlow.rooms([&](uint32_t id,const GameLobbyEntry&,const std::atomic_bool&cancel,std::atomic_bool&,const RoomPublish&publish,RoomRequests&requests){requests.skills->character(id);requests.skills->connected();remote.store(requests.skills.get());RoomReply r;r.status=RoomStatus::ready;publish(r);while(!cancel)Sleep(1);});
  auto wait=[&](auto predicate){for(int i=0;i<1000&&!predicate();++i){skillsFlow.draw();Sleep(1);}check(predicate(),"skill screen asynchronous state settles");};
  auto key=[&](unsigned k){skillsFlow.message(nullptr,WM_KEYDOWN,k,0);};
  wait([&]{return !skillsFlow.busy();});skillsFlow.selection_presentation(2000,true,true);key(VK_RETURN);
  Sleep(10);skillsFlow.draw();check(skillsFlow.busy()&&!skillsFlow.lobby_visible()&&skillsFlow.preview_character_id()==101,"salute keeps PC preview and defers fast network result");
  check(skillsFlow.take_selection_sound()&&!skillsFlow.take_selection_sound(),"salute sound consumed once");now=2100;wait([&]{return skillsFlow.lobby_visible();});
  key(VK_F6);check(skillsFlow.skill_visible()&&!skillsFlow.creation_text_entry(),"selected PC opens skill editor");key(VK_RETURN);key(VK_F10);check(!skillsFlow.skill_visible()&&skillsFlow.active_skills().entries.empty(),"offline Apply only stages draft; HUD remains unconfirmed");
  key(VK_RETURN);wait([&]{return remote.load()!=nullptr;});auto wire=remote.load();auto read=wire->take(now);check(read&&!read->write,"connection reads profile before saving draft");
  skills::RemoteProfile profile;profile.character=101;profile.capacity=4;profile.used=1;profile.token=read->token;profile.revision=1;profile.loadout={{{2,1}}};wire->receive(profile,false);skillsFlow.draw();
  check(skillsFlow.active_skills()==profile.loadout,"HUD keeps confirmed selection during pending write");auto write=wire->take(now);check(write&&write->write&&write->loadout==skills::Loadout{{{1,1}}},"explicit offline draft sent once after authoritative GET");
  // No UI tick between disconnect and new GET: old profile must not acknowledge a PUT.
  wire->disconnected();wire->connected();read=wire->take(now);profile.token=read->token;wire->receive(profile,false);skillsFlow.draw();
  check(!wire->take(now)&&skillsFlow.active_skills()==profile.loadout,"reconnect differing profile neither claims nor automatically retries draft");
  key(VK_F6);key(VK_F10);write=wire->take(now);check(write&&write->write,"explicit reapply after reconnect can send again");
  profile.token=write->token;profile.revision=2;profile.loadout=write->loadout;wire->receive(profile,true);skillsFlow.draw();check(skillsFlow.active_skills()==profile.loadout&&skillsFlow.skill_hud_labels().size()==1,"acknowledged selection becomes HUD labels");
  check(wire->fetch(),"profile refresh");read=wire->take(now);key(VK_F6);key(VK_F10);check(skillsFlow.skill_visible()&&!wire->take(now),"GET pending cannot apply stale editor selection");
  profile.token=read->token;profile.capacity=8;profile.used=8;profile.loadout.entries.clear();for(uint16_t id=1;id<=8;++id)profile.loadout.entries.push_back({id,1});wire->receive(profile,false);skillsFlow.draw();check(skillsFlow.active_skills().entries.size()==8,"eight entries require authoritative eight-capacity profile");
  key(VK_F10);write=wire->take(now);check(write&&write->loadout==profile.loadout,"open pristine editor synchronizes GET selection and capacity before PUT");profile.token=write->token;profile.revision=3;wire->receive(profile,true);skillsFlow.draw();
  check(wire->fetch(),"dirty editor refresh");read=wire->take(now);key(VK_F6);key(VK_F6);key(VK_RETURN);key(VK_F10);check(skillsFlow.skill_visible()&&!wire->take(now),"edited GET-pending draft is not implicitly submitted");
  profile.token=read->token;profile.capacity=4;profile.used=1;profile.loadout={{{2,1}}};wire->receive(profile,false);skillsFlow.draw();key(VK_F10);write=wire->take(now);check(write&&write->loadout==skills::Loadout{{{1,1}}},"dirty in-progress selection survives GET and applies only explicitly");
  auto rejected=profile;rejected.token=write->token;rejected.status=6;wire->receive(rejected,true);skillsFlow.draw();check(skillsFlow.active_skills()==profile.loadout&&skillsFlow.skill_notice().find(L"レベルを下げて")!=std::wstring::npos,"locked-level rejection explains failure without replacing confirmed HUD selection");
  key(VK_F6);read=wire->take(now);check(read&&!read->write,"rejected save reopen refreshes authoritative profile");key(VK_F10);check(skillsFlow.skill_visible()&&!wire->take(now),"rejected draft cannot bypass fresh GET");profile.token=read->token;wire->receive(profile,false);skillsFlow.draw();key(VK_ESCAPE);
  check(wire->fetch(),"invalid-profile test refresh");read=wire->take(now);profile.token=read->token;profile.used=7;wire->receive(profile,false);skillsFlow.draw();check(skillsFlow.active_skills().entries.empty(),"mismatched server cost cannot populate HUD");
  key(VK_F6);key(VK_RETURN);key(VK_F10);check(!wire->take(now),"mismatched server cost cannot authorize a write");
 }
 CharacterCreation c(&catalog);auto key=[&](unsigned k,LPARAM lp=0){c.message(nullptr,WM_KEYDOWN,k,lp);};
 auto chars=[&](const wchar_t*s){for(;*s;++s)c.message(nullptr,WM_CHAR,*s,0);};
 check(catalog.assemble(c.appearance()).selectedParts==5,"initial whole body");key(VK_END);key(VK_UP);key(VK_RETURN);check(!c.confirming()&&c.text_entry(),"empty name blocks review");
 chars(L"試作兵士");check(c.name()==L"試作兵士","Japanese text input");key(VK_DOWN);key(VK_RIGHT);check(c.appearance()[0]==1,"female selection");auto model=catalog.assemble(c.appearance());check(model.ready()&&!model.missingModels,"female preview");
 key(VK_DOWN);key(VK_RIGHT);key(VK_DOWN);key(VK_RIGHT);check(c.appearance()[7]==1,"voice choice in draft");
 key(VK_F2);key(VK_DOWN);auto old=c.appearance();key(VK_RIGHT);key(VK_SPACE);key(VK_RIGHT);check(c.appearance()!=old,"clothing and color change");
 key(VK_F3);key(VK_DOWN);key(VK_DOWN);key(VK_RIGHT);check(c.appearance()[15]!=46,"gloves selected");model=catalog.assemble(c.appearance());check(model.ready()&&!model.missingModels,"equipment preview");
 key(VK_END);key(VK_UP);key(VK_RETURN);check(c.confirming()&&c.confirmations()==1,"review opens");auto draft=c.appearance();key(VK_RIGHT);check(c.appearance()==draft,"review freezes changes");key(VK_RETURN);check(!c.confirming()&&!c.closed(),"review returns to edit without registration");
 key(VK_ESCAPE);check(c.discarding()&&!c.yes(),"discard defaults NO");key(VK_RETURN);check(!c.closed()&&c.appearance()==draft,"NO retains draft");key(VK_ESCAPE);key(VK_LEFT);c.message(nullptr,WM_KILLFOCUS,0,0);check(!c.discarding()&&!c.closed(),"focus loss cancels discard");
 key(VK_ESCAPE);key(VK_LEFT);key(VK_RETURN,1LL<<30);check(!c.closed(),"repeat cannot discard");key(VK_RETURN);check(c.closed(),"explicit discard closes");
 CharacterScreen screen([](const std::atomic_bool&){CharacterReply r;r.status=CharacterStatus::success;r.list.slots=2;CharacterEntry e;e.id=5;e.name=L"Existing";r.list.entries.push_back(e);return r;});screen.catalog(&catalog);
 for(int i=0;i<100&&!screen.preview_character_id();++i){screen.draw();Sleep(5);}check(screen.preview_character_id()==5,"existing slot loaded");screen.message(nullptr,WM_KEYDOWN,VK_RETURN,0);check(!screen.creation_visible(),"occupied slot does not create");screen.message(nullptr,WM_KEYDOWN,VK_DOWN,0);check(!screen.preview_visible(),"empty slot no model");screen.message(nullptr,WM_KEYDOWN,VK_RETURN,0);check(screen.creation_visible()&&screen.preview_visible()&&screen.preview_character_id()==0,"empty create preview has no server ID");screen.draw();screen.message(nullptr,WM_KEYDOWN,VK_ESCAPE,0);check(!screen.creation_visible()&&!screen.preview_visible()&&!screen.back(),"clean draft returns to empty slot");
 // Confirmation emits one request only; NO/focus loss/repeats do not submit.
 {
  CharacterCreation native(&catalog,true);native.unicode_names(true);
  for(auto ch:emoji16)native.message(nullptr,WM_CHAR,ch,0);
  for(auto k:{VK_END,VK_UP,VK_RETURN,VK_LEFT,VK_RETURN})native.message(nullptr,WM_KEYDOWN,k,0);
  auto request=native.take_registration();
  check(request&&request->unicode_name&&request->name==emoji16&&native.registration_busy(),"native registration keeps full 16 emoji and explicit capability mode");
  check(!native.take_registration(),"Unicode create snapshot consumed once");
 }
 CharacterCreation live(&catalog,true);auto lk=[&](unsigned k,LPARAM lp=0){live.message(nullptr,WM_KEYDOWN,k,lp);};
 for(auto ch:std::wstring(L"試作兵士"))live.message(nullptr,WM_CHAR,ch,0);
 lk(VK_END);lk(VK_UP);lk(VK_RETURN);check(live.confirming()&&!live.yes(),"registration defaults NO");lk(VK_RETURN);check(!live.take_registration()&&!live.confirming(),"NO edits without sending");
 lk(VK_RETURN);lk(VK_LEFT);live.message(nullptr,WM_KILLFOCUS,0,0);check(!live.yes(),"focus loss disarms YES");lk(VK_LEFT);lk(VK_RETURN,1LL<<30);check(!live.take_registration(),"held Enter cannot submit");lk(VK_RETURN);
 auto req=live.take_registration();check(req&&!req->unicode_name&&live.registration_busy()&&req->wire_appearance[7]==7&&req->wire_appearance[8]==15,"explicit YES snapshots mapped legacy request");
 lk(VK_RETURN);lk(VK_ESCAPE);check(!live.take_registration()&&!live.closed(),"busy inputs cannot resend or leave");live.registration_failed(L"Rejected");check(!live.registration_busy()&&live.name()==L"試作兵士"&&!live.confirming(),"rejection retains editable draft");
 // End-to-end owner state uses fake transport only, with a held response.
 for(int mode=0;mode<4;++mode){
  unsigned sends=0;std::atomic_bool release=false,stored=false;auto state=std::make_shared<CharacterRegistrationState>();
  auto list=[&](const std::atomic_bool&){CharacterReply r;r.status=CharacterStatus::success;r.list.slots=2;if(stored){CharacterEntry e;e.id=91;e.name=L"試作兵士";e.appearance=c.appearance(); r.list.entries.push_back(e);}return r;};
  // Use the native draft array for the fake server entry; no production session.
  CharacterScreen flow(list);flow.catalog(&catalog);
  flow.registration([&](const CharacterCreateRequest&r,const std::atomic_bool&cancel){++sends;while(!release&&!cancel)Sleep(1);CharacterCreateReply out;out.request_may_have_been_sent=true;
   if(mode==1){out.status=CharacterCreateStatus::rejected;out.error=9;}else if(mode>=2){out.status=CharacterCreateStatus::outcome_unknown;stored=mode==2;}else{out.status=CharacterCreateStatus::success;out.created_id=91;stored=true;}return out;},state);
  auto settle=[&]{for(int i=0;i<500&&flow.busy();++i){flow.draw();Sleep(1);}check(!flow.busy(),"asynchronous result settled");};settle();
  auto fk=[&](unsigned k){flow.message(nullptr,WM_KEYDOWN,k,0);};fk(VK_RETURN);check(flow.creation_visible(),"empty slot opens live draft");for(auto ch:std::wstring(L"試作兵士"))flow.message(nullptr,WM_CHAR,ch,0);fk(VK_END);fk(VK_UP);fk(VK_RETURN);fk(VK_LEFT);fk(VK_RETURN);
  check(flow.busy(),"registration runs asynchronously");fk(VK_RETURN);fk(VK_ESCAPE);check(flow.creation_visible()&&!flow.back(),"navigation cannot interrupt into duplicate submission");release=true;settle();check(sends==1,"one create call");
  if(mode==1)check(flow.creation_visible()&&!state->unresolved,"definitive rejection unlocks editable draft");
  else if(mode==3){check(!flow.creation_visible()&&state->unresolved,"ambiguous missing PC keeps registration locked");
   CharacterScreen reopened(list);reopened.catalog(&catalog);reopened.registration([&](const CharacterCreateRequest&,const std::atomic_bool&){++sends;return CharacterCreateReply{};},state);
   for(int i=0;i<500&&reopened.busy();++i){reopened.draw();Sleep(1);}check(state->unresolved,"screen navigation preserves ambiguous lock");
   reopened.message(nullptr,WM_KEYDOWN,VK_RETURN,0);for(auto ch:std::wstring(L"別の兵士"))reopened.message(nullptr,WM_CHAR,ch,0);for(auto k:{VK_END,VK_UP,VK_RETURN,VK_LEFT,VK_RETURN})reopened.message(nullptr,WM_KEYDOWN,k,0);check(sends==1,"locked new screen cannot submit");
  }else check(!flow.creation_visible()&&!state->unresolved&&flow.preview_character_id()==91,"success or ambiguous committed result reconciles from list");
 }
 std::cout<<"Creation multilingual boundaries, draft review/discard, gender/equipment preview and slot routing passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
