#include "stage_surface_layers.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace mgo2mt::stage {
namespace {
using D3 = std::array<double, 3>;
using D2 = std::array<double, 2>;
constexpr double kNormalDotMinimum = 1.0 - 1e-8;
// Coordinates are float: accept the representable 0.05f boundary itself.
constexpr double kPlaneDistance = static_cast<double>(0.05f);
// Projection can hide a separation of planeDistance / |dominant normal| on
// its dropped axis. The dominant component is >= 1/sqrt(3), so broad-phase
// bounds need this larger tolerance; the exact plane test remains unchanged.
constexpr double kBroadPhaseDistance = kPlaneDistance * 1.7320508075688772935;
constexpr double kLayerDistance = 0.5;
constexpr std::size_t kMaxLayer = 16;
constexpr std::size_t kMixedPart = std::numeric_limits<std::size_t>::max();

D3 sub(D3 a, D3 b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
double dot(D3 a, D3 b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
D3 cross(D3 a, D3 b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]};
}
double cross2(D2 a, D2 b) { return a[0] * b[1] - a[1] * b[0]; }
D2 sub2(D2 a, D2 b) { return {a[0] - b[0], a[1] - b[1]}; }

struct Box {
    D3 lo{}, hi{};
};
void include(Box& target, const Box& other) {
    for (std::size_t axis = 0; axis < 3; ++axis) {
        target.lo[axis] = std::min(target.lo[axis], other.lo[axis]);
        target.hi[axis] = std::max(target.hi[axis], other.hi[axis]);
    }
}
bool near(const Box& a, const Box& b) {
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (a.lo[axis] > b.hi[axis] + kBroadPhaseDistance ||
            b.lo[axis] > a.hi[axis] + kBroadPhaseDistance) return false;
    }
    return true;
}

struct Triangle {
    std::array<D3, 3> p{};
    D3 normal{};
    Box box{};
    std::size_t part = 0;
    bool degenerate = false;
};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void validate(const CharacterModel& model) {
    require(model.vertices.size() <= std::numeric_limits<std::uint32_t>::max(),
            "Stage surface vertex index extent");
    require(model.indices.size() % 3 == 0, "Stage surface triangle index count");
    for (float value : model.bounds) {
        require(std::isfinite(value), "Stage surface non-finite bounds");
    }
    for (std::size_t axis = 0; axis < 3; ++axis) {
        require(model.bounds[axis] <= model.bounds[axis + 3], "Stage surface reversed bounds");
    }
    if (model.hasOverviewBounds) {
        for (float value : model.overviewBounds) {
            require(std::isfinite(value), "Stage surface non-finite overview bounds");
        }
        for (std::size_t axis = 0; axis < 3; ++axis) {
            require(model.overviewBounds[axis] <= model.overviewBounds[axis + 3],
                    "Stage surface reversed overview bounds");
        }
    }
    for (const auto& v : model.vertices) {
        for (float value : {v.x, v.y, v.z, v.nx, v.ny, v.nz, v.u, v.v, v.u1, v.v1,
                            v.lr, v.lg, v.lb, v.lit, v.ar, v.ag, v.ab, v.aa}) {
            require(std::isfinite(value), "Stage surface non-finite vertex attribute");
        }
    }
    for (std::uint32_t index : model.indices) {
        require(index < model.vertices.size(), "Stage surface invalid vertex index");
    }
    std::size_t end = 0;
    for (const auto& part : model.parts) {
        require(part.first == end && part.count != 0 && part.count % 3 == 0 &&
                    part.count <= model.indices.size() - end,
                "Stage surface invalid or overlapping part range");
        require(part.texture < model.textures.size(), "Stage surface invalid texture index");
        for (float tint : part.tint) {
            require(std::isfinite(tint), "Stage surface non-finite material tint");
        }
        end += part.count;
    }
    require(end == model.indices.size(), "Stage surface uncovered index range");
}

