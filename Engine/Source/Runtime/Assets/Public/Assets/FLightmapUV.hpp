#pragma once

#include "Assets/UStaticMesh.hpp"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Leon {

    struct FLightmapUVValidationResult {
        bool bValid = false;
        bool bHasUV1 = false;
        bool bInUnitSquare = true;
        bool bHasOverlaps = false;
        bool bHasEnoughPadding = true;
        std::string Message;
    };

    /**
     * @brief Offline lightmap UV helpers (validation + simple chart generation).
     * Never run per-frame — asset pipeline / Lightmass only.
     */
    class FLightmapUV {
    public:
        /** True if any vertex has LightmapUV outside a tiny epsilon of (0,0) or mesh flagged. */
        static bool HasLightmapUV(const UStaticMesh& InMesh);

        /**
         * Generate non-overlapping UV1 via per-triangle packing into [0,1] with padding.
         * Deterministic given the same geometry order.
         */
        static void GenerateBoxPackedLightmapUVs(UStaticMesh& InMesh, float InPadding = 0.02f);

        static FLightmapUVValidationResult Validate(const UStaticMesh& InMesh, float InMinIslandPadding = 0.005f);

        /** Barycentric helpers for texel reconstruction. */
        static bool ComputeBarycentric(const glm::vec2& InP, const glm::vec2& InA, const glm::vec2& InB,
                                         const glm::vec2& InC, glm::vec3& OutBary);

        static glm::vec3 Interpolate(const glm::vec3& InA, const glm::vec3& InB, const glm::vec3& InC,
                                      const glm::vec3& InBary);
    };

} // namespace Leon
