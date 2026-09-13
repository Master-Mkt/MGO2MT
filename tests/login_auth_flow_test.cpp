#include "login_screen.h"
#include "login_store.h"
#include <sstream>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
void require(bool b){if(!b)throw std::runtime_error("Login authentication flow failed");}
void key(LoginScreen& s,WPARAM k){s.message(nullptr,WM_KEYDOWN,k,0);}
void text(LoginScreen& s,const wchar_t* t){while(*t)s.message(nullptr,WM_CHAR,*t++,0);}
void frames(LoginScreen& s,unsigned ms){auto until=GetTickCount64()+ms;do{s.draw();Sleep(15);}while(GetTickCount64()<until);}
std::string report(LoginScreen& s){std::ostringstream out;auto old=std::cout.rdbuf(out.rdbuf());s.report();std::cout.rdbuf(old);return out.str();}
int main(){
 auto dir=std::filesystem::current_path()/(L"login-auth-test-"+std::to_wstring(GetCurrentProcessId()));
 require(std::filesystem::create_directory(dir));auto store=dir/L"login.dat";
 try{
  std::atomic_int calls=0;
  auto denied=[&](const AuthCredentials&,const std::atomic_bool&){++calls;AuthReply r;r.status=AuthStatus::denied;r.http=200;return r;};
  LoginForm initial;initial.restore(2,L"dummy_id",L"dummy_password");save_login(store,initial);
  {LoginScreen s(store,true,denied);frames(s,1200);frames(s,2200);require(calls==1);require(report(s).find("\"authenticated\":false")!=std::string::npos);
   key(s,VK_RETURN);frames(s,100);require(calls==2);key(s,VK_ESCAPE);require(s.back());}
  calls=0;
  {LoginScreen s(store,true,denied);key(s,VK_HOME);frames(s,1200);require(calls==0);}
  calls=0;
  {LoginScreen s(store,true,[&](const AuthCredentials& c,const std::atomic_bool&){++calls;require(std::wstring_view(c.id.data())==L"dummy_id");AuthReply r;r.status=AuthStatus::success;r.http=200;r.user=7;r.session[0]=1;return r;},false);
   frames(s,1200);require(calls==1&&s.port_visible());key(s,VK_RETURN);frames(s,100);require(calls==1);require(report(s).find("\"authenticated\":true")!=std::string::npos);
   key(s,VK_ESCAPE);frames(s,40);require(!s.port_visible()&&!s.back());key(s,VK_RETURN);frames(s,40);require(s.port_visible()&&calls==1);
   LoginForm saved;require(load_login(store,saved)&&saved.credential(1)==L"dummy_password");}
  calls=0;
  {LoginScreen s(store,true,[&](const AuthCredentials&,const std::atomic_bool& cancel){++calls;while(!cancel)Sleep(5);AuthReply r;r.status=AuthStatus::cancelled;return r;});
   frames(s,1100);require(calls==1);for(int i=0;i<10;++i)key(s,VK_RETURN);require(calls==1);key(s,VK_ESCAPE);frames(s,100);require(s.back());}
  // Missing test-only GNK fails before Winsock/network. Exercise real routing safely.
  {LoginScreen s(store,true,[](const AuthCredentials&,const std::atomic_bool&){AuthReply r;r.status=AuthStatus::success;r.user=7;return r;},false,{},{},dir/L"missing.gnk");
   frames(s,1100);require(s.port_visible());key(s,VK_F4);frames(s,60);require(s.character_visible());key(s,VK_ESCAPE);frames(s,30);require(!s.character_visible()&&s.port_visible()&&!s.back());}
  initial.clear();save_login(store,initial);calls=0;
  {LoginScreen s(store,true,denied);text(s,L"dummy_id");key(s,VK_TAB);text(s,L"dummy_password");key(s,VK_TAB);
   // Select save-ID-and-password, then submit. A rejected account is never newly stored.
   for(int i=0;i<4;++i)key(s,VK_TAB);key(s,VK_RETURN);for(int i=0;i<3;++i)key(s,VK_TAB);key(s,VK_RETURN);
   frames(s,100);require(calls==1&&!std::filesystem::exists(store));}
  // Session-only local playtest: never restore or mutate an existing store, even after success.
  initial.restore(2,L"dummy_id",L"dummy_password");save_login(store,initial);calls=0;
  auto bytes=[&]{std::ifstream f(store,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});};const auto preserved=bytes();
  {LoginScreen s(store,true,[&](const AuthCredentials& c,const std::atomic_bool&){++calls;require(std::wstring_view(c.id.data())==L"manual_test");AuthReply r;r.status=AuthStatus::success;r.user=7;return r;},false,{},{},{},true);
   frames(s,1150);require(calls==0&&report(s).find("\"settings_restored\":false")!=std::string::npos);
   text(s,L"manual_test");key(s,VK_TAB);text(s,L"test_only_password");key(s,VK_TAB);
   for(int i=0;i<4;++i)key(s,VK_TAB);key(s,VK_RETURN);for(int i=0;i<3;++i)key(s,VK_TAB);key(s,VK_RETURN);
   frames(s,100);require(calls==1&&s.port_visible());require(report(s).find("\"settings_written\":false")!=std::string::npos);require(bytes()==preserved);}
  std::filesystem::remove(store);calls=0;
  {LoginScreen s(store,true,denied,false,{},{},{},true);text(s,L"manual_test");key(s,VK_TAB);text(s,L"test_only_password");key(s,VK_TAB);key(s,VK_RETURN);frames(s,100);require(calls==1&&!std::filesystem::exists(store));}
  std::filesystem::remove(dir);std::cout<<"Offline login success/failure, delayed single auto-login, key cancellation, duplicate suppression, in-flight cancellation and deferred saving passed.\n";
 }catch(...){std::error_code ec;std::filesystem::remove(store,ec);std::filesystem::remove(dir,ec);throw;}
}
