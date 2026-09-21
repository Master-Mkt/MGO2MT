#include "native_character_names.h"
#include <iostream>
#include <source_location>
#include <functional>
using namespace mgo2mt::names;
static void check(bool value,std::source_location at=std::source_location::current()){
 if(!value){std::cerr<<"native names check line "<<at.line()<<'\n';throw std::runtime_error("native names check");}
}
static void rejects(const std::function<void()>& action){bool bad=false;try{action();}catch(const std::invalid_argument&){bad=true;}check(bad);}
static std::vector<uint8_t> hex(std::string_view text){
 std::vector<uint8_t> out;for(size_t i=0;i<text.size();i+=2){auto digit=[](char c){return unsigned(c<='9'?c-'0':c-'a'+10);};out.push_back(uint8_t(digit(text[i])*16+digit(text[i+1])));}return out;
}
static void put(std::vector<uint8_t>& out,uint32_t n,unsigned bytes){for(unsigned i=bytes;i;--i)out.push_back(uint8_t(n>>(8*(i-1))));}
static std::vector<uint8_t> response(Operation op,std::span<const Record> records={}){
 auto out=hex("47574e4d0100000101020304050607080000104000000007");out[6]=uint8_t(op);out[17]=uint8_t(records.size());
 for(const auto&r:records){put(out,r.id,4);out.push_back(r.main?1:0);out.push_back(0);put(out,uint32_t(r.name.size()),2);out.insert(out.end(),r.name.begin(),r.name.end());}
 return out;
}
int main(){try{
 constexpr uint64_t nonce=0x0102030405060708ULL;
 check(query(Operation::caps,nonce)==hex("47574e4d01000000010203040506070800000000"));
 check(query(Operation::own,nonce)==hex("47574e4d01020000010203040506070800000000"));
 const std::array<uint32_t,2> ids{0x12345678,0x6bcdef01};
 check(query(Operation::ids,nonce,ids)==hex("47574e4d01010000010203040506070800020000123456786bcdef01"));
 const std::array<uint32_t,1> negativeInJava{0x80000000u};rejects([&]{query(Operation::ids,nonce,negativeInJava);});
 rejects([&]{query(Operation::ids,nonce);});rejects([&]{query(Operation::caps,nonce,ids);});rejects([&]{query(Operation::own,nonce,ids);});
 rejects([&]{query(Operation::create,nonce);});rejects([&]{query(Operation(255),nonce);});rejects([&]{query(Operation::caps,0);});
 rejects([&]{query(Operation::ids,nonce,std::array<uint32_t,2>{1,1});});rejects([&]{query(Operation::ids,nonce,std::array<uint32_t,1>{0});});
 const std::array<uint32_t,9> many{1,2,3,4,5,6,7,8,9};check(query(Operation::ids,nonce,std::span(many).first(8)).size()==52);rejects([&]{query(Operation::ids,nonce,many);});
 std::array<uint8_t,27> appearance{};for(unsigned i=0;i<27;++i)appearance[i]=uint8_t(i);
 auto expected=hex("47574e4d0103000001020304050607080004001b41424344");expected.insert(expected.end(),appearance.begin(),appearance.end());
 check(create(nonce,"ABCD",appearance)==expected);rejects([&]{create(0,"ABCD",appearance);});rejects([&]{create(nonce,"ABC",appearance);});
 rejects([&]{create(nonce,"ABCD",std::span(appearance).first(26));});rejects([&]{create(nonce,"GM_1234",appearance);});
 std::string japanese,supplementary;for(unsigned i=0;i<16;++i){japanese+="\xe6\x97\xa5";supplementary+="\xf0\x9f\x98\x80";}
 check(create(nonce,japanese,appearance).size()==95);check(create(nonce,supplementary,appearance).size()==111);
 rejects([&]{create(nonce,japanese+"a",appearance);});rejects([&]{create(nonce,supplementary+"a",appearance);});
 const auto golden=hex("47574e4d01000101010203040506070800021040000000070000000101000001410000002a00000003e697a5");
 auto read=parse(golden);check(read.status==0&&read.op==Operation::ids&&read.nonce==nonce&&read.capabilities==7&&read.records==std::vector<Record>{{1,"A",true},{42,"\xe6\x97\xa5",false}});
 for(size_t n=0;n<golden.size();++n)rejects([&]{parse(std::span(golden).first(n));});
 for(auto [offset,value]:std::array<std::pair<size_t,unsigned>,12>{{{0,0},{4,2},{5,1},{6,4},{7,0},{18,15},{19,63},{23,8},{28,2},{29,1},{30,1},{31,0}}}){auto bad=golden;bad[offset]=uint8_t(value);rejects([&]{parse(bad);});}
 {auto bad=golden;std::fill(bad.begin()+8,bad.begin()+16,uint8_t{0});rejects([&]{parse(bad);});}
 {auto bad=golden;bad.push_back(0);rejects([&]{parse(bad);});}
 {auto bad=golden;bad[27]=0;rejects([&]{parse(bad);});}
 const auto cap=parse(response(Operation::caps));check(cap.records.empty()&&cap.capabilities==7);
 check(parse(response(Operation::own)).records.empty()); // Empty account is valid.
 std::vector<Record> records;for(uint32_t id=1;id<=8;++id)records.push_back({id,supplementary,id==1});
 const auto maximum=response(Operation::ids,records);check(maximum.size()==600&&parse(maximum).records==records);
 {auto bad=maximum;bad.push_back(0);rejects([&]{parse(bad);});}
 {auto bad=maximum;bad[17]=9;rejects([&]{parse(bad);});}
 records[7].id=1;rejects([&]{parse(response(Operation::ids,records));});records.resize(1);
 rejects([&]{parse(response(Operation::caps,records));});
 for(const auto& name:std::vector<std::string>{"",std::string("A\0B",3),"\x01","\x7f","\xc2\x80","\xc0\xaf","\x80","\xe6\x97","\xed\xa0\x80","\xf4\x90\x80\x80",std::string(17,'a'),supplementary+"a"}){
  records[0].name=name;rejects([&]{parse(response(Operation::ids,records));});
 }
 // Read policy preserves old one-scalar and reserved names; only creation policy rejects them.
 records[0].name="openmgo2";check(parse(response(Operation::own,records)).records[0].name=="openmgo2");
 auto created=response(Operation::create);put(created,0,4);put(created,0x12345678,4);auto result=parse(created);
 check(result.result==0&&result.createdId==0x12345678&&result.records.empty());
 for(size_t n=24;n<32;++n)rejects([&]{parse(std::span(created).first(n));});
 {auto bad=created;bad[27]=1;rejects([&]{parse(bad);});}
 {auto bad=created;std::fill(bad.begin()+28,bad.end(),uint8_t{0});rejects([&]{parse(bad);});}
 for(uint8_t status:std::array<uint8_t,3>{1,2,3}){
  auto failure=hex("47574e4d0101000000000000000000000000000000000000");failure[5]=status;
  auto parsed=parse(failure);check(parsed.status==status&&parsed.nonce==0&&parsed.op==Operation::caps&&parsed.records.empty());
  failure[6]=uint8_t(Operation::ids);failure[15]=7;check(parse(failure).nonce==7);
  auto bad=failure;bad[7]=1;rejects([&]{parse(bad);});bad=failure;bad[18]=16;rejects([&]{parse(bad);});bad=failure;bad[23]=1;rejects([&]{parse(bad);});
  put(failure,0,4);put(failure,0,4);check(parse(failure).status==status);failure[27]=1;rejects([&]{parse(failure);});
 }
 auto rejected=hex("47574e4d0104030001020304050607080000000000000000c0ffee0300000000");
 auto denial=parse(rejected);check(denial.status==4&&denial.result==0xc0ffee03&&denial.createdId==0&&denial.op==Operation::create&&denial.nonce==nonce);
 {auto bad=rejected;std::fill(bad.begin()+24,bad.begin()+28,uint8_t{0});rejects([&]{parse(bad);});}
 {auto bad=rejected;bad[31]=1;rejects([&]{parse(bad);});}
 rejects([&]{parse(std::span(rejected).first(24));});
 std::cout<<"native names BE golden query/create/reply, UTF8 1..16 vs create4..16, 600B/111B limits and malformed boundaries PASS\n";
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
