#include "stage_music.h"
#include <windows.h>
#include <bcrypt.h>
#include <fstream>
#include <array>
#include <algorithm>
#include <cwctype>
#include <stdexcept>
#include <sstream>
#include "pcm_wave.h"
namespace mgo2mt::stage {
static bool wav(const std::filesystem::path&p){
 std::ifstream in(p,std::ios::binary|std::ios::ate);if(!in)return false;auto size=in.tellg();if(size<44||size>256*1024*1024)return false;in.seekg(0);std::vector<unsigned char>b(static_cast<size_t>(size));if(!in.read(reinterpret_cast<char*>(b.data()),size))return false;read_pcm_wave(b);return true;
}
static std::string digest(const std::filesystem::path&p){
 BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;std::string result;
 if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA256 provider");
 try{if(BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)<0)throw std::runtime_error("SHA256 hash");std::ifstream in(p,std::ios::binary);std::array<unsigned char,65536>b{};while(in){in.read(reinterpret_cast<char*>(b.data()),b.size());auto n=in.gcount();if(n&&BCryptHashData(hash,b.data(),ULONG(n),0)<0)throw std::runtime_error("SHA256 update");}if(!in.eof())throw std::runtime_error("Music read");std::array<unsigned char,32>d{};if(BCryptFinishHash(hash,d.data(),d.size(),0)<0)throw std::runtime_error("SHA256 final");for(auto x:d){result+="0123456789abcdef"[x>>4];result+="0123456789abcdef"[x&15];}}
 catch(...){if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);throw;}BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);return result;
}
MusicLibrary MusicLibrary::scan(const std::filesystem::path&root){
 MusicLibrary lib;for(bool extra:{false,true}){auto dir=extra?root/"additional":root;std::error_code ec;if(!std::filesystem::is_directory(dir,ec))continue;std::vector<std::filesystem::path>files;
  unsigned count=0;for(std::filesystem::directory_iterator it(dir,ec),end;!ec&&it!=end;it.increment(ec)){if(++count>4096){++lib.overflow;break;}auto p=it->path();auto attr=GetFileAttributesW(p.c_str());if(attr==INVALID_FILE_ATTRIBUTES||(attr&FILE_ATTRIBUTE_REPARSE_POINT)||!it->is_regular_file(ec))continue;auto ext=p.extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),towlower);if(ext==L".wav")files.push_back(p);}
  std::sort(files.begin(),files.end(),[](auto&a,auto&b){return a.filename().wstring()<b.filename().wstring();});unsigned accepted=0;
  for(auto&p:files){if(accepted>=(extra?32u:256u)){++lib.overflow;continue;}try{if(!wav(p)){++lib.rejected;continue;}Track t;t.additional=extra;t.path=p;t.title=p.stem().wstring();if(extra)t.id="additional:"+digest(p);else{auto s=p.stem().wstring();if(s.empty()||s.size()>100||std::any_of(s.begin(),s.end(),[](wchar_t c){return !((c>=L'a'&&c<=L'z')||(c>=L'0'&&c<=L'9')||c==L'_');})){++lib.rejected;continue;}t.id="original:";for(auto c:s)t.id.push_back(static_cast<char>(c));}if(lib.find(t.id)){++lib.rejected;continue;}lib.tracks.push_back(std::move(t));++accepted;}catch(...){++lib.rejected;}}
 }lib.reload_titles(root);return lib;
}
void MusicLibrary::reload_titles(const std::filesystem::path&root){
 playlistErrors=0;for(auto&t:tracks)t.title=t.path.stem().wstring();
 auto trim=[](std::wstring s){auto a=s.find_first_not_of(L" \t\r");if(a==std::wstring::npos)return std::wstring{};return s.substr(a,s.find_last_not_of(L" \t\r")-a+1);};
 for(bool extra:{false,true}){
  auto path=extra?root/"additional/playlist2.txt":root/"playlist1.txt";
  std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in)continue;auto size=in.tellg();if(size<0||size>256*1024){++playlistErrors;continue;}
  std::string bytes(static_cast<size_t>(size),'\0');in.seekg(0);if(!in.read(bytes.data(),size)){++playlistErrors;continue;}
  if(bytes.starts_with("\xef\xbb\xbf"))bytes.erase(0,3);if(bytes.empty())continue;
  int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data(),int(bytes.size()),nullptr,0);if(!n){++playlistErrors;continue;}
  std::wstring decoded(size_t(n),L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data(),int(bytes.size()),decoded.data(),n);
  std::wistringstream lines(decoded);std::wstring line;unsigned count=0;
  while(std::getline(lines,line)){if(++count>4096){++playlistErrors;break;}line=trim(std::move(line));if(line.empty()||line[0]==L'#')continue;
   auto split=line.find(L'\t');if(split==std::wstring::npos)split=line.find(L'=');if(split==std::wstring::npos){++playlistErrors;continue;}
   auto name=trim(line.substr(0,split)),title=trim(line.substr(split+1));
   if(name.empty()||name.find_first_of(L"/\\:")!=std::wstring::npos||title.empty()||title.size()>256||std::any_of(title.begin(),title.end(),[](wchar_t c){return c<32||c==127;})){++playlistErrors;continue;}
   unsigned characters=0;for(auto c:title)if(!(c>=0xdc00&&c<=0xdfff))++characters;if(characters>128){++playlistErrors;continue;}
   // Compare names only; playlist entries never become paths to open or IDs.
   for(auto&t:tracks)if(t.additional==extra){auto filename=t.path.filename().wstring();if(CompareStringOrdinal(name.data(),int(name.size()),filename.data(),int(filename.size()),TRUE)==CSTR_EQUAL){t.title=title;break;}}
  }
 }
}
const Track*MusicLibrary::find(const std::string&id)const{auto i=std::find_if(tracks.begin(),tracks.end(),[&](auto&t){return t.id==id;});return i==tracks.end()?nullptr:&*i;}
bool MusicSelection::choose(const MusicLibrary&lib,const std::string&id,bool debug,bool respawning){if(!lib.find(id))return false;if(!debug&&!respawning){pending_=id;return false;}selected_=id;pending_.clear();return true;}
void MusicSelection::respawn(){if(!pending_.empty()){selected_=std::move(pending_);pending_.clear();}}
MusicChoice MusicSelection::resolve(const MusicLibrary&lib)const{
 auto available=[](const Track*t){std::error_code ec;return t&&std::filesystem::is_regular_file(t->path,ec);};
 if(forced_){if(auto t=lib.find(*forced_);available(t))return {t,false};}
 if(auto t=lib.find(selected_);available(t))return {t,bool(forced_)};
 for(auto&t:lib.tracks)if(!t.additional&&available(&t))return {&t,bool(forced_)};
 return {nullptr,bool(forced_)};
}
}
