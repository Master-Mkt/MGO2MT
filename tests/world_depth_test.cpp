#include "world_depth.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using namespace DirectX;

// Independent physical camera contract, not aliases of the implementation.
constexpr double Near = 10.0;
constexpr double Far = 500000.0;
constexpr double VerticalFov = 1.0;

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

struct Projected {
    XMFLOAT4 clip;
    float x, y, depth;
};

Projected project(const XMMATRIX& matrix, float x, float y, float z) {
    XMFLOAT4 clip;
    XMStoreFloat4(&clip, XMVector4Transform(XMVectorSet(x, y, z, 1.f), matrix));
    require(std::isfinite(clip.w) && clip.w > 0.f, "Positive camera distance must have positive clip W");
    // Perspective division and the stored depth are float, as for D32_FLOAT.
    return {clip, clip.x / clip.w, clip.y / clip.w, clip.z / clip.w};
}

// Perspective depth is affine in reciprocal distance. Interpolate between
// its endpoint values in double; do not read or copy DirectX matrix entries.
double reverseOracle(double distance) {
    return (1.0 / distance - 1.0 / Far) / (1.0 / Near - 1.0 / Far);
}

double forwardOracle(double distance) {
    return (1.0 / Near - 1.0 / distance) / (1.0 / Near - 1.0 / Far);
}

void requireOracle(float distance, float actual) {
    const double expected = reverseOracle(distance);
    // Bound float coefficient, multiply/add and division rounding using the
    // near/distance scale, including cancellation close to the far plane.
    const double bound = 4.0 * std::numeric_limits<float>::epsilon() * Near / distance;
    require(std::isfinite(actual) && std::abs(double(actual) - expected) <= bound,
            "Reverse projection differs from the independent double distance oracle");
}

void clippingAndMonotonicity() {
    require(mgo2win::world_near_plane == Near && mgo2win::world_far_plane == Far,
            "Physical near/far clipping distances changed");
    const auto matrix = mgo2win::world_projection(16.f / 9.f);
    const auto near = project(matrix, 0, 0, float(Near));
    const auto far = project(matrix, 0, 0, float(Far));
    require(std::abs(near.depth - 1.f) < 1e-6f, "Near plane must map to reverse depth 1");
    require(std::abs(far.depth) < 1e-10f, "Far plane must map to reverse depth 0");
    require(project(matrix, 0, 0, 5.f).clip.z > 5.f,
            "A point before the physical near plane must fail Z <= W clipping");
    require(project(matrix, 0, 0, 550000.f).clip.z < 0.f,
            "A point beyond the physical far plane must fail Z >= 0 clipping");

    constexpr std::array distances{10.f, 10.25f, 20.f, 100.f, 1000.f,
        5000.f, 50000.f, 75000.f, 100000.f, 125000.f, 150000.f,
        175000.f, 200000.f, 400000.f, 499999.f, 500000.f};
    float previous = std::numeric_limits<float>::infinity();
    for (float distance : distances) {
        const auto p = project(matrix, 0, 0, distance);
        requireOracle(distance, p.depth);
        require(p.depth >= 0.f && p.depth <= 1.f, "An in-frustum point left the depth range");
        require(p.depth < previous, "Depth must strictly decrease as camera distance increases");
        require(p.clip.w == distance, "Projection changed the physical camera distance");
        previous = p.depth;
    }
}

