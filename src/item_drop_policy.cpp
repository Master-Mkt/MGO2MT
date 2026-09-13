#include "item_drop_policy.h"
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
namespace mgo2win::items {
namespace {
struct Reader {
 std::string_view s;size_t at=0;
 [[noreturn]] void fail()const{throw std::runtime_error("Invalid item drop policy JSON");}
 void ws(){while(at<s.size()&&(s[at]==' '||s[at]=='\r'||s[at]=='\n'||s[at]=='\t'))++at;}
 bool eat(char c){ws();if(at<s.size()&&s[at]==c){++at;return true;}return false;}
 void need(char c){if(!eat(c))fail();}
 bool literal(std::string_view value){ws();if(s.substr(at,value.size())==value){at+=value.size();return true;}return false;}
 uint32_t number(){ws();if(at==s.size()||s[at]<'0'||s[at]>'9')fail();size_t start=at;uint64_t n=0;while(at<s.size()&&s[at]>='0'&&s[at]<='9'){n=n*10+uint32_t(s[at++]-'0');if(n>UINT32_MAX)fail();}if(at-start>1&&s[start]=='0')fail();return uint32_t(n);}
 uint32_t hex(){uint32_t n=0;for(unsigned i=0;i<4;++i){if(at==s.size())fail();const char c=s[at++];n<<=4;if(c>='0'&&c<='9')n+=c-'0';else if(c>='a'&&c<='f')n+=c-'a'+10;else if(c>='A'&&c<='F')n+=c-'A'+10;else fail();}return n;}
 static void utf8(std::string& out,uint32_t c){if(c<128)out+=char(c);else if(c<2048){out+=char(0xc0|(c>>6));out+=char(0x80|(c&63));}else if(c<65536){out+=char(0xe0|(c>>12));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}else{out+=char(0xf0|(c>>18));out+=char(0x80|((c>>12)&63));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}}
 std::string string(){need('"');std::string out;bool end=false;while(at<s.size()){
  uint8_t c=uint8_t(s[at++]);if(c=='"'){end=true;break;}if(c<32)fail();
  if(c=='\\'){if(at==s.size())fail();char e=s[at++];switch(e){case '"':case '\\':case '/':out+=e;break;case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;case 'u':{uint32_t cp=hex();if(cp>=0xd800&&cp<=0xdbff){if(at+2>s.size()||s[at++]!='\\'||s[at++]!='u')fail();auto lo=hex();if(lo<0xdc00||lo>0xdfff)fail();cp=0x10000+((cp-0xd800)<<10)+(lo-0xdc00);}else if(cp>=0xdc00&&cp<=0xdfff)fail();utf8(out,cp);break;}default:fail();}}
  else if(c<128)out+=char(c);else{unsigned count=0;uint32_t cp=0,min=0;if(c>=0xc2&&c<=0xdf){count=1;cp=c&31;min=128;}else if(c>=0xe0&&c<=0xef){count=2;cp=c&15;min=2048;}else if(c>=0xf0&&c<=0xf4){count=3;cp=c&7;min=65536;}else fail();for(unsigned i=0;i<count;++i){if(at==s.size())fail();auto d=uint8_t(s[at++]);if((d&0xc0)!=0x80)fail();cp=(cp<<6)|(d&63);}if(cp<min||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))fail();utf8(out,cp);}
  if(out.size()>8192)fail();
 }if(!end)fail();return out;}
 std::optional<bool> boolean(){if(literal("null"))return {};if(literal("true"))return true;if(literal("false"))return false;fail();}
 template<class F> void object(F f){need('{');if(eat('}'))return;std::set<std::string> keys;do{auto key=string();if(!keys.insert(key).second)fail();need(':');f(key);}while(eat(','));need('}');}
};
PolicyEntry entry(Reader& r){PolicyEntry e;unsigned mask=0;r.object([&](const std::string& key){
 if(key=="id"){e.id=r.number();mask|=1;}else if(key=="name"){e.name=r.string();mask|=2;}
 else if(key=="domain"){auto d=r.string();if(d=="weapon")e.domain=Domain::weapon;else if(d=="equipment")e.domain=Domain::equipment;else if(d=="world_item")e.domain=Domain::world_item;else r.fail();mask|=256;}
 else if(key=="originalDrop"){e.policy.originalDrop=r.boolean();mask|=4;}else if(key=="originalEmptyDiscard"){e.policy.originalEmptyDiscard=r.boolean();mask|=8;}
 else if(key=="drop"){auto d=r.string();if(d=="default")e.policy.drop=DropOverride::original_default;else if(d=="deny")e.policy.drop=DropOverride::deny;else if(d=="allow")e.policy.drop=DropOverride::allow;else r.fail();mask|=16;}
 else if(key=="emptyDiscard"){e.policy.emptyDiscard=r.boolean();mask|=32;}
 else if(key=="originalKind"){e.originalKind=r.string();if(e.originalKind!="unknown"&&e.originalKind!="dropped"&&e.originalKind!="installed")r.fail();mask|=64;}
 else if(key=="source"){e.source=r.string();mask|=128;}else r.fail();
 });if(mask!=511||e.name.empty()||e.source.empty())r.fail();return e;}
}
bool DropPolicies::parse(std::string_view text,std::string& error){try{
 if(text.size()>2*1024*1024)throw std::runtime_error("Item drop policy exceeds 2 MiB");
 Reader r{text};std::map<uint64_t,PolicyEntry> candidate;unsigned mask=0;
 r.object([&](const std::string& key){if(key=="schema"){if(r.string()!="MGO2WIN.item_drop_policy")r.fail();mask|=1;}else if(key=="version"){if(r.number()!=1)r.fail();mask|=2;}else if(key=="entries"){
  r.need('[');if(!r.eat(']')){do{auto e=entry(r);auto entryKey=(uint64_t(e.domain)<<32)|e.id;if(candidate.size()>=65536||!candidate.emplace(entryKey,std::move(e)).second)r.fail();}while(r.eat(','));r.need(']');}mask|=4;
 }else r.fail();});r.ws();if(mask!=7||r.at!=text.size()||candidate.empty())r.fail();entries_.swap(candidate);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}}
bool DropPolicies::load(const std::filesystem::path& p,std::string& error){try{
 std::ifstream file(p,std::ios::binary|std::ios::ate);if(!file){error="Cannot open item drop policy";return false;}auto size=file.tellg();if(size<0||size>2*1024*1024){error="Invalid item drop policy size";return false;}std::string text(static_cast<size_t>(size),'\0');file.seekg(0);if(!file.read(text.data(),static_cast<std::streamsize>(text.size()))){error="Cannot read item drop policy";return false;}return parse(text,error);
 }catch(const std::exception& e){error=e.what();return false;}}
const PolicyEntry* DropPolicies::find(uint32_t id,Domain domain)const noexcept{auto i=entries_.find((uint64_t(domain)<<32)|id);return i==entries_.end()?nullptr:&i->second;}
}
