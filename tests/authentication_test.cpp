#include "authentication.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
void require(bool condition){if(!condition)throw std::runtime_error("Authentication contract test failed");}
int main(){
 const std::string denied="1,0,0_0_0_0_0_0_0_0_0_0,0000000000000000";
 const std::string good="0,123,1000000_1000000_1000000_1000000_1000000_1000000_1000000_1000000_1000000_1000000,abcdef0100000000";
 require(login_body(L"a+b&c",L"password","x+y&z")=="name=a%2Bb%26c&passwd=5f4dcc3b5aa765d61d8327deb882cf99&seed=x%2By%26z");
 require(login_body(L"id",L"abc","s")=="name=id&passwd=900150983cd24fb0d6963f7d28e17f72&seed=s");
 for(auto id:{L"",L"sixteencharacters",L"bad name",L"日本語"}){bool rejected=false;try{login_body(id,L"abc","seed");}catch(...){rejected=true;}require(rejected);}
 for(auto pw:{L"",L"a b",L"日本語"}){bool rejected=false;try{login_body(L"id",pw,"seed");}catch(...){rejected=true;}require(rejected);}
 require(parse_login_reply(200,L"text/plain; charset=UTF-8",denied).status==AuthStatus::denied);
 auto r=parse_login_reply(200,L"Text/Plain",good+"\r\n");require(r.status==AuthStatus::success&&r.user==123&&r.slots[9]==1000000&&r.session[0]==0xab&&r.session[3]==1&&r.session[7]==0);
 for(auto body:{"", "0,1,0,abcdef0100000000", "0,1,0_0_0_0_0_0_0_0_0_0,0000000000000000", "0,0,0_0_0_0_0_0_0_0_0_0,abcdef0100000000", "1,1,0_0_0_0_0_0_0_0_0_0,0000000000000000", "1,0,1_0_0_0_0_0_0_0_0_0,0000000000000000", "1,0,0_0_0_0_0_0_0_0_0_0,abcdef0100000000", "0,4294967295,0_0_0_0_0_0_0_0_0_0,abcdef0100000000", "0,1,0_0_0_0_0_0_0_0_0_0,abcdefgh00000000", "2,0,0_0_0_0_0_0_0_0_0_0,0000000000000000"}){
  auto bad=parse_login_reply(200,L"text/plain",body);require(bad.status==AuthStatus::protocol_error&&bad.user==0&&bad.session[0]==0);
 }
 for(auto type:{L"text/html",L"text/plainx",L""})require(parse_login_reply(200,type,good).status==AuthStatus::protocol_error);
 require(parse_login_reply(302,L"text/plain",good).status==AuthStatus::protocol_error);
 require(parse_login_reply(200,L"text/plain",good+",extra").status==AuthStatus::protocol_error);
 require(parse_login_reply(200,L"text/plain",good+std::string(1,'\0')).status==AuthStatus::protocol_error);
 require(parse_login_reply(200,L"text/plain",std::string(4097,'0')).status==AuthStatus::protocol_error);
 std::atomic_bool cancelled{true};AuthCredentials c(L"unused",L"unused");require(authenticate(c,cancelled).status==AuthStatus::cancelled);
 std::cout<<"Authentication request, response boundaries and pre-cancellation passed (offline).\n";
}
