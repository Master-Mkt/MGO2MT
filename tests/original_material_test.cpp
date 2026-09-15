#include "original_material_rules.h"
#include "original_color_mask.h"
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace mgo2win;
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void be32(uint8_t* bytes,uint32_t value){for(unsigned i=0;i<4;++i)bytes[i]=uint8_t(value>>(24-8*i));}
void textureFloat(OriginalTexture& texture,unsigned component,float value){be32(texture.raw.data()+8+4*component,std::bit_cast<uint32_t>(value));}
OriginalMaterial lens(){
 OriginalMaterial o;o.present=true;o.key=0x61;o.parameterCount=1;o.requestedRules=1;o.normalSlot=1;o.reflectionSlot=4;
 o.mdnPath="fixture/operator.mdn";
 o.mdnSha256="7b7880b6f79c3ab265c5113f91f3e739492620415a420d72dbad57e9dec6b1b8";
 o.packagePath="fixture/o/shader/shader.vfp";
 o.packageSha256="9f0e7152f8316058e5610d2b0b1ebfcb8aa34adeb1066d19540f02a41cd98dd2";
 o.vertexProgramSha256="fb42ba3b21ba8708cd7cc09a918c85e025c4094a49b9ea38eb1465123a927709";
 o.fragmentProgramSha256="312aedc08a8b42c11c438cfdffe9b0dd5e16535914e8b83c44f01cc6e704f1ee";
 o.ruleId="fixture explicit provisional variant 0; no live variant claim";
 o.parameters[0]={8.f,.08197021484375f,0.f,1.f};
 be32(o.vertexDeclaration.data()+4,1);o.vertexDeclaration[16]=0x78;
 o.textures.resize(5);
 for(unsigned i=0;i<o.textures.size();++i){auto& t=o.textures[i];t.image=i;t.provenance="fixture same-package original texture";textureFloat(t,0,1);textureFloat(t,1,1);}
 return o;
}
void reject(const OriginalMaterial& o,const char* why,size_t imageCount=5){
 const auto d=select_original_material(o,imageCount);
 require(d.rules==0&&!d.reason.empty(),why);
}
void run(){
 // New source-only cases; not executed while the current build hold applies.
 OriginalColorMaskState colors;
 require(std::bit_cast<uint32_t>(colors.p36[0])==0x3f0978d5&&
         std::bit_cast<uint32_t>(colors.p36[1])==0x3e926e98&&
         std::bit_cast<uint32_t>(colors.p36[2])==0x3e828f5c&&
         std::bit_cast<uint32_t>(colors.p37[0])==0x3ece5604&&
         std::bit_cast<uint32_t>(colors.p38[0])==0x3eb2b021,"original color float32 constants");
 colors.p39={2,3,4,5};colors.p37[3]=.75f;
 const auto first=colors.p36,third=colors.p38;
 colors.gcx_second_color({-1000,1500,2000});
 require(colors.p37[0]<0&&colors.p37[1]>1&&colors.p37[2]>1,"GCX color update preserves unclamped extrapolation");
 require(colors.p36==first&&colors.p38==third&&colors.p37[3]==.75f&&colors.p39[0]==2,"GCX writes only P37 RGB");
 colors.reset_colors();require(colors.p37==OriginalColorMaskState::initial37()&&colors.p39[0]==2,"reset preserves P39");
 colors.initialize();require(colors.p39==OriginalColorMaskState::Vector{},"initialize clears P39");
 OriginalColorMaskDraw draw;require(!draw.c467x,"unknown effective c467.x is inactive by default");draw.validate();
 draw.colors.p36[0]=std::numeric_limits<float>::quiet_NaN();
 bool invalid=false;try{draw.validate();}catch(const std::invalid_argument&){invalid=true;}
 require(invalid,"invalid shared state rejected before GPU mutation");
 auto mask=lens();mask.key=0x1001;mask.requestedRules=8;mask.parameterCount=0;
 mask.vertexProgramSha256="4b4deefe5014857198c8a1132aae6f459d893b667ef3f0831829a9c38ceba621";
 mask.fragmentProgramSha256="0915a07f5de936419a9b2865c07e18cc7debebfe308d6847f890325e8fdf1a73";
 reject(mask,"missing original COLOR never becomes universal white");
 be32(mask.vertexDeclaration.data()+4,2);be32(mask.vertexDeclaration.data()+8,8);
 mask.vertexDeclaration[17]=0x83;mask.vertexDeclaration[33]=4;
 require(select_original_material(mask,5).rules==8,"mask uses shared coefficients independently of absent MDN P0");
 auto malformed=mask;malformed.vertexDeclaration[33]=5;reject(malformed,"COLOR must fit original vertex stride");
 malformed=mask;malformed.key=0x1003;reject(malformed,"mask bit alone cannot enable unreviewed complete selector");
 malformed=mask;malformed.fragmentProgramSha256.assign(64,'0');reject(malformed,"color mask needs exact audited FP identity");
 mask.packageSha256=std::string(original_material_detail::patchPackage);
 require(select_original_material(mask,5).rules==8,"identical audited patch representative accepted");
 auto o=lens();auto d=select_original_material(o,5);
 require(d.rules==1&&d.reason.find("reflection inactive")!=std::string::npos,"audited lens restores normals without inventing reflection composition");
 // The source cube is currently unresolved: optional reflection failure must
 // not suppress the independently proven normal path.
 o.textures[4].image=noMaterialTexture;o.textures[4].provenance.clear();
 require(select_original_material(o,5).rules==1,"missing reflection preserves available original lens normals");
 o=lens();o.textures[1].image=noMaterialTexture;reject(o,"missing normal cannot silently use a different material texture");
 o=lens();o.textures[1].image=5;reject(o,"one-past image index rejected");
 o=lens();o.textures[1].provenance.clear();reject(o,"image without source provenance rejected");
 o=lens();o.packageSha256.assign(64,'0');reject(o,"identical shader key from unreviewed package rejected");
 o=lens();o.fragmentProgramSha256="03211872bebb2dea064297acf82bc59d0af9ffe6e83d640ed85705812bd92132";
 reject(o,"microcode fingerprint cannot masquerade as full-program fingerprint");
 o=lens();o.key=0x62;reject(o,"nearby selector does not inherit material semantics");
 o=lens();o.parameters[0][1]=std::numeric_limits<float>::quiet_NaN();reject(o,"non-finite active coefficient rejected");
 o=lens();o.parameters[7][0]=std::numeric_limits<float>::quiet_NaN();
 require(select_original_material(o,5).rules==1,"inactive coefficient bytes are retained without entering the shader");
 o=lens();textureFloat(o.textures[1],0,2.f);reject(o,"per-normal transform cannot replace original shared TEX0 mapping");
 o=lens();o.vertexDeclaration[16]=0x79;reject(o,"another UV channel cannot stand in for original UV0");
 o=lens();o.requestedRules=0;o.fallbackReason="source-only material";
 require(select_original_material(o,5).rules==0,"metadata alone never activates a rendering rule");

 // Background 0x120000 keeps its normal map but needs a separate semantic 9.
 o=lens();o.key=0x120000;o.reflectionSlot=noMaterialTexture;
 o.vertexProgramSha256="afb6ea8c295828fea3149dbc0ce20d8b2347f27901e738dde414fc48d97d4bd9";
 o.fragmentProgramSha256="20a9b5b8d1e43badfa2c8cd90aa43fc4b4f9f221eddc323115b2213974dca393";
 reject(o,"background normal cannot reuse diffuse UV0 when semantic9 is missing");
 be32(o.vertexDeclaration.data()+4,2);o.vertexDeclaration[17]=0x79;
 require(select_original_material(o,5).rules==1,"background with preserved UV1 restores its original normal path");
 o.packageSha256="f3817d31657c86d0324eb91e118b2b8a71fbd397854e59ca1dcc32d199a7ffd0";
 reject(o,"base FP cannot be attributed to patch package");
 o.fragmentProgramSha256="4a5577f9b2cf9dd4ba5f0e2f429f2f112d85623c0d093e468c2afed1f6faa179";
 require(select_original_material(o,5).rules==1,"reviewed patch background uses its own FP fingerprint");

 // 0x4003 has a separately resolved slot4. A missing mix cannot degrade to
 // an asserted original base-only shader, and authored extrapolation is valid.
 o=lens();o.key=0x4003;o.parameterCount=3;o.requestedRules=3;o.extraNormalSlot=4;o.reflectionSlot=noMaterialTexture;
 o.fragmentProgramSha256="62d7245c7345ec8777b28a137337b1499d8305012a815df1c85a102f61a86c2e";
 for(float coefficient:{-.5f,0.f,1.f,1.5f}){
  o.parameters[2][2]=coefficient;d=select_original_material(o,5);
  require(d.rules==3&&o.parameters[2][2]==coefficient,"original P2.z accepts finite extrapolation without clamp or mutation");
 }
 auto changed=o;changed.extraNormalSlot=3;reject(changed,"additional normal sampler is full-key specific");
 changed=o;changed.parameterCount=2;reject(changed,"inactive P2 is not a valid mix coefficient");
 changed=o;changed.requestedRules=1;reject(changed,"mix selector cannot silently drop authored extra-normal behavior");
 changed=o;changed.textures[4].image=noMaterialTexture;reject(changed,"unresolved extra normal rejects the full requested mix");
 changed=o;textureFloat(changed.textures[4],2,.25f);reject(changed,"extra transform cannot replace shared source UV coordinates");
 o=lens();o.key=0x40a0;o.parameterCount=3;o.requestedRules=3;o.extraNormalSlot=6;
 o.vertexProgramSha256="bd5657cbb37b412d5bd88e77e8db3e9d7268ca9b852b005eafc9434344dc954b";
 o.fragmentProgramSha256="870565bb0abb72536089be1f63e5d7796f39e41264979080017d01b8fc0bc509";
 d=select_original_material(o,5);
 require(d.rules==0&&d.reason.find("TEX1")!=std::string::npos,"verified A0 formula remains inactive while its UV consumer is unavailable");
 std::cout<<"original material PASS: provenance separation, full-program identity, partial fallback, UV fidelity, active coefficients, package variants and unclamped authored normal mix\n";
}
}
int main(){try{run();return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
