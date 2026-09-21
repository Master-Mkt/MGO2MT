#include "http_text.h"
#include <iostream>
#include <thread>
#include <stdexcept>
using namespace mgo2mt;
static void require(bool ok){if(!ok)throw std::runtime_error("HTTP contract assertion");}
int main(int argc,char** argv){try{
 if(argc==2&&std::string(argv[1])=="--live"){
  std::atomic_bool stop{false};HttpText r;std::thread worker([&]{r=fetch_policy(L"https://openmgo2.com/jp/mgo2/policy/policy.txt",stop);});worker.join();
  std::cout<<"status="<<r.status<<" bytes="<<r.bytes<<" hash="<<r.sha256<<" error="<<r.error<<"\n";return r.error.empty()?0:1;
 }
 require(allowed_policy_url(L"https://openmgo2.com/jp/mgo2/policy/policy.txt"));
 for(auto url:{L"http://openmgo2.com/policy",L"https://openmgo2.com.evil.example/policy",L"https://savemgo.com/policy",L"https://mgo2pc.com/policy",L"https://openmgo2.com@evil.example/policy",L"https://openmgo2.com/\"",L"https://openmgo2.com/\n"})require(!allowed_policy_url(url));
 require(decode_policy(200,L"text/plain; charset=utf-8","日本語\n  AA & <text>\t\n")==L"日本語\n  AA & <text>\t\n");
 require(decode_policy(200,L"TEXT/PLAIN; CHARSET=UTF-8",std::string("\xef\xbb\xbf")+"ABC")==L"ABC");
 auto rejected=[](unsigned status,const std::wstring& type,const std::string& body){try{decode_policy(status,type,body);}catch(const std::exception&){return true;}return false;};
 for(unsigned status:{301,302,308,404,500})require(rejected(status,L"text/plain","text"));
 require(rejected(200,L"text/html","<html>"));require(rejected(200,L"text/plain; charset=shift_jis","text"));
 for(auto body:{std::string(),std::string(" \n\t"),std::string("a\0b",3),std::string("\xc0\xaf",2),std::string("\xed\xa0\x80",3),std::string("a\x1b"),std::string(0x40000,'x')})require(rejected(200,L"text/plain",body));
 std::atomic_bool stop{false};require(!fetch_policy(L"https://savemgo.com/",stop).error.empty()); // Rejected before any socket.
 std::cout<<"OpenMGO2 URL, status, type, UTF-8, controls and bounds passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what();return 1;}}
