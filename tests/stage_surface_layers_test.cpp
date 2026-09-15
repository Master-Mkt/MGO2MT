#include "stage_surface_layers.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using namespace mgo2win;
using namespace mgo2win::stage;
using Point = std::array<float, 3>;
using Face = std::array<Point, 3>;

void require(bool value, std::string_view message) {
    if (!value) throw std::runtime_error(std::string(message));
}
void close(double a, double b, std::string_view message) {
    require(std::abs(a - b) < 0.00001, message);
}
Face flat(float x = 0, float y = 0, float z = 0, float size = 10) {
    return {{{x, y, z}, {x + size, y, z}, {x, y + size, z}}};
}
CharacterModel emptyModel() {
    CharacterModel model;
    model.textures.push_back({4, 4, 9, {1, 2, 3, 4, 5, 6, 7, 8}});
    model.textures.push_back({4, 4, 9, {8, 7, 6, 5, 4, 3, 2, 1}});
    return model;
}
void addFace(CharacterModel& model, const Face& points) {
    for (const auto& p : points) {
        ModelVertex v{p[0], p[1], p[2], 0.2f, 0.3f, 0.4f, 0.1f, 0.7f};
        // Deliberately unrelated to the face normal: these must be preserved.
        v.u1 = 0.6f; v.v1 = 0.9f;
        v.lr = 0.8f; v.lg = 0.3f; v.lb = 0.5f; v.lit = 0.4f;
        v.ar = 0.7f; v.ag = 0.6f; v.ab = 0.2f; v.aa = 0.1f;
        model.indices.push_back(static_cast<std::uint32_t>(model.vertices.size()));
        model.vertices.push_back(v);
        for (std::size_t axis = 0; axis < 3; ++axis) {
            model.bounds[axis] = std::min(model.bounds[axis], p[axis]);
            model.bounds[axis + 3] = std::max(model.bounds[axis + 3], p[axis]);
        }
    }
}
void addPart(CharacterModel& model, const Face& face) {
    const auto first = static_cast<std::uint32_t>(model.indices.size());
    addFace(model, face);
    const auto texture = static_cast<std::uint32_t>(model.parts.size() % 2);
    model.parts.push_back({first, 3, texture, 0xabcdefu, 100064, {0.8f, 0.4f, 0.2f}});
}
bool sameVertex(const ModelVertex& a, const ModelVertex& b) {
    return std::memcmp(&a, &b, sizeof(ModelVertex)) == 0;
}
bool sameModel(const CharacterModel& a, const CharacterModel& b) {
    if (a.bounds != b.bounds || a.overviewBounds != b.overviewBounds ||
        a.hasOverviewBounds != b.hasOverviewBounds || a.indices != b.indices ||
        a.vertices.size() != b.vertices.size() || a.parts.size() != b.parts.size() ||
        a.textures.size() != b.textures.size()) return false;
    for (std::size_t i = 0; i < a.vertices.size(); ++i) {
        if (!sameVertex(a.vertices[i], b.vertices[i])) return false;
    }
    for (std::size_t i = 0; i < a.parts.size(); ++i) {
        const auto& x = a.parts[i]; const auto& y = b.parts[i];
        if (x.first != y.first || x.count != y.count || x.texture != y.texture ||
            x.flags != y.flags || x.materialShader != y.materialShader || x.tint != y.tint) return false;
    }
    for (std::size_t i = 0; i < a.textures.size(); ++i) {
        const auto& x = a.textures[i]; const auto& y = b.textures[i];
        if (x.width != y.width || x.height != y.height || x.codec != y.codec || x.pixels != y.pixels) return false;
    }
    return true;
}
void unchangedPair(const Face& a, const Face& b, std::string_view message) {
    auto model = emptyModel(); addPart(model, a); addPart(model, b);
    const auto original = model;
    const auto stats = separate_stage_surfaces(model);
    require(stats.triangles == 2 && stats.overlapPairs == 0 && stats.shiftedTriangles == 0 &&
                stats.maxLayer == 0 && !stats.layerLimitExceeded && !stats.searchLimitExceeded, message);
    require(sameModel(model, original), "An excluded pair must be completely untouched");
}

