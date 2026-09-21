#include "collision_preview.h"
#include <filesystem>
int main(int argc,char** argv){if(argc!=3)return 2;return mgo2mt::collision_test_capture(std::filesystem::path(argv[1]),std::filesystem::path(argv[2]));}
