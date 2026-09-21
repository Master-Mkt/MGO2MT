#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace mgo2mt {
// A bounded GCX subset, reading original compiled bytes. No JSON AST at runtime.
struct GcxValue {
 enum class Kind { integer, hash, text, block } kind=Kind::integer;
 int64_t number=0;
 std::string text;
};
struct GcxOption { uint32_t code=0; char letter=0; std::vector<GcxValue> values; };
struct GcxRequest {
 uint32_t procedure=0,code=0; size_t offset=0;
 std::vector<GcxValue> arguments;
 std::vector<GcxOption> options;
 int64_t argument(size_t index) const;
 int64_t option(uint32_t code) const;
};
class GcxRuntime {
 struct Token {
  uint8_t tag=0; size_t offset=0,end=0; uint32_t code=0; char letter=0;
  GcxValue value;
  std::vector<Token> arguments,children;
 };
 struct NativeRoute {uint32_t procedure;int argument;std::vector<GcxRequest> requests;};
 bool native_=false;std::vector<NativeRoute> nativeRoutes_;
 std::vector<char> bytes_;
 std::vector<size_t> procedures_;
 size_t proc_end_=0, budget_=0;
 std::map<uint32_t,int32_t> variables_;
 std::vector<uint32_t> calls_;
 std::vector<GcxRequest> requests_;
 uint32_t read(size_t p,size_t n,size_t end) const;
 Token token(size_t& p,size_t end,unsigned depth);
 std::vector<Token> sequence(size_t p,size_t end,unsigned depth);
 GcxValue value(const Token&,const std::vector<GcxValue>&);
 void block(const std::vector<Token>&,const std::vector<GcxValue>&,uint32_t,unsigned);
 void invoke(uint32_t,const std::vector<GcxValue>&,unsigned);
 void spend();
public:
 explicit GcxRuntime(std::vector<char> bytes);
 std::string compile_title_program();
 bool native_program()const{return native_;}
 // Explicit host bindings, not a general emulation of PS3 variable banks.
 void bind(uint32_t descriptor,int32_t value);
 void execute(uint32_t procedure,const std::vector<GcxValue>& arguments={});
 const std::vector<GcxRequest>& requests() const {return requests_;}
 const std::vector<uint32_t>& calls() const {return calls_;}
 size_t procedure_count() const {return procedures_.size();}
};
std::string gcx_request_json(const GcxRequest&,const char* disposition);
}
