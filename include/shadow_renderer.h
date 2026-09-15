#pragma once
#include "shadow_cascade.h"
#include "render_device.h"
#include <span>
#include <string>
namespace mgo2win {class CharacterRenderer;}
namespace mgo2win::shadows {
struct Caster {CharacterRenderer* renderer=nullptr;float yaw=0;std::array<float,3> origin{};};
struct Statistics {unsigned cascades=0,resolution=0;uint64_t allocatedBytes=0,drawCalls=0,triangles=0;bool active=false,reduced=false;};
class Renderer {
 template<class T>using Ptr=render_backend::Handle<T>;
 Ptr<ID3D11Texture2D> depth_;Ptr<ID3D11ShaderResourceView> view_;std::array<Ptr<ID3D11DepthStencilView>,6> targets_;
 Ptr<ID3D11VertexShader> vs_;Ptr<ID3D11PixelShader> ps_;Ptr<ID3D11InputLayout> layout_;
 Ptr<ID3D11Buffer> casterBuffer_,receiverBuffer_;Ptr<ID3D11RasterizerState> raster_;
 Ptr<ID3D11DepthStencilState> depthState_;Ptr<ID3D11SamplerState> compare_,alpha_;
 Settings settings_{};Allocation allocation_{};Plan plan_{};Statistics stats_{};
 bool configured_=false;std::string status_;
public:
 bool configure(ID3D11Device*,const Settings&);
 bool render(ID3D11DeviceContext*,const WorldView&,std::array<float,3> direction,std::array<float,3> sun,const std::array<float,6>& sceneBounds,std::span<const Caster>);
 void bind(ID3D11DeviceContext*)const;
 static void unbind(ID3D11DeviceContext*);
 const Statistics& statistics()const{return stats_;}
 const std::string& status()const{return status_;}
 const Plan& current_plan()const{return plan_;}
 ID3D11ShaderResourceView* depth_view()const{return view_.Get();}
};
const char* receiver_shader();
}