void aspectAndFieldOfView() {
    constexpr std::array aspects{.5625f, 1.f, 4.f / 3.f, 16.f / 9.f, 2.4f};
    constexpr std::array distances{10.f, 1000.f, 50000.f, 200000.f, 500000.f};
    const auto square = mgo2win::world_projection(1.f);
    const double tangent = std::tan(VerticalFov / 2.0);
    for (float aspect : aspects) {
        const auto matrix = mgo2win::world_projection(aspect);
        const auto legacy = XMMatrixPerspectiveFovLH(float(VerticalFov), aspect, float(Near), float(Far));
        for (float distance : distances) {
            const float x = float(.6 * distance * tangent * aspect);
            const float y = float(-.7 * distance * tangent);
            const auto p = project(matrix, x, y, distance);
            const auto previous = project(legacy, x, y, distance);
            requireOracle(distance, p.depth);
            require(p.depth == project(square, 0, 0, distance).depth,
                    "Aspect or off-axis position changed depth at a fixed camera distance");
            require(std::abs(p.x + previous.x) < 1e-6f && std::abs(p.y - previous.y) < 1e-6f,
                    "Source projection must flip only horizontal screen position, keeping visible size");
            const double expectedX = -double(x) / (distance * tangent * aspect);
            const double expectedY = double(y) / (distance * tangent);
            require(std::abs(p.x - expectedX) < 1e-6 && std::abs(p.y - expectedY) < 1e-6,
                    "Projection differs from the independent one-radian FOV oracle");

            const auto edge = project(matrix, 0, float(distance * tangent), distance);
            require(std::abs(edge.y - 1.f) < 1e-6f, "Vertical frustum edge or FOV changed");
            const auto outside = project(matrix, 0, float(1.01 * distance * tangent), distance);
            require(outside.clip.y > outside.clip.w, "A point above the frustum must remain outside");
        }
    }
}

std::uint32_t quantizeD24(double depth) {
    constexpr double maximum = (1u << 24) - 1u;
    return std::uint32_t(std::floor(std::clamp(depth, 0.0, 1.0) * maximum + .5));
}

void distantAdjacentSurfaces() {
    const auto matrix = mgo2win::world_projection(16.f / 9.f);
    constexpr std::array distances{50000.f, 75000.f, 100000.f, 125000.f,
                                   150000.f, 175000.f, 200000.f};
    for (float distance : distances) {
        constexpr float separation = .5f;
        const float behind = distance + separation;
        require(behind - distance == separation, "Fixture lost its world-space separation");
        // Even exact double forward depth loses these two surfaces in D24.
        // This isolates storage precision from legacy matrix-rounding errors.
        const auto oldNear = quantizeD24(forwardOracle(distance));
        const auto oldFar = quantizeD24(forwardOracle(behind));
        require(oldNear == oldFar, "Fixture must expose a legacy D24 quantization collision");

        const float nearDepth = project(matrix, 0, 0, distance).depth;
        const float farDepth = project(matrix, 0, 0, behind).depth;
        requireOracle(distance, nearDepth);
        requireOracle(behind, farDepth);
        require(nearDepth > farDepth && farDepth > 0.f,
                "D32 reverse depth failed to separate distant surfaces by physical distance");
        const double expectedGap = reverseOracle(distance) - reverseOracle(behind);
        const double measuredGap = double(nearDepth) - farDepth;
        require(std::abs(measuredGap - expectedGap) <= expectedGap * .2,
                "The separated depths no longer represent the actual half-unit distance");
        const float ulp = std::nextafter(nearDepth, std::numeric_limits<float>::infinity()) - nearDepth;
        require(measuredGap >= 8.0 * ulp, "Separation has insufficient float precision margin");

        // Scalar GREATER depth tests show why both submission orders retain
        // the nearer surface. This is a numerical test, not a GPU capture.
        for (bool nearFirst : {false, true}) {
            float stored = 0.f;
            int winner = -1;
            for (int step = 0; step < 2; ++step) {
                const bool nearer = (step == 0) == nearFirst;
                const float depth = nearer ? nearDepth : farDepth;
                if (depth > stored) { stored = depth; winner = nearer ? 1 : 0; }
            }
            require(winner == 1 && stored == nearDepth,
                    "Reverse GREATER comparison did not preserve the nearer surface");
        }
    }
}
}

int main() {
    try {
        clippingAndMonotonicity();
        aspectAndFieldOfView();
        distantAdjacentSurfaces();
        std::cout << "World reverse depth: clipping, monotonicity, unchanged FOV/aspect, double oracle, "
                     "and seven distant half-unit surfaces separated beyond D24 precision passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
