#include "product_identity.h"
#include "multi_ui.h"
#include "multi_ui_json.h"
#include "multi_ui_audio.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cwctype>
#include <fstream>
#include <stdexcept>
namespace mgo2mt::multi_ui {
namespace {
using J=json::Value;
void require(bool b,const char*m){if(!b)throw std::runtime_error(m);}
const J*field(const J&v,const char*k){auto i=v.o.find(k);return i==v.o.end()?nullptr:&i->second;}
std::string str(const J&v,const char*k,std::string fallback,size_t max=4096){auto p=field(v,k);if(!p)return fallback;require(p->type==J::string&&p->s.size()<=max,"Invalid UI string field");return p->s;}
float number(const J&v,const char*k,float fallback,float low,float high){auto p=field(v,k);if(!p)return fallback;require(p->type==J::number&&p->n>=low&&p->n<=high,"UI number outside bounds");return float(p->n);}
bool boolean(const J&v,const char*k,bool fallback){auto p=field(v,k);if(!p)return fallback;require(p->type==J::boolean,"Invalid UI boolean");return p->b;}
void keys(const J&v,std::initializer_list<std::string_view>allowed){require(v.type==J::object,"Expected UI JSON object");for(auto&[k,_]:v.o)require(std::find(allowed.begin(),allowed.end(),k)!=allowed.end(),"Unknown UI schema field");}
std::set<std::string> names(const J&v){require(v.type==J::array&&v.a.size()<=64,"UI flags must be a bounded array");std::set<std::string>r;for(const auto&x:v.a){require(x.type==J::string&&!x.s.empty()&&x.s.size()<=128&&r.insert(x.s).second,"Invalid or duplicate UI flag");}return r;}
std::wstring wide(std::string_view s){json::utf8(s);std::wstring w(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0),L'\0');if(!w.empty())MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),w.data(),int(w.size()));return w;}
std::filesystem::path safe_asset(const std::filesystem::path&root,const std::string&s){
 require(!s.empty()&&s.size()<=512&&s.find('\\')==s.npos&&s.find(':')==s.npos,"Invalid relative UI asset path");auto p=std::filesystem::path(wide(s));require(!p.is_absolute()&&!p.has_root_path(),"Absolute UI asset path rejected");for(const auto&part:p)require(part!=L".."&&part!=L"."&&!part.empty(),"UI asset traversal rejected");
 auto resolved=std::filesystem::canonical(root/p);auto relative=resolved.lexically_relative(root);require(!relative.empty()&&!relative.is_absolute(),"UI asset outside root");for(const auto&part:relative)require(part!=L"..","UI linked asset escapes root");return resolved;
}
void blend(uint8_t*d,const uint8_t*s){const unsigned sa=s[3],da=d[3],a=sa*255+da*(255-sa);if(!a)return;for(int c=0;c<3;++c)d[c]=uint8_t((unsigned(s[c])*sa*255+unsigned(d[c])*da*(255-sa)+a/2)/a);d[3]=uint8_t((a+127)/255);}
Image text_image(std::string_view text,std::string_view family,float fontSize,unsigned w,unsigned h,unsigned align=0){
 require(w&&h&&w<=1280&&h<=720,"UI text rectangle exceeds design canvas");Image image{w,h,std::vector<uint8_t>(size_t(w)*h*4)};if(text.empty())return image;
 auto dc=CreateCompatibleDC(nullptr);require(dc!=nullptr,"UI text DC creation failed");BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=LONG(w);info.bmiHeader.biHeight=-LONG(h);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void*data=nullptr;auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&data,nullptr,0);auto font=CreateFontW(-int(std::lround(fontSize)),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,wide(family).c_str());
 if(!bitmap||!font){if(bitmap)DeleteObject(bitmap);if(font)DeleteObject(font);DeleteDC(dc);throw std::runtime_error("UI text raster allocation failed");}
 auto oldBitmap=SelectObject(dc,bitmap),oldFont=SelectObject(dc,font);std::memset(data,0,size_t(w)*h*4);SetTextColor(dc,RGB(255,255,255));SetBkMode(dc,TRANSPARENT);RECT rect{0,0,LONG(w),LONG(h)};auto value=wide(text);DrawTextW(dc,value.data(),int(value.size()),&rect,(align?(DT_VCENTER|DT_SINGLELINE|(align==1?DT_CENTER:DT_LEFT)):(DT_LEFT|DT_TOP|DT_WORDBREAK))|DT_NOPREFIX);GdiFlush();auto source=static_cast<uint8_t*>(data);for(size_t i=0;i<image.rgba.size();i+=4){image.rgba[i]=image.rgba[i+1]=image.rgba[i+2]=255;image.rgba[i+3]=source[i];}
 SelectObject(dc,oldFont);SelectObject(dc,oldBitmap);DeleteObject(font);DeleteObject(bitmap);DeleteDC(dc);return image;
}
}
struct Runtime::Impl {
 static constexpr size_t none=size_t(-1);
 struct Action {std::string type,name,value,target,sound;std::filesystem::path path;float volume=1;bool wait=true,visible=true;uint32_t ms=0;};
 struct Option {std::string value,label;};
 struct Element {
  std::string id,kind,screen,state,texture,text,bind,font,selected;
  float x=0,y=0,w=0,h=0,z=0,fontSize=24,ax=0,ay=0,shadowX=0,shadowY=0;
  bool visible=true;uint32_t delayMs=0;
  std::array<uint8_t,4>color{255,255,255,255},hoverColor{},pressedColor{},shadowColor{},textColor{255,255,255,255};
  bool hasHover=false,hasPressed=false;
  std::set<std::string>all,noneFlags;std::shared_ptr<const Image>image;
  mutable Image glyphs;mutable std::string glyphText;
  std::vector<Action>actions,changes;std::vector<Option>options;
  bool control()const{return kind=="button"||kind=="dropdown";}
  float left()const{return ax*(1280-w)+x;}float top()const{return ay*(720-h)+y;}
 };
 struct Hit {size_t element=none;int option=-1;bool operator==(const Hit&)const=default;};
 struct Popup {float left=0,top=0,w=0,row=24;size_t first=0,count=0;bool pages=false;float height()const{return row*float(count+(pages?2:0));}};
 std::vector<Element>elements;
 std::map<std::string,std::string>variables,selections;std::map<std::string,bool>visibility;
 Context context;bool contextSeen=false,focused=true,soundEnabled=true,active=false,waiting=false,waitForEnd=true,change=false,delaying=false,clockSeen=false,pointerDown=false,keyFocus=false;
 size_t element=0,step=0,open=none,scroll=0,focus=none;int keyboardOption=-1;uint64_t token=0,now=0,deadline=0;std::string actionValue;Hit hover,pressed;
 bool shown(const Element&e,const Context&c)const{auto v=visibility.find(e.id);if(!(v==visibility.end()?e.visible:v->second)||(e.screen!="*"&&e.screen!=c.screen)||(e.state!="*"&&e.state!=c.state))return false;for(const auto&f:e.all)if(!c.flags.contains(f))return false;for(const auto&f:e.noneFlags)if(c.flags.contains(f))return false;return true;}
 std::string selection(const Element&e,const Context&c)const{
  auto value=e.selected;if(auto at=selections.find(e.id);at!=selections.end())value=at->second;
  if(!e.bind.empty()){if(auto at=c.bindings.find(e.bind);at!=c.bindings.end())value=at->second;if(auto at=variables.find(e.bind);at!=variables.end())value=at->second;}
  for(const auto&o:e.options)if(o.value==value)return value;
  return e.options.empty()?std::string{}:e.options.front().value;
 }
 Popup popup()const{
  const auto&e=elements[open];Popup p;p.left=std::clamp(e.left(),0.f,std::max(0.f,1280-e.w));p.w=e.w;p.row=std::clamp(e.h,20.f,64.f);
  const float below=std::max(0.f,720-e.top()-e.h),above=std::max(0.f,e.top());float room=std::max(below,above);
  if(room<p.row*3)room=720;
  const size_t slots=std::max<size_t>(3,size_t(room/p.row));p.pages=e.options.size()>slots;p.count=std::min(e.options.size(),p.pages?slots-2:slots);
  p.first=std::min(scroll,e.options.size()-p.count);const float height=p.height();
  p.top=below>=height?e.top()+e.h:above>=height?e.top()-height:std::clamp(e.top()+e.h,0.f,720-height);return p;
 }
 Hit hit(float x,float y,const Context&c)const{
  if(!std::isfinite(x)||!std::isfinite(y)||x<0||y<0||x>=1280||y>=720)return {};
  if(open!=none&&shown(elements[open],c)){auto p=popup();if(x>=p.left&&x<p.left+p.w&&y>=p.top&&y<p.top+p.height()){
   auto row=size_t((y-p.top)/p.row);if(p.pages){if(!row)return {open,-2};if(row==p.count+1)return {open,-3};--row;}return {open,int(p.first+row)};}}
  for(size_t n=elements.size();n>0;--n){const auto&e=elements[n-1];if((!e.control()&&e.actions.empty())||!shown(e,c))continue;if(x>=e.left()&&x<e.left()+e.w&&y>=e.top()&&y<e.top()+e.h)return {n-1,-1};}return {};
 }
};
Runtime::Runtime()=default;Runtime::Runtime(std::unique_ptr<AudioBackend>a):audio_(std::move(a)){}Runtime::~Runtime()=default;Runtime::Runtime(Runtime&&)noexcept=default;Runtime&Runtime::operator=(Runtime&&)noexcept=default;
bool Runtime::ready()const noexcept{return bool(impl_);}
bool Runtime::busy()const{return impl_&&(impl_->active||impl_->token);}
void Runtime::notice(ActionEvent e){if(events_.size()>=128)events_.erase(events_.begin());events_.push_back(std::move(e));}
std::vector<ActionEvent> Runtime::take_events(){auto out=std::move(events_);events_.clear();return out;}
void Runtime::cancel_actions(std::string reason){
 // A native page transition may happen between emitting and draining events.
 // Do not allow a completed old-page click to confirm the following page.
 std::erase_if(events_,[](const ActionEvent&e){return e.kind=="event"||e.kind=="variable"||e.kind=="visibility"||e.kind=="selection";});
 if(audio_)audio_->stop();if(!impl_)return;auto&i=*impl_;if(i.active||i.token)notice({"cancelled",i.elements[i.element].id,{},{},std::move(reason)});i.active=false;i.waiting=false;i.delaying=false;i.token=0;
}
void Runtime::advance_actions(){
 if(!impl_||!impl_->active||impl_->waiting)return;auto&i=*impl_;auto&e=i.elements[i.element];
 if(i.delaying){if(i.now<i.deadline)return;i.delaying=false;}
 const auto&actions=i.change?e.changes:e.actions;
 while(i.active&&!i.waiting){
  if(i.step>=actions.size()){i.active=false;notice({"completed",e.id,{},i.actionValue});break;}const auto&a=actions[i.step];
  if(a.type=="delay"){++i.step;if(a.ms){i.deadline=i.now>UINT64_MAX-a.ms?UINT64_MAX:i.now+a.ms;i.delaying=true;return;}continue;}
  if(a.type=="playSound"){try{if(!audio_)audio_=make_audio_backend();if(nextToken_==UINT64_MAX)throw std::runtime_error("UI audio token exhausted");i.token=nextToken_++;i.waiting=true;i.waitForEnd=a.wait;audio_->mute(!i.soundEnabled);std::string error;if(!audio_->start(a.path,a.volume,i.token,error))throw std::runtime_error(error);notice({"soundRequested",e.id,a.sound});}catch(const std::exception&error){error_=error.what();notice({"failed",e.id,{},{},error_});cancel_actions("audio failed");}return;}
  if(a.type=="emitEvent")notice({"event",e.id,a.name,i.actionValue});
  else if(a.type=="setVariable"){i.variables[a.name]=a.value;++revision_;notice({"variable",e.id,a.name,a.value});}
  else if(a.type=="setVisible"){i.visibility[a.target]=a.visible;++revision_;notice({"visibility",e.id,a.target,a.visible?"true":"false"});}
  ++i.step;
 }
}
void Runtime::update(const Context&context,bool focused,bool soundEnabled,uint64_t now){
 if(!impl_)return;auto&i=*impl_;if(now==UINT64_MAX)now=GetTickCount64();i.now=i.clockSeen?std::max(i.now,now):now;i.clockSeen=true;
 const bool switched=i.contextSeen&&(i.context.screen!=context.screen||i.context.state!=context.state);
 if(switched){cancel_actions("screen or state changed");i.variables.clear();i.visibility.clear();i.selections.clear();++revision_;}
 i.context=context;i.contextSeen=true;i.focused=focused;i.soundEnabled=soundEnabled;
 if(switched||!focused){if(i.open!=Impl::none||i.hover.element!=Impl::none||i.pressed.element!=Impl::none||i.keyFocus)++revision_;i.open=Impl::none;i.focus=Impl::none;i.keyboardOption=-1;i.hover={};i.pressed={};i.pointerDown=false;i.keyFocus=false;}
 if(!focused){cancel_actions("focus lost");return;}
 if(i.open!=Impl::none&&!i.shown(i.elements[i.open],context)){i.open=Impl::none;++revision_;}
 if(audio_){audio_->mute(!soundEnabled);for(const auto&n:audio_->poll()){
  if(!i.token||n.token!=i.token)continue;const auto id=i.elements[i.element].id;
  if(n.result==AudioResult::failed){error_=n.message;notice({"failed",id,{},{},n.message});cancel_actions("audio failed");continue;}
  if(n.result==AudioResult::started){notice({"soundStarted",id});if(i.waiting&&!i.waitForEnd){i.waiting=false;++i.step;advance_actions();}}
  else if(n.result==AudioResult::completed){notice({"soundCompleted",id});i.token=0;if(i.waiting){i.waiting=false;++i.step;advance_actions();}}
 }}advance_actions();
}
bool Runtime::begin_sequence(size_t element,bool change,std::string value){
 auto&i=*impl_;if(!i.focused||busy())return false;i.element=element;i.change=change;i.actionValue=std::move(value);i.step=0;i.active=true;i.waiting=false;
 const auto&e=i.elements[element];i.delaying=e.delayMs!=0;i.deadline=i.now>UINT64_MAX-e.delayMs?UINT64_MAX:i.now+e.delayMs;notice({"started",e.id,{},i.actionValue});advance_actions();return true;
}
bool Runtime::trigger(std::string_view id,const Context&context){
 if(!impl_)return false;update(context,impl_->focused,impl_->soundEnabled,impl_->clockSeen?impl_->now:UINT64_MAX);auto&i=*impl_;if(!i.focused||busy())return false;
 auto e=std::find_if(i.elements.begin(),i.elements.end(),[&](const auto&e){return e.id==id;});if(e==i.elements.end()||(!e->control()&&e->actions.empty())||!i.shown(*e,context))return false;
 const auto index=size_t(e-i.elements.begin());if(e->kind=="dropdown"){i.focus=index;i.open=i.open==index?Impl::none:index;i.scroll=0;i.keyboardOption=-1;if(i.open!=Impl::none){const auto selected=i.selection(*e,context);for(size_t n=0;n<e->options.size();++n)if(e->options[n].value==selected){i.scroll=n;i.keyboardOption=int(n);}}++revision_;return true;}return begin_sequence(index,false);
}
bool Runtime::activate(float x,float y,const Context&context){
 auto&i=*impl_;if(!i.focused)return false;const auto hit=i.hit(x,y,context);
 if(i.open!=Impl::none){
  if(hit.element!=i.open){i.open=Impl::none;++revision_;return true;}
  if(hit.option==-2||hit.option==-3){const auto p=i.popup();if(hit.option==-2)i.scroll=p.first>p.count?p.first-p.count:0;else i.scroll=std::min(p.first+p.count,i.elements[i.open].options.size()-p.count);i.keyboardOption=int(i.scroll);++revision_;return true;}
  if(hit.option>=0){const auto index=i.open;auto&e=i.elements[index];const auto value=e.options[size_t(hit.option)].value;const bool changed=value!=i.selection(e,context);i.open=Impl::none;++revision_;if(!busy()&&changed){i.selections[e.id]=value;if(!e.bind.empty()){i.variables[e.bind]=value;notice({"variable",e.id,e.bind,value});}notice({"selection",e.id,e.bind,value});begin_sequence(index,true,value);}return true;}
 }
 if(hit.element==Impl::none)return false;if(!busy())trigger(i.elements[hit.element].id,context);return true;
}
bool Runtime::click(float x,float y,const Context&context){
 if(!impl_||!std::isfinite(x)||!std::isfinite(y)||x<0||x>=1280||y<0||y>=720)return false;
 update(context,impl_->focused,impl_->soundEnabled,impl_->clockSeen?impl_->now:UINT64_MAX);return activate(x,y,context);
}
bool Runtime::pointer(float x,float y,bool down,const Context&context){
 if(!impl_)return false;update(context,impl_->focused,impl_->soundEnabled,impl_->clockSeen?impl_->now:UINT64_MAX);auto&i=*impl_;if(!i.focused)return false;
 const auto hit=i.hit(x,y,context);const auto oldHover=i.hover,oldPressed=i.pressed;const bool wasDown=i.pointerDown;bool consumed=hit.element!=Impl::none||i.open!=Impl::none||i.pressed.element!=Impl::none;
 if((oldHover!=hit||down!=wasDown)&&i.keyFocus){i.keyFocus=false;++revision_;}i.hover=hit;if(down&&!wasDown){i.pressed=busy()?Impl::Hit{}:hit;if(hit.element!=Impl::none){i.focus=hit.element;if(hit.option>=0)i.keyboardOption=hit.option;}}
 if(!down&&wasDown){const auto press=i.pressed;i.pressed={};if(press==hit&&(press.element!=Impl::none||i.open!=Impl::none))consumed=activate(x,y,context)||consumed;}
 i.pointerDown=down;if(oldHover!=i.hover||oldPressed!=i.pressed||wasDown!=down)++revision_;return consumed;
}
bool Runtime::key(unsigned key,const Context&context){
 if(!impl_)return false;update(context,impl_->focused,impl_->soundEnabled,impl_->clockSeen?impl_->now:UINT64_MAX);auto&i=*impl_;if(!i.focused)return false;
 if(key==VK_ESCAPE){if(i.open!=Impl::none){i.open=Impl::none;i.keyboardOption=-1;++revision_;return true;}if(busy()){cancel_actions("escape");return true;}return false;}
 if(key==VK_TAB){if(busy())return i.focus!=Impl::none;i.open=Impl::none;for(size_t n=0;n<i.elements.size();++n){const auto index=i.focus==Impl::none?n:(i.focus+n+1)%i.elements.size();const auto&e=i.elements[index];if((e.control()||!e.actions.empty())&&i.shown(e,context)){i.focus=index;i.keyFocus=true;++revision_;return true;}}return false;}
 if(i.focus==Impl::none||!i.shown(i.elements[i.focus],context))return false;
 if(key!=VK_UP&&key!=VK_DOWN&&key!=VK_RETURN&&key!=VK_SPACE)return false;
 if(busy())return true;if(!i.keyFocus){i.keyFocus=true;++revision_;}
 auto&e=i.elements[i.focus];if(e.kind!="dropdown")return (key==VK_RETURN||key==VK_SPACE)?trigger(e.id,context):false;
 if(i.open!=i.focus){trigger(e.id,context);return true;}
 if(key==VK_UP||key==VK_DOWN){i.keyboardOption=std::clamp(i.keyboardOption+(key==VK_DOWN?1:-1),0,int(e.options.size())-1);auto p=i.popup();if(size_t(i.keyboardOption)<p.first)i.scroll=size_t(i.keyboardOption);else if(size_t(i.keyboardOption)>=p.first+p.count)i.scroll=size_t(i.keyboardOption)-p.count+1;++revision_;return true;}
 i.keyboardOption=std::clamp(i.keyboardOption,0,int(e.options.size())-1);auto p=i.popup();if(size_t(i.keyboardOption)<p.first)i.scroll=size_t(i.keyboardOption);else if(size_t(i.keyboardOption)>=p.first+p.count)i.scroll=size_t(i.keyboardOption)-p.count+1;p=i.popup();const auto row=size_t(i.keyboardOption)-p.first+(p.pages?1:0);return activate(p.left+std::min(10.f,p.w*.5f),p.top+(float(row)+.5f)*p.row,context);
}
bool Runtime::load(const std::filesystem::path&p,const std::filesystem::path&assetRoot){
 cancel_actions("layout changed");impl_.reset();++revision_;error_.clear();try{require(std::filesystem::is_regular_file(p)&&std::filesystem::file_size(p)<=1024*1024,"UI layout file missing or too large");std::ifstream f(p,std::ios::binary|std::ios::ate);auto n=f.tellg();require(n>0,"UI layout is empty");std::string s(static_cast<size_t>(n),'\0');f.seekg(0);require(bool(f.read(s.data(),n)),"UI layout read failed");return load_json(s,assetRoot.empty()?p.parent_path():assetRoot);}catch(const std::exception&e){error_=e.what();return false;}
}
bool Runtime::load_json(std::string_view s,const std::filesystem::path&assetRoot){
 cancel_actions("layout changed");impl_.reset();++revision_;error_.clear();try{
  auto root=std::filesystem::canonical(assetRoot);require(std::filesystem::is_directory(root),"UI asset root is not a folder");auto v=json::parse(s);keys(v,{"format","width","height","elements"});require(str(v,"format","")==mgo2mt::brand::Format{"MGO2MT.UI_LAYOUT.1"}&&number(v,"width",0,0,1280)==1280&&number(v,"height",0,0,720)==720,"Unsupported UI format/design canvas");
  auto list=field(v,"elements");require(list&&list->type==J::array&&list->a.size()<=4096,"UI element count/type (4096)");auto next=std::make_unique<Impl>();std::set<std::string>ids,variableNames;std::map<std::filesystem::path,std::shared_ptr<const Image>>images;size_t imageBytes=0,textBytes=0,actionCount=0;double paintWork=0;
  auto milliseconds=[](const J&row,const char*name){auto p=field(row,name);if(!p)return uint32_t(0);require(p->type==J::number&&p->n>=0&&p->n<=60000&&std::floor(p->n)==p->n,"UI delay must be integer milliseconds in 0..60000");return uint32_t(p->n);};
  auto color=[](const J&row,const char*name,std::array<uint8_t,4>&out){auto p=field(row,name);if(!p)return false;require(p->type==J::array&&p->a.size()==4,"UI color must be RGBA");for(unsigned n=0;n<4;++n){const auto&x=p->a[n];require(x.type==J::number&&x.n>=0&&x.n<=255&&std::floor(x.n)==x.n,"UI color byte range");out[n]=uint8_t(x.n);}return true;};
  auto local_name=[&](const std::string&name){require(!name.empty(),"UI local variable name required");variableNames.insert(name);require(variableNames.size()<=128,"UI local variable limit");};
  auto actions=[&](const J&item,const char*name,std::vector<Impl::Action>&out){auto rows=field(item,name);if(!rows)return;require(rows->type==J::array&&rows->a.size()<=32,"UI action sequence limit (32)");actionCount+=rows->a.size();require(actionCount<=8192,"UI total action limit (8192)");
   for(const auto&row:rows->a){Impl::Action action;action.type=str(row,"type","",32);
    if(action.type=="playSound"){keys(row,{"type","sound","volume","waitForEnd"});action.sound=str(row,"sound","",512);action.path=safe_asset(root,action.sound);auto ext=action.path.extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),[](wchar_t c){return wchar_t(std::towlower(c));});require((ext==L".wav"||ext==L".mp3")&&std::filesystem::is_regular_file(action.path)&&std::filesystem::file_size(action.path)>0&&std::filesystem::file_size(action.path)<=64*1024*1024,"UI sound must be WAV/MP3, 1 byte..64 MiB");action.volume=number(row,"volume",1,0,1);action.wait=boolean(row,"waitForEnd",true);}
    else if(action.type=="delay"){keys(row,{"type","ms"});require(field(row,"ms")!=nullptr,"UI delay ms required");action.ms=milliseconds(row,"ms");}
    else if(action.type=="emitEvent"){keys(row,{"type","name"});action.name=str(row,"name","",128);require(!action.name.empty(),"UI event name required");}
    else if(action.type=="setVariable"){keys(row,{"type","name","value"});action.name=str(row,"name","",128);action.value=str(row,"value","",4096);local_name(action.name);}
    else if(action.type=="setVisible"){keys(row,{"type","target","visible"});action.target=str(row,"target","",128);require(!action.target.empty(),"UI target ID required");action.visible=boolean(row,"visible",true);}
    else require(false,"Unknown UI action type");out.push_back(std::move(action));
   }
  };
  for(const auto&item:list->a){
   keys(item,{"id","kind","screen","state","flagsAll","flagsNone","x","y","width","height","anchor","z","visible","color","texture","text","bind","fontSize","font","onClick","onChange","hoverColor","pressedColor","textColor","shadowColor","shadowX","shadowY","delayMs","options","selected"});Impl::Element e;
   e.id=str(item,"id","",128);require(!e.id.empty()&&ids.insert(e.id).second,"Empty or duplicate UI element ID");e.kind=str(item,"kind","");require(e.kind=="panel"||e.kind=="image"||e.kind=="text"||e.control(),"Unknown UI element kind");if(e.control())e.color={48,52,60,255};
   e.screen=str(item,"screen","*",128);e.state=str(item,"state","*",128);require(!e.screen.empty()&&!e.state.empty(),"Empty UI state condition");e.visible=boolean(item,"visible",true);e.x=number(item,"x",0,-8192,8192);e.y=number(item,"y",0,-8192,8192);e.w=number(item,"width",0,0,4096);e.h=number(item,"height",0,0,4096);require(e.w>0&&e.h>0,"Empty UI rectangle");e.z=number(item,"z",0,-100000,100000);
   const auto anchor=str(item,"anchor","top-left");constexpr const char*anchors[]={"top-left","top-center","top-right","middle-left","center","middle-right","bottom-left","bottom-center","bottom-right"};auto index=std::find(std::begin(anchors),std::end(anchors),anchor);require(index!=std::end(anchors),"Unknown UI anchor");auto a=index-std::begin(anchors);e.ax=float(a%3)*.5f;e.ay=float(a/3)*.5f;
   color(item,"color",e.color);e.hasHover=color(item,"hoverColor",e.hoverColor);e.hasPressed=color(item,"pressedColor",e.pressedColor);color(item,"shadowColor",e.shadowColor);color(item,"textColor",e.textColor);e.shadowX=number(item,"shadowX",0,-256,256);e.shadowY=number(item,"shadowY",0,-256,256);e.delayMs=milliseconds(item,"delayMs");
   paintWork+=std::min(e.w,1280.f)*std::min(e.h,720.f)*(1+(e.control()?1:0)+(e.shadowColor[3]?1:0));require(paintWork<=32.*1280*720,"UI paint-work budget");
   if(auto p=field(item,"flagsAll"))e.all=names(*p);if(auto p=field(item,"flagsNone"))e.noneFlags=names(*p);for(const auto&flag:e.all)require(!e.noneFlags.contains(flag),"Conflicting UI flag condition");
   e.texture=str(item,"texture","");e.text=str(item,"text","");e.bind=str(item,"bind","",128);e.font=str(item,"font","Yu Gothic",128);e.fontSize=number(item,"fontSize",24,1,256);
   if(e.kind=="image"||(!e.texture.empty()&&e.control())){auto path=safe_asset(root,e.texture);if(!images.contains(path)){require(images.size()<512,"UI texture count budget");auto image=std::make_shared<Image>(decode_image(path));imageBytes+=image->rgba.size();require(imageBytes<=128*1024*1024,"UI decoded texture budget");images.emplace(path,std::move(image));}e.image=images.at(path);}else require(e.texture.empty(),"Texture requires an image or control element");
   if(e.kind=="text"||e.control()){require(e.w<=1280&&e.h<=720&&!e.font.empty(),"UI text bounds/font");textBytes+=size_t(std::ceil(e.w))*size_t(std::ceil(e.h))*4;require(textBytes<=32*1024*1024,"UI text raster budget");e.glyphText=e.text;e.glyphs=text_image(e.text,e.font,e.fontSize,unsigned(std::ceil(e.w)),unsigned(std::ceil(e.h)),e.kind=="button"?1:e.kind=="dropdown"?2:0);}else require(e.text.empty()&&e.bind.empty(),"Text/binding requires text or control element");
   if(auto options=field(item,"options")){require(options->type==J::array&&options->a.size()<=64,"UI dropdown option limit (64)");std::set<std::string>values;for(const auto&o:options->a){keys(o,{"value","label"});Impl::Option option{str(o,"value","",128),str(o,"label","",512)};auto value=wide(option.value);require(!option.value.empty()&&std::any_of(value.begin(),value.end(),[](wchar_t c){return !std::iswspace(c);})&&std::none_of(value.begin(),value.end(),[](wchar_t c){return c<32||c==127;})&&!option.label.empty()&&values.insert(option.value).second,"Invalid or duplicate UI dropdown option");e.options.push_back(std::move(option));}}
   e.selected=str(item,"selected","",128);if(e.kind=="dropdown"){require(!e.options.empty(),"Dropdown needs at least one option");if(e.selected.empty())e.selected=e.options.front().value;require(std::any_of(e.options.begin(),e.options.end(),[&](const auto&o){return o.value==e.selected;}),"Dropdown selected value is unknown");if(!e.bind.empty())local_name(e.bind);}else require(e.options.empty()&&e.selected.empty(),"Options/selection require dropdown element");
   actions(item,"onClick",e.actions);actions(item,"onChange",e.changes);require(e.kind=="dropdown"||e.changes.empty(),"onChange requires dropdown element");next->elements.push_back(std::move(e));
  }
  for(const auto&e:next->elements)for(const auto*list:{&e.actions,&e.changes})for(const auto&a:*list)if(a.type=="setVisible")require(ids.contains(a.target),"UI action targets an unknown element");
  std::stable_sort(next->elements.begin(),next->elements.end(),[](const auto&a,const auto&b){return a.z<b.z;});impl_=std::move(next);return true;
 }catch(const std::exception&e){error_=e.what();return false;}
}
Viewport viewport(unsigned w,unsigned h){require(w&&h&&w<=8192&&h<=8192&&uint64_t(w)*h<=16*1024*1024,"UI output dimensions exceed budget");const float scale=std::min(float(w)/1280,float(h)/720);return {(w-1280*scale)*.5f,(h-720*scale)*.5f,scale,1280*scale,720*scale};}
bool Runtime::paint(std::span<uint8_t>rgba,unsigned w,unsigned h,const Context&context,size_t stride)const{
 if(!impl_)return false;try{error_.clear();auto vp=viewport(w,h);if(!stride)stride=size_t(w)*4;require(stride>=size_t(w)*4&&stride<=size_t(8192)*4&&rgba.size()>=stride*size_t(h),"UI output buffer extent");require(context.screen.size()<=128&&context.state.size()<=128&&context.flags.size()<=64&&context.bindings.size()<=128,"UI context size limit");std::vector<uint8_t>canvas(1280*720*4);
 auto draw=[&](float left,float top,float width,float height,std::array<uint8_t,4> tint,const Image*image=nullptr,bool alphaOnly=false){
  if(width<=0||height<=0||!tint[3])return;int x0=std::max(0,int(std::floor(left))),y0=std::max(0,int(std::floor(top))),x1=std::min(1280,int(std::ceil(left+width))),y1=std::min(720,int(std::ceil(top+height)));
  for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){auto color=tint;if(image){auto ix=std::clamp(int((x+.5f-left)/width*image->width),0,int(image->width)-1),iy=std::clamp(int((y+.5f-top)/height*image->height),0,int(image->height)-1);auto pixel=image->rgba.data()+(size_t(iy)*image->width+ix)*4;for(int k=alphaOnly?3:0;k<4;++k)color[k]=uint8_t((unsigned(color[k])*pixel[k]+127)/255);}blend(canvas.data()+(size_t(y)*1280+x)*4,color.data());}
 };
 auto boundText=[&](const Impl::Element&e){auto text=e.text;if(!e.bind.empty()){if(auto i=context.bindings.find(e.bind);i!=context.bindings.end())text=i->second;if(auto i=impl_->variables.find(e.bind);i!=impl_->variables.end())text=i->second;}require(text.size()<=4096,"UI bound text exceeds budget");json::utf8(text);return text;};
 for(size_t n=0;n<impl_->elements.size();++n){const auto&e=impl_->elements[n];if(!impl_->shown(e,context))continue;const float left=e.left(),top=e.top();auto color=e.color;
  if(e.hasHover&&((impl_->hover.element==n&&impl_->hover.option==-1)||(impl_->keyFocus&&impl_->focus==n)))color=e.hoverColor;
  if(e.hasPressed&&impl_->pointerDown&&impl_->pressed==impl_->hover&&impl_->pressed.element==n&&impl_->pressed.option==-1)color=e.pressedColor;
  if(e.kind!="text"){draw(left+e.shadowX,top+e.shadowY,e.w,e.h,e.shadowColor,e.image.get(),true);draw(left,top,e.w,e.h,color,e.image.get());}
  if(e.kind=="text"||e.control()){
   auto text=boundText(e);if(e.kind=="dropdown"){auto value=impl_->selection(e,context);for(const auto&o:e.options)if(o.value==value){text=o.label;break;}}
   const float inset=e.control()?std::min(8.f,e.w*.1f):0,arrow=e.kind=="dropdown"?std::min(24.f,e.w*.2f):0;const auto width=unsigned(std::max(1.f,std::ceil(e.w-2*inset-arrow))),height=unsigned(std::ceil(e.h));
   if(e.glyphText!=text||e.glyphs.width!=width||e.glyphs.height!=height){e.glyphs=text_image(text,e.font,e.fontSize,width,height,e.kind=="button"?1:e.kind=="dropdown"?2:0);e.glyphText=std::move(text);}
   if(e.kind=="text")draw(left+e.shadowX,top+e.shadowY,e.w,e.h,e.shadowColor,&e.glyphs,true);
   draw(left+inset,top,float(width),e.h,e.control()?e.textColor:color,&e.glyphs);
   if(arrow){auto glyph=text_image(impl_->open==n?"\xe2\x96\xb2":"\xe2\x96\xbc",e.font,std::min(e.fontSize,18.f),unsigned(std::max(1.f,arrow)),height,1);draw(left+e.w-arrow-inset,top,arrow,e.h,e.textColor,&glyph);}
  }
 }
 // The open popup paints last so lower-z controls cannot cover its options.
 if(impl_->open!=Impl::none&&impl_->shown(impl_->elements[impl_->open],context)){
  const auto&e=impl_->elements[impl_->open];const auto p=impl_->popup();draw(p.left+e.shadowX,p.top+e.shadowY,p.w,p.height(),e.shadowColor);
  auto row=[&](size_t screenRow,int option,std::string_view label,bool selected){
   auto color=e.color;const Impl::Hit hit{impl_->open,option};if(selected)for(unsigned k=0;k<3;++k)color[k]=uint8_t(std::min(255u,unsigned(color[k])+24));
   if(e.hasHover&&(impl_->hover==hit||(impl_->keyFocus&&impl_->keyboardOption==option)))color=e.hoverColor;if(e.hasPressed&&impl_->pointerDown&&impl_->pressed==hit&&impl_->hover==hit)color=e.pressedColor;
   const float top=p.top+float(screenRow)*p.row;draw(p.left,top,p.w,p.row,color,e.image.get());const float inset=std::min(8.f,p.w*.1f);auto labelImage=text_image(label,e.font,std::min(e.fontSize,p.row-2),unsigned(std::max(1.f,std::ceil(p.w-2*inset))),unsigned(std::ceil(p.row)),option<0?1:2);draw(p.left+inset,top,p.w-2*inset,p.row,e.textColor,&labelImage);
  };
  if(p.pages)row(0,-2,"\xe2\x96\xb2",false);const auto selected=impl_->selection(e,context);
  for(size_t n=0;n<p.count;++n){const auto&o=e.options[p.first+n];row(n+(p.pages?1:0),int(p.first+n),o.label,o.value==selected);}
  if(p.pages)row(p.count+1,-3,"\xe2\x96\xbc",false);
 }
 if(w==1280&&h==720){for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)blend(rgba.data()+size_t(y)*stride+x*4,canvas.data()+(size_t(y)*1280+x)*4);return true;}
 // Match the game's normalized-coordinate D3D linear sampler. Font rasterization
 // and element geometry always happen at design resolution, never at target DPI.
 for(unsigned y=0;y<h;++y){float ly=(float(y)+.5f-vp.y)/vp.scale;if(ly<0||ly>=720)continue;for(unsigned x=0;x<w;++x){float lx=(float(x)+.5f-vp.x)/vp.scale;if(lx<0||lx>=1280)continue;float sx=lx-.5f,sy=ly-.5f;int ix=int(std::floor(sx)),iy=int(std::floor(sy));float fx=sx-ix,fy=sy-iy;std::array<uint8_t,4>color{};for(int k=0;k<4;++k){float value=0;for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx)value+=canvas[(size_t(std::clamp(iy+dy,0,719))*1280+std::clamp(ix+dx,0,1279))*4+k]*(dx?fx:1-fx)*(dy?fy:1-fy);color[k]=uint8_t(std::clamp(int(std::lround(value)),0,255));}blend(rgba.data()+size_t(y)*stride+x*4,color.data());}}return true;
 }catch(const std::exception&e){error_=e.what();return false;}
}
Context parse_context(std::string_view screen,std::string_view state,std::string_view flags,std::string_view bindings){Context c;c.screen=screen;c.state=state;json::utf8(screen);json::utf8(state);require(screen.size()<=128&&state.size()<=128,"UI context name length");if(!flags.empty())c.flags=names(json::parse(flags));if(!bindings.empty()){auto v=json::parse(bindings);require(v.type==J::object&&v.o.size()<=128,"UI bindings object limit");for(auto&[k,x]:v.o){require(!k.empty()&&k.size()<=128&&x.type==J::string&&x.s.size()<=4096,"Invalid UI text binding");c.bindings.emplace(k,x.s);}}return c;}
}
