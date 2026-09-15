#include "notification_wire.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <charconv>
#include <cstring>
#include <map>
#include <stdexcept>
namespace mgo2win::notices {
namespace {
void require(bool ok){if(!ok)throw std::runtime_error("Invalid native notification record");}
bool hex(std::string_view s,size_t n){return s.size()==n&&std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
struct Json {
 enum Kind {object,string,number,boolean,null} kind=null;std::map<std::string,Json> fields;std::string text;uint64_t integer=0;bool flag=false;
 const Json& at(const char* key)const{auto it=fields.find(key);require(kind==object&&it!=fields.end());return it->second;}
 uint64_t num(uint64_t max=UINT64_MAX)const{require(kind==number&&integer<=max);return integer;}
 bool bit()const{require(kind==boolean);return flag;}
 std::string str()const{require(kind==string);return text;}
};
class Parser {
 std::string_view bytes_;size_t pos_=0,nodes_=0;
 char peek(){require(pos_<bytes_.size());return bytes_[pos_];}
 void ws(){while(pos_<bytes_.size()&&(bytes_[pos_]==' '||bytes_[pos_]=='\t'||bytes_[pos_]=='\r'||bytes_[pos_]=='\n'))++pos_;}
 unsigned code(){require(bytes_.size()-pos_>=4);unsigned n=0;for(int i=0;i<4;++i){char c=bytes_[pos_++];require((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'));n=n*16+(c<='9'?c-'0':(c|32)-'a'+10);}return n;}
 void utf(std::string& out,unsigned u){if(u<128)out+=char(u);else if(u<2048){out+=char(192|(u>>6));out+=char(128|(u&63));}else if(u<65536){out+=char(224|(u>>12));out+=char(128|((u>>6)&63));out+=char(128|(u&63));}else{out+=char(240|(u>>18));out+=char(128|((u>>12)&63));out+=char(128|((u>>6)&63));out+=char(128|(u&63));}}
 std::string string(){
  require(peek()=='"');++pos_;std::string out;bool ended=false;
  while(pos_<bytes_.size()){unsigned char c=bytes_[pos_++];if(c=='"'){ended=true;break;}require(c>=32);if(c!='\\')out+=char(c);else{
   require(pos_<bytes_.size());char e=bytes_[pos_++];if(e=='"'||e=='\\'||e=='/')out+=e;else if(e=='n')out+='\n';else if(e=='r')out+='\r';else if(e=='t')out+='\t';else if(e=='b')out+='\b';else if(e=='f')out+='\f';else if(e=='u'){unsigned u=code();if(u>=0xd800&&u<=0xdbff){require(bytes_.size()-pos_>=6&&bytes_[pos_]=='\\'&&bytes_[pos_+1]=='u');pos_+=2;unsigned low=code();require(low>=0xdc00&&low<=0xdfff);u=0x10000+((u-0xd800)<<10)+(low-0xdc00);}else require(u<0xdc00||u>0xdfff);utf(out,u);}else require(false);
  }require(out.size()<=4096);}
  require(ended&&(out.empty()||MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,out.data(),int(out.size()),nullptr,0)>0));return out;
 }
 Json value(unsigned depth){
  require(depth<=5&&++nodes_<=64);ws();char c=peek();Json j;
  if(c=='{'){j.kind=Json::object;++pos_;ws();if(peek()=='}'){++pos_;return j;}for(;;){ws();auto key=string();require(key.size()<=32);ws();require(peek()==':');++pos_;auto v=value(depth+1);require(j.fields.emplace(key,std::move(v)).second);ws();c=peek();++pos_;if(c=='}')break;require(c==',');}return j;}
  if(c=='"'){j.kind=Json::string;j.text=string();return j;}
  if(c>='0'&&c<='9'){auto start=pos_++;while(pos_<bytes_.size()&&bytes_[pos_]>='0'&&bytes_[pos_]<='9')++pos_;require(pos_-start==1||c!='0');j.kind=Json::number;auto result=std::from_chars(bytes_.data()+start,bytes_.data()+pos_,j.integer);require(result.ec==std::errc{}&&result.ptr==bytes_.data()+pos_);return j;}
  for(auto literal:{"true","false","null"})if(bytes_.substr(pos_,strlen(literal))==literal){pos_+=strlen(literal);j.kind=literal[0]=='n'?Json::null:Json::boolean;j.flag=literal[0]=='t';return j;}require(false);return {};
 }
public:
 explicit Parser(std::string_view bytes):bytes_(bytes){}
 Json parse(){auto result=value(0);ws();require(pos_==bytes_.size());return result;}
};
std::optional<Media> media(const Json& j,bool video){if(j.kind==Json::null)return {};require(j.fields.size()==3);Media m{j.at("path").str(),j.at("sha256").str(),uint32_t(j.at("size").num(UINT32_MAX))};require(valid_media(m,video));return m;}
uint64_t nonce(std::string_view s){require(hex(s,16));uint64_t n=0;auto r=std::from_chars(s.data(),s.data()+s.size(),n,16);require(r.ec==std::errc{}&&n);return n;}
}
bool valid_media(const Media&m,bool video){
 if(m.path.empty()||m.path.size()>96||!hex(m.sha256,64)||!m.size||m.size>(video?32u:8u)*1024*1024||m.path.find("..")!=std::string::npos)return false;
 if(!std::all_of(m.path.begin(),m.path.end(),[](char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.';}))return false;
 if(m.path.front()=='.'||m.path.back()=='.')return false;
 const auto dot=m.path.rfind('.');if(dot==std::string::npos)return false;auto stem=m.path.substr(0,m.path.find('.')),ext=m.path.substr(dot);
 for(auto&c:stem)if(c>='a'&&c<='z')c-=32;
 if(stem=="CON"||stem=="PRN"||stem=="AUX"||stem=="NUL"||(stem.size()==4&&(stem.starts_with("COM")||stem.starts_with("LPT"))&&stem[3]>='1'&&stem[3]<='9'))return false;
 return video?(ext==".mp4"||ext==".wmv"):ext==".wav";
}
std::vector<uint8_t> request(uint64_t n,uint32_t character,std::optional<uint32_t> ping,std::string_view version){
 require(n&&character&&character<=INT32_MAX&&(!ping||*ping<=30000));std::string v=version.empty()?std::string(64,'0'):std::string(version);require(hex(v,64));
 std::vector<uint8_t> b{'G','W','N','T',1,1,0,0};auto u=[&](uint64_t x,unsigned size){for(unsigned i=size;i>0;--i)b.push_back(uint8_t(x>>((i-1)*8)));};u(n,8);u(character,4);u(ping.value_or(UINT32_MAX),4);b.insert(b.end(),v.begin(),v.end());return b;
}
Reply parse(std::span<const uint8_t> b){
 require(!b.empty()&&b.size()<=8192);auto j=Parser({reinterpret_cast<const char*>(b.data()),b.size()}).parse();
 require(j.kind==Json::object&&j.at("v").num()==1&&j.at("pollAfterMs").num()==poll_ms);
 Reply r;r.nonce=nonce(j.at("nonce").str());r.character=uint32_t(j.at("character").num(INT32_MAX));require(r.character);
 r.status=uint32_t(j.at("status").num(UINT32_MAX));r.serverTime=j.at("serverTime").num(4102444800000ull);require(r.serverTime);
 r.version=j.at("version").str();require(hex(r.version,64));r.changed=j.at("changed").bit();
 require(j.fields.size()==(r.changed?9:8));if(r.status){require(!r.changed);return r;}
 if(r.changed){
  const auto&s=j.at("snapshot");require(s.kind==Json::object&&s.fields.size()==2);Snapshot snapshot;
  const auto&m=s.at("mail");require(m.fields.size()==2);snapshot.mailCount=uint32_t(m.at("count").num(UINT32_MAX));snapshot.latestMailId=uint32_t(m.at("latestId").num(UINT32_MAX));require((snapshot.mailCount==0)==(snapshot.latestMailId==0));
  const auto&a=s.at("alert");if(a.kind!=Json::null){require(a.fields.size()==7);Alert alert;alert.id=uint32_t(a.at("id").num(UINT32_MAX));alert.version=uint32_t(a.at("version").num(UINT32_MAX));require(alert.id&&alert.version);alert.publishedAt=a.at("publishedAt").num(r.serverTime);alert.expiresAt=a.at("expiresAt").num(4102444800000ull);require(!alert.expiresAt||alert.expiresAt>r.serverTime);alert.text=a.at("text").str();require(!alert.text.empty()&&alert.text.find('\0')==std::string::npos);alert.audio=media(a.at("audio"),false);alert.video=media(a.at("video"),true);snapshot.alert=std::move(alert);}
  r.snapshot=std::move(snapshot);
 }return r;
}
void Session::connect(uint64_t scope,uint32_t character,uint64_t now){std::lock_guard lock(mutex_);auto generation=state_.generation+1;state_={};state_.generation=generation;state_.scope=scope;state_.character=character;state_.connected=scope&&character;next_=clock_=now;deadline_=nonce_=sequence_=0;pending_=disabled_=false;}
void Session::disconnect(){std::lock_guard lock(mutex_);auto generation=state_.generation+1;state_={};state_.generation=generation;pending_=false;disabled_=true;}
std::optional<std::vector<uint8_t>> Session::take(uint64_t now,std::optional<uint32_t> ping){
 std::lock_guard lock(mutex_);if(!state_.connected||disabled_||now<clock_)return {};clock_=now;
 if(pending_){if(now<deadline_)return {};pending_=false;if(!state_.supported){disabled_=true;return {};}}
 if(now<next_)return {};
 if(++sequence_==0){disabled_=true;return {};}nonce_=state_.scope+sequence_;if(!nonce_){if(++sequence_==0){disabled_=true;return {};}nonce_=state_.scope+sequence_;}
 pending_=true;deadline_=now+reply_timeout_ms;next_=now+poll_ms;
 return request(nonce_,state_.character,ping,state_.ready?state_.version:std::string_view{});
}
bool Session::receive(std::span<const uint8_t>b,uint64_t now){
 Reply r;try{r=parse(b);}catch(...){return false;}
 std::lock_guard lock(mutex_);if(!state_.connected||!pending_||now<clock_||now>=deadline_||r.nonce!=nonce_||r.character!=state_.character)return false;
 if(!r.status&&!r.changed&&(!state_.ready||r.version!=state_.version))return false;
 if(!r.status&&state_.ready&&(r.serverTime<state_.serverTime||(r.changed&&r.version==state_.version&&*r.snapshot!=state_.snapshot)))return false;
 clock_=now;pending_=false;state_.supported=true;
 if(r.status)return true;
 if(r.changed){state_.snapshot=*r.snapshot;state_.version=r.version;++state_.serial;}
 state_.ready=true;state_.serverTime=r.serverTime;state_.receivedAt=now;return true;
}
}
