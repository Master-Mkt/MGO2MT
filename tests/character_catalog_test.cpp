#include "character_catalog.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <stdexcept>
using namespace mgo2win;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(int argc,char**argv){try{
 if(argc!=2)return 2;std::ifstream in(argv[1],std::ios::binary);std::vector<char>b((std::istreambuf_iterator<char>(in)),{});
 CharacterCatalog catalog(b);check(catalog.mesh_count()>100,"catalog coverage");
 std::array<uint8_t,28>a{};a[2]=11;a[3]=22;a[15]=46;a[17]=57;
 auto first=catalog.assemble(a);check(first.ready()&&first.selectedParts==5&&first.missingModels==0,"male parts");
 // Original mode2 RGB changes only shader0x10. The goggles' two frame
 // materials must retain white tint; original image bytes are not recolored.
 if(uint8_t(b[4])==3)for(unsigned gender=0;gender<2;++gender){
  std::array<std::array<float,3>,6> colors{{{.25f,.25f,.25f},{2,2,0},{2,.23f,.41f},{2,1,0},{1,.68f,0},{2,2,2}}};
  auto control=a;control[0]=uint8_t(gender);auto body=catalog.assemble(control);
  for(unsigned color=0;color<colors.size();++color){auto goggles=control;goggles[18]=103;goggles[25]=uint8_t(color);auto before=goggles;auto shown=catalog.assemble(goggles);
   check(shown.selectedParts==6&&!shown.missingColors&&shown.issues.empty()&&goggles==before,"goggle RGB and unchanged received data");
   unsigned colored=0,frame=0;for(size_t i=0;i<shown.model.parts.size();++i){auto&p=shown.model.parts[i];if(i>=body.model.parts.size()&&p.materialShader==0x10){++colored;for(unsigned c=0;c<3;++c)check(std::abs(p.tint[c]-colors[color][c])<.00001f,"original scaled RGB");}else{check(p.tint==std::array<float,3>{1,1,1},"other materials unchanged");if(i>=body.model.parts.size())++frame;}}
   check(colored&&frame,"goggle lens and frame isolation");
  }
 }
 // These exact palettes previously lost their pattern image because two
 // source archive families shared a raw image index before DCI remapping.
 for(unsigned gender=0;gender<2;++gender)for(unsigned upper:{11u,15u})for(unsigned color:{14u,15u,16u,17u,18u,19u,20u,26u,27u}){
  auto recovered=a;recovered[0]=uint8_t(gender);recovered[2]=uint8_t(upper);recovered[5]=uint8_t(upper==15?0:color);
  auto restored=catalog.assemble(recovered);check(restored.ready()&&restored.selectedParts==5&&!restored.missingModels&&!restored.missingColors&&restored.issues.empty(),"restored archive-family palette");
  check(!catalog.creation_choices(gender,200).empty(),"restored colors available in creation");
 }
 // Zero is the candidate server's stored lower value. Render the same geometry,
 // colors and animation as explicit standard trousers, without editing input.
 for(unsigned gender=0;gender<2;++gender)for(unsigned color:{0u,5u}){
  auto zero=a;zero[0]=uint8_t(gender);zero[3]=0;zero[6]=uint8_t(color);const auto received=zero;
  auto explicitLower=zero;explicitLower[3]=22;
  auto restored=catalog.assemble(zero),standard=catalog.assemble(explicitLower);
  check(zero==received,"received appearance preserved");
  check(restored.defaultedLower&&!standard.defaultedLower&&restored.selectedParts==5&&restored.missingModels==0&&restored.missingColors==0,"zero lower fallback coverage");
  check(restored.model.indices==standard.model.indices&&restored.model.bounds==standard.model.bounds&&restored.model.textures.size()==standard.model.textures.size(),"lower topology/bounds/textures");
  for(size_t i=0;i<restored.model.textures.size();++i)check(restored.model.textures[i].pixels==standard.model.textures[i].pixels,"lower color retained");
  catalog.pose(restored,.73);catalog.pose(standard,.73);check(restored.model.vertices.size()==standard.model.vertices.size(),"full animated body");
  for(size_t i=0;i<restored.model.vertices.size();++i){auto&x=restored.model.vertices[i];auto&y=standard.model.vertices[i];check(x.x==y.x&&x.y==y.y&&x.z==y.z,"restored lower animation matches explicit lower");}
 }
 for(uint8_t id:{uint8_t(1),uint8_t(21),uint8_t(28),uint8_t(255)}){auto invalid=a;invalid[3]=id;auto result=catalog.assemble(invalid);check(!result.defaultedLower&&result.missingModels==1&&result.selectedParts==4,"nonzero unknown lower stays unsupported");}
 auto vertices=first.model.vertices;catalog.pose(first,.55);double delta=0;
 for(size_t i=0;i<vertices.size();++i){auto&v=first.model.vertices[i];check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z),"finite pose");delta+=std::abs(v.x-vertices[i].x)+std::abs(v.y-vertices[i].y)+std::abs(v.z-vertices[i].z);}
 check(delta>100,"original animation moves vertices");catalog.pose(first,double(catalog.frames())/60);check(std::abs(first.model.vertices[0].y-vertices[0].y)<.01,"animation wraps");
 a[5]=5;a[6]=5;auto recolored=catalog.assemble(a);check(recolored.ready()&&recolored.missingColors==0,"supported colors");
 bool colorChanged=recolored.model.textures.size()!=first.model.textures.size();for(size_t i=0;!colorChanged&&i<recolored.model.textures.size();++i)colorChanged=recolored.model.textures[i].pixels!=first.model.textures[i].pixels;check(colorChanged,"clothing color changes texture");a[5]=a[6]=0;
 a[15]=47;auto gloves=catalog.assemble(a);check(gloves.selectedParts==5&&gloves.missingModels==0,"equipment with distinct bind positions");catalog.pose(gloves,.3);a[15]=46;
 a[1]=1;a[2]=12;auto other=catalog.assemble(a);check(other.ready()&&other.selectedParts==5,"second male appearance");
 bool differs=other.bind.size()!=first.bind.size();for(size_t i=0;!differs&&i<other.bind.size();++i)differs=other.bind[i].x!=first.bind[i].x||other.bind[i].y!=first.bind[i].y;check(differs,"face/clothing geometry changes");
 a[0]=1;auto female=catalog.assemble(a);check(female.ready()&&female.selectedParts==5&&female.missingModels==0,"female parts");catalog.pose(female,.8);
 a[2]=255;auto missing=catalog.assemble(a);check(missing.missingModels==1&&missing.selectedParts==4,"unknown equipment not substituted");
 check(!missing.issues.empty()&&std::string(missing.issues.front().reason)=="unknown_model"&&missing.issues.front().id==255,"actionable appearance diagnostics");
 a[0]=255;check(!catalog.assemble(a).ready(),"invalid gender");
 auto rejected=[](const std::vector<char>&bytes){try{CharacterCatalog bad(bytes);return false;}catch(const std::exception&){return true;}};
 auto bad=b;bad[0]='X';check(rejected(bad),"magic");bad=b;bad.push_back(0);check(rejected(bad),"trailing bytes");bad=b;bad.resize(bad.size()-1);check(rejected(bad),"truncation");bad=b;bad[8]=char(255);bad[9]=char(255);check(rejected(bad),"counts");bad=b;bad[60]=char(0xff);bad[61]=char(0xff);bad[62]=char(0xff);bad[63]=char(0x7f);check(rejected(bad),"nonfinite quaternion");
 std::cout<<"Appearance/skin/animation/malformed catalog checks passed; delta="<<delta<<"\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
