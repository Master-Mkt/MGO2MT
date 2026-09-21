#include "multi_ui.h"
#include "multi_ui_layer.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt::multi_ui;
namespace {
unsigned checks=0;
void check(bool value,const char*message){++checks;if(!value)throw std::runtime_error(message);}
std::string layout(std::string elements){return R"({"format":"MGO2MT.UI_LAYOUT.1","width":1280,"height":720,"elements":[)"+elements+"]}";}
void load(Runtime&runtime,const std::string&data,const std::filesystem::path&root){if(!runtime.load_json(data,root))throw std::runtime_error(runtime.error());runtime.update({},true,true,0);runtime.take_events();}
std::vector<ActionEvent> of_kind(Runtime&runtime,const std::string&kind){auto events=runtime.take_events();std::erase_if(events,[&](const auto&e){return e.kind!=kind;});return events;}
std::vector<uint8_t> paint(Runtime&runtime,const Context&context={}){std::vector<uint8_t> pixels(1280*720*4);check(runtime.paint(pixels,1280,720,context),"paint controls");return pixels;}
std::array<uint8_t,4> pixel(const std::vector<uint8_t>&pixels,unsigned x,unsigned y){const auto*p=pixels.data()+(y*1280+x)*4;return {p[0],p[1],p[2],p[3]};}
void click(Runtime&runtime,float x,float y){check(runtime.pointer(x,y,true,{}),"control captures pointer down");check(runtime.pointer(x,y,false,{}),"control captures pointer release");}
}
int main(){try{
 const auto root=std::filesystem::temp_directory_path()/("mgo2mt-ui-controls-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(root);
 const auto image=encode_dds({1,1,{200,100,50,128}});{std::ofstream output(root/"background.dds",std::ios::binary);output.write(reinterpret_cast<const char*>(image.data()),image.size());}
 const auto button=layout(R"({"id":"button","kind":"button","x":100,"y":100,"width":100,"height":40,"text":"START","fontSize":18,"color":[10,20,30,255],"hoverColor":[40,50,60,255],"pressedColor":[70,80,90,255],"textColor":[255,255,255,255],"shadowColor":[1,2,3,128],"shadowX":6,"shadowY":8,"onClick":[{"type":"emitEvent","name":"menu.confirm"}]})");
 Runtime runtime;load(runtime,button,root);auto pixels=paint(runtime);check(pixel(pixels,101,101)==std::array<uint8_t,4>{10,20,30,255},"normal button background");check(pixel(pixels,204,144)==std::array<uint8_t,4>{1,2,3,128},"shadow offset and independent alpha");
 size_t bright=0;for(unsigned y=100;y<140;++y)for(unsigned x=100;x<200;++x){const auto p=pixel(pixels,x,y);bright+=p[0]>200&&p[1]>200&&p[2]>200;}check(bright>25,"button text color independent of dark background");
 check(runtime.pointer(150,120,false,{}),"hover consumes button");check(pixel(paint(runtime),101,101)==std::array<uint8_t,4>{40,50,60,255},"hover color");
 check(runtime.pointer(150,120,true,{}),"press consumes button");check(pixel(paint(runtime),101,101)==std::array<uint8_t,4>{70,80,90,255},"pressed color");check(of_kind(runtime,"event").empty(),"press alone does not activate");
 runtime.pointer(150,120,true,{});runtime.pointer(150,120,true,{});check(of_kind(runtime,"event").empty(),"held pointer does not repeat");runtime.pointer(150,120,false,{});auto events=of_kind(runtime,"event");check(events.size()==1&&events[0].name=="menu.confirm","release emits exactly one action");
 runtime.pointer(500,500,false,{});check(pixel(paint(runtime),101,101)==std::array<uint8_t,4>{10,20,30,255},"pointer leaving restores normal color");
 runtime.pointer(150,120,true,{});runtime.pointer(500,500,true,{});runtime.pointer(500,500,false,{});check(of_kind(runtime,"event").empty(),"drag away cancels activation");
 check(runtime.click(150,120,{}),"legacy logical click remains available");check(of_kind(runtime,"event").size()==1,"legacy click emits once");
 load(runtime,layout(R"({"id":"image","kind":"button","x":10,"y":10,"width":40,"height":30,"texture":"background.dds","color":[255,255,255,255],"hoverColor":[128,255,255,255],"shadowColor":[7,8,9,100],"shadowX":10,"shadowY":10})"),root);pixels=paint(runtime);check(pixel(pixels,11,11)==std::array<uint8_t,4>{200,100,50,128},"button background image preserves original alpha");check(pixel(pixels,55,45)==std::array<uint8_t,4>{7,8,9,50},"image shadow uses alpha mask rather than RGB tint");runtime.pointer(20,20,false,{});check(pixel(paint(runtime),11,11)==std::array<uint8_t,4>{100,100,50,128},"hover color tints background image");

 const auto delayed=layout(R"({"id":"wait","kind":"button","x":10,"y":10,"width":100,"height":40,"delayMs":100,"onClick":[{"type":"delay","ms":50},{"type":"emitEvent","name":"first"},{"type":"delay","ms":200},{"type":"emitEvent","name":"second"}]})");
 load(runtime,delayed,root);click(runtime,20,20);check(runtime.busy(),"element delay keeps sequence busy");check(!runtime.trigger("wait",{}),"busy sequence ignores repeated activation");runtime.update({},true,true,99);check(of_kind(runtime,"event").empty(),"element delay waits full duration");runtime.update({},true,true,100);check(of_kind(runtime,"event").empty(),"action delay begins after element delay");runtime.update({},true,true,149);check(of_kind(runtime,"event").empty(),"action delay waits full duration");runtime.update({},true,true,150);events=of_kind(runtime,"event");check(events.size()==1&&events[0].name=="first","first delayed event");runtime.update({},true,true,349);check(of_kind(runtime,"event").empty()&&runtime.busy(),"second delay is not skipped");runtime.update({},true,true,350);events=of_kind(runtime,"event");check(events.size()==1&&events[0].name=="second"&&!runtime.busy(),"sequence completes at deadline");
 load(runtime,delayed,root);click(runtime,20,20);runtime.update({},false,true,50);runtime.update({},true,true,1000);check(!runtime.busy()&&of_kind(runtime,"event").empty(),"focus loss cancels delay permanently");
 load(runtime,delayed,root);click(runtime,20,20);check(runtime.key(27,{}),"Escape consumes active delay");runtime.update({},true,true,1000);check(!runtime.busy()&&of_kind(runtime,"event").empty(),"Escape cancels delayed actions");

 const auto dropdown=layout(R"({"id":"choice","kind":"dropdown","x":400,"y":400,"width":200,"height":32,"bind":"weather","selected":"sun","options":[{"value":"sun","label":"Sunny"},{"value":"rain","label":"Rain"}],"delayMs":100,"onChange":[{"type":"emitEvent","name":"weather.changed"}]},{"id":"bound","kind":"text","x":620,"y":400,"width":200,"height":32,"text":"before","bind":"weather"})");
 load(runtime,dropdown,root);auto before=paint(runtime);click(runtime,420,415);click(runtime,420,480);events=runtime.take_events();check(std::any_of(events.begin(),events.end(),[](const auto&e){return e.kind=="variable"&&e.name=="weather"&&e.value=="rain";}),"selection updates local binding immediately");check(std::any_of(events.begin(),events.end(),[](const auto&e){return e.kind=="selection"&&e.value=="rain";}),"selection notification carries value");check(std::none_of(events.begin(),events.end(),[](const auto&e){return e.kind=="event";}),"dropdown onChange honors element delay");auto after=paint(runtime);bool labelChanged=false;for(unsigned y=400;y<432;++y)for(unsigned x=620;x<820;++x)labelChanged|=pixel(before,x,y)!=pixel(after,x,y);check(labelChanged,"bound text paints changed selection");runtime.update({},true,true,100);events=of_kind(runtime,"event");check(events.size()==1&&events[0].name=="weather.changed"&&events[0].value=="rain","onChange event includes selected value");click(runtime,420,415);click(runtime,420,480);check(!runtime.busy()&&of_kind(runtime,"event").empty(),"selecting current option does not emit duplicate change");
 load(runtime,dropdown,root);check(runtime.key(9,{}),"Tab focuses dropdown");check(runtime.key(13,{}),"Enter opens focused dropdown");check(runtime.key(40,{}),"Down changes highlighted option");check(runtime.key(13,{}),"Enter selects highlighted option");runtime.update({},true,true,100);events=of_kind(runtime,"event");check(events.size()==1&&events[0].value=="rain","keyboard chooses second option");check(runtime.key(13,{}),"keyboard reopens dropdown");check(runtime.key(38,{}),"Up changes highlighted option");check(runtime.key(27,{}),"Escape closes open dropdown");check(of_kind(runtime,"event").empty(),"Escape does not change selection");
 std::string options;for(unsigned n=0;n<64;++n){if(n)options+=',';options+="{\"value\":\"v"+std::to_string(n)+"\",\"label\":\"Option "+std::to_string(n)+"\"}";}
 const auto paged=layout("{\"id\":\"many\",\"kind\":\"dropdown\",\"x\":400,\"y\":680,\"width\":200,\"height\":32,\"options\":["+options+"],\"onChange\":[{\"type\":\"emitEvent\",\"name\":\"chosen\"}]}");
 load(runtime,paged,root);click(runtime,420,695);check(pixel(paint(runtime),401,9)[3]==255,"bottom-edge popup flips above and stays on canvas");for(int n=0;n<3;++n)click(runtime,420,664);click(runtime,420,632);events=of_kind(runtime,"event");check(events.size()==1&&events[0].value=="v63","pointer paging reaches final of64 options");
 load(runtime,paged,root);runtime.key(9,{});runtime.key(13,{});for(int n=0;n<63;++n)runtime.key(40,{});runtime.key(13,{});events=of_kind(runtime,"event");check(events.size()==1&&events[0].value=="v63","keyboard navigation scrolls to last option");
 load(runtime,paged,root);click(runtime,420,695);click(runtime,420,664);check(runtime.key(13,{}),"keyboard selection after pointer paging");events=of_kind(runtime,"event");check(events.size()==1&&events[0].value=="v19","mixed pointer and keyboard paging has valid row");

 // Completed sequences still have queued actionable events until the caller drains them.
 load(runtime,button,root);runtime.click(150,120,{});Context next;next.state="lobby";runtime.update(next,true,true,1);check(of_kind(runtime,"event").empty(),"page change drops completed old-page event before drain");
 load(runtime,button,root);runtime.click(150,120,{});runtime.update({},false,true,1);check(of_kind(runtime,"event").empty(),"focus loss drops completed queued event");
 load(runtime,button,root);runtime.click(150,120,{});runtime.cancel_actions();events=runtime.take_events();check(std::none_of(events.begin(),events.end(),[](const auto&e){return e.kind=="event";}),"manual cancellation drops completed queued event");check(std::any_of(events.begin(),events.end(),[](const auto&e){return e.kind=="completed";}),"cancellation preserves diagnostic completion");
 load(runtime,button,root);runtime.click(150,120,{});check(runtime.load_json(button,root),"layout reload");check(of_kind(runtime,"event").empty(),"layout reload drops old queued event");
 const auto old=layout(R"({"id":"old","kind":"panel","width":10,"height":10,"onChange":[],"delayMs":0,"shadowX":0,"shadowY":0,"options":[],"selected":""})");check(runtime.load_json(old,root),"Designer empty control defaults remain valid on old element kinds");
 for(const auto&row:{
  R"({"id":"bad","kind":"dropdown","width":100,"height":30,"options":[]})",
  R"({"id":"bad","kind":"dropdown","width":100,"height":30,"options":[{"value":"x","label":"one"},{"value":"x","label":"two"}]})",
  R"({"id":"bad","kind":"dropdown","width":100,"height":30,"options":[{"value":" ","label":"one"}]})",
  R"({"id":"bad","kind":"dropdown","width":100,"height":30,"selected":"y","options":[{"value":"x","label":"one"}]})",
  R"({"id":"bad","kind":"button","width":100,"height":30,"delayMs":60001})",
  R"({"id":"bad","kind":"button","width":100,"height":30,"delayMs":0.5})",
  R"({"id":"bad","kind":"button","width":100,"height":30,"delayMs":1.00000001})",
  R"({"id":"bad","kind":"button","width":100,"height":30,"onClick":[{"type":"delay","ms":-1}]})",
  R"({"id":"bad","kind":"button","width":100,"height":30,"onClick":[{"type":"delay"}]})",
  R"({"id":"bad","kind":"button","width":100,"height":30,"textColor":[0,0,0,256]})",
  R"({"id":"bad","kind":"button","width":100,"height":30,"shadowX":257})",
  R"({"id":"bad","kind":"button","width":100,"height":30,"texture":"../background.dds"})",
  R"({"id":"bad","kind":"button","width":100,"height":30,"onChange":[{"type":"emitEvent","name":"bad"}]})"
 })check(!runtime.load_json(layout(row),root)&&!runtime.ready(),"invalid control configuration fails transactionally");
 check(runtime.load_json(layout(R"({"id":"max","kind":"button","width":100,"height":30,"delayMs":60000,"onClick":[{"type":"delay","ms":60000}]})"),root),"maximum permitted delay loads");
 std::string many;for(unsigned n=0;n<4096;++n){if(n)many+=',';many+="{\"id\":\"n"+std::to_string(n)+"\",\"kind\":\"panel\",\"width\":1,\"height\":1}";}check(runtime.load_json(layout(many),root),"4096 small LA2-derived elements fit bounded layout");many+=R"(,{"id":"overflow","kind":"panel","width":1,"height":1})";check(!runtime.load_json(layout(many),root),"4097 elements exceed layout bound");
 std::string layers;for(unsigned n=0;n<32;++n){if(n)layers+=',';layers+="{\"id\":\"layer"+std::to_string(n)+"\",\"kind\":\"panel\",\"width\":1280,\"height\":720}";}check(runtime.load_json(layout(layers),root),"32 canvas layers allow actual21.3-canvas title import");layers+=R"(,{"id":"layer_overflow","kind":"panel","width":1,"height":1})";check(!runtime.load_json(layout(layers),root),"paint budget rejects even onepixel above32 canvases");
 std::filesystem::remove_all(root);std::cout<<checks<<" native UI control checks passed: pointer, rendering, image/shadow/text, dropdown binding, keyboard, paging, delay and scope cancellation\n";
}catch(const std::exception&error){std::cerr<<error.what()<<'\n';return 1;}}
