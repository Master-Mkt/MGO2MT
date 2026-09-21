#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include "authentication.h"
#include <algorithm>
#include <charconv>
#include <cwctype>
#include <stdexcept>
#include <vector>
namespace mgo2mt {
namespace {
struct Handle {HINTERNET h=nullptr;~Handle(){if(h)WinHttpCloseHandle(h);}operator HINTERNET()const{return h;}};
struct Wiped {std::string text;~Wiped(){if(!text.empty())SecureZeroMemory(text.data(),text.size());}};
void check(bool b){if(!b)throw std::runtime_error("Authentication operation failed");}
std::string ascii(std::wstring_view s){check(std::all_of(s.begin(),s.end(),[](wchar_t c){return c>=33&&c<=126;}));std::string out;out.reserve(s.size());for(wchar_t c:s)out.push_back(static_cast<char>(c));return out;}
std::string hex(const unsigned char*p,size_t n){std::string s;for(size_t i=0;i<n;++i){s+= "0123456789abcdef"[p[i]>>4];s+="0123456789abcdef"[p[i]&15];}return s;}
std::string md5(std::string_view text){
 BCRYPT_ALG_HANDLE a=nullptr;BCRYPT_HASH_HANDLE h=nullptr;unsigned char bytes[16]{};
 try{check(BCryptOpenAlgorithmProvider(&a,BCRYPT_MD5_ALGORITHM,nullptr,0)>=0);check(BCryptCreateHash(a,&h,nullptr,0,nullptr,0,0)>=0);
  check(BCryptHashData(h,reinterpret_cast<PUCHAR>(const_cast<char*>(text.data())),static_cast<ULONG>(text.size()),0)>=0);check(BCryptFinishHash(h,bytes,16,0)>=0);
  BCryptDestroyHash(h);h=nullptr;BCryptCloseAlgorithmProvider(a,0);a=nullptr;auto out=hex(bytes,16);SecureZeroMemory(bytes,16);return out;
 }catch(...){if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);SecureZeroMemory(bytes,16);throw;}
}
std::string encode(std::string_view s){std::string out;for(unsigned char c:s){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out+=c;else{out+='%';out+="0123456789ABCDEF"[c>>4];out+="0123456789ABCDEF"[c&15];}}return out;}
std::vector<std::string_view> split(std::string_view s,char delimiter){std::vector<std::string_view> r;size_t at=0;for(;;){auto end=s.find(delimiter,at);r.push_back(s.substr(at,end==s.npos?s.size()-at:end-at));if(end==s.npos)return r;at=end+1;}}
uint32_t number(std::string_view s){check(!s.empty()&&s.size()<=10&&std::all_of(s.begin(),s.end(),[](char c){return c>='0'&&c<='9';}));uint32_t v=0;auto r=std::from_chars(s.data(),s.data()+s.size(),v);check(r.ec==std::errc{}&&r.ptr==s.data()+s.size()&&v<=2147483647);return v;}
AuthReply post(std::string body,const std::atomic_bool& cancel){
 Wiped payload{std::move(body)};AuthReply result;
 try{
  // The deployed login service matches this protocol token exactly to enable
  // the native 64-pixel emblem response. Keep it until server negotiation moves.
  Handle session{WinHttpOpen(L"OpenMGO2-MGO2WIN/0.2",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};check(session.h);check(WinHttpSetTimeouts(session,3000,3000,3000,3000));
  Handle connection{WinHttpConnect(session,L"openmgo2.com",443,0)};check(connection.h);
  const wchar_t* types[]={L"text/plain",nullptr};Handle request{WinHttpOpenRequest(connection,L"POST",L"/index.php",nullptr,WINHTTP_NO_REFERER,types,WINHTTP_FLAG_SECURE)};check(request.h);
  DWORD disabled=WINHTTP_DISABLE_REDIRECTS|WINHTTP_DISABLE_COOKIES|WINHTTP_DISABLE_AUTHENTICATION;check(WinHttpSetOption(request,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled)));
  check(!cancel);check(WinHttpSendRequest(request,L"Content-Type: application/x-www-form-urlencoded\r\nCache-Control: no-store\r\n",static_cast<DWORD>(-1),payload.text.data(),static_cast<DWORD>(payload.text.size()),static_cast<DWORD>(payload.text.size()),0));
  check(WinHttpReceiveResponse(request,nullptr));DWORD size=sizeof(result.http);check(WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&result.http,&size,WINHTTP_NO_HEADER_INDEX));
  if(result.http!=200)return result;
  wchar_t type[256]{};size=sizeof(type);check(WinHttpQueryHeaders(request,WINHTTP_QUERY_CONTENT_TYPE,WINHTTP_HEADER_NAME_BY_INDEX,type,&size,WINHTTP_NO_HEADER_INDEX));
  Wiped response;char buffer[1024]{};auto deadline=GetTickCount64()+6000;
  for(;;){check(!cancel&&GetTickCount64()<deadline);DWORD n=0;check(WinHttpReadData(request,buffer,sizeof(buffer),&n));if(!n)break;if(response.text.size()+n>4096){result.status=AuthStatus::protocol_error;SecureZeroMemory(buffer,sizeof(buffer));return result;}response.text.append(buffer,n);SecureZeroMemory(buffer,sizeof(buffer));}
  result=parse_login_reply(result.http,type,response.text);
 }catch(...){result.status=AuthStatus::network_error;}
 if(cancel)result.status=AuthStatus::cancelled;return result;
}
}
AuthCredentials::AuthCredentials(std::wstring_view name,std::wstring_view pw){check(name.size()<=64&&pw.size()<=64);std::copy(name.begin(),name.end(),id.begin());std::copy(pw.begin(),pw.end(),password.begin());}
AuthCredentials::~AuthCredentials(){SecureZeroMemory(id.data(),sizeof(id));SecureZeroMemory(password.data(),sizeof(password));}
std::string login_body(std::wstring_view name,std::wstring_view password,std::string_view seed){
 check(!name.empty()&&name.size()<=15&&!password.empty()&&password.size()<=64&&!seed.empty()&&seed.size()<=128);
 Wiped pw{ascii(password)},digest{md5(pw.text)};auto id=ascii(name);
 return "name="+encode(id)+"&passwd="+digest.text+"&seed="+encode(seed);
}
AuthReply parse_login_reply(unsigned http,std::wstring_view type,std::string_view body){
 AuthReply r;r.http=http;r.status=AuthStatus::protocol_error;
 try{
  auto lower=std::wstring(type);std::transform(lower.begin(),lower.end(),lower.begin(),[](wchar_t c){return std::towlower(c);});
  check(http==200&&(lower==L"text/plain"||lower.rfind(L"text/plain;",0)==0)&&!body.empty()&&body.size()<=4096);
  while(!body.empty()&&(body.back()=='\r'||body.back()=='\n'))body.remove_suffix(1);
  auto parts=split(body,',');check(parts.size()==4);auto status=number(parts[0]);check(status<=1);r.user=number(parts[1]);
  auto slots=split(parts[2],'_');check(slots.size()==10);for(size_t i=0;i<10;++i)r.slots[i]=number(slots[i]);
  check(parts[3].size()==16);for(size_t i=0;i<8;++i){unsigned value=0;auto h=parts[3].substr(2*i,2);check(std::all_of(h.begin(),h.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');}));auto parsed=std::from_chars(h.data(),h.data()+2,value,16);check(parsed.ec==std::errc{});r.session[i]=static_cast<uint8_t>(value);}
  if(status==1){check(r.user==0&&std::all_of(r.slots.begin(),r.slots.end(),[](auto v){return v==0;})&&std::all_of(r.session.begin(),r.session.end(),[](auto v){return v==0;}));r.status=AuthStatus::denied;}
  else{check(r.user>0&&std::any_of(r.session.begin(),r.session.end(),[](auto v){return v!=0;}));r.status=AuthStatus::success;}
 }catch(...){r.user=0;r.slots.fill(0);r.session.fill(0);r.status=AuthStatus::protocol_error;}
 return r;
}
AuthReply authenticate(const AuthCredentials& c,const std::atomic_bool& cancel){
 if(cancel){AuthReply r;r.status=AuthStatus::cancelled;return r;}
 try{unsigned char random[16]{};check(BCryptGenRandom(nullptr,random,sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0);return post(login_body(c.id.data(),c.password.data(),hex(random,sizeof(random))),cancel);}
 catch(...){AuthReply r;r.status=cancel?AuthStatus::cancelled:AuthStatus::protocol_error;return r;}
}
AuthReply probe_login_route(){std::atomic_bool cancel{false};return post("name=&passwd=&seed=MGO2MT-route-check",cancel);}
}
