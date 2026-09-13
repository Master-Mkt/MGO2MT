#include "unicode_name_projection.h"
#include "unicode_character_name.h"
#include <iostream>
#include <stdexcept>
#include <unordered_map>

using namespace mgo2win::unicode_name_projection;
static void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
static std::string repeat(std::string_view s, unsigned n) { std::string out; while (n--) out += s; return out; }
static Projection get(std::string_view s, Termination t, std::string_view prefix="") {
    auto p = project(s,t,prefix); check(bool(p), "projection accepted");
    check(p->displayPrefix == std::string(prefix)+p->namePrefix,"marker separate from name");
    check(p->displayPrefix.size()<=16,"fixed extent budget");
    check(s.starts_with(p->namePrefix),"contiguous input prefix");
    for (size_t i=0;i<16;++i) check(p->field[i]==(i<p->displayPrefix.size()?uint8_t(p->displayPrefix[i]):0),"exact field and zero padding");
    return *p;
}
int main() { try {
    constexpr auto nul=Termination::nul_required, full=Termination::full_field_allowed;
    for (unsigned n: {15u,16u,17u}) {
        auto text=std::string(n,'A');auto a=get(text,nul),b=get(text,full);
        check(a.namePrefix.size()==15 && a.truncated==(n>15) && a.hasTerminator,"15-byte NUL policy");
        check(b.namePrefix.size()==(n<16?n:16) && b.truncated==(n>16) && b.hasTerminator==(n<16),"full 16-byte policy");
    }
    const auto japanese=repeat("日",16),emoji=repeat("😀",16),mixed=repeat("日😀A",5)+"字";
    check(bool(mgo2win::unicode_character_name::validate(japanese)) && japanese.size()==48,"16 Japanese scalars are full name");
    check(bool(mgo2win::unicode_character_name::validate(emoji)) && emoji.size()==64,"16 emoji scalars are full name");
    check(bool(mgo2win::unicode_character_name::validate(mixed)),"mixed 16 scalar full name");
    for (auto text: {japanese,emoji,mixed}) for (auto t: {nul,full}) for (auto marker: {"","*","主"}) {
        const auto original=text; auto p=get(text,t,marker);
        check(p.truncated && text==original,"projection never mutates authoritative full name");
        check(bool(detail::prefixBoundary(p.displayPrefix,16)),"output remains strict UTF-8");
        if(t==nul) check(p.hasTerminator,"required NUL is present");
    }
    check(get(japanese,nul).namePrefix==repeat("日",5),"Japanese prefix stops at 15 bytes");
    check(get(japanese,nul,"*").namePrefix==repeat("日",4),"marker and NUL leave 14-byte name budget");
    check(get(japanese,full,"*").displayPrefix.size()==16,"marker and 5 Japanese characters fill 16");
    check(get(emoji,nul).namePrefix==repeat("😀",3),"four-byte scalar never cut at byte 15");
    check(get(emoji,full).namePrefix==repeat("😀",4),"four emoji fill 16 bytes");
    check(get(std::string(14,'A')+"日",nul).namePrefix==std::string(14,'A'),"cannot skip nonfitting scalar");
    check(get(std::string(14,'A')+"日B",full).namePrefix==std::string(14,'A'),"later ASCII cannot be packed past cut");
    check(get("*ABCD",nul,"*").displayPrefix=="**ABCD","literal star never confused with metadata");
    check(get("e\xcc\x81" "AB",nul).namePrefix=="e\xcc\x81" "AB","no Unicode normalization");
    check(get("",nul).displayPrefix.empty(),"projection has no creation minimum policy");
    check(get("A",full,std::string(16,'*')).namePrefix.empty(),"explicit prefix may exhaust budget");
    check(!project("ABCD",nul,std::string(16,'*')),"prefix cannot consume required NUL");
    check(!project("ABCD",full,std::string(17,'*')),"overlong prefix rejected");
    check(!project("ABCD",static_cast<Termination>(123),""),"invalid contract rejected");
    for (const auto& bad: {std::string("\0",1),std::string("\x80",1),std::string("\xc0\xaf",2),
         std::string("\xe0\x80\xaf",3),std::string("\xed\xa0\x80",3),std::string("\xf0\x80\x80\xaf",4),
         std::string("\xf4\x90\x80\x80",4),std::string("\xf5\x80\x80\x80",4),
         std::string("\xc2",1),std::string("\xe6\x97",2),std::string("\xf0\x9f\x98",3),std::string("\xe6" "A\x80",3)}) {
        check(!project(std::string(20,'A')+bad,full,""),"invalid suffix beyond projection remains rejected");
        check(!project("ABCD",full,bad),"invalid marker rejected");
    }
    for (auto scalar: {std::string("日"),std::string("😀")}) for(size_t n=1;n<scalar.size();++n)
        check(!project(std::string(16,'A')+scalar.substr(0,n),full,""),"all partial multibyte tails rejected");
    // Deliberately colliding display prefixes: IDs remain the lookup keys.
    struct Record { std::uint32_t pc; std::string fullName; Projection legacy; };
    std::unordered_map<std::uint32_t,Record> records;
    for (auto [id,tail]: {std::pair{101u,"甲"},std::pair{202u,"乙"}}) {
        auto name=repeat("日",15)+tail;records.emplace(id,Record{id,name,get(name,nul,"*")});
    }
    check(records.at(101).legacy.displayPrefix==records.at(202).legacy.displayPrefix,"legacy display collision expected");
    check(records.size()==2 && records.at(101).pc==101 && records.at(202).pc==202
          && records.at(101).fullName!=records.at(202).fullName,"identity and full name kept independently");
    std::cout<<"Unicode legacy projection: UTF-8, 15/16/17 byte budgets, marker/NUL, 16-scalar names and identity collision passed\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; } }
