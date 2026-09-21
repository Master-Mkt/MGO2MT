#include "combat_action_preview.h"
#include <string_view>
int main(int argc,char**argv){return argc==3||argc==4?mgo2mt::run_combat_action_preview(argv[1],argv[2],argc==4&&std::string_view(argv[3])=="shadows",argc==4&&std::string_view(argv[3])=="hemisphere"):2;}
