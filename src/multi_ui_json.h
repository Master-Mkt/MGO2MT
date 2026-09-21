#pragma once
#include <map>
#include <string>
#include <string_view>
#include <vector>
namespace mgo2mt::multi_ui::json {
struct Value {enum Type{null,boolean,number,string,array,object}type=null;bool b=false;double n=0;std::string s;std::vector<Value>a;std::map<std::string,Value>o;};
Value parse(std::string_view text);
void utf8(std::string_view text);
}