void excludedGeometry() {
    auto empty = emptyModel();
    require(separate_stage_surfaces(empty).triangles == 0, "Empty rendering copy is a no-op");
    unchangedPair(flat(), flat(30), "Separated triangles must not be biased");
    unchangedPair(flat(0, 0, 0, 4), flat(3, 3, 0, 4), "AABB overlap is not triangle overlap");
    unchangedPair(flat(), {{{10, 0, 0}, {10, 10, 0}, {0, 10, 0}}},
                  "A shared diagonal has zero intersection area");
    unchangedPair(flat(), flat(10), "A single shared point has zero area");
    auto reversed = flat(); std::swap(reversed[1], reversed[2]);
    unchangedPair(flat(), reversed, "Opposite winding must not be reordered");
    auto tilted = flat(); tilted[2][2] = 0.01f;
    unchangedPair(flat(), tilted, "Small plane distances alone do not imply parallel normals");
    unchangedPair(flat(), flat(0, 0, 0.051f), "Distinct parallel planes outside tolerance stay put");
    unchangedPair(flat(), flat(0, 0, std::nextafter(0.05f, 1.0f)), "Next float above plane threshold stays put");
    // A's vertices are near B's almost parallel plane, but B has a distant tip.
    unchangedPair(flat(0, 0, 0, 1), {{{0, 0, 0}, {100000, 0, 0}, {0, 100000, 10}}},
                  "Every vertex on both sides must satisfy the plane test");
    auto model = emptyModel(); addPart(model, flat()); addFace(model, flat());
    model.parts[0].count = 6;
    const auto original = model;
    require(separate_stage_surfaces(model).overlapPairs == 0 && sameModel(model, original),
            "Duplicates within one part are excluded");
    auto zero = emptyModel(); addPart(zero, {{{0, 0, 0}, {1, 0, 0}, {2, 0, 0}}}); addPart(zero, flat());
    require(separate_stage_surfaces(zero).degenerateTriangles == 1, "Degenerate faces are counted and skipped");
}

void thresholdAndNormalOffset() {
    for (float z : {0.0f, 0.04f, -0.04f, 0.05f}) {
        auto model = emptyModel(); addPart(model, flat()); addPart(model, flat(0, 0, z));
        const auto stats = separate_stage_surfaces(model);
        require(stats.overlapPairs == 1 && stats.shiftedTriangles == 1 && stats.maxLayer == 1,
                "Coplanar/near threshold pair must create one upper layer");
        close(model.vertices[model.indices[3]].z, double(z) + 0.5, "Offset must follow face normal");
        const auto separated = model;
        require(separate_stage_surfaces(model).shiftedTriangles == 0 && sameModel(model, separated),
                "Already separated geometry must not accumulate another bias");
    }
    auto model = emptyModel();
    const Face slope{{{100000, 200000, 300000}, {100010, 200000, 300010}, {100000, 200010, 300010}}};
    addPart(model, slope); addPart(model, slope);
    const auto stats = separate_stage_surfaces(model);
    require(stats.shiftedTriangles == 1, "Sloped translated overlap must be recognized");
    const auto& v = model.vertices[model.indices[3]];
    // Float output at this world coordinate has a 0.03125-unit quantization.
    require(std::abs((double(v.x) - slope[0][0]) + 0.5 / std::sqrt(3.0)) < 0.032 &&
            std::abs((double(v.y) - slope[0][1]) + 0.5 / std::sqrt(3.0)) < 0.032 &&
            std::abs((double(v.z) - slope[0][2]) - 0.5 / std::sqrt(3.0)) < 0.032,
            "Use geometric slope normal instead of authored vertex normals or a fixed axis");
    auto negative = emptyModel(); auto face = flat(); std::swap(face[1], face[2]);
    addPart(negative, face); addPart(negative, face);
    separate_stage_surfaces(negative);
    close(negative.vertices[negative.indices[3]].z, -0.5, "Matching reverse-facing faces shift along negative normal");

    // Dropping the largest-normal axis may hide more than 0.05 of axis
    // separation even though the perpendicular plane distance is under 0.05.
    // This pair has an X-axis AABB gap of 0.079, but plane gap about 0.0465.
    // Make X the unambiguous dominant normal despite float input rounding.
    const Face tinySlope{{{0, 0, 0}, {0.001f, 0, 0.00101f}, {0, 0.001f, 0.001f}}};
    auto shiftedSlope = tinySlope;
    for (auto& point : shiftedSlope) point[0] += 0.08f;
    auto broadPhase = emptyModel(); addPart(broadPhase, tinySlope); addPart(broadPhase, shiftedSlope);
    const auto broadStats = separate_stage_surfaces(broadPhase);
    require(broadStats.overlapPairs == 1 && broadStats.shiftedTriangles == 1,
            "Broad phase must retain a sloped near-plane pair separated along the projected-away axis");
    // Larger AABB tolerance must not weaken the perpendicular-plane predicate.
    unchangedPair(flat(), flat(0, 0, 0.08f), "Broader AABB must retain the strict 0.05 plane rejection");
}

