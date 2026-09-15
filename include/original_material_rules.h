#pragma once
#include "character_model.h"
#include <bit>
#include <cmath>
#include <string_view>

namespace mgo2win {
struct MaterialRestoreDecision { uint32_t rules=0; std::string reason; };
namespace original_material_detail {
inline constexpr std::string_view basePackage="9f0e7152f8316058e5610d2b0b1ebfcb8aa34adeb1066d19540f02a41cd98dd2";
inline constexpr std::string_view patchPackage="f3817d31657c86d0324eb91e118b2b8a71fbd397854e59ca1dcc32d199a7ffd0";
struct Profile { uint32_t key,extra; const char* vp; const char* fp; const char* patchFp; };
// Complete Cg program blob SHA-256, NOT microcode SHA-256. Only explicitly
// provisional variant 0 is selected. Runtime RSX variant selection is unknown.
// Evidence: slot-material-verification-20260914/{followup,key-bits-followup}.
inline constexpr Profile profiles[]={
 {0x1001,noMaterialTexture,"4b4deefe5014857198c8a1132aae6f459d893b667ef3f0831829a9c38ceba621","0915a07f5de936419a9b2865c07e18cc7debebfe308d6847f890325e8fdf1a73",nullptr},
 {0x100000,noMaterialTexture,"c2ad4d50584fce9b5664c22ba8dde0f2e2f04fa0c99f90d7c775fb90c5c0eea1","bcdd5885f24e70cab03d94588daf2c5d9e2cb3a640c4057969061214ca3ad8be","eb372ceb473ac67ca218553ae2d3768ea8869b43aaf66ec60298a21c77b39a9d"},
 {0x120000,noMaterialTexture,"afb6ea8c295828fea3149dbc0ce20d8b2347f27901e738dde414fc48d97d4bd9","20a9b5b8d1e43badfa2c8cd90aa43fc4b4f9f221eddc323115b2213974dca393","4a5577f9b2cf9dd4ba5f0e2f429f2f112d85623c0d093e468c2afed1f6faa179"},
 {0x10,noMaterialTexture,"ed6e3caff228050159087240a0efc8dc6f3d09fe31f28ba7ee781f8f40a79d10","4069a2f321bfdec61851a71c5b5859bf537fc5b863ec674324970ab9b8652f3d",nullptr},
 {0x50,noMaterialTexture,"fb42ba3b21ba8708cd7cc09a918c85e025c4094a49b9ea38eb1465123a927709","212ff30d4a3ab5154657bb305674295070ead7f75a4b4e116afefed815a57f3d",nullptr},
 {0x61,noMaterialTexture,"fb42ba3b21ba8708cd7cc09a918c85e025c4094a49b9ea38eb1465123a927709","312aedc08a8b42c11c438cfdffe9b0dd5e16535914e8b83c44f01cc6e704f1ee",nullptr},
 {0x4001,3,"fb42ba3b21ba8708cd7cc09a918c85e025c4094a49b9ea38eb1465123a927709","6bbcf2eecaebfc5d3a97db4e0b60dedaffa3babb5ab7c0d2df680c9ae6cddb19",nullptr},
 {0x4003,4,"fb42ba3b21ba8708cd7cc09a918c85e025c4094a49b9ea38eb1465123a927709","62d7245c7345ec8777b28a137337b1499d8305012a815df1c85a102f61a86c2e",nullptr},
 {0x40a0,6,"bd5657cbb37b412d5bd88e77e8db3e9d7268ca9b852b005eafc9434344dc954b","870565bb0abb72536089be1f63e5d7796f39e41264979080017d01b8fc0bc509",nullptr},
 {0x5003,4,"4b4deefe5014857198c8a1132aae6f459d893b667ef3f0831829a9c38ceba621","a59c17ce2528461c831bc539c5b48a8a348f4251ec60410551085379b99b5475",nullptr},
 {0x50a0,6,"c46bc33e55233e1015a65362ac9cceec92580edfd10dbd18ed3e67838a5e448d","ae24701f2f1e661035c428e4f3de0657b5fcc3ff1aecf87f4e89cc0270b1e0d1",nullptr}
};
inline uint32_t be32(const uint8_t* p) {
 return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];
}
inline bool sha(std::string_view s) {
 if(s.size()!=64)return false;
 for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
 return true;
}
inline bool hasHalfUv(const OriginalMaterial& o,uint32_t semantic) {
 const auto n=be32(o.vertexDeclaration.data()+4);
 if(n>16)return false;
 for(uint32_t i=0;i<n;++i)if(o.vertexDeclaration[16+i]==((7u<<4)|semantic))return true;
 return false;
}
inline bool resolved(const OriginalMaterial& o,uint32_t slot,size_t count) {
 return slot<o.textures.size() && o.textures[slot].image<count &&
  o.textures[slot].image!=noMaterialTexture && !o.textures[slot].provenance.empty();
}
inline bool hasOriginalRgba8(const OriginalMaterial& o) {
 const auto n=be32(o.vertexDeclaration.data()+4),stride=be32(o.vertexDeclaration.data()+8);
 if(n>16)return false;
 for(uint32_t i=0;i<n;++i)if(o.vertexDeclaration[16+i]==0x83)
  return uint32_t(o.vertexDeclaration[32+i])+4<=stride;
 return false;
}
inline bool finiteTransform(const OriginalTexture& texture) {
 for(size_t i=8;i<24;i+=4)if(!std::isfinite(std::bit_cast<float>(be32(texture.raw.data()+i))))return false;
 return true;
}
inline bool identityTransform(const OriginalTexture& texture) {
 constexpr float identity[4]={1,1,0,0};
 for(size_t i=0;i<4;++i)if(std::bit_cast<float>(be32(texture.raw.data()+8+4*i))!=identity[i])return false;
 return true;
}
}
inline MaterialRestoreDecision select_original_material(const OriginalMaterial& o,size_t textureCount) {
 using namespace original_material_detail;
 auto reject=[](const char* why){return MaterialRestoreDecision{0,why};};
 if(!o.present)return reject("No MAT3 source material; legacy rendering");
 if(!o.requestedRules)return {0,o.fallbackReason.empty()?"No explicit provisional shader request":o.fallbackReason};
 if((o.requestedRules&~15u)!=0)return reject("Unknown requested material rule");
 if(o.mdnPath.empty()||!sha(o.mdnSha256)||o.packagePath.empty()||o.ruleId.empty())
  return reject("Missing source MDN/package/rule provenance");
 const bool patch=o.packageSha256==patchPackage;
 if(!patch&&o.packageSha256!=basePackage)return reject("Shader package outside audited base/patch VFP fingerprints");
 const Profile* p=nullptr;
 for(const auto& candidate:profiles)if(candidate.key==o.key){p=&candidate;break;}
 if(!p)return reject("Unreviewed full selector; palette remains inactive pending source UV11/runtime binding");
 const std::string_view vp=(patch&&o.key==0x100000)?
  "79d85f294c898d086552e710883195f9a07cf0084da4645615b27fa3748f02ff":p->vp;
 const std::string_view fp=(patch&&p->patchFp)?p->patchFp:p->fp;
 if(o.vertexProgramSha256!=vp||o.fragmentProgramSha256!=fp)
  return reject("VP/FP full-program SHA mismatch; only provisional variant 0 audited here");
 if(o.key==0x1001){
  if(o.requestedRules!=8u)return reject("0x1001 only supports the audited three-color mask request");
  if(!hasOriginalRgba8(o))return reject("Original RGBA8 COLOR absent; queued MDN defaults are RGBA=1 but this asset draw-path binding is unresolved");
  if(!hasHalfUv(o,8)||!resolved(o,0,textureCount)||!finiteTransform(o.textures[0])||!identityTransform(o.textures[0]))
   return reject("Three-color mask requires original UV0 and resolved identity base texture");
  return {8u,"Audited 0x1001 variant 0 color mask; original shared FP P36..38 defaults; effective c467.x must be supplied per draw, otherwise inactive; native lighting/RSX rounding remain approximate"};
 }
 if(o.requestedRules&8u)return reject("Three-color mask not audited for this full selector");
 if(o.key==0x40a0||o.key==0x50a0)
  return reject("Audited normal mix uses TEX1.xy; its VP UV consumer is not yet represented by this renderer");
 if(o.parameterCount==0||o.parameterCount>8)return reject("Missing active P0 tangent handedness");
 for(uint32_t i=0;i<o.parameterCount;++i)for(float v:o.parameters[i])
  if(!std::isfinite(v))return reject("Non-finite active original material coefficient");
 if(!hasHalfUv(o,8))return reject("Original half UV semantic 8 absent");
 const bool background=o.key==0x100000||o.key==0x120000;
 if(background&&!hasHalfUv(o,9))return reject("Background normal requires original half UV semantic 9; UV1 unavailable");
 if(o.normalSlot!=1||!resolved(o,0,textureCount)||!resolved(o,1,textureCount))
  return reject("Original base/normal slot unresolved or texture provenance missing");
 for(const auto& t:o.textures)if(!finiteTransform(t))return reject("Non-finite original texture UV transform");
 // The present renderer stores one UV0 plus UV1. Do not silently apply a
 // per-normal texture transform when the original FP shares TEX0 from slot0.
 if(!identityTransform(o.textures[0])||!identityTransform(o.textures[1]))
  return reject("Non-identity original UV transform not yet bound to the audited VP consumer");
 MaterialRestoreDecision d;
 if(o.requestedRules&1u)d.rules|=1u;
 if(p->extra!=noMaterialTexture) {
  // Even a request for only the base normal must not erase an authored mix.
  if((o.requestedRules&3u)!=3u||o.parameterCount<3||o.extraNormalSlot!=p->extra||
     !resolved(o,p->extra,textureCount))
   return reject("Audited extra-normal selector requires both maps and active P2.z");
  if(!identityTransform(o.textures[p->extra]))
   return reject("Extra-normal texture transform differs from the original shared TEX0 consumer");
  d.rules|=3u;
 } else if(o.requestedRules&2u)return reject("Extra-normal rule requested for a selector outside the five audited pairs");
 d.reason="Provisional variant 0: original AG XY and sqrt(saturate(1-dot(XY,XY))); derivative TBN approximates the original tangent and P0.w handedness; original RSX lighting/rounding not reproduced";
 if(p->extra!=noMaterialTexture)d.reason+="; original P2.z interpolation without clamping; runtime writer unknown";
 if(o.reflectionSlot!=noMaterialTexture||o.key==0x10||o.key==0x61||o.key==0x100000)
  d.reason+="; reflection inactive pending proven sampler dimensionality, source image and original lighting composition";
 if(o.requestedRules&4u)d.reason+="; palette UV inactive pending complete source UV11 and runtime color join";
 return d;
}
}
