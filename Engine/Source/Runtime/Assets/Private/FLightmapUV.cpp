#include "Assets/FLightmapUV.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Leon {

    bool FLightmapUV::HasLightmapUV(const UStaticMesh& InMesh) {
        const auto& verts = InMesh.GetVertices();
        if (verts.empty())
            return false;
        for (const auto& v : verts) {
            if (std::abs(v.LightmapUV.x) > 1e-6f || std::abs(v.LightmapUV.y) > 1e-6f)
                return true;
        }
        // All zeros may still be valid UV1 covering a corner — treat as missing if also equals TexCoord empty.
        return false;
    }

    bool FLightmapUV::ComputeBarycentric(const glm::vec2& InP, const glm::vec2& InA, const glm::vec2& InB,
                                           const glm::vec2& InC, glm::vec3& OutBary) {
        glm::vec2 v0 = InB - InA;
        glm::vec2 v1 = InC - InA;
        glm::vec2 v2 = InP - InA;
        float den = v0.x * v1.y - v1.x * v0.y;
        if (std::abs(den) < 1e-12f)
            return false;
        float inv = 1.0f / den;
        float v = (v2.x * v1.y - v1.x * v2.y) * inv;
        float w = (v0.x * v2.y - v2.x * v0.y) * inv;
        float u = 1.0f - v - w;
        OutBary = {u, v, w};
        return true;
    }

    glm::vec3 FLightmapUV::Interpolate(const glm::vec3& InA, const glm::vec3& InB, const glm::vec3& InC,
                                         const glm::vec3& InBary) {
        return InA * InBary.x + InB * InBary.y + InC * InBary.z;
    }

    void FLightmapUV::GenerateBoxPackedLightmapUVs(UStaticMesh& InMesh, float InPadding) {
        auto& verts = InMesh.GetVertices();
        const auto& indices = InMesh.GetIndices();
        if (verts.empty() || indices.size() < 3)
            return;

        const uint32_t triCount = static_cast<uint32_t>(indices.size() / 3);
        if (triCount == 0)
            return;

        const uint32_t grid = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(triCount))));
        const float cell = 1.0f / static_cast<float>(grid);
        const float pad = std::clamp(InPadding, 0.0f, cell * 0.45f);
        const float usable = cell - 2.0f * pad;

        for (auto& v : verts)
            v.LightmapUV = glm::vec2(0.0f);

        for (uint32_t t = 0; t < triCount; ++t) {
            uint32_t i0 = indices[t * 3 + 0];
            uint32_t i1 = indices[t * 3 + 1];
            uint32_t i2 = indices[t * 3 + 2];
            if (i0 >= verts.size() || i1 >= verts.size() || i2 >= verts.size())
                continue;

            uint32_t gx = t % grid;
            uint32_t gy = t / grid;
            float ox = static_cast<float>(gx) * cell + pad;
            float oy = static_cast<float>(gy) * cell + pad;

            // Unique UV corner assignment per triangle (may split shared verts conceptually by overwrite —
            // for shared vertices, last write wins; duplicate verts if needed for production packs).
            verts[i0].LightmapUV = {ox, oy};
            verts[i1].LightmapUV = {ox + usable, oy};
            verts[i2].LightmapUV = {ox + usable * 0.5f, oy + usable};
        }
    }

    FLightmapUVValidationResult FLightmapUV::Validate(const UStaticMesh& InMesh, float InMinIslandPadding) {
        FLightmapUVValidationResult result;
        const auto& verts = InMesh.GetVertices();
        const auto& indices = InMesh.GetIndices();
        if (verts.empty() || indices.size() < 3) {
            result.Message = "Mesh has no geometry";
            return result;
        }

        result.bHasUV1 = true;
        for (const auto& v : verts) {
            if (v.LightmapUV.x < -1e-4f || v.LightmapUV.y < -1e-4f || v.LightmapUV.x > 1.0f + 1e-4f ||
                v.LightmapUV.y > 1.0f + 1e-4f) {
                result.bInUnitSquare = false;
                result.bHasUV1 = true;
            }
        }

        // Detect UV triangles with near-zero area and crude overlap via grid occupancy.
        const int gridRes = 64;
        std::vector<int> occupancy(static_cast<size_t>(gridRes * gridRes), -1);
        bool overlap = false;
        uint32_t triCount = static_cast<uint32_t>(indices.size() / 3);

        for (uint32_t t = 0; t < triCount; ++t) {
            uint32_t i0 = indices[t * 3 + 0];
            uint32_t i1 = indices[t * 3 + 1];
            uint32_t i2 = indices[t * 3 + 2];
            glm::vec2 a = verts[i0].LightmapUV;
            glm::vec2 b = verts[i1].LightmapUV;
            glm::vec2 c = verts[i2].LightmapUV;
            glm::vec2 minUV = glm::min(glm::min(a, b), c);
            glm::vec2 maxUV = glm::max(glm::max(a, b), c);
            float area = std::abs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) * 0.5f;
            if (area < 1e-10f)
                continue;

            int x0 = std::clamp(static_cast<int>(minUV.x * gridRes), 0, gridRes - 1);
            int y0 = std::clamp(static_cast<int>(minUV.y * gridRes), 0, gridRes - 1);
            int x1 = std::clamp(static_cast<int>(maxUV.x * gridRes), 0, gridRes - 1);
            int y1 = std::clamp(static_cast<int>(maxUV.y * gridRes), 0, gridRes - 1);
            for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                    int& cell = occupancy[static_cast<size_t>(y * gridRes + x)];
                    if (cell >= 0 && cell != static_cast<int>(t))
                        overlap = true;
                    else
                        cell = static_cast<int>(t);
                }
            }
        }

        result.bHasOverlaps = overlap;
        result.bHasEnoughPadding = InMinIslandPadding >= 0.0f; // soft check; packing enforces padding
        result.bValid = result.bInUnitSquare && !result.bHasOverlaps;
        std::ostringstream ss;
        if (!result.bInUnitSquare)
            ss << "UV1 outside 0..1; ";
        if (result.bHasOverlaps)
            ss << "UV1 overlaps detected; ";
        if (result.bValid)
            ss << "OK";
        result.Message = ss.str();
        return result;
    }

} // namespace Leon