void layersAndPrivateVertices() {
    auto source = emptyModel(); addPart(source, flat());
    source.parts.push_back({3, 3, 1, 0x12, 100063, {0.7f, 0.6f, 0.5f}});
    source.indices.insert(source.indices.end(), {0, 1, 2}); // Shared with base part.
    source.parts.push_back({6, 3, 0, 0x34, 100065, {0.2f, 0.4f, 0.6f}});
    source.indices.insert(source.indices.end(), {0, 1, 2});
    source.hasOverviewBounds = true; source.overviewBounds = {-100, -200, -300, 100, 200, 300};
    const auto sourceSnapshot = source;
    auto draw = source;
    const auto stats = separate_stage_surfaces(draw);
    require(stats.triangles == 3 && stats.overlapPairs == 3 && stats.shiftedTriangles == 2 && stats.maxLayer == 2,
            "Three overlapping parts need distinct ordered layers");
    require(sameModel(source, sourceSnapshot), "Source/physics model must remain untouched when copy is processed");
    require(draw.vertices.size() == source.vertices.size() + 6 && draw.indices.size() == source.indices.size(),
            "Keep geometry and allocate three private vertices for each shifted triangle");
    for (std::size_t i = 0; i < source.vertices.size(); ++i) {
        require(sameVertex(draw.vertices[i], source.vertices[i]), "Original shared vertices must never move");
    }
    for (std::size_t tri = 0; tri < 3; ++tri) {
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const std::size_t at = tri * 3 + corner;
            auto actual = draw.vertices[draw.indices[at]];
            const auto& original = source.vertices[source.indices[at]];
            close(actual.z, double(tri) * 0.5, "Later original order receives the upper layer");
            actual.x = original.x; actual.y = original.y; actual.z = original.z;
            require(sameVertex(actual, original), "Normals, UV0/UV1, light and authored color must be preserved");
        }
    }
    auto metadata = draw; metadata.bounds = source.bounds; metadata.indices = source.indices; metadata.vertices = source.vertices;
    require(sameModel(metadata, source), "Parts, material/shader/tint/flags, textures and overview bounds must be retained");
    close(draw.bounds[5], 1.0, "Rendering bounds must include the displaced vertices");

    auto partial = emptyModel(); addPart(partial, flat()); addPart(partial, flat());
    addFace(partial, {{{10, 0, 0}, {10, 10, 0}, {0, 10, 0}}}); partial.parts[1].count = 6;
    const auto unpaired = partial.indices[6];
    const auto partialStats = separate_stage_surfaces(partial);
    require(partialStats.shiftedTriangles == 1 && partial.indices[6] == unpaired,
            "Do not propagate bias into a merely adjacent, nonoverlapping triangle");
}

