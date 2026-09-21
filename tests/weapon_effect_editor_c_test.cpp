#include "weapon_effect_editor_c.h"
#include "weapon_effect_config.h"
#include "multi_ui.h"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <iostream>
#include <stdexcept>
static unsigned checks=0;static void check(bool ok,const char*msg){++checks;if(!ok)throw std::runtime_error(msg);}
int main(int argc,char**argv){try{namespace fs=std::filesystem;using namespace mgo2mt;check(argc==3,"paths");fs::path bundle=argv[1],root=argv[2];fs::create_directories(root);
 auto count=mw_texture_keys(bundle.c_str(),nullptr,0);check(count==26,"original texture count");std::vector<uint32_t> keys(count);check(mw_texture_keys(bundle.c_str(),keys.data(),count)==count,"original keys read");check(mw_texture_keys(bundle.c_str(),keys.data(),count-1)==-1,"key bounds");
 uint32_t w=0,h=0;check(mw_image(bundle.c_str(),keys[0],nullptr,&w,&h,0)==2&&w&&h,"image query");std::vector<uint8_t>rgba(size_t(w)*h*4);check(mw_image(bundle.c_str(),keys[0],rgba.data(),&w,&h,int(rgba.size()))==1,"BC image decoded");check(mw_image(bundle.c_str(),keys[0],rgba.data(),&w,&h,1)==0,"image bounds");
 multi_ui::Image image{2,2,{255,0,0,255,0,255,0,100,0,0,255,50,255,255,255,0}};auto dds=multi_ui::encode_dds(image);auto file=root/"custom.dds";{std::ofstream f(file,std::ios::binary);f.write(reinterpret_cast<const char*>(dds.data()),dds.size());}check(mw_image(file.c_str(),0,nullptr,&w,&h,0)==2&&w==2&&h==2,"import image query");rgba.resize(16);check(mw_image(file.c_str(),0,rgba.data(),&w,&h,16)==1&&rgba==image.rgba,"import alpha exact");
 const std::string json=R"({"format":"MGO2MT.WeaponEffects","version":1,"weapons":[{"id":25,"particles":{"muzzle":[{"texture":"custom.dds","count":4,"lifetimeMs":900,"emissionMs":100,"radius":120,"stretch":[2,0.5],"sizeRandom":0.5,"alphaRandom":0.2,"rotationRandom":2,"speed":100,"spread":0.7,"sizeCurve":[[0,0.5],[0.4,2],[1,4]],"alphaCurve":[[0,1],[1,0]]}]}}]})";
 weapon_effect::Config config;std::string error;check(config.load_text(json,error),"native config");auto expected=weapon_effect::sample(*config.particles(25,"muzzle"),{0,0,0},{0,0,1},17,500);check(mw_sample(json.data(),int(json.size()),25,"muzzle",500,17,nullptr,0)==int(expected.size()),"sample count parity");std::vector<MWParticle> actual(expected.size());check(mw_sample(json.data(),int(json.size()),25,"muzzle",500,17,actual.data(),int(actual.size()))==int(expected.size()),"sample fetch");
 for(size_t i=0;i<expected.size();++i){const auto&a=actual[i];const auto&e=expected[i];check(std::equal(e.position.begin(),e.position.end(),a.position)&&e.radius==a.radius&&e.rotation==a.rotation&&std::equal(e.stretch.begin(),e.stretch.end(),a.stretch)&&std::equal(e.rgba.begin(),e.rgba.end(),a.rgba)&&std::equal(e.uv.begin(),e.uv.end(),a.uv)&&e.texture==a.texture&&unsigned(e.additive)==a.additive,"editor and game exact sample parity");}
 char path[256]{};check(mw_texture_path(actual.front().texture,path,sizeof(path))==10&&std::string(path)=="custom.dds","import ID path reverse lookup");check(mw_sample(json.data(),int(json.size()),25,"muzzle",90000,17,nullptr,0)==0,"expired particles hidden");check(mw_sample("{",1,25,"muzzle",100,17,nullptr,0)==-1,"invalid document rejected");check(mw_last_error(path,sizeof(path))>0,"error available");std::cout<<checks<<" editor native parity checks passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
