#pragma once
#include <array>
#include <memory>
#include <cstddef>
#include <d3d11.h>

namespace mgo2mt::effects {
// Optional native effects. This is not a recovered original RSX pipeline.
struct Settings {
    bool ssao=false,ssr=false,bloom=false,hdr=false,fxaa=false;
    float exposure=1.f;
    float aoRadius=1000.f,aoStrength=.8f,aoBias=15.f;
    float ssrDistance=20000.f,ssrThickness=120.f,ssrStrength=.5f;
    float bloomThreshold=.8f,bloomStrength=.2f;
    bool operator==(const Settings&)const=default;
};
bool valid(const Settings&) noexcept;
// Row-major projection and inverse projection, not world/view matrices.
// D3D reverse Z: clear 0, near 1. Includes the source camera's X reflection.
struct Camera {std::array<float,16> projection{},inverseProjection{};};
struct Inputs {
    ID3D11ShaderResourceView* color=nullptr;
    ID3D11ShaderResourceView* depth=nullptr;
    // Linear data: R=reflectivity [0,1], G=roughness [0,1]. Null disables SSR.
    ID3D11ShaderResourceView* reflectionMask=nullptr;
    Camera camera{};
};
class Renderer {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    explicit Renderer(ID3D11Device*);
    ~Renderer();
    Renderer(const Renderer&)=delete;
    Renderer& operator=(const Renderer&)=delete;
    // Borrowed output remains valid until the next effects call or resize.
    // All effects OFF returns the exact source pointer with no GPU work.
    ID3D11ShaderResourceView* opaque(ID3D11DeviceContext*,const Inputs&,const Settings&);
    // Apply after transparent 3D, before UI. linearInput requests one sRGB
    // transfer on the final output; HDR additionally applies exposure/ACES.
    ID3D11ShaderResourceView* finish(ID3D11DeviceContext*,ID3D11ShaderResourceView*,const Settings&,bool linearInput=false);
    // Useful for applying opaque effects back to the scene before alpha draws.
    // Source and destination may have different compatible color formats.
    void copy_to(ID3D11DeviceContext*,ID3D11ShaderResourceView*,ID3D11RenderTargetView*);
    unsigned width()const noexcept;
    unsigned height()const noexcept;
    size_t allocated_bytes()const noexcept;
};
}
