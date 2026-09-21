#include "weapon_icons.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt::weapons;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char**argv){try{
 auto dir=std::filesystem::temp_directory_path()/("mgo2mt-icon-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(dir);
 struct Cleanup{std::filesystem::path dir;~Cleanup(){std::error_code ec;std::filesystem::remove(dir/"index.tsv",ec);std::filesystem::remove(dir/"weapon_25.png",ec);std::filesystem::remove(dir,ec);}} cleanup{dir};
 // Independent 2x1 RGBA fixture: half-alpha red, fully transparent green.
 const unsigned char png[]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,1,8,6,0,0,0,244,34,127,138,0,0,0,17,73,68,65,84,120,156,99,248,207,192,208,192,240,159,129,1,0,13,126,2,127,33,223,169,152,0,0,0,0,73,69,78,68,174,66,96,130};
 {std::ofstream f(dir/"weapon_25.png",std::ios::binary);f.write(reinterpret_cast<const char*>(png),sizeof(png));}
 auto index=dir/"index.tsv";auto write=[&](std::string body){std::ofstream f(index);f<<"MGO2MT_WEAPON_ICONS\t1\n"<<body;};
 write("ICON\t25\tweapon_25.png\n");Icons icons;std::string error;check(icons.load(index,error),error.c_str());auto icon=icons.find(25);
 check(icons.size()==1&&icon&&icon->width==2&&icon->height==1&&icon->bgra[0]==0x80ff0000&&icon->bgra[1]==0x0000ff00,"WIC keeps source alpha/color");
 check(icons.override_paths(dir,{{3,"weapon_25.png"},{25,"weapon_25.png"}},error),error.c_str());
 check(icons.size()==2&&icons.find(3)&&icons.find(25),"JSON icon override merges without removing original icons");
 for(auto path:{"../outside.png","/absolute.png","weapon_25.png/../weapon_25.png","missing.png","weapon_25.jpg"}){
  check(!icons.override_paths(dir,{{25,path}},error)&&icons.size()==2&&icons.find(25)->bgra[0]==0x80ff0000,"invalid override preserves previous image set");
 }
 check(!icons.override_paths(dir,{{3,"weapon_25.png"},{25,"missing.png"}},error)&&icons.size()==2,"mixed valid/invalid override is atomic");
 {std::ofstream f(index);f<<"MGO2WIN_WEAPON_ICONS\t1\nICON\t25\tweapon_25.png\n";}
 check(icons.load(index,error),"pre-rename icon index remains readable");icon=icons.find(25);
 std::vector<uint32_t> dst(4*4,0xff0000ff);paint_icon(*icon,dst,4,4,0,0,4,4);
 check(dst[0]==0xff0000ff&&dst[12]==0xff0000ff,"aspect ratio letterbox untouched");
 check(dst[4]==0xff80007f&&dst[7]==0xff0000ff,"straight alpha over opaque, transparent pixel retained");
 dst.assign(16,0);paint_icon(*icon,dst,4,4,0,0,4,4);check(dst[4]==0x80ff0000&&dst[7]==0,"transparent destination keeps original straight color");
 check((dst[5]&0x00ffffff)==0x00ff0000,"alpha-aware resize has no green fringe");
 dst.assign(16,0);paint_icon(*icon,dst,4,4,0,0,4,4,true);check((dst[4]>>24)<128&&((dst[4]>>16)&255)==(dst[4]&255),"unavailable presentation desaturates and fades");
 for(auto body:{"ICON\t25\t../outside.png\n","ICON\t256\tweapon_25.png\n","ICON\t25\tweapon_25.png\nICON\t25\tweapon_25.png\n","ICON\t25\tmissing.png\n","ICON\t25\tweapon_25.png\textra\n",""}){
  write(body);check(!icons.load(index,error)&&icons.size()==0&&!error.empty(),"invalid/missing icon bundle rejected without stale cache");
 }
 write("ICON\t25\tweapon_25.png\n");{std::ofstream f(dir/"weapon_25.png");f<<"invalid PNG";}check(!icons.load(index,error),"corrupt PNG rejects");
 if(argc>1){check(icons.load(argv[1],error),error.c_str());check(icons.size()>0,"original icon coverage");std::cout<<"original icons="<<icons.size()<<'\n';}
 std::cout<<"icon decode, bounded index, alpha, aspect, filtering and failure handling passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