class Bvh {
public:
    struct Node {
        Box box{};
        std::size_t first = 0, count = 0, left = 0, right = 0;
        std::size_t minimumTriangle = 0, uniformPart = kMixedPart;
    };
    explicit Bvh(const std::vector<Triangle>& triangles) : triangles_(triangles) {
        order.reserve(triangles.size());
        for (std::size_t i = 0; i < triangles.size(); ++i) {
            if (!triangles[i].degenerate) order.push_back(i);
        }
        if (!order.empty()) {
            nodes.reserve(order.size() / 2 + 1);
            build(0, order.size());
        }
    }
    std::vector<std::size_t> order;
    std::vector<Node> nodes;

private:
    std::size_t build(std::size_t first, std::size_t count) {
        const std::size_t id = nodes.size();
        nodes.push_back({});
        Node node;
        node.first = first;
        node.count = count;
        node.minimumTriangle = order[first];
        node.uniformPart = triangles_[order[first]].part;
        node.box = triangles_[order[first]].box;
        for (std::size_t i = first + 1; i < first + count; ++i) {
            const auto& triangle = triangles_[order[i]];
            include(node.box, triangle.box);
            node.minimumTriangle = std::min(node.minimumTriangle, order[i]);
            if (node.uniformPart != triangle.part) node.uniformPart = kMixedPart;
        }
        if (count > 8) {
            std::size_t axis = 0;
            for (std::size_t i = 1; i < 3; ++i) {
                if (node.box.hi[i] - node.box.lo[i] > node.box.hi[axis] - node.box.lo[axis]) axis = i;
            }
            const std::size_t middle = first + count / 2;
            auto center = [&](std::size_t index) {
                const auto& b = triangles_[index].box;
                return (b.lo[axis] + b.hi[axis]) * 0.5;
            };
            std::nth_element(order.begin() + static_cast<std::ptrdiff_t>(first),
                             order.begin() + static_cast<std::ptrdiff_t>(middle),
                             order.begin() + static_cast<std::ptrdiff_t>(first + count),
                             [&](std::size_t a, std::size_t b) {
                                 const double ca = center(a), cb = center(b);
                                 return ca == cb ? a < b : ca < cb;
                             });
            node.count = 0;
            node.left = build(first, middle - first);
            node.right = build(middle, first + count - middle);
        }
        nodes[id] = node;
        return id;
    }
    const std::vector<Triangle>& triangles_;
};

bool positiveProjectedOverlap(const Triangle& a, const Triangle& b) {
    // Drop the largest-normal axis, and use a local origin to avoid cancellation
    // in translated stages. Triangle/triangle clipping produces at most 6 sides.
    std::size_t drop = 0;
    for (std::size_t axis = 1; axis < 3; ++axis) {
        if (std::abs(a.normal[axis]) > std::abs(a.normal[drop])) drop = axis;
    }
    const std::size_t x = (drop + 1) % 3, y = (drop + 2) % 3;
    auto project = [&](D3 p) -> D2 { return {p[x] - a.p[0][x], p[y] - a.p[0][y]}; };
    std::array<D2, 12> polygon{};
    std::array<D2, 3> clipping{};
    for (std::size_t i = 0; i < 3; ++i) {
        polygon[i] = project(a.p[i]);
        clipping[i] = project(b.p[i]);
    }
    if (cross2(sub2(clipping[1], clipping[0]), sub2(clipping[2], clipping[0])) < 0.0) {
        std::swap(clipping[1], clipping[2]);
    }
    double edgeSquared = 0.0;
    for (std::size_t i = 0; i < 3; ++i) {
        for (D2 edge : {sub2(polygon[(i + 1) % 3], polygon[i]),
                        sub2(clipping[(i + 1) % 3], clipping[i])}) {
            edgeSquared = std::max(edgeSquared, edge[0] * edge[0] + edge[1] * edge[1]);
        }
    }
    std::size_t count = 3;
    for (std::size_t edge = 0; edge < 3 && count != 0; ++edge) {
        const D2 start = clipping[edge], direction = sub2(clipping[(edge + 1) % 3], start);
        auto signedDistance = [&](D2 p) { return cross2(direction, sub2(p, start)); };
        std::array<D2, 12> next{};
        std::size_t nextCount = 0;
        auto append = [&](D2 p) {
            require(nextCount < next.size(), "Stage surface clipping extent");
            next[nextCount++] = p;
        };
        D2 previous = polygon[count - 1];
        double previousDistance = signedDistance(previous);
        for (std::size_t i = 0; i < count; ++i) {
            const D2 current = polygon[i];
            const double currentDistance = signedDistance(current);
            const bool previousInside = previousDistance >= 0.0, currentInside = currentDistance >= 0.0;
            if (previousInside != currentInside) {
                const double t = previousDistance / (previousDistance - currentDistance);
                append({previous[0] + t * (current[0] - previous[0]),
                        previous[1] + t * (current[1] - previous[1])});
            }
            if (currentInside) append(current);
            previous = current;
            previousDistance = currentDistance;
        }
        polygon = next;
        count = nextCount;
    }
    if (count < 3) return false;
    double twiceArea = 0.0;
    for (std::size_t i = 1; i + 1 < count; ++i) {
        twiceArea += cross2(sub2(polygon[i], polygon[0]), sub2(polygon[i + 1], polygon[0]));
    }
    const double areaEpsilon = std::max(1e-10, 64.0 * std::numeric_limits<double>::epsilon() * edgeSquared);
    return std::abs(twiceArea) > 2.0 * areaEpsilon;
}

bool overlaps(const Triangle& a, const Triangle& b) {
    if (dot(a.normal, b.normal) < kNormalDotMinimum) return false;
    for (D3 p : a.p) {
        if (std::abs(dot(sub(p, b.p[0]), b.normal)) > kPlaneDistance) return false;
    }
    for (D3 p : b.p) {
        if (std::abs(dot(sub(p, a.p[0]), a.normal)) > kPlaneDistance) return false;
    }
    return positiveProjectedOverlap(a, b);
}
}

