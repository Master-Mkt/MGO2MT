#include "multi_ui.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt::multi_ui;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> bytes(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
struct Temporary {std::filesystem::path path=std::filesystem::temp_directory_path()/("mgo2mt-image-formats-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));Temporary(){std::filesystem::create_directory(path);}~Temporary(){std::error_code ec;std::filesystem::remove_all(path,ec);}};
}
int main(int argc,char** argv){try{
 check(argc==2,"Pass tests/fixtures/multi_ui_images as the sole argument");const std::filesystem::path fixtures=argv[1];Temporary temporary;
 const auto png=decode_image(fixtures/"alpha.png");check(png.width==16&&png.height==16,"PNG dimensions");
 const uint8_t alpha[]={0,64,128,255};
 for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x){size_t at=(y*16+x)*4;check(png.rgba[at]==x*17&&png.rgba[at+1]==y*17&&png.rgba[at+2]==(x^y)*17&&png.rgba[at+3]==alpha[x%4],"PNG authored straight RGBA");}
 for(const char* name:{"alpha.png","animated.gif","pages.tiff","sizes.ico"}){
  const auto original=bytes(fixtures/name);const auto image=decode_image(fixtures/name);check(image.width==16&&image.height==16,"Static first frame/page/icon chosen");
  if(std::string_view(name)=="animated.gif"){
   const uint8_t palette[][4]={{0,0,0,0},{255,23,7,255},{11,241,53,255},{19,47,239,255}};
   for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x)for(unsigned c=0;c<4;++c)check(image.rgba[(y*16+x)*4+c]==palette[(x+y)%4][c],"GIF first frame transparent palette RGBA");
  }else check(image.rgba==png.rgba,"TIFF/ICO first frame preserves straight partial alpha");
  auto dds=temporary.path/(std::string(name)+".dds");export_dds(fixtures/name,dds);check(decode_image(dds).rgba==image.rgba,"Optional DDS conversion exact decoded RGBA");check(bytes(fixtures/name)==original,"Decoding/conversion retains original file bytes");
  Runtime runtime;const auto json="{\"format\":\"MGO2MT.UI_LAYOUT.1\",\"width\":1280,\"height\":720,\"elements\":[{\"id\":\"source\",\"kind\":\"image\",\"width\":16,\"height\":16,\"texture\":\""+std::string(name)+"\"}]}";
  check(runtime.load_json(json,fixtures),"Original image format loads directly into layout");std::vector<uint8_t> painted(1280*720*4);check(runtime.paint(painted,1280,720,{}),"Original format paints without DDS conversion");
  for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x){auto source=(y*16+x)*4,target=(y*1280+x)*4;check(painted[target+3]==image.rgba[source+3],"Runtime preserves image alpha");if(image.rgba[source+3])for(unsigned c=0;c<3;++c)check(painted[target+c]==image.rgba[source+c],"Runtime preserves visible straight RGB");}
  std::cout<<name<<": first frame, authored alpha, direct layout paint, DDS roundtrip and unchanged source bytes passed\n";
 }
 const auto invalid=temporary.path/"unsupported.svg";{std::ofstream f(invalid);f<<"<svg xmlns='http://www.w3.org/2000/svg'><rect width='16' height='16'/></svg>";}bool rejected=false;try{decode_image(invalid);}catch(const std::exception& e){rejected=std::string(e.what()).find("Unsupported or invalid UI raster image")!=std::string::npos;}check(rejected,"Vector data gets explicit unsupported raster diagnostic");
 std::cout<<"multi UI WIC GIF/TIFF/ICO first-frame formats passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
