#include "native_character_names.h"
#include "unicode_name_projection.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace mgo2mt;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char**argv){try{
 check(argc==2,"Java fixture directory required");const std::filesystem::path root=argv[1];
 auto read=[&](const char* file){std::ifstream input(root/file,std::ios::binary);check(bool(input),file);return std::vector<uint8_t>(std::istreambuf_iterator<char>(input),{});};
 constexpr uint64_t nonce=0x0102030405060708ULL;
 const std::string jp="日本語名日本語名日本語名日本語名";std::string supplementary;for(int i=0;i<16;++i)supplementary+="𠀀";
 check(names::query(names::Operation::own,nonce)==read("account-own-request.bin"),"C++ own request equals actual Java bytes");
 const std::array<uint32_t,2> ids{42,43};check(names::query(names::Operation::ids,nonce,ids)==read("game-ids-request.bin"),"C++ ID request equals Java");
 std::array<uint8_t,27> appearance;for(unsigned i=0;i<27;++i)appearance[i]=uint8_t(i);
 check(names::create(nonce,jp,appearance)==read("create-48-request.bin"),"C++ 48-byte name create equals Java");
 auto own=names::parse(read("account-own-response.bin"));check(own.nonce==nonce&&own.op==names::Operation::own&&own.capabilities==7&&own.records==std::vector<names::Record>{{42,jp,true}},"Java own metadata decoded without marker in canonical name");
 auto game=names::parse(read("game-ids-response.bin"));check(game.records==std::vector<names::Record>{{42,jp,false},{43,supplementary,false}},"Japanese48 and supplementary64 character names");
 auto empty=names::parse(read("empty-response.bin"));check(!empty.status&&empty.records.empty(),"valid missing-ID empty response");
 auto denied=names::parse(read("denied-response.bin"));check(denied.status==2&&denied.nonce==nonce&&!denied.capabilities&&denied.records.empty(),"PS3/unknown native query denial");
 auto created=names::parse(read("create-success-response.bin"));check(!created.status&&created.createdId==42&&created.nonce==nonce,"confirmed stable new ID");
 auto duplicate=names::parse(read("create-name-taken-response.bin"));check(duplicate.status==4&&duplicate.result==0xfffffefcu&&!duplicate.createdId,"legacy duplicate code preserved");
 auto maximum=names::parse(read("maximum-600-response.bin"));check(maximum.records.size()==8,"maximum600 response decoded");
 auto legacy=read("legacy-16-and-next-field.bin");auto projected=unicode_name_projection::project(jp,unicode_name_projection::Termination::full_field_allowed,"");
 check(projected&&legacy.size()==20&&std::equal(projected->field.begin(),projected->field.end(),legacy.begin())&&names::detail::read(legacy,16,4)==0x11223344,"legacy fixed16 preserves following field");
 auto main=read("legacy-main-alias-and-id.bin");projected=unicode_name_projection::project(jp,unicode_name_projection::Termination::full_field_allowed,"*");
 check(projected&&main.size()==21&&std::equal(projected->field.begin(),projected->field.end(),main.begin())&&!main[16]&&names::detail::read(main,17,4)==42,"legacy first-record name16 plus existing separator1 plus ID remains exact");
 std::cout<<"12 actual Java wire vectors: C++ encode/decode, 48/64 byte names, 600B max and PS3 fixed16 boundaries PASS\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
