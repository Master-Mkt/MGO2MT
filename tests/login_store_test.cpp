#include <windows.h>
#include "login_store.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace mgo2mt;
void require(bool b){if(!b)throw std::runtime_error("Protected login storage contract");}
int main(){
 auto dir=std::filesystem::current_path()/(L"login-store-test-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
 try{
  require(std::filesystem::create_directory(dir));auto path=dir/L"login.dat";
  LoginForm initial;require(!load_login(path,initial));
  initial.restore(2,L"dummy_id",L"dummy_password_42");save_login(path,initial);
  std::ifstream f(path,std::ios::binary);std::string raw((std::istreambuf_iterator<char>(f)),{});f.close();
  std::wstring pw=L"dummy_password_42";require(raw.find(std::string(reinterpret_cast<const char*>(pw.data()),pw.size()*sizeof(wchar_t)))==std::string::npos);
  LoginForm restored;require(load_login(path,restored)&&restored.save_mode()==2&&restored.credential(0)==L"dummy_id"&&restored.credential(1)==pw);
  initial.restore(1,L"id_only",L"not_to_be_saved");save_login(path,initial);
  LoginForm idOnly;require(load_login(path,idOnly)&&idOnly.save_mode()==1&&idOnly.credential(0)==L"id_only"&&idOnly.length(1)==0);
  {std::ofstream bad(path,std::ios::binary|std::ios::trunc);bad<<"invalid";}
  bool rejected=false;try{LoginForm broken;load_login(path,broken);}catch(const std::exception&){rejected=true;}require(rejected);
  initial.clear();save_login(path,initial);require(!std::filesystem::exists(path));std::filesystem::remove(dir);
  std::cout<<"Current-user DPAPI roundtrip, encrypted bytes, ID-only password exclusion, corruption rejection and saved-data removal passed\n";return 0;
 }catch(const std::exception&e){std::error_code ec;std::filesystem::remove(dir/L"login.dat",ec);std::filesystem::remove(dir,ec);std::cerr<<e.what()<<'\n';return 1;}
}
