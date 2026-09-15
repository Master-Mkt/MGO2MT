#pragma once

#include "character_model.h"
#include <cstddef>

namespace mgo2win::stage {

struct SurfaceLayerStats {
    std::size_t triangles = 0;
    std::size_t overlapPairs = 0;
    std::size_t shiftedTriangles = 0;
    std::size_t maxLayer = 0;
    std::size_t candidatePairs = 0;
    std::size_t degenerateTriangles = 0;
    bool layerLimitExceeded = false;
    bool searchLimitExceeded = false;
};

// Native rendering mitigation, not recovered original material/physics policy.
// Call on a rendering COPY. Only triangles in different parts with matching
// geometric winding, mutually coplanar vertices (0.05 units) and positive-area
// projected overlap are ordered. Later part/index order receives the higher
// layer, at 0.5 native units/layer along its geometric face normal.
//
// Each shifted triangle receives three private vertex copies. Attributes and
// materials are preserved; only xyz, its three indices and expanded bounds
// change. Unmatched neighboring triangles are NOT shifted merely to propagate
// a layer: a small step can remain along a partially overlapped patch boundary.
//
// Over 16 layers or a bounded candidate-search budget: return flags and do not
// mutate the model. Counts/maxLayer then describe the search up to its abort,
// not the entire model (maxLayer >=17 on a layer-limit abort). Invalid input
// throws std::runtime_error. All validation/analysis/allocation precedes commit.
SurfaceLayerStats separate_stage_surfaces(CharacterModel& renderingCopy);

}
