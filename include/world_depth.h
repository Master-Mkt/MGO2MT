#pragma once
#include <DirectXMath.h>
#include "source_coordinates.h"
#include "camera_projection.h"

namespace mgo2mt {
// Native floating-point reverse Z. All passes sharing world depth must use
// this projection, a depth clear of 0, and GREATER / GREATER_EQUAL tests.
// Lens changes preserve world coordinates and physical clipping distances.
inline constexpr float world_near_plane = 10.f;
inline constexpr float world_far_plane = 500000.f;
inline DirectX::XMMATRIX source_projection(float fov,float aspect,float farPlane,float nearPlane) {
    return DirectX::XMMatrixPerspectiveFovLH(fov,aspect,farPlane,nearPlane)*DirectX::XMMatrixScaling(source_screen_x,1,1);
}
inline DirectX::XMMATRIX world_projection(float aspect,float verticalFov=default_vertical_fov) {
    return source_projection(verticalFov, aspect, world_far_plane, world_near_plane);
}
}
