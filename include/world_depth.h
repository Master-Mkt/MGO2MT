#pragma once
#include <DirectXMath.h>
#include "source_coordinates.h"

namespace mgo2win {
// Native floating-point reverse Z. All passes sharing world depth must use
// this projection, a depth clear of 0, and GREATER / GREATER_EQUAL tests.
// World coordinates, field of view and physical clipping distances are unchanged.
inline constexpr float world_near_plane = 10.f;
inline constexpr float world_far_plane = 500000.f;
inline DirectX::XMMATRIX source_projection(float fov,float aspect,float farPlane,float nearPlane) {
    return DirectX::XMMatrixPerspectiveFovLH(fov,aspect,farPlane,nearPlane)*DirectX::XMMatrixScaling(source_screen_x,1,1);
}
inline DirectX::XMMATRIX world_projection(float aspect) {
    return source_projection(1.f, aspect, world_far_plane, world_near_plane);
}
}