void atomicLimitsAndValidation() {
    auto limited = emptyModel();
    for (int i = 0; i < 18; ++i) addPart(limited, flat());
    const auto original = limited;
    const auto stats = separate_stage_surfaces(limited);
    require(stats.layerLimitExceeded && stats.maxLayer > 16 && stats.shiftedTriangles == 0 && sameModel(limited, original),
            "Layer overflow must leave the entire rendering copy unchanged");

    auto valid = emptyModel(); addPart(valid, flat()); addPart(valid, flat());
    auto reject = [&](auto mutate) {
        auto bad = valid; mutate(bad);
        const auto indices = bad.indices;
        const auto vertexCount = bad.vertices.size();
        bool rejected = false;
        try { separate_stage_surfaces(bad); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected && bad.indices == indices && bad.vertices.size() == vertexCount,
                "Invalid input must be rejected before geometry mutation");
    };
    reject([](auto& m) { m.vertices[0].x = std::numeric_limits<float>::quiet_NaN(); });
    reject([](auto& m) { m.vertices[0].u1 = std::numeric_limits<float>::infinity(); });
    reject([](auto& m) { m.vertices[0].nz = -std::numeric_limits<float>::infinity(); });
    reject([](auto& m) { m.bounds[0] = std::numeric_limits<float>::quiet_NaN(); });
    reject([](auto& m) { m.parts[0].tint[1] = std::numeric_limits<float>::quiet_NaN(); });
    reject([](auto& m) { m.indices[0] = 9999; });
    reject([](auto& m) { m.indices.pop_back(); });
    reject([](auto& m) { m.parts[1].first = 0; });
    reject([](auto& m) { m.parts[1].count = 6; });
    reject([](auto& m) { m.parts[0].texture = 99; });
}

void largeSpatialMesh() {
    constexpr std::size_t count = 107500;
    auto model = emptyModel();
    model.vertices.reserve(count * 6); model.indices.reserve(count * 6);
    for (std::size_t part = 0; part < 2; ++part) {
        const auto first = static_cast<std::uint32_t>(model.indices.size());
        for (std::size_t i = 0; i < count; ++i) {
            addFace(model, flat(static_cast<float>(i % 500) * 3, static_cast<float>(i / 500) * 3, 0, 1));
        }
        model.parts.push_back({first, static_cast<std::uint32_t>(count * 3), static_cast<std::uint32_t>(part), 0});
    }
    const auto start = std::chrono::steady_clock::now();
    const auto stats = separate_stage_surfaces(model);
    const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    require(stats.triangles == count * 2 && stats.overlapPairs == count && stats.shiftedTriangles == count &&
            stats.maxLayer == 1 && !stats.searchLimitExceeded && !stats.layerLimitExceeded,
            "QQ-sized 215k-triangle mesh must separate its spatially isolated matching pairs");
    require(stats.candidatePairs <= count * 2, "Broad phase must avoid quadratic pair enumeration on a spatial mesh");
    close(model.vertices[model.indices[count * 3]].z, 0.5, "Large mesh first upper triangle");
    close(model.vertices[model.indices.back()].z, 0.5, "Large mesh final upper triangle");
    std::cout << "215000 triangles: " << stats.candidatePairs << " candidates, " << seconds << " seconds\n";
}

void denseSearchFallback() {
    auto model = emptyModel();
    for (std::size_t part = 0; part < 2; ++part) {
        const auto first = static_cast<std::uint32_t>(model.indices.size());
        for (int i = 0; i < 1100; ++i) addFace(model, flat());
        model.parts.push_back({first, 3300, static_cast<std::uint32_t>(part), 0});
    }
    const auto original = model;
    const auto stats = separate_stage_surfaces(model);
    require(stats.searchLimitExceeded && !stats.layerLimitExceeded && stats.shiftedTriangles == 0 && sameModel(model, original),
            "Pathological dense overlap must stop within a bounded search and leave no partial bias");
}
}

int main() {
    try {
        excludedGeometry();
        thresholdAndNormalOffset();
        layersAndPrivateVertices();
        atomicLimitsAndValidation();
        largeSpatialMesh();
        denseSearchFallback();
        std::cout << "Stage surface layers: strict overlap, facing/plane limits, COW attributes, atomic fallback and BVH passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
