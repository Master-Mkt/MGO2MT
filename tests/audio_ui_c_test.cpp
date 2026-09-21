#include "multi_ui_c.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
int main(){void*h=mui_create();try{
 if(!h)throw std::runtime_error("create");std::string doc=R"({"format":"MGO2MT.UI_LAYOUT.1","width":1280,"height":720,"elements":[{"id":"old","kind":"panel","width":20,"height":20,"onClick":[{"type":"emitEvent","name":"OLD_START"}]}]})";
 int checks=0;auto check=[&](bool v,const char*s){++checks;if(!v)throw std::runtime_error(s);};auto load=[&]{check(mui_load(h,doc.c_str(),doc.size(),".")==1,"load");check(mui_tick(h,"title","idle","[]","{}",1,0)==1,"tick");check(mui_trigger(h,"old")==1,"trigger");check(mui_action_events(h,nullptr,0)>2,"event size query cache");};auto clean=[&]{auto n=mui_action_events(h,nullptr,0);std::vector<char>buf(n);check(mui_action_events(h,buf.data(),n)==n,"copy");check(std::string(buf.data()).find("OLD_START")==std::string::npos,"stale event leaked from old layout");};
 load();check(mui_load(h,doc.c_str(),doc.size(),".")==1,"reload same layout");clean();
 load();check(mui_load(h,"{}",2,".")==0,"failed load");clean();
 load();mui_cancel_actions(h);clean();
 load();check(mui_tick(h,"title","idle","[]","{}",0,0)==1,"lost focus");clean();
 load();check(mui_tick(h,"menu","idle","[]","{}",1,0)==1,"changed page");clean();
 mui_destroy(h);std::cout<<"PASS "<<checks<<" C API cached event lifecycle checks\n";return 0;
 }catch(const std::exception&e){mui_destroy(h);std::cerr<<e.what()<<'\n';return 1;}}
