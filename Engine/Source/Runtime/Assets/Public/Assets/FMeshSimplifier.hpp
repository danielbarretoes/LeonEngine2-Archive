#pragma once

#include "Assets/UStaticMesh.hpp"
#include "Assets/FLODSettings.hpp"

namespace Leon {

    /**
     * CPU mesh reduction used to build automatic static-mesh LODs.
     * Per-submesh vertex clustering (grid) keeps material boundaries; normals/tangents
     * are rebuilt after collapse so PBR still has a valid TBN.
     */
    struct FMeshSimplifier {
        static bool Simplify(const std::vector<FStaticMeshVertex>& InVertices, const std::vector<uint32_t>& InIndices,
                             const std::vector<FStaticSubmesh>& InSubmeshes, float InTriangleRatio,
                             uint32_t InMinTriangleCount, FStaticMeshLOD& OutLOD);
    };

} // namespace Leon
