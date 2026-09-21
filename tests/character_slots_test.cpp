#include "character_slots.h"
#include "character_screen.h"
#include <stdexcept>
#include <sstream>
#include <iostream>
using namespace mgo2mt;
void check(bool b){if(!b)throw std::runtime_error("slot UI contract failed");}
int main(){
 CharacterList l;l.slots=3;l.entries.push_back({101,L"Test"});CharacterSlots s;s.load(l);
 check(s.count()==5&&s.occupied()&&s.preview_id()==101&&std::wstring(s.action())==L"PCを選択");
 s.press(100,false);check(!s.tick(3099)&&s.held_ms(3099)==2999);check(s.tick(3100)&&s.dialog()&&!s.yes());check(s.confirm()==0);
 s.press(3200,true);check(!s.tick(7000));s.release();s.press(7100,false);s.release();check(!s.tick(11000));
 s.press(12000,false);s.press(14000,true);check(s.tick(15000));s.choose(true);check(s.confirm()==101);
 s.release();s.press(20000,false);s.select(1);check(!s.occupied()&&!s.preview_id()&&std::wstring(s.action())==L"PCを新規登録");check(!s.tick(24000));
 s.press(25000,false);check(!s.tick(30000));s.select(0);s.press(31000,false);s.reset_hold();check(!s.tick(35000));
 s.press(36000,false);check(s.tick(39000));check(!s.select(1));s.cancel_dialog();check(!s.dialog());
 s.load({3,{}});check(s.count()==5&&!s.occupied());s.press(1,false);check(!s.tick(4000));
 check(s.capacity()==4);s.select(3);check(s.can_create()&&!s.purchase_required());s.select(4);check(s.purchase_required()&&!s.can_create()&&std::wstring(s.action())==L"PCスロットを購入");s.press(1,false);check(!s.tick(4000));
 s.load({5,{}});check(s.capacity()==5&&s.count()==6);s.select(4);check(s.can_create()&&!s.purchase_required());s.select(5);check(s.purchase_required());
 s.load({8,{}});check(s.capacity()==8&&s.count()==8);s.select(7);check(s.can_create()&&!s.purchase_required());
 s.load({255,{}});check(!s.count()&&!s.can_create());
 s.load({});check(!s.count()&&!s.preview_id());
 // A purchase row cannot open character creation or trigger registration.
 {CharacterScreen purchase([](const std::atomic_bool&){CharacterReply r;r.status=CharacterStatus::success;r.list.slots=3;return r;});
  for(int i=0;i<100&&purchase.busy();++i){purchase.draw();Sleep(2);}check(!purchase.busy());
  for(int i=0;i<4;++i)purchase.message(nullptr,WM_KEYDOWN,VK_DOWN,0);
  purchase.message(nullptr,WM_KEYDOWN,VK_RETURN,0);check(!purchase.creation_visible()&&!purchase.preview_visible());
  purchase.message(nullptr,WM_KEYDOWN,VK_UP,0);purchase.message(nullptr,WM_KEYDOWN,VK_RETURN,0);check(purchase.creation_visible());
 }
 // Screen integration: physical Backspace is held, release/focus loss cancels;
 // default NO and explicit YES never issue deletion in this UI-only stage.
 uint64_t now=100;CharacterScreen ui([&](const std::atomic_bool&){CharacterReply r;r.status=CharacterStatus::success;r.list=l;return r;},[&]{return now;});
 for(int i=0;i<20;++i){ui.draw();Sleep(2);}check(ui.preview_character_id()==101);
 auto key=[&](unsigned k,LPARAM flags=0){ui.message(nullptr,WM_KEYDOWN,k,flags);};
 const auto initialYaw=ui.model_yaw();key(VK_RIGHT);check(ui.model_yaw()==initialYaw);
 ui.model_available(true);key(VK_RIGHT);check(ui.model_yaw()>initialYaw);key(VK_LEFT);check(ui.model_yaw()==initialYaw);
 auto report=[&]{std::ostringstream out;auto old=std::cout.rdbuf(out.rdbuf());ui.report();std::cout.rdbuf(old);return out.str();};
 key(VK_BACK);now=3099;ui.draw();check(report().find("\"delete_dialogs\":0")!=std::string::npos);now=3100;ui.draw();check(report().find("\"delete_dialogs\":1")!=std::string::npos);key(VK_RETURN);check(report().find("\"delete_yes\":0")!=std::string::npos);
 ui.message(nullptr,WM_KEYUP,VK_BACK,0);now=4000;key(VK_BACK);now=7000;ui.draw();key(VK_LEFT);key(VK_RETURN);check(report().find("\"delete_yes\":1")!=std::string::npos&&report().find("\"deletion_sent\":false")!=std::string::npos);
 ui.message(nullptr,WM_KEYUP,VK_BACK,0);now=8000;key(VK_BACK);ui.message(nullptr,WM_KILLFOCUS,0,0);now=12000;ui.draw();check(report().find("\"delete_dialogs\":2")!=std::string::npos);
 key(VK_DOWN);check(!ui.preview_character_id());key(VK_BACK);now=16000;ui.draw();check(report().find("\"delete_dialogs\":2")!=std::string::npos);key(VK_RETURN);check(!ui.back());
 key(VK_RIGHT);check(ui.model_yaw()==initialYaw);check(report().find("\"model_rendered\":false")!=std::string::npos);
 std::cout<<"Empty/occupied slots, 2999/3000 ms, repeat/release/focus guards, default NO, YES intent and preview eligibility passed. No deletion transport.\n";
}
