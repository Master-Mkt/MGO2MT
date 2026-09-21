#include "character_renderer.h"
#include "shadow_renderer.h"
#include "world_depth.h"
#include "original_material_rules.h"
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <syncstream>
namespace mgo2mt {
static void ok(HRESULT h){if(FAILED(h))throw std::runtime_error("Character D3D11 resource failure");}
CharacterRenderer::CharacterRenderer(ID3D11Device*d,const CharacterModel&m,bool sky):parts_(m.parts),bounds_(m.bounds),sky_(sky),vertexCount_(m.vertices.size()){
 overviewBounds_=m.overviewBounds;hasOverviewBounds_=m.hasOverviewBounds;
 opaqueShadow_=std::all_of(m.vertices.begin(),m.vertices.end(),[](const ModelVertex&v){return v.lit>=.5f;});
 resize_target(d,616,392);
 auto buffer=[&](UINT size,UINT bind,const void* data,ID3D11Buffer**out){D3D11_BUFFER_DESC b{};b.ByteWidth=size;b.BindFlags=bind;b.Usage=bind==D3D11_BIND_VERTEX_BUFFER?D3D11_USAGE_DEFAULT:(data?D3D11_USAGE_IMMUTABLE:D3D11_USAGE_DEFAULT);D3D11_SUBRESOURCE_DATA s{};s.pSysMem=data;ok(d->CreateBuffer(&b,data?&s:nullptr,out));};
 buffer(UINT(m.vertices.size()*sizeof(ModelVertex)),D3D11_BIND_VERTEX_BUFFER,m.vertices.data(),&vertices_);buffer(UINT(m.indices.size()*4),D3D11_BIND_INDEX_BUFFER,m.indices.data(),&indices_);buffer(688,D3D11_BIND_CONSTANT_BUFFER,nullptr,&constants_);
 const std::string shader=std::string(shadows::receiver_shader())+R"(
 cbuffer EnvironmentFrame:register(b2){float4 envFront;float4 envBack;float4 envAxis;float4 envDirect;float4 envDirection;float4 envScale;};
 struct PointLight{float4 positionRadius;float4 colorIntensity;};
 cbuffer Camera:register(b0){row_major float4x4 wvp;row_major float4x4 world;float4 material;float4 tint;float4 lightCount;PointLight pointLights[8];float4 sourceP[8];float4 restore;float4 normalUV;float4 extraUV;float4 paletteUV;float4 colorP36;float4 colorP37;float4 colorP38;float4 colorMask;};
 struct V{float3 p:POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;float2 uv1:TEXCOORD1;float4 light:COLOR0;float4 authored:COLOR1;float2 uv2:TEXCOORD2;};
 struct P{float4 p:SV_POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;float2 uv1:TEXCOORD1;float3 worldPosition:TEXCOORD2;float4 light:COLOR0;float4 authored:COLOR1;float3 masks:TEXCOORD3;float2 uv2:TEXCOORD4;};
 float3 originalVertexMasks(float4 color,float c467x){return color.rgb*(c467x-color.a);}
 P vs(V v){P o;o.p=mul(float4(v.p,1),wvp);if(material.z>0)o.p.z=0;o.n=mul(float4(v.n,0),world).xyz;o.worldPosition=mul(float4(v.p,1),world).xyz;o.uv=v.uv;o.uv1=v.uv1;o.uv2=v.uv2;o.light=v.light;o.authored=v.authored;o.masks=colorMask.x>.5?originalVertexMasks(v.authored,colorMask.y):float3(0,0,0);return o;}
 Texture2D tex:register(t0);Texture2D pattern:register(t1);SamplerState sampleLinear:register(s0);
 Texture2D floorMap:register(t5);Texture2D normalMap:register(t2);Texture2D extraNormalMap:register(t3);
 // Verified AG unpack and five exact-key mixes (key-bits-followup). The
 // derivative frame is an explicit Windows approximation to original TBN.
 float2 unpackAG(float4 value){return 2*value.ag-1;}
 float3 originalTangentNormal(float2 a){return float3(a,sqrt(saturate(1-dot(a,a))));}
 float2 originalNormalMix(float2 base,float2 extra,float coefficient){return base+coefficient*(extra-base);}
 float2 originalPaletteOffset(float2 uv,float4 p2){return uv+float2(p2.y,-p2.z);}
 // Verified algebra only. No guessed cube/2D projection, reflection weight,
 // blend state, or alpha path is enabled while sampler bindings are unknown.
 float3 originalReflectionDirection(float3 n,float3 v){return v-2*dot(n,v)*n;}
 float originalLensAngle(float3 n,float3 v,float p0y){return pow(saturate(1-abs(dot(n,v))),p0y);}
 // 0x1000 representative VP0/FP594: q must be formed BEFORE interpolation.
 // Shared FP P36..38 defaults/upload are now proved; keep them separate from
 // MDN P0..7. Missing original COLOR or unknown effective c467.x stays inactive.
 float3 originalThreeColor(float3 baseLit,float3 q,float3 p36,float3 p37,float3 p38){float3 m=lerp(lerp(lerp(float3(1,1,1),p37,q.g),p36,q.r),p38,q.b);float coverage=1-(1-q.r)*(1-q.g)*(1-q.b);return baseLit*(1+coverage*(m-1));}
 float3 mappedNormal(P p){
  float3 n=normalize(p.n);if(restore.x<.5)return n;
  float2 uv=restore.z>.5?p.uv1:p.uv;uv=uv*normalUV.xy+normalUV.zw;
  float2 xy=unpackAG(normalMap.Sample(sampleLinear,uv));
  if(restore.y>.5){float2 extra=p.uv*extraUV.xy+extraUV.zw;xy=originalNormalMix(xy,unpackAG(extraNormalMap.Sample(sampleLinear,extra)),sourceP[2].z);}
  float3 px=ddx(p.worldPosition),py=ddy(p.worldPosition);float2 ux=ddx(uv),uy=ddy(uv);
  float det=ux.x*uy.y-ux.y*uy.x;if(abs(det)<1e-10)return n;
  float3 t=(px*uy.y-py*ux.y)/det;t-=n*dot(n,t);
  float t2=dot(t,t);if(t2<1e-16)return n;t*=rsqrt(t2);
  float3 b=(py*ux.x-px*uy.x)/det;float handed=dot(cross(n,t),b)<0?-1:1;
  float3 tn=originalTangentNormal(xy);return normalize(tn.x*t+tn.y*cross(n,t)*handed+tn.z*n);
 }
 // Material 0x120000: base FP714..717 / patch FP731..734 add COLOR0.rgb*2
 // independently of its alpha. The three-basis dynamic term still uses the
 // native LT3 diffuse approximation; this is not complete RSX shader parity.
 float3 encodeColor(float3 v){v=max(v,0);return lightCount.y>.5?float3(v.r<=.0031308?12.92*v.r:1.055*pow(v.r,1/2.4)-.055,v.g<=.0031308?12.92*v.g:1.055*pow(v.g,1/2.4)-.055,v.b<=.0031308?12.92*v.b:1.055*pow(v.b,1/2.4)-.055):v;}
 struct Pixel{float4 color:SV_Target0;float2 reflection:SV_Target1;};Pixel pixel(float4 c){Pixel o;o.color=c;o.reflection=lightCount.zw;return o;}
 Pixel ps(P p){float2 baseUV=p.uv;if(restore.w>.5)baseUV=originalPaletteOffset(baseUV*paletteUV.xy+paletteUV.zw,sourceP[2]);if(material.z>1.5)baseUV=p.uv*normalUV.xy+normalUV.zw;float4 c=tex.Sample(sampleLinear,baseUV);if(material.z>0){float3 sky=c.rgb*tint.rgb;if(material.z>1.5){sky=lerp(c.rgb,pattern.Sample(sampleLinear,p.uv1*extraUV.xy+extraUV.zw).rgb,p.authored.a);if(sourceP[2].y>.5)sky+=normalMap.Sample(sampleLinear,p.uv2*paletteUV.xy+paletteUV.zw+float2(sourceP[2].x,0)).rgb;sky*=p.authored.rgb*sourceP[0].rgb;sky=lerp(sky,sourceP[1].rgb,sourceP[1].w);}return pixel(float4(encodeColor(sky),1));}if(colorMask.z>.5){float4 layer=floorMap.Sample(sampleLinear,p.uv2);c.rgb=lerp(c.rgb,layer.rgb,p.authored.a*layer.a);}float3 shadingNormal=mappedNormal(p);if(material.w<.5&&p.light.a<.5)clip(c.a-.25);if(material.x>0)c.rgb*=pattern.Sample(sampleLinear,p.uv1).rgb;float light=.48+.52*abs(dot(shadingNormal,normalize(float3(-.3,.7,1))));float3 illumination=p.light.a>.5?p.light.rgb:float3(light,light,light);if(p.light.a>.5)illumination+=p.authored.rgb*material.y;
 float3 ambient=0;if(envFront.w>.5){ambient=lerp(envBack.rgb,envFront.rgb,saturate((1-dot(shadingNormal,envAxis.xyz))*.5))*envScale.rgb;illumination=ambient+envDirect.rgb*saturate(dot(shadingNormal,-envDirection.xyz));if(p.light.a>.5)illumination+=p.authored.rgb*material.y;}
 float3 cascadeColor;float visibility=shadowVisibility(p.worldPosition,normalize(shadingNormal),cascadeColor);
 if(shadowParams.x>0){float3 directional=shadowSun.rgb*saturate(dot(normalize(shadingNormal),-shadowDirection.xyz));
  float3 available=envFront.w>.5?max(0,illumination-ambient):(p.light.a>.5?illumination:max(0,illumination-.48));illumination-=min(available,directional)*(1-visibility);}
 // Native temporary point lighting: Lambert times linear falloff. The single
 // radius is the existing CPU PointLight range==extendedRange subset.
 // Added to existing lighting, with no shadow or original RSX parity claim.
 if(lightCount.x>0){float normalSquared=dot(shadingNormal,shadingNormal);float3 normal=normalSquared>=1e-16?shadingNormal*rsqrt(normalSquared):float3(0,1,0);
  [loop]for(int i=0;i<(int)lightCount.x;++i){float3 delta=pointLights[i].positionRadius.xyz-p.worldPosition;
   float distanceSquared=dot(delta,delta);float distance=sqrt(distanceSquared);
   float falloff=saturate(1-distance/pointLights[i].positionRadius.w);
   float diffuse=distanceSquared>1e-12?saturate(dot(normal,delta*rsqrt(distanceSquared))):1;
   illumination+=pointLights[i].colorIntensity.rgb*(pointLights[i].colorIntensity.w*falloff*diffuse);
  }
 }
 float3 baseLit=c.rgb*tint.rgb*illumination;
 if(colorMask.x>.5)baseLit=originalThreeColor(baseLit,p.masks,colorP36.rgb,colorP37.rgb,colorP38.rgb);
 if(shadowParams.x>0&&shadowBias.y>.5)baseLit=lerp(baseLit,cascadeColor,.45);
 float coverage=material.w>.5?saturate(c.a*p.authored.a):1;if(material.w>.5)clip(coverage-1e-7);return pixel(float4(encodeColor(baseLit),coverage*colorMask.w));}
 )";
 const bool csmSupported=d->GetFeatureLevel()>=D3D_FEATURE_LEVEL_11_0;const D3D_SHADER_MACRO legacyMacros[]={{"NO_CSM","1"},{nullptr,nullptr}};
 Ptr<ID3DBlob>v,p,error;auto compile=[&](const char* entry,const char* target,ID3DBlob** blob){error.Reset();auto result=D3DCompile(shader.data(),shader.size(),nullptr,csmSupported?nullptr:legacyMacros,nullptr,entry,target,D3DCOMPILE_ENABLE_STRICTNESS,0,blob,&error);if(FAILED(result)&&error)std::cerr.write(static_cast<const char*>(error->GetBufferPointer()),error->GetBufferSize());ok(result);};compile("vs","vs_4_0",&v);compile("ps",csmSupported?"ps_5_0":"ps_4_0",&p);
 ok(d->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&vs_));ok(d->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&ps_));
 D3D11_INPUT_ELEMENT_DESC e[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,40,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",1,DXGI_FORMAT_R32G32B32A32_FLOAT,0,56,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",2,DXGI_FORMAT_R32G32_FLOAT,0,72,D3D11_INPUT_PER_VERTEX_DATA,0}};ok(d->CreateInputLayout(e,7,v->GetBufferPointer(),v->GetBufferSize(),&layout_));
 sampler_=render_backend::Device(d).sampler(0);
 D3D11_RASTERIZER_DESC r{};r.FillMode=D3D11_FILL_SOLID;r.CullMode=D3D11_CULL_NONE;r.DepthClipEnable=TRUE;ok(d->CreateRasterizerState(&r,&raster_));
 D3D11_DEPTH_STENCIL_DESC z{};z.DepthEnable=TRUE;z.DepthWriteMask=sky_?D3D11_DEPTH_WRITE_MASK_ZERO:D3D11_DEPTH_WRITE_MASK_ALL;z.DepthFunc=sky_?D3D11_COMPARISON_GREATER_EQUAL:D3D11_COMPARISON_GREATER;ok(d->CreateDepthStencilState(&z,&depthState_));
 z.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;ok(d->CreateDepthStencilState(&z,&fadeDepth_));
 D3D11_BLEND_DESC fade{};auto& blend=fade.RenderTarget[0];blend.BlendEnable=TRUE;blend.SrcBlend=D3D11_BLEND_SRC_ALPHA;blend.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;blend.BlendOp=D3D11_BLEND_OP_ADD;blend.SrcBlendAlpha=D3D11_BLEND_ONE;blend.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;blend.BlendOpAlpha=D3D11_BLEND_OP_ADD;blend.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;ok(d->CreateBlendState(&fade,&fadeBlend_));
 for(size_t i=0;i<parts_.size();++i){const auto&p=parts_[i];if(p.surfaceAlpha>1)throw std::invalid_argument("Invalid original surface alpha policy");(p.surfaceAlpha?alphaParts_:opaqueParts_).push_back(i);std::array<float,3> minimum{1e30f,1e30f,1e30f},maximum{-1e30f,-1e30f,-1e30f};for(size_t j=p.first;j<size_t(p.first)+p.count;++j){const auto&v=m.vertices.at(m.indices.at(j));const std::array<float,3> point{v.x,v.y,v.z};for(unsigned k=0;k<3;++k){minimum[k]=std::min(minimum[k],point[k]);maximum[k]=std::max(maximum[k],point[k]);}}std::array<float,3> center{};if(p.count)for(unsigned k=0;k<3;++k)center[k]=(minimum[k]+maximum[k])*.5f;partCenters_.push_back(center);}
 for(size_t i=0;i<parts_.size();++i){const auto&p=parts_[i];auto decision=select_original_material(p.original,m.textures.size());if(sky_){decision.rules=0;decision.reason="sky retains diffuse-only rendering";}materialRules_.push_back(decision.rules);
  std::ostringstream out;out<<"part="<<i<<" key=0x"<<std::hex<<p.materialShader<<" name=0x"<<p.original.nameHash<<std::dec<<" sourceIndex="<<p.original.sourceIndex<<" mdn="<<p.original.mdnPath<<" sha256="<<p.original.mdnSha256<<" package="<<p.original.packagePath<<" packageSHA256="<<p.original.packageSha256<<" vp="<<p.original.vertexProgramSha256<<" fp="<<p.original.fragmentProgramSha256<<" rules="<<decision.rules<<" ruleId="<<p.original.ruleId<<" reason="<<decision.reason<<" sourceFallback="<<p.original.fallbackReason;
  for(unsigned j=0;j<p.original.parameterCount;++j){out<<" P"<<j<<"=";for(float v:p.original.parameters[j])out<<v<<',';}
  for(size_t j=0;j<p.original.textures.size();++j)out<<" slot"<<j<<"="<<p.original.textures[j].image<<" provenance="<<p.original.textures[j].provenance;
  materialDiagnostics_.push_back(out.str());
  if(p.original.present)std::osyncstream(std::clog)<<"material_restore "<<std::quoted(materialDiagnostics_.back())<<'\n';
 }
 for(const auto&im:m.textures){D3D11_TEXTURE2D_DESC td{};td.Width=im.width;td.Height=im.height;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;td.Format=im.codec==9?DXGI_FORMAT_BC1_TYPELESS:DXGI_FORMAT_BC3_TYPELESS;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
  D3D11_SUBRESOURCE_DATA sd{};sd.pSysMem=im.pixels.data();sd.SysMemPitch=((im.width+3)/4)*(im.codec==9?8:16);Ptr<ID3D11Texture2D>image;Ptr<ID3D11ShaderResourceView>view;ok(d->CreateTexture2D(&td,&sd,&image));D3D11_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=im.codec==9?DXGI_FORMAT_BC1_UNORM:DXGI_FORMAT_BC3_UNORM;srv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;srv.Texture2D.MipLevels=1;ok(d->CreateShaderResourceView(image.Get(),&srv,&view));textures_.push_back(view);colorTextures_.push_back(render_backend::Device(d).color_view(view.Get()));}
}
void CharacterRenderer::resize_target(ID3D11Device*d,unsigned width,unsigned height,bool hdr,bool reflections){
 if(!d||!width||!height||width>8192||height>8192)throw std::invalid_argument("Invalid render target size");
 if(color_&&width==width_&&height==height_&&targetHdr_==hdr&&bool(reflection_)==reflections)return;
 auto r=render_backend::Device(d).target(width,height,hdr);
 Ptr<ID3D11Texture2D> mask;Ptr<ID3D11RenderTargetView> maskTarget;Ptr<ID3D11ShaderResourceView> maskView;
 if(reflections){D3D11_TEXTURE2D_DESC td{};td.Width=width;td.Height=height;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;td.Format=DXGI_FORMAT_R8G8_UNORM;td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;ok(d->CreateTexture2D(&td,nullptr,&mask));ok(d->CreateRenderTargetView(mask.Get(),nullptr,&maskTarget));ok(d->CreateShaderResourceView(mask.Get(),nullptr,&maskView));}
 color_=std::move(r.color);depth_=std::move(r.depth);target_=std::move(r.target);view_=std::move(r.view);depthView_=std::move(r.depthView);readOnlyDepthView_=std::move(r.readOnlyDepthView);depthResource_=std::move(r.depthResource);reflection_=std::move(mask);reflectionTarget_=std::move(maskTarget);reflectionView_=std::move(maskView);width_=width;height_=height;targetHdr_=hdr;
}
void CharacterRenderer::preview_vertical_offset(float heightFraction){
 if(!std::isfinite(heightFraction))throw std::runtime_error("Invalid PC preview vertical offset");
 previewVerticalOffset_=std::clamp(heightFraction,-.25f,.25f);
}
void CharacterRenderer::prepare_lod(ID3D11Device* device,const CharacterModel& model){
 if(lodReady_)return;if(!device||sky_||model.vertices.size()!=vertexCount_||model.parts.size()!=parts_.size())throw std::invalid_argument("Static LOD model mismatch");
 auto mesh=render_lod::build(model);std::vector<std::array<Ptr<ID3D11Buffer>,2>> buffers(mesh.parts.size());
 for(size_t i=0;i<mesh.parts.size();++i)for(unsigned level=0;level<2;++level){const auto&indices=level?mesh.parts[i].coarse.indices:mesh.parts[i].mid.indices;if(indices.empty())continue;D3D11_BUFFER_DESC desc{};desc.ByteWidth=UINT(indices.size()*sizeof(uint32_t));desc.BindFlags=D3D11_BIND_INDEX_BUFFER;desc.Usage=D3D11_USAGE_IMMUTABLE;D3D11_SUBRESOURCE_DATA data{indices.data(),0,0};ok(device->CreateBuffer(&desc,&data,&buffers[i][level]));}
 lod_=std::move(mesh);lodIndices_=std::move(buffers);lodLevels_.resize(lod_.parts.size());lodReady_=true;
}
void CharacterRenderer::update_vertices(ID3D11DeviceContext*c,std::span<const ModelVertex> v){
 if(v.size()!=vertexCount_)throw std::runtime_error("Character vertex count changed");
 // A previously static model becoming animated must never use stale simplification.
 if(lodReady_){lod_={};lodIndices_.clear();lodLevels_.clear();lodReady_=false;}
 opaqueShadow_=std::all_of(v.begin(),v.end(),[](const ModelVertex&vertex){return vertex.lit>=.5f;});
 c->UpdateSubresource(vertices_.Get(),0,nullptr,v.data(),0,0);
}
void CharacterRenderer::render(ID3D11DeviceContext*c,float yaw,bool overview,const WorldView* camera,CharacterRenderer* surface,const std::array<float,3>* origin,std::span<const DynamicPointLight> lights,const OriginalColorMaskDraw* colorMask,const shadows::Renderer* shadow,const EnvironmentLight* environment,float opacity,CharacterPass pass,const SkyFrame* skyFrame){
 if(!std::isfinite(opacity)||opacity<0||opacity>1||((sky_||!surface)&&opacity!=1))throw std::invalid_argument("Invalid actor opacity");
 if(skyFrame){if(!sky_)throw std::invalid_argument("Sky frame on ordinary model");for(auto values:{skyFrame->position,skyFrame->degrees,skyFrame->color,skyFrame->fogColor})for(auto v:values)if(!std::isfinite(v))throw std::invalid_argument("Nonfinite original sky frame");if(!std::isfinite(skyFrame->fog)||!std::isfinite(skyFrame->cloudU))throw std::invalid_argument("Nonfinite sky phase");}
 if(pass==CharacterPass::alpha&&!surface)throw std::invalid_argument("Alpha pass needs an existing world surface");
 if(opacity==0||(pass==CharacterPass::alpha&&alphaParts_.empty()))return;
 validate_dynamic_lights(lights); // Reject the entire call before GPU state/target changes.
 if(colorMask)colorMask->validate();
 if(environment){environment->validate();if(!camera||sky_)throw std::invalid_argument("Environment requires world actor");}
 if(sky_&&(!camera||!surface||origin||overview||yaw!=0||!lights.empty()))throw std::invalid_argument("Sky requires a world camera and existing surface");
 if(camera&&(!std::isfinite(camera->aspect)||camera->aspect<=0||camera->aspect>32||!valid_vertical_fov(camera->verticalFov)))throw std::invalid_argument("Invalid camera aspect");
 Ptr<ID3D11Device> device;c->GetDevice(&device);render_backend::Device backend(device.Get());auto options=backend.options();const bool linearTarget=(surface?surface:this)->targetHdr_;options.linearColor=options.linearColor||linearTarget;
 ID3D11Buffer* environmentBinding=nullptr;
 if(environment){if(!environmentBuffer_){D3D11_BUFFER_DESC desc{};desc.ByteWidth=96;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;ok(device->CreateBuffer(&desc,nullptr,&environmentBuffer_));}
  std::array<std::array<float,4>,6> values{};unsigned index=0;for(auto v:{environment->front,environment->back,environment->axis,environment->direct,environment->direction,environment->scale}){std::copy(v.begin(),v.end(),values[index++].begin());}values[0][3]=1;c->UpdateSubresource(environmentBuffer_.Get(),0,nullptr,values.data(),0,0);environmentBinding=environmentBuffer_.Get();}
 c->PSSetConstantBuffers(2,1,&environmentBinding);
 if(samplerAnisotropy_!=options.anisotropy){sampler_=backend.sampler(options.anisotropy);samplerAnisotropy_=options.anisotropy;}
 if(!textureOptionsReady_||textureOptions_.mipmaps!=options.mipmaps||textureOptions_.linearColor!=options.linearColor){
  mipTextures_.clear();mipTextures_.resize(textures_.size());uint64_t budget=0;
  if(options.mipmaps)for(const auto& part:parts_)for(auto index:{part.texture,part.flags?part.flags-1:noMaterialTexture}){
   if(index>=textures_.size()||mipTextures_[index])continue;
   Ptr<ID3D11Resource> resource;textures_[index]->GetResource(&resource);Ptr<ID3D11Texture2D> image;ok(resource.As(&image));D3D11_TEXTURE2D_DESC desc{};image->GetDesc(&desc);
   const uint64_t bytes=uint64_t(desc.Width)*desc.Height*16/3;
   if(bytes>268435456-budget)continue; // Per-model additional color mip budget 256 MiB.
   try{mipTextures_[index]=backend.mip_chain(c,options.linearColor?colorTextures_[index].Get():textures_[index].Get(),options.linearColor);budget+=bytes;}catch(const std::exception& e){std::clog<<"color_mip_fallback "<<e.what()<<'\n';}
  }
  textureOptions_=options;textureOptionsReady_=true;
 }
 using namespace DirectX;ID3D11ShaderResourceView*nil=nullptr;c->PSSetShaderResources(0,1,&nil);
 auto destination=surface?surface:this;
 if(shadow&&camera&&!sky_)shadow->bind(c);else shadows::Renderer::unbind(c);
 if(!surface){float clear[4]={};c->ClearRenderTargetView(target_.Get(),clear);if(reflectionTarget_)c->ClearRenderTargetView(reflectionTarget_.Get(),clear);c->ClearDepthStencilView(depthView_.Get(),D3D11_CLEAR_DEPTH,0,0);}
 ID3D11RenderTargetView* targets[]={destination->target_.Get(),destination->reflectionTarget_.Get()};c->OMSetRenderTargets(opacity<1||sky_?1:2,targets,destination->depthView_.Get());c->OMSetDepthStencilState(opacity<1?fadeDepth_.Get():depthState_.Get(),0);c->OMSetBlendState(opacity<1?fadeBlend_.Get():nullptr,nullptr,0xffffffff);
 D3D11_VIEWPORT vp{0,0,float(destination->width_),float(destination->height_),0,1};c->RSSetViewports(1,&vp);c->RSSetState(raster_.Get());
 const auto&frameBounds=overview&&hasOverviewBounds_?overviewBounds_:bounds_;
 float center[3];for(int i=0;i<3;++i)center[i]=(frameBounds[i]+frameBounds[i+3])*.5f;
 float height=bounds_[4]-bounds_[1],radius=std::max(height,bounds_[3]-bounds_[0]);
 // Move only the displayed preview framing. Authored vertices, world cameras,
 // standing bounds, view distance and projection scale remain unchanged.
 auto world=XMMatrixTranslation(-center[0],-center[1]+(!overview&&!camera?height*previewVerticalOffset_:0.f),-center[2])*XMMatrixRotationY(yaw);
 auto view=XMMatrixLookAtLH(XMVectorSet(0,height*.04f,radius*1.65f,1),XMVectorZero(),XMVectorSet(0,1,0,0));
 auto projection=source_projection(.65f,440.f/280,radius*5,10);
 if(overview){float dx=frameBounds[3]-frameBounds[0],dy=frameBounds[4]-frameBounds[1],dz=frameBounds[5]-frameBounds[2];float sphere=std::max(1.f,std::sqrt(dx*dx+dy*dy+dz*dz)*.5f);view=XMMatrixLookAtLH(XMVectorSet(0,sphere*2.3f,sphere*2.8f,1),XMVectorZero(),XMVectorSet(0,1,0,0));projection=source_projection(.65f,float(destination->width_)/destination->height_,sphere*16,std::max(.01f,sphere*.001f));}
 if(camera){
  for(auto x:camera->eye)if(!std::isfinite(x)||std::abs(x)>=1e7f)throw std::runtime_error("Invalid stage camera position");
  float length=0;for(auto x:camera->direction){if(!std::isfinite(x))throw std::runtime_error("Invalid stage camera direction");length+=x*x;}
  if(!std::isfinite(length)||length<.001f||camera->direction[0]*camera->direction[0]+camera->direction[2]*camera->direction[2]<.00001f)throw std::runtime_error("Degenerate stage camera");
  world=XMMatrixIdentity();
  if(origin){for(float v:*origin)if(!std::isfinite(v)||std::abs(v)>=1e7f)throw std::runtime_error("Invalid avatar position");world=XMMatrixRotationY(yaw)*XMMatrixTranslation((*origin)[0],(*origin)[1],(*origin)[2]);}
  if(skyFrame)world=XMMatrixRotationRollPitchYaw(XMConvertToRadians(skyFrame->degrees[0]),XMConvertToRadians(skyFrame->degrees[1]),XMConvertToRadians(skyFrame->degrees[2]))*XMMatrixTranslation(skyFrame->position[0],skyFrame->position[1],skyFrame->position[2]);
  auto eye=XMVectorSet(camera->eye[0],camera->eye[1],camera->eye[2],1);auto direction=XMVectorSet(camera->direction[0],camera->direction[1],camera->direction[2],0);
  view=XMMatrixLookToLH(eye,direction,XMVectorSet(0,1,0,0));projection=world_projection(camera->aspect,camera->verticalFov);
 }
 struct PointLight{XMFLOAT4 positionRadius,colorIntensity;};
 struct Constants{XMFLOAT4X4 wvp,world;XMFLOAT4 material,tint,lightCount;PointLight lights[maximum_dynamic_lights];XMFLOAT4 sourceP[8],restore,normalUV,extraUV,paletteUV,colorP36,colorP37,colorP38,colorMask;} data{};
 static_assert(sizeof(Constants)==688);
 const OriginalColorMaskState initialColors;
 const auto& colors=colorMask?colorMask->colors:initialColors;
 auto colorVector=[](const auto& v){return XMFLOAT4{v[0],v[1],v[2],v[3]};};
 data.colorP36=colorVector(colors.p36);data.colorP37=colorVector(colors.p37);data.colorP38=colorVector(colors.p38);
 XMStoreFloat4x4(&data.wvp,world*view*projection);XMStoreFloat4x4(&data.world,world);data.lightCount.x=float(lights.size());data.lightCount.y=options.linearColor&&!linearTarget?1.f:0.f;data.material.z=sky_?(skyFrame?2.f:1.f):0.f;
 for(size_t i=0;i<lights.size();++i){const auto&light=lights[i];XMFLOAT3 position{light.position[0],light.position[1],light.position[2]};
  // Overview/preview recenter authored geometry for presentation. Move the
  // lights with that same frame; real world cameras leave world lights fixed.
  if(!camera)XMStoreFloat3(&position,XMVector3TransformCoord(XMLoadFloat3(&position),world));
  data.lights[i]={{position.x,position.y,position.z,light.radius},{light.color[0],light.color[1],light.color[2],light.intensity}};
 }
 c->UpdateSubresource(constants_.Get(),0,nullptr,&data,0,0);
 auto cb=constants_.Get();c->VSSetConstantBuffers(0,1,&cb);c->PSSetConstantBuffers(0,1,&cb);c->VSSetShader(vs_.Get(),nullptr,0);c->PSSetShader(ps_.Get(),nullptr,0);auto samp=sampler_.Get();c->PSSetSamplers(0,1,&samp);c->IASetInputLayout(layout_.Get());
 auto vb=vertices_.Get();UINT stride=sizeof(ModelVertex),offset=0;c->IASetVertexBuffers(0,1,&vb,&stride,&offset);c->IASetIndexBuffer(indices_.Get(),DXGI_FORMAT_R32_UINT,0);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 std::vector<size_t> order=pass==CharacterPass::alpha?std::vector<size_t>{}:opaqueParts_;auto alpha=pass==CharacterPass::opaque?std::vector<size_t>{}:alphaParts_;
 if(camera&&!alpha.empty()){auto depth=[&](size_t index){const auto&p=partCenters_[index];XMFLOAT3 worldPoint;XMStoreFloat3(&worldPoint,XMVector3TransformCoord(XMVectorSet(p[0],p[1],p[2],1),world));return (worldPoint.x-camera->eye[0])*camera->direction[0]+(worldPoint.y-camera->eye[1])*camera->direction[1]+(worldPoint.z-camera->eye[2])*camera->direction[2];};std::stable_sort(alpha.begin(),alpha.end(),[&](size_t a,size_t b){return depth(a)>depth(b);});}
 order.insert(order.end(),alpha.begin(),alpha.end());bool lastBlend=opacity<1;if(pass!=CharacterPass::alpha)lodDrawn_=lodOriginal_=lodParts_=0;
 for(size_t i:order){const auto&p=parts_[i];const bool blend=opacity<1||p.surfaceAlpha;data.material.w=float(p.surfaceAlpha);if(blend!=lastBlend){c->OMSetRenderTargets(blend||sky_?1:2,targets,destination->depthView_.Get());c->OMSetDepthStencilState(blend?fadeDepth_.Get():depthState_.Get(),0);c->OMSetBlendState(blend?fadeBlend_.Get():nullptr,nullptr,0xffffffff);lastBlend=blend;}const auto&o=p.original;auto rules=materialRules_[i];auto reflection=render_reflections::material(options.reflectionMaterials,p.materialShader);if(o.present&&o.reflectionSlot<o.textures.size()&&!std::any_of(options.reflectionMaterials.rules.begin(),options.reflectionMaterials.rules.begin()+options.reflectionMaterials.count,[&](const auto&r){return r.shader==p.materialShader;}))reflection={.35f,.35f};data.lightCount.z=(!sky_&&!blend)?reflection[0]:0.f;data.lightCount.w=reflection[1];data.material.x=p.flags?1.f:0.f;const bool floor=p.floorBlend<textures_.size()&&p.floorNormal<textures_.size()&&p.materialShader==0x130003;if(floor)rules|=1;data.material.y=(p.materialShader==0x120000||floor)?2.f:0.f;data.tint={p.tint[0],p.tint[1],p.tint[2],1};
  for(unsigned j=0;j<8;++j){const auto&v=o.parameters[j];data.sourceP[j]=j<o.parameterCount?XMFLOAT4{v[0],v[1],v[2],v[3]}:XMFLOAT4{};}
  data.restore={float(bool(rules&1)),float(bool(rules&2)),float(p.materialShader==0x100000||p.materialShader==0x120000||floor),float(bool(rules&4))};
  data.colorMask={float(bool((rules&8)&&colorMask&&colorMask->c467x.has_value())),colorMask?colorMask->c467x.value_or(0.f):0.f,float(floor),opacity};
  auto uv=[&](uint32_t slot){XMFLOAT4 result{1,1,0,0};if(slot<o.textures.size()){float v[4];for(unsigned j=0;j<4;++j){uint32_t bits=0;for(unsigned k=0;k<4;++k)bits=(bits<<8)|o.textures[slot].raw[8+j*4+k];std::memcpy(&v[j],&bits,4);}result={v[0],v[1],v[2],v[3]};}return result;};
  data.normalUV=uv(o.normalSlot);data.extraUV=uv(o.extraNormalSlot);data.paletteUV=uv(o.paletteSlot);
  if(skyFrame){if((p.materialShader!=0x100064&&p.materialShader!=0x100067)||o.textures.size()!=(p.materialShader==0x100067?3u:2u))throw std::invalid_argument("Unsupported layered original sky");data.sourceP[0]={skyFrame->color[0],skyFrame->color[1],skyFrame->color[2],1};data.sourceP[1]={skyFrame->fogColor[0],skyFrame->fogColor[1],skyFrame->fogColor[2],skyFrame->fog};data.sourceP[2]={skyFrame->cloudU,p.materialShader==0x100067?1.f:0.f,0,0};data.normalUV=uv(0);data.extraUV=uv(1);data.paletteUV=uv(2);}

  auto image=[&](uint32_t slot)->ID3D11ShaderResourceView*{if(slot>=o.textures.size()||o.textures[slot].image>=textures_.size())return nullptr;return textures_[o.textures[slot].image].Get();};
  c->UpdateSubresource(constants_.Get(),0,nullptr,&data,0,0);auto colorImage=[&](uint32_t index){return mipTextures_[index]?mipTextures_[index].Get():options.linearColor?colorTextures_[index].Get():textures_[index].Get();};ID3D11ShaderResourceView* tex[]={colorImage(p.texture),p.flags?colorImage(p.flags-1):nullptr,floor?textures_[p.floorNormal].Get():(rules&1)?image(o.normalSlot):nullptr,(rules&2)?image(o.extraNormalSlot):nullptr,floor?colorImage(p.floorBlend):nullptr};if(skyFrame){for(unsigned j=0;j<o.textures.size();++j){if(o.textures[j].image>=textures_.size())throw std::invalid_argument("Missing original sky image");tex[j]=colorImage(o.textures[j].image);}}c->PSSetShaderResources(0,4,tex);c->PSSetShaderResources(5,1,tex+4);
  unsigned selected=0;if(lodReady_&&options.lod&&camera&&!origin&&!blend&&i<lod_.parts.size()){
   const auto&part=lod_.parts[i];float length=0,depth=0;for(unsigned k=0;k<3;++k){length+=camera->direction[k]*camera->direction[k];depth+=(part.center[k]-camera->eye[k])*camera->direction[k];}depth/=std::sqrt(length);
   selected=render_lod::select(part,depth,camera->verticalFov,destination->height_,lodLevels_[i],1.5f,camera->aspect);lodLevels_[i]=selected;
  }else if(i<lodLevels_.size())lodLevels_[i]=0;
  const auto* variant=selected?(selected==1?&lod_.parts[i].mid:&lod_.parts[i].coarse):nullptr;
  c->IASetIndexBuffer(selected?lodIndices_[i][selected-1].Get():indices_.Get(),DXGI_FORMAT_R32_UINT,0);
  c->DrawIndexed(variant?UINT(variant->indices.size()):p.count,variant?0:p.first,0);
  lodDrawn_+=(variant?variant->indices.size():p.count)/3;lodOriginal_+=p.count/3;lodParts_+=selected!=0;
 }
 ID3D11ShaderResourceView* cleared[4]={};c->PSSetShaderResources(0,4,cleared);c->PSSetShaderResources(5,1,cleared);c->OMSetRenderTargets(0,nullptr,nullptr);
}
}
