#include "gcx_runtime.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("Expected original GCX input and new GWP output");
 if(std::filesystem::exists(argv[2]))throw std::runtime_error("Output already exists");
 std::ifstream input(argv[1],std::ios::binary);if(!input)throw std::runtime_error("Input missing");
 std::vector<char> bytes((std::istreambuf_iterator<char>(input)),{});
 mgo2win::GcxRuntime vm(std::move(bytes));auto program=vm.compile_title_program();
 std::ofstream output(argv[2],std::ios::binary);output<<program;if(!output)throw std::runtime_error("Output failure");
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
