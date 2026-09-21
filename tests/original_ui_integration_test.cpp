#include "player_menu.h"
#include "menu_font.h"
#include "system_ui_icons.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void bitmap(const void* pixels,const std::filesystem::path& path){
 BITMAPFILEHEADER h{};BITMAPINFOHEADER i{};h.bfType=0x4d42;h.bfOffBits=sizeof(h)+sizeof(i);h.bfSize=h.bfOffBits+1280*720*4;
 i.biSize=sizeof(i);i.biWidth=1280;i.biHeight=-720;i.biPlanes=1;i.biBitCount=32;
 std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<char*>(&h),sizeof(h));f.write(reinterpret_cast<char*>(&i),sizeof(i));f.write(static_cast<const char*>(pixels),1280*720*4);require(bool(f),"capture write");
}
int main(int argc,char**argv){try{
 require(argc==3,"fixture and output required");const std::filesystem::path root=argv[1],out=argv[2];std::filesystem::create_directories(out);
 require(menu_font_resources().load(root/"fonts"),"original TTF private registration");
 auto dc=CreateCompatibleDC(nullptr);
 for(auto weight:{FW_NORMAL,FW_BOLD}){
  auto font=create_menu_font(23,weight);auto old=SelectObject(dc,font);wchar_t face[80]{};GetTextFaceW(dc,80,face);
  require(std::wstring(face)==original_menu_font_face(weight),"GDI silently substituted a different font");
  const std::wstring sample=L"装備 武器 選択 決定 戻る 設定 読み込み 日本語 あいうえお アイウエオ";
  std::vector<WORD> glyphs(sample.size());require(GetGlyphIndicesW(dc,sample.data(),int(sample.size()),glyphs.data(),GGI_MARK_NONEXISTING_GLYPHS)!=GDI_ERROR,"glyph query");
  for(auto g:glyphs)require(g!=0xffff,"Japanese glyph missing");SelectObject(dc,old);DeleteObject(font);
 }
 DeleteDC(dc);
 equipment::Icons icons;std::string error;require(icons.load(root/"equipment-icons",error),error.c_str());require(icons.size()==18,"all original equipment IDs");
 weapons::Icons weapons;require(weapons.load(root/"weapon-icons/index.tsv",error),error.c_str());
 require(icons.find(22)&&(!weapons.find(22)||icons.find(22)->bgra!=weapons.find(22)->bgra),"ENVG remains available independently of weapon 22 coverage");
 require(icons.find(8)&&weapons.find(8)&&icons.find(8)->bgra!=weapons.find(8)->bgra,"shared numeric ID never shares equipment/weapon artwork");
 const auto* extent=icons.extent(19);require(extent&&std::abs(extent->width/extent->height-4.)>.1,"original LA2 nonuniform display aspect retained");
 require(!icons.find(0)&&!icons.find(25),"missing equipment never borrows weapon icon");
 require(system_ui_icons().load(root/"system-ui/index.tsv",error)&&system_ui_icons().size()==1,"only reviewed direct system UI binding");
 const auto* diagram=system_ui_icons().find(0);require(diagram&&diagram->width==360&&diagram->height==240,"original controller image extent");
 const auto temp=std::filesystem::temp_directory_path()/("mgo2mt-original-ui-"+std::to_string(GetCurrentProcessId()));std::filesystem::create_directories(temp);
 auto input=std::make_shared<ControllerInput>(temp/"input.cfg");input->config.device=1;auto graphics=std::make_shared<GraphicsSettings>(temp/"graphics.cfg");
 PlayerMenu menu(temp/"input.cfg",input,graphics),without(temp/"missing.cfg",input,graphics);menu.hold_assets(root);without.hold_assets(temp);
 constexpr uint16_t ids[]={6,8,9,10,16,19,20,21,22,23,24,61,62,63,64,65,66,67};
 for(auto id:ids){
  hold_selection::Snapshot s;s.scope={9,7,2,1,33,101,4};s.eligible=true;s.equipment={{0,id,1,1,0,0,70,false}};s.selectedEquipment=0;
  hold_selection::Input hold;hold.equipment=true;
  menu.hold_selection_step(s,{});without.hold_selection_step(s,{});menu.hold_selection_step(s,hold);without.hold_selection_step(s,hold);require(menu.hold_visible(),"equipment UI opens");Sleep(165);
  const auto* pixels=static_cast<const uint32_t*>(menu.draw());const auto* empty=static_cast<const uint32_t*>(without.draw());
  size_t difference=0;for(int y=600;y<660;++y)for(int x=75;x<275;++x){auto p=pixels[y*1280+x];int r=(p>>16)&255,g=(p>>8)&255,b=p&255;difference+=p!=empty[y*1280+x]&&r>60&&std::abs(r-g)<20&&std::abs(g-b)<20;}
  require(difference>10,"original gray/white equipment pixels visible separately from amber labels");bitmap(pixels,out/("equipment_"+std::to_string(id)+".bmp"));
  auto choice=menu.hold_selection_step(s,{});require(choice&&choice->item.item==id&&choice->kind==hold_selection::Kind::equipment,"icon does not alter equip identity");without.cancel_hold_selection();
 }
 menu.open(player::Menu::settings);menu.message(nullptr,WM_KEYDOWN,VK_END,0); // normal settings routing remains active
 for(int page=0;page<3;++page)menu.message(nullptr,WM_KEYDOWN,VK_NEXT,0);
 auto pixels=static_cast<const uint32_t*>(menu.draw());size_t white=0;for(int y=426;y<554;++y)for(int x=925;x<1122;++x){auto p=pixels[y*1280+x];white+=((p&255)>130&&((p>>8)&255)>130&&((p>>16)&255)>130);}
 require(white>100,"original controller diagram composited after menu alpha pass");bitmap(pixels,out/"controller_original_font.bmp");
 // Missing/invalid display metadata must clear a formerly valid cache.
 auto bad=temp/"equipment-icons";std::filesystem::create_directories(bad);std::filesystem::copy_file(root/"equipment-icons/index.tsv",bad/"index.tsv",std::filesystem::copy_options::overwrite_existing);
 for(auto id:ids)std::filesystem::copy_file(root/"equipment-icons"/("equipment_"+std::to_string(id)+".png"),bad/("equipment_"+std::to_string(id)+".png"),std::filesystem::copy_options::overwrite_existing);
 {std::ofstream f(bad/"display.tsv");f<<"MGO2MT_EQUIPMENT_DISPLAY 1\n22 nan 64\n";}
 require(!icons.load(bad,error)&&icons.size()==0,"bad aspect does not retain stale image mapping");
 std::cout<<"original UI: 18 equipment IDs, separate namespaces, LA2 aspect, private Japanese regular/bold, controller pixels PASS\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
