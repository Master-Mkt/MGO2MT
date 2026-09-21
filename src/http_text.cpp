// Native replacement for AB30AC -> D7D050. GET only, OpenMGO2 origin only.
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <algorithm>
#include <cwctype>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include "http_text.h"
namespace mgo2mt {
namespace {
constexpr size_t limit=0x40000; // Original AB30AC receive buffer capacity.
struct Handle { HINTERNET h=nullptr; ~Handle(){if(h)WinHttpCloseHandle(h);} operator HINTERNET()const{return h;} };
void check(BOOL ok){if(!ok)throw std::runtime_error("Network error "+std::to_string(GetLastError()));}
std::string hash(const std::string& bytes){
 BCRYPT_ALG_HANDLE a=nullptr;BCRYPT_HASH_HANDLE h=nullptr;
 auto ok=[](NTSTATUS s){if(s<0)throw std::runtime_error("SHA-256 failure");};
 try {ok(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0));ok(BCryptCreateHash(a,&h,nullptr,0,nullptr,0,0));
  ok(BCryptHashData(h,reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),static_cast<ULONG>(bytes.size()),0));
  unsigned char out[32];ok(BCryptFinishHash(h,out,32,0));BCryptDestroyHash(h);h=nullptr;BCryptCloseAlgorithmProvider(a,0);a=nullptr;
  std::ostringstream s;for(auto b:out)s<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(b);return s.str();
 }catch(...){if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);throw;}
}
}
bool allowed_policy_url(const std::wstring& url){
 if(url.size()>2048||url.rfind(L"https://openmgo2.com/",0)!=0)return false;
 return std::none_of(url.begin(),url.end(),[](wchar_t c){return c<=32||c>=127||c==L'#'||c==L'"'||c==L'\\';});
}
std::wstring decode_policy(unsigned status,const std::wstring& type,const std::string& bytes){
 if(status!=200)throw std::runtime_error("HTTP "+std::to_string(status));
 auto lower=type;std::transform(lower.begin(),lower.end(),lower.begin(),[](wchar_t c){return std::towlower(c);});
 if(lower!=L"text/plain"&&lower.rfind(L"text/plain;",0)!=0)throw std::runtime_error("Expected text/plain");
 auto charset=lower.find(L"charset=");
 if(charset!=std::wstring::npos){auto encoding=lower.substr(charset+8);if(encoding!=L"utf-8"&&encoding!=L"\"utf-8\"")throw std::runtime_error("Expected UTF-8");}
 if(bytes.empty()||bytes.size()>=limit)throw std::runtime_error("Empty or oversized text");
 if(bytes.find('\0')!=std::string::npos)throw std::runtime_error("NUL in policy text");
 const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data(),static_cast<int>(bytes.size()),nullptr,0);
 if(n<=0)throw std::runtime_error("Invalid UTF-8");
 std::wstring text(n,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data(),static_cast<int>(bytes.size()),text.data(),n);
 if(!text.empty()&&text[0]==0xfeff)text.erase(0,1);
 if(std::any_of(text.begin(),text.end(),[](wchar_t c){return (c<32&&c!=L'\r'&&c!=L'\n'&&c!=L'\t')||c==127;}))throw std::runtime_error("Control character in text");
 if(std::all_of(text.begin(),text.end(),[](wchar_t c){return std::iswspace(c);}))throw std::runtime_error("Blank policy text");
 return text;
}
HttpText fetch_policy(const std::wstring& url,const std::atomic_bool& cancel){
 HttpText result;
 try {
  if(!allowed_policy_url(url))throw std::runtime_error("Only https://openmgo2.com is allowed");
  Handle session{WinHttpOpen(L"OpenMGO2-MGO2MT/0.1",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
  check(session.h!=nullptr);check(WinHttpSetTimeouts(session,3000,3000,3000,3000));
  Handle connection{WinHttpConnect(session,L"openmgo2.com",INTERNET_DEFAULT_HTTPS_PORT,0)};check(connection.h!=nullptr);
  const auto path=url.substr(std::wstring(L"https://openmgo2.com").size());
  const wchar_t* types[]={L"text/plain",nullptr};
  Handle request{WinHttpOpenRequest(connection,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,types,WINHTTP_FLAG_SECURE)};check(request.h!=nullptr);
  DWORD disabled=WINHTTP_DISABLE_REDIRECTS|WINHTTP_DISABLE_COOKIES|WINHTTP_DISABLE_AUTHENTICATION;
  check(WinHttpSetOption(request,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled)));
  if(cancel)throw std::runtime_error("Cancelled");
  check(WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0));check(WinHttpReceiveResponse(request,nullptr));
  DWORD status=0,size=sizeof(status);check(WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX));result.status=status;
  if(status!=200)throw std::runtime_error("HTTP "+std::to_string(status));
  wchar_t type[256]{};size=sizeof(type);check(WinHttpQueryHeaders(request,WINHTTP_QUERY_CONTENT_TYPE,WINHTTP_HEADER_NAME_BY_INDEX,type,&size,WINHTTP_NO_HEADER_INDEX));
  std::string bytes;char chunk[8192];const auto deadline=GetTickCount64()+10000;
  for(;;){if(cancel||GetTickCount64()>deadline)throw std::runtime_error(cancel?"Cancelled":"Read deadline exceeded");
   DWORD n=0;check(WinHttpReadData(request,chunk,sizeof(chunk),&n));if(!n)break;
   if(bytes.size()+n>=limit)throw std::runtime_error("Text exceeds 256 KiB");bytes.append(chunk,n);
  }
  result.text=decode_policy(status,type,bytes);result.bytes=bytes.size();result.sha256=hash(bytes);
 }catch(const std::exception& e){result.error=e.what();}
 return result;
}
}
