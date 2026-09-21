#include "multi_ui_json.h"
#include <windows.h>
#include <charconv>
#include <cmath>
#include <stdexcept>
namespace mgo2mt::multi_ui::json {
void utf8(std::string_view s){if(s.find('\0')!=s.npos||s.size()>1024*1024||(!s.empty()&&!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0)))throw std::runtime_error("Invalid UTF-8 text");}
namespace {
struct Parser {
 std::string_view s;size_t at=0,nodes=0;
 [[noreturn]]void bad(){throw std::runtime_error("Invalid UI JSON at byte "+std::to_string(at));}
 void ws(){while(at<s.size()&&(s[at]==' '||s[at]=='\n'||s[at]=='\r'||s[at]=='\t'))++at;}
 bool take(char c){ws();if(at<s.size()&&s[at]==c){++at;return true;}return false;}
 unsigned hex(){if(at+4>s.size())bad();unsigned n=0;for(int i=0;i<4;++i){char c=s[at++];n<<=4;if(c>='0'&&c<='9')n|=c-'0';else if(c>='a'&&c<='f')n|=c-'a'+10;else if(c>='A'&&c<='F')n|=c-'A'+10;else bad();}return n;}
 void code(std::string&o,unsigned n){if(n<128)o+=char(n);else if(n<2048){o+=char(0xc0|(n>>6));o+=char(0x80|(n&63));}else if(n<65536){o+=char(0xe0|(n>>12));o+=char(0x80|((n>>6)&63));o+=char(0x80|(n&63));}else{o+=char(0xf0|(n>>18));o+=char(0x80|((n>>12)&63));o+=char(0x80|((n>>6)&63));o+=char(0x80|(n&63));}}
 std::string text(){if(!take('"'))bad();std::string o;while(at<s.size()){unsigned char c=s[at++];if(c=='"'){if(o.size()>65536)bad();utf8(o);return o;}if(c<32)bad();if(c!='\\'){o+=char(c);continue;}if(at==s.size())bad();char e=s[at++];switch(e){case '"':case '\\':case '/':o+=e;break;case 'b':o+='\b';break;case 'f':o+='\f';break;case 'n':o+='\n';break;case 'r':o+='\r';break;case 't':o+='\t';break;case 'u':{unsigned n=hex();if(n>=0xd800&&n<=0xdbff){if(at+2>s.size()||s[at++]!='\\'||s[at++]!='u')bad();unsigned low=hex();if(low<0xdc00||low>0xdfff)bad();n=0x10000+((n-0xd800)<<10)+low-0xdc00;}else if(n>=0xdc00&&n<=0xdfff)bad();code(o,n);break;}default:bad();}}bad();}
 Value value(unsigned depth){ws();if(depth>16||++nodes>262144||at==s.size())bad();Value v;char c=s[at];
  if(c=='"'){v.type=Value::string;v.s=text();return v;}
  if(c=='['){++at;v.type=Value::array;if(take(']'))return v;do{if(v.a.size()>=4096)bad();v.a.push_back(value(depth+1));if(take(']'))return v;}while(take(','));bad();}
  if(c=='{'){++at;v.type=Value::object;if(take('}'))return v;do{if(v.o.size()>=512)bad();auto key=text();if(!take(':')||v.o.contains(key))bad();v.o.emplace(std::move(key),value(depth+1));if(take('}'))return v;}while(take(','));bad();}
  for(auto [literal,type,b]:{std::tuple{"true",Value::boolean,true},{"false",Value::boolean,false},{"null",Value::null,false}}){std::string_view t=literal;if(s.substr(at,t.size())==t){at+=t.size();v.type=type;v.b=b;return v;}}
  auto begin=at;if(s[at]=='-')++at;if(at==s.size())bad();if(s[at]=='0')++at;else{if(s[at]<'1'||s[at]>'9')bad();while(at<s.size()&&s[at]>='0'&&s[at]<='9')++at;}
  if(at<s.size()&&s[at]=='.'){++at;auto start=at;while(at<s.size()&&s[at]>='0'&&s[at]<='9')++at;if(at==start)bad();}
  if(at<s.size()&&(s[at]=='e'||s[at]=='E')){++at;if(at<s.size()&&(s[at]=='+'||s[at]=='-'))++at;auto start=at;while(at<s.size()&&s[at]>='0'&&s[at]<='9')++at;if(at==start)bad();}
  v.type=Value::number;auto result=std::from_chars(s.data()+begin,s.data()+at,v.n);if(result.ec!=std::errc{}||!std::isfinite(v.n))bad();return v;
 }
};
}
Value parse(std::string_view s){if(s.empty()||s.size()>1024*1024)throw std::runtime_error("UI JSON size limit");utf8(s);Parser p{s};auto v=p.value(0);p.ws();if(p.at!=s.size())p.bad();return v;}
}
