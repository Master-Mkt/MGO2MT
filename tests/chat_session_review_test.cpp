#include "chat_session.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2win::chat;
static void check(bool value,const char* label){if(!value)throw std::runtime_error(label);}
static void join(Session& s){s.connect(7);s.enter(17);s.roster({{7,"SELF",1},{8,"PEER",2}},true);}
int main(){try{
    Session s;s.connect(7);s.enter(17);
    check(!s.request_capability(11,1),"capability must wait for completed membership");
    s.roster({{7,"SELF",1},{8,"PEER",2}},true);
    auto request=s.request_capability(11,2);check(bool(request),"joined member can query capability");
    auto reply=*request;reply.resize(28);reply[5]=0;reply[6]=1;reply[7]=1;
    check(s.receive_capability(reply,3),"matched capability accepted");
    check(!s.state().teamSupported,"room-only capability cannot enable team");
    auto generation=s.state().generation;
    check(s.submit(generation,"draft",false,4)==Submit::accepted,"queued explicit draft");
    s.roster({{7,"SELF",1},{7,"DUPLICATE",1}},true);
    check(!s.state().joined&&!s.take(5),"invalid roster revokes stale membership and queued send");
    check(s.submit(generation,"stale",false,6)==Submit::not_joined,"invalid roster cannot send using old self");
    join(s);generation=s.state().generation;
    s.submit(generation,"body",false,100);check(bool(s.take(101)),"pending before self removal");
    s.roster({{8,"PEER",2}},true);
    check(!s.receive({7,0,"body"},102),"removed self late echo cannot restore joined status");
    check(s.state().delivery==Delivery::none,"removed self has no pending delivery");
    join(s);generation=s.state().generation;
    check(s.submit(generation,"OpenMGO2 | text",false,200)==Submit::accepted,"server-prefix text is not local sender authority");
    check(bool(s.take(201)),"explicit reserved prefix gets sent for existing server rejection");
    s.receive({7,0,"OpenMGO2 | rejected"},202);
    check(s.state().delivery==Delivery::awaiting_echo,"server rejection text is not exact echo");
    check(s.state().lines.back().name=="SELF","body prefix does not replace roster name");
    s.leave();s.enter(17);s.roster({{7,"SELF",1}},true);
    check(!s.receive_capability(reply,203),"previous room capability without new request refused");
    check(s.submit(generation,"old UI",false,204)==Submit::not_joined,"old UI generation refused");
    std::cout<<"chat_session_review_test PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
