// Source ELF SHA256 1a55a41ee5afdebd89075bd205e412c8311f569ba9dc022f629bb63fb4bfd13a.
// Contracts: DEC10 / DF408 / DEFD0, D6928 / D69C0 / D6D90, E3320 / E2AC8.
// Intentionally rejects unimplemented semantics. External effects remain host requests.
#include "gcx_runtime.h"
#include "product_identity.h"
#include "mgo2mt/gcl_lengths.hpp"
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>
namespace mgo2mt {
namespace {
[[noreturn]] void fail(const char* what,size_t at=0){std::ostringstream s;s<<"GCX "<<what<<" at 0x"<<std::hex<<at;throw std::runtime_error(s.str());}
int64_t number(const GcxValue& v){if(v.kind!=GcxValue::Kind::integer&&v.kind!=GcxValue::Kind::hash)fail("numeric value required");return v.number;}
std::string quoted(const std::string& text){std::ostringstream s;s<<'"';for(unsigned char c:text){if(c=='"'||c=='\\')s<<'\\'<<c;else if(c<32)s<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c);else s<<c;}s<<'"';return s.str();}
std::string json(const GcxValue& v){if(v.kind==GcxValue::Kind::text)return quoted(v.text);if(v.kind==GcxValue::Kind::block)return "{\"block_offset\":"+std::to_string(v.number)+"}";return std::to_string(v.number);}
}
int64_t GcxRequest::argument(size_t i)const {if(i>=arguments.size())fail("missing argument",offset);return number(arguments[i]);}
int64_t GcxRequest::option(uint32_t hash)const {for(const auto& o:options)if(o.code==hash){if(o.values.size()!=1)fail("option arity",offset);return number(o.values[0]);}fail("missing option",offset);}
uint32_t GcxRuntime::read(size_t p,size_t n,size_t end)const {
 if(end>bytes_.size()||p>end||n>end-p||n>4)fail("truncated",p);
 uint32_t v=0;for(size_t i=0;i<n;++i)v|=uint32_t(static_cast<unsigned char>(bytes_[p+i]))<<(8*i);return v;
}
void GcxRuntime::spend(){if(!budget_)fail("instruction/parse budget exceeded");--budget_;}
GcxRuntime::GcxRuntime(std::vector<char> bytes):bytes_(std::move(bytes)) {
 if(bytes_.size()>32*1024*1024)fail("file size limit");
 if(std::string_view(bytes_.data(),bytes_.size()).starts_with("MGO2MT.GWP.RUNTIME ")||std::string_view(bytes_.data(),bytes_.size()).starts_with("MGO2WIN.GWP.RUNTIME ")){
  if(bytes_.size()>65536)fail("native program size");
  std::istringstream in(std::string(bytes_.begin(),bytes_.end()));std::string tag;unsigned version,count,routes;
  auto word=[&](const char* expected){if(!(in>>tag)||brand::normalize_format(tag)!=expected)fail("native program syntax");};
  word("MGO2MT.GWP.RUNTIME");if(!(in>>version>>count>>routes)||version!=1||count!=21||routes!=14)fail("native program header");
  auto val=[&](){GcxValue v;unsigned kind;if(!(in>>kind>>v.number>>std::quoted(v.text))||kind>3||v.text.size()>128)fail("native value");v.kind=GcxValue::Kind(kind);if(kind==3&&v.number!=0)fail("native block must be deferred placeholder");if(kind!=2&&!v.text.empty())fail("native numeric text");return v;};
  for(unsigned i=0;i<routes;++i){NativeRoute route{};unsigned nr;word("route");if(!(in>>route.procedure>>route.argument>>nr)||nr>32)fail("native route");
   if(!((route.procedure==18||route.procedure==5)?route.argument==-1:(route.procedure==19||route.procedure==20)&&route.argument>=0&&route.argument<=5))fail("native route key");
   for(const auto&r:nativeRoutes_)if(r.procedure==route.procedure&&r.argument==route.argument)fail("duplicate native route");
   for(unsigned j=0;j<nr;++j){GcxRequest r;unsigned na,no;word("request");if(!(in>>r.code>>r.procedure>>na>>no)||!r.procedure||r.procedure>count||na>16||no>16)fail("native request");if(r.code!=0x82bc9&&r.code!=0x6592a7&&r.code!=0x37c884)fail("native command");
    for(unsigned k=0;k<na;++k)r.arguments.push_back(val());
    for(unsigned k=0;k<no;++k){GcxOption o;unsigned letter,nv;word("option");if(!(in>>o.code>>letter>>nv)||letter>127||nv>16)fail("native option");o.letter=char(letter);for(const auto&old:r.options)if(old.code==o.code)fail("duplicate native option");for(unsigned v=0;v<nv;++v)o.values.push_back(val());r.options.push_back(std::move(o));}route.requests.push_back(std::move(r));
   }nativeRoutes_.push_back(std::move(route));
  }if(in>>tag)fail("native trailing content");native_=true;procedures_.resize(count);return;
 }

 size_t p=4;std::vector<uint32_t> offsets;
 while(read(p,4,bytes_.size())!=0xffffffff){
  if(offsets.size()>=4096)fail("procedure count limit",p);
  // Three LE offset bytes followed by selector/local flags, not a u32 offset.
  if(read(p+3,1,bytes_.size()))fail("procedure locals/selectors not implemented",p);
  offsets.push_back(read(p,3,bytes_.size()));p+=4;
 }
 size_t base=p+4;read(base+16,4,bytes_.size());
 for(size_t i=0;i<4;++i){auto off=read(base+i*4,4,bytes_.size());if(off<20||off>bytes_.size()-base)fail("section offset",base+i*4);}
 size_t script=base+read(base,4,bytes_.size());size_t begin=script+4;
 size_t length=read(script,4,bytes_.size());if(begin>bytes_.size()||length>bytes_.size()-begin)fail("procedure section extent",script);
 proc_end_=begin+length;size_t main_size=read(proc_end_,4,bytes_.size());
 if(main_size>bytes_.size()-proc_end_-4)fail("main extent",proc_end_);
 for(auto off:offsets){if(off>=length)fail("procedure offset",off);procedures_.push_back(begin+off);}
 // Encrypted string resources are deliberately not decoded: tag 0x0E rejects.
 // The implemented title path uses inline strings only.
}
std::vector<GcxRuntime::Token> GcxRuntime::sequence(size_t p,size_t end,unsigned depth){
 std::vector<Token> result;while(p<end)result.push_back(token(p,end,depth));return result;
}
GcxRuntime::Token GcxRuntime::token(size_t& p,size_t end,unsigned depth){
 spend();if(depth>64)fail("nesting limit",p);Token t;t.offset=p;t.tag=static_cast<uint8_t>(read(p,1,end));unsigned kind=t.tag&0xf0;
 if(kind==0x30||kind==0x50||kind==0x60||kind==0x70||kind==0x80){
  auto span=std::span(reinterpret_cast<const uint8_t*>(bytes_.data()+p),end-p);
  auto decoded=gcl::block_length(span);if(!decoded)fail("block length header",p);
  size_t length=decoded->value;p+=decoded->header_bytes;
  if(p>end||length>end-p)fail("block extent",t.offset);size_t finish=p+length;
  if(kind==0x60){t.code=read(p,3,finish);p+=3;
   auto command=gcl::command_length(std::span(reinterpret_cast<const uint8_t*>(bytes_.data()+p),finish-p));if(!command)fail("command length header",p);
   size_t n=command->value;p+=command->header_bytes;
   if(n>finish-p)fail("command argument extent",p);t.arguments=sequence(p,p+n,depth+1);p+=n;
  }else if(kind==0x50){t.letter=static_cast<char>(read(p,1,finish));t.code=read(p+1,3,finish);p+=4;}
  else if(kind==0x70){t.code=read(p,2,finish);p+=2;}
  t.children=sequence(p,finish,depth+1);p=finish;
 }else if(kind==0x10){t.code=0;for(int i=0;i<4;++i)t.code=(t.code<<8)|read(p++,1,end);}
 else if(kind==0x40){t.value.number=t.tag&15;++p;if(t.value.number==15)t.value.number+=read(p++,1,end);}
 else if(kind==0xa0||kind==0xb0){t.value.number=t.tag&31;++p;}
 else if(kind>=0xc0){t.value.number=(t.tag&63)-1;++p;}
 else {
  ++p;size_t n=0;
  switch(t.tag){
   case 0:break;
   case 1:n=2;break;case 2:case 3:case 4:n=1;break;
   case 6:n=3;t.value.kind=GcxValue::Kind::hash;break;
   case 8:n=2;break;case 9:case 10:n=4;break;
   case 7:{size_t len=read(p++,1,end);if(!len||p>end||len>end-p||read(p+len-1,1,end))fail("inline string",t.offset);
    t.value.kind=GcxValue::Kind::text;t.value.text.assign(bytes_.data()+p,len-1);p+=len;break;}
   default:fail("unsupported token",t.offset);
  }
  if(n){auto raw=read(p,n,end);p+=n;t.value.number=t.tag==1?static_cast<int16_t>(raw):t.tag==9?static_cast<int32_t>(raw):int64_t(raw);}
 }
 t.end=p;return t;
}
void GcxRuntime::bind(uint32_t descriptor,int32_t v){
 if(descriptor!=0x19000000&&descriptor!=0x12000006)fail("unreviewed host variable binding");
 if(descriptor==0x12000006&&(v<0||v>255))fail("u8 binding range");variables_[descriptor]=v;
}
GcxValue GcxRuntime::value(const Token& t,const std::vector<GcxValue>& args){
 spend();auto kind=t.tag&0xf0;
 if(kind==0x10){auto it=variables_.find(t.code);if(it==variables_.end())fail("unbound variable",t.offset);return {GcxValue::Kind::integer,it->second,{}};}
 if(kind==0x40){auto n=t.value.number;if(n<=0||uint64_t(n)>args.size())fail("missing/return argument",t.offset);return args[static_cast<size_t>(n)-1];}
 if(kind==0x80)return {GcxValue::Kind::block,static_cast<int64_t>(t.offset),{}};
 if(kind==0x30){
  std::vector<std::pair<GcxValue,const Token*>> stack;
  for(const auto& n:t.children){unsigned k=n.tag&0xf0;if(k==0xa0||k==0xb0){
    auto op=n.value.number;if(op==0){if(stack.size()!=1||n.end!=t.end)fail("expression terminator/stack",n.offset);return stack.back().first;}
    if((op!=11&&op!=22)||stack.size()<2)fail("unsupported operator/stack",n.offset);
    auto right=stack.back();stack.pop_back();auto& left=stack.back();int64_t v=number(right.first);
    if(op==11)v=number(left.first)==v;
    else{if((left.second->tag&0xf0)!=0x10)fail("assignment target",n.offset);bind(left.second->code,static_cast<int32_t>(v));}
    left.first={GcxValue::Kind::integer,v,{}};
   }else {if(stack.size()>=16)fail("expression stack limit",n.offset);stack.push_back({value(n,args),&n});}
  }fail("missing expression terminator",t.offset);
 }
 if(kind>=0xc0||t.tag==1||t.tag==2||t.tag==3||t.tag==4||t.tag==6||t.tag==7||t.tag==8||t.tag==9||t.tag==10)return t.value;
 fail("unsupported value",t.offset);
}
void GcxRuntime::block(const std::vector<Token>& nodes,const std::vector<GcxValue>& args,uint32_t proc,unsigned depth){
 if(depth>32)fail("execution depth limit");
 for(const auto& t:nodes){spend();auto kind=t.tag&0xf0;
  if(t.tag==0)break;
  if(kind==0x30){value(t,args);continue;}
  if(kind==0x70){std::vector<GcxValue> a;for(const auto& n:t.children){if(!n.tag)break;a.push_back(value(n,args));}invoke(t.code,a,depth+1);continue;}
  if(kind!=0x60)fail("unsupported statement",t.offset);
  if(t.code==0xd86){
   auto branch=[&](const std::vector<Token>& ns,bool otherwise){
    size_t i=0;if(!otherwise){if(ns.empty())fail("missing if condition",t.offset);if(!number(value(ns[i++],args)))return false;}
    if(i>=ns.size()||(ns[i].tag&0xf0)!=0x80)fail("missing if block",t.offset);
    block(ns[i].children,args,proc,depth+1);return true;
   };
   if(branch(t.arguments,false))continue;
   for(const auto& o:t.children){if(!o.tag)break;if((o.tag&0xf0)!=0x50||(o.letter!='i'&&o.letter!='e'))fail("if option",o.offset);if(branch(o.children,o.letter=='e'))break;}
   continue;
  }
  GcxRequest request;request.procedure=proc;request.code=t.code;request.offset=t.offset;
  for(const auto& n:t.arguments){if(!n.tag)break;request.arguments.push_back(value(n,args));}
  for(const auto& o:t.children){if(!o.tag)break;if((o.tag&0xf0)!=0x50)fail("command option",o.offset);
   GcxOption opt;opt.code=o.code;opt.letter=o.letter;for(const auto& n:o.children){if(!n.tag)break;opt.values.push_back(value(n,args));}
   for(const auto& prev:request.options)if(prev.code==opt.code)fail("duplicate option",o.offset);
   request.options.push_back(std::move(opt));
  }
  // Request collection does not emulate command return values. This subset
  // permits only known void-effect commands used on the reviewed title route.
  if(request.code!=0x82bc9&&request.code!=0x6592a7&&request.code!=0x3ab23b&&request.code!=0x37c884)fail("unregistered command",t.offset);
  requests_.push_back(std::move(request));
 }
}
void GcxRuntime::invoke(uint32_t proc,const std::vector<GcxValue>& args,unsigned depth){
 spend();if(!proc||proc>procedures_.size()||depth>32||args.size()>16)fail("procedure/argument/depth range",proc);
 calls_.push_back(proc);size_t p=procedures_[proc-1],end=proc_end_;
 for(auto other:procedures_)if(other>p&&other<end)end=other;
 auto t=token(p,end,0);
 if((t.tag&0xf0)!=0x80)fail("procedure block",t.offset);block(t.children,args,proc,depth);
}
void GcxRuntime::execute(uint32_t proc,const std::vector<GcxValue>& args){
 budget_=100000;requests_.clear();calls_.clear();
 if(native_){
  if(variables_[0x19000000]!=0||variables_[0x12000006]!=2||args.size()>1)fail("native bootstrap/argument");
  int argument=-1;if(!args.empty()){auto n=number(args[0]);if(n<0||n>5)fail("native argument range");argument=int(n);}
  for(const auto&r:nativeRoutes_)if(r.procedure==proc&&r.argument==argument){requests_=r.requests;calls_={proc};return;}
  fail("native route unavailable");
 }

 // Roll back host bindings and unpublished effects after a malformed call.
 auto saved=variables_;try{invoke(proc,args,0);}catch(...){variables_=std::move(saved);requests_.clear();calls_.clear();throw;}
}
std::string GcxRuntime::compile_title_program(){
 if(native_)fail("already compiled");
 std::ostringstream out;out<<"MGO2MT.GWP.RUNTIME 1 21 14\n";
 auto emit=[&](uint32_t proc,int argument){
  GcxRuntime vm(bytes_);if(vm.procedure_count()!=21)fail("unreviewed title procedure count");vm.bind(0x19000000,0);vm.bind(0x12000006,2);std::vector<GcxValue> args;if(argument>=0)args.push_back({GcxValue::Kind::integer,argument,{}});vm.execute(proc,args);
  unsigned count=0;for(const auto&r:vm.requests())if(r.code!=0x3ab23b)++count;
  out<<"route "<<proc<<' '<<argument<<' '<<count<<'\n';
  auto value=[&](const GcxValue&v){out<<unsigned(v.kind)<<' '<<(v.kind==GcxValue::Kind::block?0:v.number)<<' '<<std::quoted(v.text)<<'\n';};
  for(const auto&r:vm.requests())if(r.code!=0x3ab23b){out<<"request "<<r.code<<' '<<r.procedure<<' '<<r.arguments.size()<<' '<<r.options.size()<<'\n';for(const auto&v:r.arguments)value(v);for(const auto&o:r.options){out<<"option "<<o.code<<' '<<unsigned(static_cast<unsigned char>(o.letter))<<' '<<o.values.size()<<'\n';for(const auto&v:o.values)value(v);}}
 };
 emit(18,-1);emit(5,-1);for(auto proc:{19u,20u})for(int arg=0;arg<=5;++arg)emit(proc,arg);
 auto text=out.str();GcxRuntime checked(std::vector<char>(text.begin(),text.end()));return text;
}
std::string gcx_request_json(const GcxRequest& r,const char* disposition){
 std::ostringstream s;s<<"{\"gcx_request\":"<<r.code<<",\"procedure\":"<<r.procedure<<",\"offset\":"<<r.offset<<",\"disposition\":"<<quoted(disposition)<<",\"arguments\":[";
 for(size_t i=0;i<r.arguments.size();++i){if(i)s<<',';s<<json(r.arguments[i]);}s<<"],\"options\":[";
 for(size_t i=0;i<r.options.size();++i){if(i)s<<',';const auto& o=r.options[i];s<<"{\"code\":"<<o.code<<",\"values\":[";for(size_t j=0;j<o.values.size();++j){if(j)s<<',';s<<json(o.values[j]);}s<<"]}";}
 return s.str()+"]}";
}
}
