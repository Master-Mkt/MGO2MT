#include <windows.h>
#include <wincrypt.h>
#include "login_store.h"
#include <fstream>
#include <vector>
#include <stdexcept>
#include <cstring>
namespace mgo2mt {
namespace {
struct Stored {unsigned version=1,mode=0;wchar_t id[65]{},password[65]{};~Stored(){SecureZeroMemory(this,sizeof(*this));}};
struct Blob {DATA_BLOB data{};~Blob(){if(data.pbData){SecureZeroMemory(data.pbData,data.cbData);LocalFree(data.pbData);}}};
void fail(){throw std::runtime_error("Login settings could not be read or saved");}
bool valid(const wchar_t* p){bool ended=false;for(size_t i=0;i<65;++i){if(!p[i])ended=true;else if(ended||i==64||p[i]<33||p[i]>126)return false;}return ended;}
}
bool load_login(const std::filesystem::path& path,LoginForm& form){
 if(!std::filesystem::exists(path))return false;
 std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)fail();auto n=f.tellg();if(n<=0||n>16384)fail();
 std::vector<unsigned char> bytes(static_cast<size_t>(n));f.seekg(0);if(!f.read(reinterpret_cast<char*>(bytes.data()),n))fail();
 DATA_BLOB input{static_cast<DWORD>(bytes.size()),bytes.data()};Blob plain;
 if(!CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&plain.data)||plain.data.cbData!=sizeof(Stored))fail();
 Stored value;std::memcpy(&value,plain.data.pbData,sizeof(value));
 if(value.version!=1||value.mode<1||value.mode>2||!valid(value.id)||!valid(value.password)||!value.id[0]||(value.mode==1&&value.password[0])||(value.mode==2&&!value.password[0]))fail();
 form.restore(value.mode,value.id,value.password);return true;
}
void save_login(const std::filesystem::path& path,const LoginForm& form){
 if(form.save_mode()==0){std::filesystem::remove(path);return;}
 if(!form.length(0)||(form.save_mode()==2&&!form.length(1)))fail();
 Stored value;value.mode=form.save_mode();auto id=form.credential(0);std::copy(id.begin(),id.end(),value.id);
 if(value.mode==2){auto password=form.credential(1);std::copy(password.begin(),password.end(),value.password);}
 DATA_BLOB input{sizeof(value),reinterpret_cast<BYTE*>(&value)};Blob encrypted;
 if(!CryptProtectData(&input,L"OpenMGO2 login",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&encrypted.data))fail();
 std::filesystem::create_directories(path.parent_path());auto temporary=path;temporary+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
 try {
  {std::ofstream f(temporary,std::ios::binary|std::ios::trunc);if(!f)fail();f.write(reinterpret_cast<char*>(encrypted.data.pbData),encrypted.data.cbData);f.close();if(!f)fail();}
  if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))fail();
 }catch(...){std::error_code ec;std::filesystem::remove(temporary,ec);throw;}
}
}