SurfaceLayerStats separate_stage_surfaces(CharacterModel& model) {
    validate(model);
    SurfaceLayerStats stats;
    stats.triangles = model.indices.size() / 3;
    if (stats.triangles == 0) return stats;
    std::vector<Triangle> triangles;
    triangles.reserve(stats.triangles);
    for (std::size_t part = 0; part < model.parts.size(); ++part) {
        const auto& range = model.parts[part];
        for (std::size_t at = range.first; at < std::size_t(range.first) + range.count; at += 3) {
            Triangle t;
            t.part = part;
            for (std::size_t corner = 0; corner < 3; ++corner) {
                const auto& v = model.vertices[model.indices[at + corner]];
                t.p[corner] = {v.x, v.y, v.z};
            }
            t.box = {t.p[0], t.p[0]};
            for (std::size_t corner = 1; corner < 3; ++corner) include(t.box, {t.p[corner], t.p[corner]});
            t.normal = cross(sub(t.p[1], t.p[0]), sub(t.p[2], t.p[0]));
            const double length = std::sqrt(dot(t.normal, t.normal));
            require(std::isfinite(length), "Stage surface non-finite geometric normal");
            t.degenerate = length <= 1e-12;
            if (t.degenerate) ++stats.degenerateTriangles;
            else for (double& value : t.normal) value /= length;
            triangles.push_back(t);
        }
    }
    const Bvh bvh(triangles);
    std::vector<std::size_t> layers(stats.triangles, 0), stack;
    stack.reserve(64);
    // Genuine all-overlap geometry has quadratic output even with a BVH. Bound
    // that adversarial case, and fall back atomically instead of stalling load.
    const std::size_t pairBudget = std::max<std::size_t>(1'000'000,
        std::min<std::size_t>(stats.triangles, 250'000) * 64);
    for (std::size_t i = 0; i < triangles.size(); ++i) {
        const auto& t = triangles[i];
        if (t.degenerate || bvh.nodes.empty()) continue;
        stack.clear();
        stack.push_back(0);
        while (!stack.empty()) {
            const auto& node = bvh.nodes[stack.back()];
            stack.pop_back();
            if (node.minimumTriangle >= i || node.uniformPart == t.part || !near(t.box, node.box)) continue;
            if (node.count == 0) {
                stack.push_back(node.right);
                stack.push_back(node.left);
                continue;
            }
            for (std::size_t at = node.first; at < node.first + node.count; ++at) {
                const std::size_t j = bvh.order[at];
                if (j >= i || triangles[j].part == t.part || !near(t.box, triangles[j].box)) continue;
                if (++stats.candidatePairs > pairBudget) {
                    stats.searchLimitExceeded = true;
                    return stats;
                }
                if (!overlaps(t, triangles[j])) continue;
                ++stats.overlapPairs;
                layers[i] = std::max(layers[i], layers[j] + 1);
                stats.maxLayer = std::max(stats.maxLayer, layers[i]);
                if (stats.maxLayer > kMaxLayer) {
                    stats.layerLimitExceeded = true;
                    return stats;
                }
            }
        }
    }
    const std::size_t shifted = static_cast<std::size_t>(std::count_if(layers.begin(), layers.end(),
        [](std::size_t layer) { return layer != 0; }));
    if (shifted == 0) return stats;
    require(shifted <= (std::numeric_limits<std::uint32_t>::max() - model.vertices.size()) / 3,
            "Stage surface private vertex index extent");
    auto vertices = model.vertices;
    auto indices = model.indices;
    auto bounds = model.bounds;
    vertices.reserve(vertices.size() + shifted * 3);
    for (std::size_t i = 0; i < triangles.size(); ++i) {
        if (layers[i] == 0) continue;
        const double distance = kLayerDistance * static_cast<double>(layers[i]);
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const auto& original = model.vertices[model.indices[i * 3 + corner]];
            auto vertex = original;
            vertex.x = static_cast<float>(original.x + triangles[i].normal[0] * distance);
            vertex.y = static_cast<float>(original.y + triangles[i].normal[1] * distance);
            vertex.z = static_cast<float>(original.z + triangles[i].normal[2] * distance);
            require(std::isfinite(vertex.x) && std::isfinite(vertex.y) && std::isfinite(vertex.z),
                    "Stage surface unrepresentable offset");
            const D3 displacement{double(vertex.x) - original.x, double(vertex.y) - original.y,
                                  double(vertex.z) - original.z};
            require(dot(displacement, triangles[i].normal) > 0.0,
                    "Stage surface offset below coordinate precision");
            indices[i * 3 + corner] = static_cast<std::uint32_t>(vertices.size());
            vertices.push_back(vertex);
            const std::array<float, 3> position{vertex.x, vertex.y, vertex.z};
            for (std::size_t axis = 0; axis < 3; ++axis) {
                bounds[axis] = std::min(bounds[axis], position[axis]);
                bounds[axis + 3] = std::max(bounds[axis + 3], position[axis]);
            }
        }
    }
    model.vertices.swap(vertices);
    model.indices.swap(indices);
    model.bounds = bounds;
    stats.shiftedTriangles = shifted;
    return stats;
}

}
